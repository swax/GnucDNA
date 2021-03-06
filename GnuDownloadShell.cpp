/********************************************************************************

	GnucDNA - The Gnucleus Library
	Copyright (C) 2000-2005 John Marshall Group

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	By contributing code you grant John Marshall Group an unlimited, non-exclusive
	license to your contribution.

	For support, questions, commercial use, etc...
	E-Mail: swabby@c0re.net

********************************************************************************/


#include "stdafx.h"
#include "GnuCore.h"
#include "GnuPrefs.h"

#include "GnuTransfers.h"
#include "GnuCache.h"
#include "GnuRouting.h"
#include "GnuNetworks.h"
#include "GnuControl.h"
#include "GnuProtocol.h"
#include "G2Control.h"
#include "GnuNode.h"
#include "GnuShare.h"
#include "GnuMeta.h"
#include "GnuSchema.h"
#include "GnuSearch.h"
#include "GnuFileHash.h"
#include "hash/tigertree2.h"

#include <process.h>

#include "GnuDownload.h"
#include "GnuDownloadShell.h"


#define BUFF_SIZE  32768

CGnuDownloadShell::CGnuDownloadShell(CGnuTransfers* pTrans)
{
	m_pTrans = pTrans;
	m_pNet   = pTrans->m_pNet;
	m_pPrefs = pTrans->m_pPrefs;
	m_pCore  = pTrans->m_pCore;


	if(pTrans->m_NextDownloadID < 1)
		pTrans->m_NextDownloadID = 1;

	m_DownloadID = pTrans->m_NextDownloadID++;
	pTrans->m_DownloadMap[m_DownloadID] = this;

	m_ShellStatus = ePending;
	
	m_Cooling    = 0;
	m_HostTryPos = 1;
	m_G2ResearchInt = G2_RESEARCH_INT;
	
	m_HashComputed = false;
	m_HashVerified = false;
	m_FileMoved    = false;

	m_TotalSecCount = 0;

	m_Retry			  = false;
	m_UpdatedInSecond = false;

	m_MetaID = 0;

	m_NextHostID  = 1;

	m_BackupBytes = 0;
	m_BackupHosts = 0;
	m_BackupInterval = 0;
	
	// Research
	m_SearchID = 0;

	m_NextReSearch		= time(NULL) + (60*60);
	m_ReSearchInterval	= 0;

	// Tiger Tree
	m_TigerTree = NULL;
	m_TreeSize  = 0;
	m_TreeRes   = 0;

	// Proxy
	m_UseProxy = false;

	// Bandwidth
	m_AllocBytes      = 0;
	m_AllocBytesTotal = 0;
	m_AvgSpeed		  = 0;

	m_LastConnectAttempt = 0;

	// Part Size
	m_PartSize = DOWNLOAD_CHUNK_SIZE;  // 512KB // Must be power of 2
}

CGnuDownloadShell::~CGnuDownloadShell()
{
	std::map<int, CGnuDownloadShell*>::iterator itDown = m_pTrans->m_DownloadMap.find(m_DownloadID);
	if(itDown != m_pTrans->m_DownloadMap.end())
		m_pTrans->m_DownloadMap.erase(itDown);

	std::map<CString, CGnuDownloadShell*>::iterator itDownHash = m_pTrans->m_DownloadHashMap.find(m_Sha1Hash);
	if(itDownHash != m_pTrans->m_DownloadHashMap.end())
		m_pTrans->m_DownloadHashMap.erase(itDownHash);

	// Update search result
	std::map<CString, ResultGroup*>::iterator itHash;
	for(int i = 0; i < m_pNet->m_SearchList.size(); i++)
	{
		itHash = m_pNet->m_SearchList[i]->m_ResultHashMap.find(m_Sha1Hash);
		if(itHash != m_pNet->m_SearchList[i]->m_ResultHashMap.end())
		{
			ResultGroup* pResult = itHash->second;
			
			pResult->State = m_pNet->m_SearchList[i]->UpdateResultState(m_Sha1Hash);
			m_pNet->m_SearchList[i]->TransferUpdate(pResult->ResultID);
		}
	}

	m_ShellAccess.Lock();

	m_ShellStatus = eDone;

	m_Cooling = 0;
	
	while(m_Sockets.size())
	{
		CGnuDownload* pDead = m_Sockets.back();
		m_Sockets.pop_back();
		delete pDead;
	}

	if(m_TigerTree)
	{
		delete [] m_TigerTree;
		m_TigerTree = NULL;
	}

	m_pNet->EndSearch(m_SearchID);

	// Close file
	m_File.Abort();
	//m_CheckFile.Abort();

	m_ShellAccess.Unlock();
}

void CGnuDownloadShell::Init(CString Name, uint64 FileSize, int HashID, CString Hash)
{
	m_Name = Name;

	m_FileLength = FileSize;

	if( m_FileLength )
		CreatePartList();

	if(HashID == HASH_SHA1)
		m_Sha1Hash = Hash;
	
	if( !Hash.IsEmpty() )
	{
		m_pTrans->m_DownloadHashMap[m_Sha1Hash] = this;
		//m_CheckFile.Open( m_pPrefs->m_DownloadPath + "\\" + Hash.Left(4) + ".mp3", CFile::modeRead);
	}
}

void CGnuDownloadShell::CreatePartList()
{
	ASSERT(m_FileLength);

	m_PartList.clear();

	// Create file parts
	for(uint64 i = 0; i < m_FileLength; i += m_PartSize)
	{
		FilePart NewPart;

		NewPart.StartByte = i;
		
		if(i + m_PartSize > m_FileLength)
			NewPart.EndByte = m_FileLength - 1;
		else
			NewPart.EndByte = i + m_PartSize - 1;

		NewPart.BytesCompleted = 0;
		NewPart.SourceHostID   = 0;
		NewPart.Verified       = false;

		m_PartList.push_back(NewPart);
	}
}

void CGnuDownloadShell::AddHost(FileSource HostInfo)
{
	// filter for self
	if((HostInfo.Address.Host.S_addr == m_pNet->m_CurrentIP.S_addr ||
		StrtoIP("127.0.0.1").S_addr	 == HostInfo.Address.Host.S_addr  ) && 
		HostInfo.Address.Port	     == m_pNet->m_CurrentPort)
		return;

	// Usually DownloadFile with unknown size
	if( m_FileLength == 0 && HostInfo.Size && HostInfo.Sha1Hash == m_Sha1Hash)
	{
		m_FileLength = HostInfo.Size;
		CreatePartList();
	}		


	if(HostInfo.Size != m_FileLength || HostInfo.Sha1Hash != m_Sha1Hash)
		return;


	if(m_ShellStatus == eCooling || m_ShellStatus == eWaiting)
		m_ShellStatus = ePending;


	int SubnetLimit = 0;

	// Check for duplicate hosts
	if (m_AddressMap.find(HostInfo.Address) != m_AddressMap.end())
	{		
		unsigned i = m_AddressMap[HostInfo.Address];

		m_Queue[i].FileIndex = HostInfo.FileIndex;
		m_Queue[i].Name      = HostInfo.Name;
		m_Queue[i].NameLower = HostInfo.NameLower;
		m_Queue[i].Vendor    = HostInfo.Vendor;

		m_Queue[i].GnuRouteID = HostInfo.GnuRouteID;
		m_Queue[i].PushID     = HostInfo.PushID;
		m_Queue[i].RetryWait  = 0;

		if (m_Queue[i].Status == FileSource::eFailed)
			SetHostState(i, FileSource::eUntested);

		if(HostInfo.SupportF2F)
			m_Queue[i].SupportF2F = true;

		m_UntestedQueue.push_back(i);
		m_UdpQueue.push_back(m_Queue[i]);
		
		return;
	}

	// new host - add to the subnet count
	if (!IsPrivateIP(HostInfo.Address.Host))
	{
		IP Subnet = HostInfo.Address.Host;
		Subnet.d = 0;

		if (m_SubnetMap.find(Subnet) == m_SubnetMap.end())
			m_SubnetMap[Subnet] = 0;

		// only let the first SUBNET_LIMIT hosts of any subnet in
		if (m_SubnetMap[Subnet] >= SUBNET_LIMIT)
			return;

		m_SubnetMap[Subnet] = m_SubnetMap[Subnet] + 1;
	}

	HostInfo.Handshake = "";
	HostInfo.Error     = "";
	HostInfo.RetryWait = 0;
	HostInfo.Tries     = 0;
	HostInfo.Status    = FileSource::eUntested;

	HostInfo.RealBytesPerSec	= 0;

	HostInfo.SourceID = m_Queue.size();
	m_AddressMap[HostInfo.Address] = HostInfo.SourceID;
	m_UntestedQueue.push_back(HostInfo.SourceID);

	m_Queue.push_back(HostInfo);

	m_UdpQueue.push_back(HostInfo); // needs Address and Network

	if (m_ShellStatus == eCooling || m_ShellStatus == eWaiting)
		m_ShellStatus = eActive;

	m_UpdatedInSecond   = true;
}


void CGnuDownloadShell::TryNextHost()
{
	// dont max out half connections
	int MaxHalfConnects = m_pNet->GetMaxHalfConnects();

	if( m_pNet->NetworkConnecting(NETWORK_GNUTELLA) )
		MaxHalfConnects /= 2;
	if( m_pNet->NetworkConnecting(NETWORK_G2) )
		MaxHalfConnects /= 2;

	// return as soon as we know we're over the limit
	if( m_pNet->TransfersConnecting() >= MaxHalfConnects || m_pNet->TcpBacklog())
		return;

	// loop till we find a host to connect to
	while (true)
 	{
		unsigned PosToTry = -1;
		int status = 0;

		// TODO: Top priority hosts should be ones we've successfully
		// gotten pieces from, in order of speed

		// first try the ones that sent us udp back
		if (m_UdpSuccessQueue.size() > 0)
		{
			PosToTry = m_UdpSuccessQueue.front();
			m_UdpSuccessQueue.pop_front();

			status = FileSource::eUdpAlive;
		}

		// next try hosts we haven't tried yet
		else if (m_UntestedQueue.size() > 0)
		{
			PosToTry = m_UntestedQueue.front();
			m_UntestedQueue.pop_front();

			status = FileSource::eUntested;
		}

		else if (m_ReadyHeap.size() > 0)
		{
			unsigned i = m_ReadyHeap.front().first;
			uint32 t = m_ReadyHeap.front().second;
			pop_heap(m_ReadyHeap.begin(), m_ReadyHeap.end(), HeapComparator());
			m_ReadyHeap.pop_back();

			ASSERT(i < m_Queue.size());
			
			// if the LastTried time has been changed, ignore this entry
			if (m_Queue[i].LastTried != t)
				continue;

			m_Queue[i].OnHeapTime = 0;
		
			status = FileSource::eAlive | FileSource::eTryAgain;
			PosToTry = i;
		}

		// we failed if we have no one to pick
		if (PosToTry == -1)
			break;

		ASSERT(PosToTry < m_Queue.size());

		// if this host is already being tried, don't try it again
		// if the state isn't one we anticipated, don't use this host
		if (m_Queue[PosToTry].HasDownload ||
			!(status & m_Queue[PosToTry].Status))
			continue;
 
		// host is good - lets go!
		CreateDownload( m_Queue[PosToTry] );
		return;
	}


	// All hosts in list have been attempted
	// And nothing is trying to connect
	if(m_Sockets.size() == 0)
	{		
		if (m_WaitHeap.size())
		{
			m_Retry   = true; // Gone through host list once so now we are retrying hosts
			m_ShellStatus = eCooling;
			m_Cooling = m_WaitHeap.front().second - time(NULL);
		}
		else
 		{
			m_ShellStatus     = eWaiting;
			m_ReasonDead	  = "Waiting, more hosts needed";
			m_Retry			  = false;
			m_UpdatedInSecond = true;
			m_Cooling		  = 0;
		}
	}
} 

void CGnuDownloadShell::TryNextUdp ()
{
	// TODO: limit total UDP per second for transfers as a whole
	for (int i = 0; i < UDP_PER_TICK; i++)
	{
		if (m_UdpQueue.size () == 0)
			return;

		FileSource fs;
		fs = m_UdpQueue.front ();
		m_UdpQueue.pop_front ();

		UdpTransmit (fs);
	}
}

void CGnuDownloadShell::UdpTransmit(const FileSource &fs)
{
	// send out udp
	if(m_pNet->m_pGnu && fs.Network == NETWORK_GNUTELLA)
	{
		m_pNet->m_pGnu->m_pProtocol->Send_Ping(NULL, 1, false, NULL, fs.Address);
	}

	if(m_pNet->m_pG2 && fs.Network == NETWORK_G2)
	{
		G2_PI Ping;
		m_pNet->m_pG2->Send_PI(fs.Address, Ping, NULL);
	}
}

void CGnuDownloadShell::UdpResponse(IPv4 Source)
{
	std::map<IPv4,unsigned>::iterator itAddr = m_AddressMap.find(Source);

	if (itAddr == m_AddressMap.end())
		return;

	uint32 SourceID = itAddr->second;

	ASSERT(SourceID >= 0 && SourceID < m_Queue.size());

	// if we don't have multisource on, never open connections here
	// save up connections if our SYNs are all used up
	if( !m_pPrefs->m_Multisource || m_pNet->TcpBacklog() )
	{
		if (m_Queue[SourceID].Status == FileSource::eUntested)
		{
			SetHostState(SourceID, FileSource::eUdpAlive);
			m_UdpSuccessQueue.push_back(SourceID);
		}
	}
	else
	{
		CreateDownload( m_Queue[SourceID] );
	}
}

void CGnuDownloadShell::CreateDownload(FileSource &Source)
{
	Source.Error = "";
	Source.Tries++;
	SetHostState(Source.SourceID, FileSource::eTrying);
	Source.RetryWait = RETRY_WAIT;
	Source.LastTried = time(NULL);

	CGnuDownload* pSock = new CGnuDownload(this, Source.SourceID);

	if(pSock->StartDownload())
	{
		m_LastConnectAttempt = time(NULL);

		m_Sockets.push_back(pSock);
	}
	else
	{
		delete pSock;
		pSock = NULL;
	}
	
	if(m_HostTryPos >= m_Queue.size())
		m_HostTryPos = 1;
	else
		m_HostTryPos++;
}

void CGnuDownloadShell::SetHostState(uint32 SourceID, FileSource::States state)
{
	ASSERT(SourceID < m_Queue.size());
	FileSource &fs(m_Queue[SourceID]);

	if (state == FileSource::eTryAgain && fs.Status == FileSource::eFailed)
		return;

	ASSERT(state == FileSource::eTryAgain ? fs.Status &
		(FileSource::eTrying | FileSource::eTryAgain | FileSource::eAlive) : true);
	ASSERT(state == FileSource::eFailed ? fs.Status & 
		(FileSource::eTrying | FileSource::eTryAgain | FileSource::eAlive | FileSource::eFailed) : true);
	ASSERT(state == FileSource::eAlive ? fs.Status & 
		(FileSource::eTrying | FileSource::eTryAgain | FileSource::eAlive) : true);
	ASSERT(state == FileSource::eTryAgain ? fs.RetryWait > 0 : true);
	
	fs.Status = state;
}

void CGnuDownloadShell::AddToAliveHeap(uint32 SourceID)
{
	ASSERT(SourceID < m_Queue.size());
	FileSource &fs(m_Queue[SourceID]);

	if (!(fs.Status & (FileSource::eAlive | FileSource::eTryAgain)))
		return;

	ASSERT(fs.RetryWait == 0 || fs.TimeToTry <= time(NULL));

	if (fs.OnHeapTime == fs.LastTried)
		return; // already on heap
	
	fs.OnHeapTime = fs.LastTried;

	std::pair<unsigned,uint64> forheap(fs.SourceID,fs.LastTried);
	m_ReadyHeap.push_back(forheap);
	push_heap(m_ReadyHeap.begin(), m_ReadyHeap.end(), HeapComparator());
}

void CGnuDownloadShell::AddToWaitHeap(uint32 SourceID)
{
	ASSERT(SourceID < m_Queue.size());
	FileSource &fs(m_Queue[SourceID]);

	ASSERT(fs.Status & (FileSource::eAlive | FileSource::eTryAgain));
	ASSERT(fs.RetryWait > 0);

	// check if its already on the heap with this same time
	if (fs.TimeToTry == time(NULL) + fs.RetryWait)
		return;

	fs.TimeToTry = time(NULL) + fs.RetryWait;

	std::pair<unsigned,uint64> forheap(fs.SourceID,fs.TimeToTry);
	m_WaitHeap.push_back(forheap);
	push_heap(m_WaitHeap.begin(), m_WaitHeap.end(), HeapComparator());
}

#if 0
// use this to confirm the heap is working properly
bool CGnuDownloadShell::CheckHeap()
{
	std::vector<std::pair<unsigned, uint64> >::iterator i(m_ReadyHeap.begin());
	std::ostringstream heapstr;
	for (; i != m_ReadyHeap.end(); i++)
	{
		heapstr << "H>" << i->second << " (" << i->first << ")\n";
	}
	heapstr << "\n";
	TRACE0(heapstr.str().c_str());

	sort_heap(m_ReadyHeap.begin(),m_ReadyHeap.end(), HeapComparator());

	i = m_ReadyHeap.begin();
	std::ostringstream heapstr2;
	for (; i != m_ReadyHeap.end(); i++)
	{
		heapstr2 << "S>" << i->second << " (" << i->first << ")\n";
	}
	heapstr2 << "\n";
	TRACE0(heapstr2.str().c_str());

	if (m_ReadyHeap.size() <= 1)
		return true;

	i = m_ReadyHeap.begin();
	std::vector<std::pair<unsigned, uint64> >::iterator j(i+1);
	// now it should go recent->old (or high->low)
	for (; j != m_ReadyHeap.end(); i++, j++)
	{
		uint64 ti = i->second;
		uint64 tj = j->second;
		if (ti < tj)
			return false;
	}
	
	make_heap(m_ReadyHeap.begin(),m_ReadyHeap.end(), HeapComparator());
	return true;
}
#endif

void CGnuDownloadShell::ConnectionDeleted(uint32 SourceID)
{
	if (m_ShellStatus == eDone)
		return;

	ASSERT(SourceID < m_Queue.size());
	m_Queue[SourceID].HasDownload = false;

	FileSource &fs(m_Queue[SourceID]);

	if (m_Queue[SourceID].Status & (FileSource::eAlive | FileSource::eTryAgain))
	{
		if (m_Queue[SourceID].RetryWait == 0)
			AddToAliveHeap(SourceID);
		else
			AddToWaitHeap(SourceID);
	}
}

void CGnuDownloadShell::Start()
{
	m_HostTryPos = 1;
	
	if(m_FileLength == 0 || GetBytesCompleted() < m_FileLength)
	{
		m_ShellStatus = eActive;

		m_Cooling   = 0;

		//m_ReSearchInterval	= 15;
		m_NextReSearch		= time(NULL) + (60*60);
	}
}

// Should only be called from interface
void CGnuDownloadShell::Stop()
{
	m_HostTryPos = 1;

	m_UdpQueue.clear();
	m_UntestedQueue.clear();

	for(int i = 0; i < m_Queue.size(); i++)
	{
		m_Queue[i].Status = FileSource::eUntested;
		m_Queue[i].RetryWait = 0;
		m_Queue[i].Tries     = 0;
		m_UdpQueue.push_back(m_Queue[i]);
		m_UntestedQueue.push_back(i);
	}

	m_ShellStatus = eDone;
	
	m_Cooling   = 0;

	m_pNet->EndSearch(m_SearchID);
	
	m_ReasonDead  = "Stopped";
}

bool CGnuDownloadShell::IsDownloading()
{
	for(int i = 0; i < m_Sockets.size(); i++)
		if(m_Sockets[i]->m_Status == TRANSFER_RECEIVING)
			return true;

	return false;
}


bool CGnuDownloadShell::IsRemotelyQueued()
{
	for(int i = 0; i < m_Sockets.size(); i++)
		if(m_Sockets[i]->m_Status == TRANSFER_QUEUED)
			return true;

	return false;
}

CGnuDownload* CGnuDownloadShell::GetCurrent()
{
	for(int i = 0; i < m_Sockets.size(); i++)
		if(m_Sockets[i]->m_Status == TRANSFER_RECEIVING)
			return m_Sockets[i];

	return NULL;
}

void CGnuDownloadShell::Timer()
{	
	int i;

	CheckCompletion();


	if(m_ShellStatus != eDone)
	{
		#ifdef _DEBUG
			// for testing
			for(i = 0; i < m_Queue.size(); i++)
			{
				FileSource &foo = m_Queue[i];
				time_t now = time(NULL);
				int bar = foo.Status + 7 + now;
			}
		#endif

		// move items from the wait heap to the ready heap
		while (!m_WaitHeap.empty())
		{
			unsigned SourceID = m_WaitHeap.front().first;
			uint32 TimeToTry  = m_WaitHeap.front().second;

			// stop if the next one is in the future
			if (TimeToTry > time(NULL))
				break;

			// pop off this entry
			pop_heap(m_WaitHeap.begin(), m_WaitHeap.end(), HeapComparator());
			m_WaitHeap.pop_back();

			ASSERT(SourceID < m_Queue.size());

			// if its been put on this heap again in the meantime, ignore it
			if (TimeToTry != m_Queue[SourceID].TimeToTry)
				continue;

			AddToAliveHeap(SourceID);
		}
	}

	// If download is pending
	if(m_ShellStatus == ePending)
	{
		if(!m_pPrefs->m_MaxDownloads)
			Start();

		else if(m_pTrans->CountDownloading() < m_pPrefs->m_MaxDownloads)
			Start();
	}

	// If this download is activated
	else if(m_ShellStatus == eActive)
	{
		if( !AllPartsActive() || m_FileLength == 0) // Do not combine with below, avoid trynexthost()
		{
			// send out UDP packets if we have more to send
			TryNextUdp();

			// try to connect to someone else
			if( m_pPrefs->m_Multisource || !IsDownloading() )
			{
				TryNextHost();
				m_UpdatedInSecond = true;
			}
		}
	}

	// If it is waiting to retry
	else if(m_ShellStatus == eCooling)
	{
		m_Cooling--;

		if(m_Cooling <= 0)
			m_ShellStatus = ePending;

		m_UpdatedInSecond = true;
	}
	
	// Download waiting for more sources
	else if(m_ShellStatus == eWaiting)
	{

	}

	// Else this download is dead
	else if (m_ShellStatus == eDone)
	{
		for(i = 0; i < m_Sockets.size(); i++)
			m_Sockets[i]->Close();
	}
	else
		ASSERT(0);

	
	// Backup parts and hosts
	if(m_BackupInterval == 5)
	{
		if(m_BackupHosts != m_Queue.size())
		{
			BackupHosts();
			m_BackupHosts = m_Queue.size();
		}

		if( m_BackupBytes != GetBytesCompleted())
		{
			BackupParts();
			m_BackupBytes = GetBytesCompleted();
		}

		m_BackupInterval = 0;
	}
	else
		m_BackupInterval++;


	// Run timer for each socket
	for(i = 0; i < m_Sockets.size(); i++)
		m_Sockets[i]->Timer();


	// Remove dead sockets
	std::vector<CGnuDownload*>::iterator itSock;

	itSock = m_Sockets.begin();
	while( itSock != m_Sockets.end())
	{
		CGnuDownload *pSock = *itSock;

		if(pSock->m_Status == TRANSFER_CLOSED)
		{
			delete *itSock;
			
			itSock = m_Sockets.erase(itSock);
		}
		else
			itSock++;
	}

	if(m_UpdatedInSecond)
	{
		m_UpdatedInSecond = false;
		
		// Update Download
		m_pTrans->DownloadUpdate(m_DownloadID);
		
		// Update any search results
		std::map<CString, ResultGroup*>::iterator itHash;
		for(int i = 0; i < m_pNet->m_SearchList.size(); i++)
		{
			itHash = m_pNet->m_SearchList[i]->m_ResultHashMap.find(m_Sha1Hash);
			if(itHash != m_pNet->m_SearchList[i]->m_ResultHashMap.end())
			{
				ResultGroup* pResult = itHash->second;
				
				int NewState = m_pNet->m_SearchList[i]->UpdateResultState(m_Sha1Hash);

				if(NewState != pResult->State)
				{
					pResult->State = NewState;
					m_pNet->m_SearchList[i]->TransferUpdate(pResult->ResultID);
				}
			}
		}

	}
}

bool CGnuDownloadShell::ReadyFile()
{
	// Stream Test
	//if(m_Name == "stream.mp3")
	//	return true;


	if(m_File.m_hFile == CFileLock::hFileNull)
	{
		// Get plain file name with out directory crap
		CString FileName = m_Name;
		FileName.Replace("\\", "/");
		FileName = FileName.Mid( FileName.ReverseFind('/') + 1);

		// Create the file in the partial directory
		CString PartialDir = m_pPrefs->m_PartialDir;
		CreateDirectory(PartialDir, NULL);

		m_PartialPath = PartialDir + "\\" + FileName;
		m_PartialPath.Replace("\\\\", "\\");

		int dotpos = m_PartialPath.ReverseFind('.');
		if(dotpos == -1)
			m_PartialPath += ".partial";
		else
			m_PartialPath.Insert(dotpos, ".partial");
		

		// Create partial file
		if( !m_File.Open(m_PartialPath, CFileLock::modeCreate | CFileLock::modeNoTruncate | CFileLock::modeReadWrite | CFileLock::shareDenyWrite) )
			return false;


		m_File.SetLength(m_FileLength);
	}

	return true;
}

DWORD CGnuDownloadShell::GetStatus()
{
	if(m_ShellStatus == ePending)
		return TRANSFER_PENDING;

	if(m_ShellStatus == this->eActive)
	{
		if(IsDownloading())
			return TRANSFER_RECEIVING;
		else if (IsRemotelyQueued())
			return TRANSFER_QUEUED;
		else
			return TRANSFER_CONNECTING;
	}

	if(m_ShellStatus == eCooling)
		return TRANSFER_COOLDOWN;

	if(m_ShellStatus == eWaiting)
		return TRANSFER_NOSOURCES;

	// Completed or Failed
	if(m_ShellStatus == eDone)
	{
		if(m_FileLength && m_FileLength == GetBytesCompleted())
		{
			if(!m_HashComputed)
				return TRANSFER_RECEIVING;
			else if(m_HashVerified)
				return TRANSFER_COMPLETED;
		}
		
		return TRANSFER_CLOSED;
	}

	
	return TRANSFER_CLOSED;
}

uint64 CGnuDownloadShell::GetBytesCompleted()
{
	uint64 BytesCompleted = 0;

	for(int i = 0; i < m_PartList.size(); i++)
		BytesCompleted += m_PartList[i].BytesCompleted;

	return BytesCompleted;
}

bool CGnuDownloadShell::CheckCompletion()
{
	if(m_FileLength == 0)
		return false;

	// Make sure all parts finished, this also sub-hashes finished parts
	bool PartsFinished = true;

	for(int i = 0; i < m_PartList.size(); i++)
		if( PartDone(i) )		
		{
			if( m_TigerTree && !PartVerified(i) )
				PartsFinished = false;
		}	
		else
			PartsFinished = false;

	if( !PartsFinished )
		return false;


	m_ShellStatus = eDone;
	
	if(m_FileMoved)
		return true;
	
	m_Cooling   = 0;
	
	m_pNet->EndSearch(m_SearchID);

	// Close File
	m_File.Abort();


	CString FinalPath = GetFinalPath();

	// Match hash of download with expected hash
	if(!m_HashComputed)
	{
		m_HashComputed = true;

		if( !m_Sha1Hash.IsEmpty())
		{
			CString FinalHash = m_pCore->m_pShare->m_pHash->GetFileHash(m_PartialPath);


			if(FinalHash.CompareNoCase(m_Sha1Hash) == 0)
				m_HashVerified = true;

			else
			{
				if( FinalHash.IsEmpty() )
					m_ReasonDead = "Complete - Could not Check File Integrity";
				else
					m_ReasonDead = "Complete - Integrity Check Failed";

				int slashpos = FinalPath.ReverseFind('\\');
				FinalPath.Insert(slashpos + 1, "(Unverified) ");
			
				
			}
		}
		else
			m_HashVerified = true;
	}


	if( !m_FileMoved )
	{
		if( !MoveFile(m_PartialPath, FinalPath) )
		{
			m_OverrideName = m_Name;
			m_OverrideName.Replace("\\", "/");
			m_OverrideName = m_OverrideName.Mid( m_OverrideName.ReverseFind('/') + 1);

			while( !MoveFile(m_PartialPath, FinalPath) )
				if(GetLastError() == ERROR_ALREADY_EXISTS)
				{
					m_OverrideName = IncrementName(m_OverrideName);

					FinalPath = GetFinalPath();	
				}
				else
				{
					if(CopyFile(m_PartialPath, FinalPath, false))
						m_FileMoved = true;

					break;
				}

			m_FileMoved = true;
		}

		// Save file meta
		CString MetaString = GetMetaXML(true);
		if( !MetaString.IsEmpty() )
		{
			CString Path = FinalPath;

			int SlashPos = Path.ReverseFind('\\');
			if(SlashPos != -1)
			{
				CString Dirpath = Path.Left(SlashPos) + "\\Meta";
				CreateDirectory(Dirpath, NULL);
				
				Path.Insert(SlashPos, "\\Meta");
				Path += ".xml";

				CStdioFile MetaFile;
				if(MetaFile.Open(Path, CFile::modeWrite | CFile::modeCreate))
					MetaFile.WriteString( MetaString );
			}
		}

		// Check for viruses
		if( m_pPrefs->m_AntivirusEnabled )
            _spawnl(_P_WAIT, m_pPrefs->m_AntivirusPath, "%s", FinalPath, NULL);
          

		// Signal completed
		if(m_FileMoved)
			m_pTrans->DownloadUpdate(m_DownloadID);

		// Save hash computed/verified values
		BackupHosts();


		// Update shared
		if( !m_pPrefs->m_NoReload )
		{
			m_pCore->m_pShare->m_UpdateShared = true;
			m_pCore->m_pShare->m_TriggerThread.SetEvent();
		}
	}

	return true;

}
			
void CGnuDownloadShell::BackupHosts()
{	
	CString PartialDir = m_pPrefs->m_PartialDir;
	CreateDirectory(PartialDir, NULL);

	m_BackupPath = PartialDir + "\\" + m_Name + ".info";
	m_BackupPath.Replace("\\\\", "\\");

	CStdioFile BackupFile;
	if( !BackupFile.Open(m_BackupPath, CFileLock::modeCreate | CFileLock::modeWrite) )
		return;
		
	CString Backup;

	// Save main download info
	Backup += "[Download]\n";
	Backup += "Status="			+ NumtoStr(m_ShellStatus) + "\n";
	Backup += "Name="			+ m_Name + "\n";
	Backup += "FileLength="		+ NumtoStr(m_FileLength) + "\n";
	Backup += "PartSize="		+ NumtoStr(m_PartSize) + "\n";
	Backup += "OverrideName="	+ m_OverrideName + "\n";
	Backup += "OverridePath="	+ m_OverridePath + "\n";
	Backup += "PartialPath="	+ m_PartialPath + "\n";
	Backup += "Sha1Hash="		+ m_Sha1Hash + "\n";
	Backup += "Search="			+ m_Search + "\n";
	Backup += "Meta="			+ m_MetaXml + "\n"; // dont load meta here because schemas might not be loaded yet
	Backup += "AvgSpeed="		+ NumtoStr(m_AvgSpeed) + "\n";
	Backup += "HashComputed="	+ NumtoStr(m_HashComputed) + "\n";
	Backup += "HashVerified="	+ NumtoStr(m_HashVerified) + "\n";
	Backup += "FileMoved="		+ NumtoStr(m_FileMoved) + "\n";
	Backup += "ReasonDead="		+ m_ReasonDead + "\n";
	
	Backup += "UseProxy="		+ NumtoStr(m_UseProxy) + "\n";
	Backup += "DefaultProxy="	+ m_DefaultProxy + "\n";

	Backup += "TigerHash="		+ m_TigerHash + "\n";
	Backup += "TreeSize="		+ NumtoStr(m_TreeSize) + "\n";
	Backup += "TreeRes="		+ NumtoStr(m_TreeRes) + "\n";

	Backup += "TigerTree=";
	for(int j = 0; j < m_TreeSize; j += 24)
			Backup += EncodeBase32(m_TigerTree + j, 24) + ".";
	Backup += "\n";


	// Save alternate hosts
	for(int i = 0; i < m_Queue.size(); i++)
	{
		Backup += "[Host " + NumtoStr(i) + "]\n";

		Backup += "Name="			+ m_Queue[i].Name + "\n";
		Backup += "Sha1Hash="		+ m_Queue[i].Sha1Hash + "\n";
		//Backup += "BitprintHash="	+ m_Queue[i].BitprintHash + "\n";
		Backup += "FileIndex="		+ NumtoStr(m_Queue[i].FileIndex) + "\n";
		Backup += "Size="			+ NumtoStr(m_Queue[i].Size) + "\n";
		
		Backup += "Host="			+ IPtoStr(m_Queue[i].Address.Host) + "\n";
		Backup += "Port="			+ NumtoStr(m_Queue[i].Address.Port) + "\n";
		Backup += "Network="		+ NumtoStr(m_Queue[i].Network) + "\n";
		Backup += "HostStr="		+ m_Queue[i].HostStr + "\n";
		Backup += "Path="			+ m_Queue[i].Path + "\n";
		Backup += "Speed="			+ NumtoStr(m_Queue[i].Speed) + "\n";
		Backup += "Vendor="			+ m_Queue[i].Vendor + "\n";
		
		Backup += "Firewall="		+ NumtoStr(m_Queue[i].Firewall) + "\n";
		Backup += "OpenSlots="		+ NumtoStr(m_Queue[i].OpenSlots) + "\n";
		Backup += "Busy="			+ NumtoStr(m_Queue[i].Busy) + "\n";
		Backup += "Stable="			+ NumtoStr(m_Queue[i].Stable) + "\n";
		Backup += "ActualSpeed="	+ NumtoStr(m_Queue[i].ActualSpeed) + "\n";
		Backup += "SupportF2F="		+ NumtoStr(m_Queue[i].SupportF2F) + "\n";
		Backup += "PushID="			+ EncodeBase16((byte*) &m_Queue[i].PushID, 16) + "\n";
		
		CString Nodes;
		for(int x = 0; x < m_Queue[i].DirectHubs.size(); x++)
			Nodes += IPv4toStr( m_Queue[i].DirectHubs[x]) + ", ";

		Nodes.Trim(", ");
		if(!Nodes.IsEmpty())
			Backup += "Direct="	+ Nodes + "\n";
	}

	BackupFile.Write(Backup, Backup.GetLength());

	BackupFile.Abort();
}

void CGnuDownloadShell::BackupParts()
{
	CString PartialDir = m_pPrefs->m_PartialDir;
	CreateDirectory(PartialDir, NULL);

	CString PartPath = PartialDir + "\\" + m_Name + ".part";
	PartPath.Replace("\\\\", "\\");

	CStdioFile PartFile;
	if( !PartFile.Open(PartPath, CFileLock::modeCreate | CFileLock::modeWrite) )
		return;
		
	CString FileData;


	// Go through part list and write to file
	for(int i = 0; i < m_PartList.size(); i++)
	{
		FilePart CurrentPart = m_PartList[i];

		FileData += "[Part " + NumtoStr(i) + "]\n";

		// Set in host file with m_PartSize variable
		//FileData += "StartByte="			+ NumtoStr(CurrentPart.StartByte) + "\n";
		//FileData += "EndByte="				+ NumtoStr(CurrentPart.EndByte) + "\n";
		FileData += "BytesCompleted="		+ NumtoStr(CurrentPart.BytesCompleted) + "\n";
		FileData += "Verified="				+ NumtoStr(CurrentPart.Verified) + "\n";
		FileData += "SourceHostID="			+ NumtoStr(CurrentPart.SourceHostID) + "\n";
	}


	PartFile.Write(FileData, FileData.GetLength());

	PartFile.Abort();
}

void CGnuDownloadShell::Erase()
{
	CString PartialDir = m_pPrefs->m_PartialDir;
	CreateDirectory(PartialDir, NULL);

	m_ShellAccess.Lock();
	
	m_ShellStatus = eDone;

	// Remove sockets
	while(m_Sockets.size())
	{
		CGnuDownload* pDead = m_Sockets.back();
		m_Sockets.pop_back();
		delete pDead;
	}

	m_ShellAccess.Unlock();

	// Delete File
	m_File.Abort();
	DeleteFile(m_PartialPath);

	// Delete backup
	CString BackupPath = PartialDir + "\\" + m_Name + ".info";
	BackupPath.Replace("\\\\", "\\");
	DeleteFile(BackupPath);
	
	// Delete parts file
	CString PartPath = PartialDir + "\\" + m_Name + ".part";
	PartPath.Replace("\\\\", "\\");
	DeleteFile(PartPath);

	// Delete meta file
	CString MetaPath = m_pPrefs->m_DownloadPath + "\\meta\\" + m_Name + ".xml";
	MetaPath.Replace("\\\\", "\\");
	DeleteFile(MetaPath);
}

void CGnuDownloadShell::AddAltLocation(IPv4 Address)
{
	ASSERT(Address.Port);
	if(Address.Port == 0)
		return;

	if( IsPrivateIP(Address.Host) )
		return;

	if( !m_pNet->NotLocal( Node( IPtoStr(Address.Host), Address.Port) ) )
		return;

	// Remove from Nalt list
	std::deque<IPv4>::iterator itNalt;
	for(itNalt = m_NaltHosts.begin(); itNalt != m_NaltHosts.end(); itNalt++)
		if(itNalt->Host.S_addr == Address.Host.S_addr)
		{
			m_NaltHosts.erase(itNalt);
			break;
		}

	// check for dupe
	for(int i = 0; i < m_AltHosts.size(); i++)
		if(Address.Host.S_addr == m_AltHosts[i].Host.S_addr)
		{
			m_AltHosts[i] = Address; // maybe new port
			return;
		}

	m_AltHosts.push_back(Address);
}

CString CGnuDownloadShell::GetAltLocHeader(IP ToIP, int HostCount)
{
	CString Header = "X-Alt: ";

	int j = 0;

	if(m_AltHosts.size() < HostCount)
		HostCount = m_AltHosts.size(); 

	std::vector<int> HostIndexes;

	// Get random indexes to send to host
	while(HostCount > 0)
	{
		HostCount--;

		int NewIndex = rand() % m_AltHosts.size() + 0;

		if(ToIP.S_addr == m_AltHosts[NewIndex].Host.S_addr)
			continue;

		bool found = false;

		for(j = 0; j < HostIndexes.size(); j++)
			if(HostIndexes[j] == NewIndex)
				found = true;

		if(!found)
			HostIndexes.push_back(NewIndex);
	}

	for(j = 0; j < HostIndexes.size(); j++)
		Header += IPv4toStr(m_AltHosts[ HostIndexes[j] ]) + ", ";

	if( HostIndexes.size() == 0)
		return "";

	Header.Trim(", ");
	
	Header += "\r\n";

	return Header;
}

void CGnuDownloadShell::AddNaltLocation(IPv4 Address)
{
	ASSERT(Address.Port);
	if(Address.Port == 0)
		return;

	if( IsPrivateIP(Address.Host) )
		return;

	if( !m_pNet->NotLocal( Node( IPtoStr(Address.Host), Address.Port) ) )
		return;

	// Remove from Alt list
	std::deque<IPv4>::iterator itAlt;
	for(itAlt = m_AltHosts.begin(); itAlt != m_AltHosts.end(); itAlt++)
		if(itAlt->Host.S_addr == Address.Host.S_addr)
		{
			m_AltHosts.erase(itAlt);
			break;
		}

	// check for dupe
	for(int i = 0; i < m_NaltHosts.size(); i++)
		if(Address.Host.S_addr == m_NaltHosts[i].Host.S_addr)
		{
			m_NaltHosts[i] = Address; // maybe new port
			return;
		}

	m_NaltHosts.push_back(Address);
}

CString CGnuDownloadShell::GetNaltLocHeader(IP ToIP, int HostCount)
{
	CString Header = "X-NAlt: ";

	int j = 0;

	if(m_NaltHosts.size() < HostCount)
		HostCount = m_NaltHosts.size(); 

	std::vector<int> HostIndexes;

	// Get random indexes to send to host
	while(HostCount > 0)
	{
		HostCount--;

		int NewIndex = rand() % m_NaltHosts.size() + 0;

		if(ToIP.S_addr == m_NaltHosts[NewIndex].Host.S_addr)
			continue;

		bool found = false;

		for(j = 0; j < HostIndexes.size(); j++)
			if(HostIndexes[j] == NewIndex)
				found = true;

		if(!found)
			HostIndexes.push_back(NewIndex);
	}

	for(j = 0; j < HostIndexes.size(); j++)
		Header += IPv4toStr(m_NaltHosts[ HostIndexes[j] ]) + ", ";

	if( HostIndexes.size() == 0)
		return "";

	Header.Trim(", ");
	
	Header += "\r\n";

	return Header;
}

CString CGnuDownloadShell::GetFilePath()
{
	CString Path;

	if(GetBytesCompleted() == 0)
		return "";

	if(m_FileLength && m_FileLength == GetBytesCompleted())
		Path = GetFinalPath();
	else
		Path = m_PartialPath;

	return Path;
}

CString CGnuDownloadShell::GetFinalPath()
{
	// Get Name
	CString FileName = m_Name;
	
	if( !m_OverrideName.IsEmpty() )
		FileName = m_OverrideName;

	FileName.Replace("\\", "/");
	FileName = FileName.Mid( FileName.ReverseFind('/') + 1);


	// Attach Path
	CString FilePath = m_pPrefs->m_DownloadPath + "\\" + FileName;

	if( !m_OverridePath.IsEmpty() )
		FilePath = m_OverridePath + "\\" + FileName;
		
	FilePath.Replace("\\\\", "\\");

	return FilePath;
}

void CGnuDownloadShell::RunFile()
{
	CString Path = GetFilePath();
				
	if(Path != "")
		ShellExecute(NULL, "open", Path, NULL, NULL, SW_SHOWNORMAL);
}

void CGnuDownloadShell::ReSearch()
{
	if(m_FileLength && GetBytesCompleted() >= m_FileLength )
		return;


	// Go through all search results to find matches
	// Especially needed for DownloadFile() and concurrent/similar searches
	for(int i = 0; i < m_pCore->m_pNet->m_SearchList.size(); i++)
	{
		std::map<CString, ResultGroup*>::iterator itResult = m_pCore->m_pNet->m_SearchList[i]->m_ResultHashMap.find(m_Sha1Hash);
		if(itResult == m_pCore->m_pNet->m_SearchList[i]->m_ResultHashMap.end())
			continue;

		ResultGroup* pResult = itResult->second;
		for(int k = 0; k < pResult->ResultList.size(); k++)
			AddHost( pResult->ResultList[k] );
	}

	// End current search
	m_pNet->EndSearch(m_SearchID); 

	// Create new search by hash
	CGnuSearch* pSearch = new CGnuSearch(m_pNet);
	m_pNet->m_SearchList.push_back(pSearch);
	
	m_SearchID = pSearch->m_SearchID;
	
	pSearch->SendHashQuery("", HASH_SHA1, m_Sha1Hash);


	m_ShellStatus	= eActive;
	m_Retry			= false;
	m_Cooling		= 0;
	m_HostTryPos	= 1;

	// limit research to once per hour across all downloads
	// reason: 800,000 nodes on network all searching 1 per hour
	// 800,000 / 360 = 222 queries/sec which is too many anyways
	/*// research done at 15m, 30m, 1h, 2h, 4h, 8h, 8h...
	ASSERT(m_ReSearchInterval);
	if(m_ReSearchInterval == 0)
		m_ReSearchInterval = 15;

	if(m_ReSearchInterval < 8 * 60)
		m_ReSearchInterval *= 2;
	else
		m_ReSearchInterval = 8 * 60;
	*/

	m_NextReSearch = time(NULL) + (60*60); // 1 hour
}


CString CGnuDownloadShell::AvailableRangesCommaSeparated()
{
	//Returns a comma separated list of all downloaded ranges (partial file and chunks)

	CString list;

	bool    chain = false;
	uint64  startByte = 0;
	uint64  endByte   = 0;

	for (int i = 0; i < m_PartList.size(); i++)
	{
		// Only share verified parts
		if( m_PartList[i].Verified )
		{
			if(chain == false)
				startByte = m_PartList[i].StartByte;
			
			endByte   = m_PartList[i].EndByte;

			chain = true;
		}
		else
		{
			if(chain)
			{
				if (!list.IsEmpty())
					list += ",";

				list += NumtoStr(startByte) + "-" + NumtoStr(endByte);
			}
			
			chain = false;
		}
	}

	return list;
}

int CGnuDownloadShell::GetRange(uint64 pos, unsigned char *buf, int len)
{
	if(m_File.m_hFile == CFile::hFileNull || len == 0)
		return 0;

	for(int i = 0; i < m_PartList.size(); i++)
		if( m_PartList[i].Verified )
			if( pos >= m_PartList[i].StartByte && pos <= m_PartList[i].EndByte)
			{
				int cpylen = m_PartList[i].EndByte + 1 - pos;

				if(len < cpylen)
					cpylen = len;

				try
				{
					return m_File.SeekandRead(pos, buf, cpylen);
				}
				catch(...)
				{
					break;
				}
			}

	return 0;
}

bool CGnuDownloadShell::PartDone(int PartNumber)
{
	if(PartNumber >= m_PartList.size())
		return true;


	FilePart* pPart = &m_PartList[PartNumber];

	if( (pPart->StartByte + pPart->BytesCompleted) > pPart->EndByte )
		return true;


	return false;
}

CGnuDownload* CGnuDownloadShell::PartActive(int PartNumber)
{
	for(int i = 0; i < m_Sockets.size(); i++)
		if( m_Sockets[i]->m_PartNumber == PartNumber)
			if(m_Sockets[i]->m_PartActive)
				return m_Sockets[i];

	return NULL;
}

bool CGnuDownloadShell::AllPartsActive()
{
	for(int i = 0; i < m_PartList.size(); i++)
	{
		if( PartDone(i) )
			continue;

		if( PartActive(i) )
			continue;

		return false;
	}

	return true;
}

bool CGnuDownloadShell::PartVerified(int PartNumber)
{	
	FilePart* pPart = &m_PartList[PartNumber];
		
	if( pPart->Verified )
		return true;

	// If TreeRes larger than PartSize make sure this Part is at front of block needing to be verified
	if(m_TreeRes > m_PartSize)
		if( pPart->StartByte % m_TreeRes != 0)
			return false;

	// Make sure all parts in block are done before verifying
	int BlockSize = 0;
	for(int i = PartNumber; i < m_PartList.size(); i++)
		if( m_PartList[i].StartByte < pPart->StartByte + m_TreeRes)
		{
			if( !PartDone(i) )
				return false;
		
			BlockSize += m_PartList[i].BytesCompleted;
		} 
		else
			break;

	// Tiger Tree Hash Part File
	if( m_File.m_hFile == CFileLock::hFileNull)
		return false;

	tt2_context Tiger_Context;
	tt2_init(&Tiger_Context);
	tt2_initTree(&Tiger_Context, BlockSize);
	
	
	int  FilePos   = 0;
	byte Buffer[BUFF_SIZE];

	while(FilePos < BlockSize)
	{
		int BytesRead = 0;
		int ReadSize = BlockSize - FilePos;

		if( ReadSize > BUFF_SIZE)
			ReadSize = BUFF_SIZE;

		try
		{
			BytesRead = m_File.SeekandRead(pPart->StartByte + FilePos, Buffer, ReadSize);
		}
		catch(CFileException* e)
		{
			e->Delete();
			return false;
		}

		tt2_update(&Tiger_Context, Buffer,  BytesRead);
		FilePos += BytesRead;
	}

	byte Tiger_Digest[24];
	tt2_digest(&Tiger_Context,	Tiger_Digest);


	// Check file tree hash with part hash
	int BaseNodes = m_FileLength / m_TreeRes;
	if(m_FileLength % m_TreeRes > 0)
		BaseNodes++;

	int NodeOffset = 0;
	for(uint64 i = 0; i < m_FileLength; i += m_TreeRes, NodeOffset++)
		if(i == pPart->StartByte)
			break;

	int TreeOffset = m_TreeSize - (BaseNodes * 24) + (NodeOffset * 24);

	if(TreeOffset >= 0 && TreeOffset <= m_TreeSize - 24)
		if(memcmp(m_TigerTree + TreeOffset, Tiger_Digest, 24) == 0)
		{
			for(i = PartNumber; i < m_PartList.size(); i++)
				if( m_PartList[i].StartByte < pPart->StartByte + m_TreeRes)
					m_PartList[i].Verified = true;
			
			tt2_init(&Tiger_Context); // frees tree memory
			return true;
		}
	

	// Tag corrupt hosts
	ASSERT(pPart->SourceHostID >= 0 && pPart->SourceHostID < m_Queue.size());
	m_Queue[pPart->SourceHostID].Status = FileSource::eCorrupt;

	for(i = PartNumber; i < m_PartList.size(); i++)
		if( m_PartList[i].StartByte < pPart->StartByte + m_TreeRes)
		{
			m_PartList[i].BytesCompleted = 0;
			m_PartList[i].SourceHostID   = 0;
		}

	tt2_init(&Tiger_Context); // frees tree memory
	
	return false;
}

bool CGnuDownloadShell::URLtoSource(FileSource &WebSource, CString URL)
{
	WebSource.Network  = NETWORK_WEB;
	WebSource.Size     = m_FileLength;
	WebSource.Sha1Hash = m_Sha1Hash;
	
	// Convert URL into host, port, and path
	CString strURL = URL;

	int StartPos = strURL.Find("://");
	if(StartPos != -1)
		StartPos += 3;

	strURL = strURL.Mid(StartPos);

	int EndPos = strURL.Find("/");
	if( EndPos == -1)
		return false;

	CString WebSite = strURL.Left(EndPos);

	// Set Port
	int ColonPos = WebSite.Find(":");

	if(ColonPos != -1)
	{
		WebSource.Address.Port = atoi( WebSite.Mid(ColonPos + 1) );
		WebSite = WebSite.Mid(ColonPos);
	}
	else
		WebSource.Address.Port = 80;

	// Set Host
	hostent* pHost = gethostbyname(WebSite);
	if ( pHost == NULL)
		return false;
		
	in_addr* p = (in_addr*) pHost->h_addr_list[0];
	WebSource.Address.Host = StrtoIP( inet_ntoa(*p) );

	// Set Path
	WebSource.HostStr = WebSite;
	WebSource.Path    = strURL.Mid(EndPos);

	if( WebSource.Path.IsEmpty() )
		WebSource.Path = "/";

	return true;
}

CString CGnuDownloadShell::GetMetaXML(bool file)
{
	std::map<int, CGnuSchema*>::iterator itMeta = m_pCore->m_pMeta->m_MetaIDMap.find(m_MetaID);
	if(itMeta != m_pCore->m_pMeta->m_MetaIDMap.end())
	{
		if(file)
			return itMeta->second->AttrMaptoFileXML(m_AttributeMap);
		else
			return itMeta->second->AttrMaptoNetXML(m_AttributeMap);
	}

	return "";
}
