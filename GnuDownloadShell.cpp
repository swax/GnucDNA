/********************************************************************************

	GnucDNA - A Gnutella Library
    Copyright (C) 2000-2004 John Marshall

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


	For support, questions, comments, etc...
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


#define RETRY_WAIT 30
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

	m_NextReSearch		= 0;
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

void CGnuDownloadShell::Init(CString Name, int FileSize, int HashID, CString Hash)
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
	for(int i = 0; i < m_FileLength; i += m_PartSize)
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
	for(int i = 0; i < m_Queue.size(); i++)
	{
		if(memcmp(&HostInfo.Address.Host.S_addr, &m_Queue[i].Address.Host.S_addr, 3) == 0)
			SubnetLimit++;

		if( HostInfo.Address.Host.S_addr == m_Queue[i].Address.Host.S_addr && HostInfo.Address.Port == m_Queue[i].Address.Port )
		{
			m_Queue[i].FileIndex = HostInfo.FileIndex;
			m_Queue[i].Name      = HostInfo.Name;
			m_Queue[i].NameLower = HostInfo.NameLower;
			m_Queue[i].Vendor    = HostInfo.Vendor;

			m_Queue[i].GnuRouteID = HostInfo.GnuRouteID;
			m_Queue[i].PushID     = HostInfo.PushID;
			m_Queue[i].RetryWait  = 0;
			m_Queue[i].Status     = FileSource::eUntested;

			return;
		}
	}

	if(SubnetLimit > SUBNET_LIMIT)
		return;

	HostInfo.Handshake = "";
	HostInfo.Error     = "";
	HostInfo.RetryWait = 0;
	HostInfo.Tries     = 0;
	HostInfo.Status    = FileSource::eUntested;

	HostInfo.RealBytesPerSec	= 0;

	HostInfo.SourceID = m_NextHostID++;
	m_HostMap[HostInfo.SourceID] = m_Queue.size();
	

	m_Queue.push_back(HostInfo);


	m_UpdatedInSecond   = true;
}


void CGnuDownloadShell::TryNextHost()
{
	if(m_HostTryPos >= m_Queue.size())
		m_HostTryPos = 1;
	else
		m_HostTryPos++;

	// Rolls through host list looking for untried hosts
	int tried_hosts = 0; 

	// List already ordered to put fastest nodes at top
	for(int i = 0; i < m_Queue.size(); i++)
		if(m_Queue[i].Status == FileSource::eUntested || m_Queue[i].Status == FileSource::eAlive )
		{
			if(m_Queue[i].RetryWait)
				continue;
			
			if( !m_pNet->ConnectingSlotsOpen() )
				return;

			// Make sure host is not active
			bool Active = false;
			for(int j = 0; j < m_Sockets.size(); j++)
				if(m_Sockets[j]->m_HostID == m_Queue[i].SourceID)
					Active = true;

			if(Active)
				continue;

			m_Queue[i].Error = "";
			CGnuDownload* pSock = new CGnuDownload(this, m_Queue[i].SourceID);

			if(pSock->StartDownload())
				m_Sockets.push_back(pSock);
			else
			{
				delete pSock;
				pSock = NULL;
			}
			
			m_Queue[i].Tries++;
			m_Queue[i].Status = FileSource::eTrying;
			m_Queue[i].RetryWait = RETRY_WAIT;

			// eg. File has 90 results, try 3 at a time
			tried_hosts++;
			if (tried_hosts < (m_Queue.size() - 1) / RETRY_WAIT + 1) 
				continue; 
			
			return;
		}

	if (tried_hosts) 
		return;


	// All hosts in list have been attempted
	// And nothing is trying to connect
	if(m_Sockets.size() == 0)
	{
		bool StillHope = false;

		for(i = 0; i < m_Queue.size(); i++)
			if(m_Queue[i].Status == FileSource::eAlive)
			{
				if(!m_Cooling)
					m_Cooling = m_Queue[i].RetryWait;

				if(m_Cooling > m_Queue[i].RetryWait)
						m_Cooling = m_Queue[i].RetryWait;

				StillHope = true;
			}

		m_ShellStatus = StillHope ? eCooling : eWaiting;

		m_Retry   = true; // Gone through host list once so now we are retrying hosts

		if(m_ShellStatus == eWaiting)
		{
			m_ReasonDead	  = "Waiting, more hosts needed";
			m_Retry			  = false;
			m_UpdatedInSecond = true;
			m_Cooling		  = 0;
		}
	}
} 

void CGnuDownloadShell::Start()
{
	m_HostTryPos = 1;
	
	if(m_FileLength == 0 || GetBytesCompleted() < m_FileLength)
	{
		m_ShellStatus = eActive;

		m_Cooling   = 0;

		m_ReSearchInterval	= 15;
		m_NextReSearch		= time(NULL) + (60 * 10);
	}
}

// Should only be called from interface
void CGnuDownloadShell::Stop()
{
	m_HostTryPos = 1;

	for(int i = 0; i < m_Queue.size(); i++)
	{
		m_Queue[i].RetryWait = 0;
		m_Queue[i].Tries     = 0;
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
		for(i = 0; i < m_Queue.size(); i++)
			if(m_Queue[i].RetryWait > 0)
				m_Queue[i].RetryWait--;
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
			if( IsDownloading() )
			{
				float NetUtilized = 0;

				if(m_pNet->m_RealSpeedDown)
				{
					if( m_pNet->m_pGnu )
						NetUtilized += (float) m_pNet->m_pGnu->m_NetSecBytesDown;

					NetUtilized += (float) m_pTrans->m_DownloadSecBytes;

					NetUtilized /= (float) m_pNet->m_RealSpeedDown;
				}

				if(m_pPrefs->m_Multisource && NetUtilized < 0.6)
					TryNextHost();
			}
			else
			{
				m_UpdatedInSecond = true;

				TryNextHost();
			}
		}
	}

	// If it is waiting to retry
	else if(m_ShellStatus == eCooling)
	{
		m_Cooling--;

		if(m_Cooling == 0)
			m_ShellStatus = eActive;

		m_UpdatedInSecond = true;
	}
	
	// Download waiting for more sources
	else if(m_ShellStatus == eWaiting)
	{

	}

	// Else this socket is dead
	else
	{
		for(i = 0; i < m_Sockets.size(); i++)
			m_Sockets[i]->Close();
	}

	
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

		// Set where the file will go upon completion
		CreateDirectory(m_pPrefs->m_DownloadPath, NULL);
		m_FilePath = m_pPrefs->m_DownloadPath + "\\" + FileName;
		m_FilePath.Replace("\\\\", "\\");

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

int CGnuDownloadShell::GetBytesCompleted()
{
	int BytesCompleted = 0;

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
				//AfxMessageBox(m_FilePath + "\nRight: " + m_Sha1Hash + "\nWrong: " + FinalHash);
				
				if( FinalHash.IsEmpty() )
					m_ReasonDead = "Complete - Could not Check File Integrity";
				else
					m_ReasonDead = "Complete - Integrity Check Failed";

				int slashpos = m_FilePath.ReverseFind('\\');
				m_FilePath.Insert(slashpos + 1, "(Unverified) ");
			
				
			}
		}
		else
			m_HashVerified = true;
	}


	if( !m_FileMoved )
	{
		if( !MoveFile(m_PartialPath, m_FilePath) )
		{
			CString TempName = m_Name;

			while( !MoveFile(m_PartialPath, m_FilePath) )
				if(GetLastError() == ERROR_ALREADY_EXISTS)
				{

					CString FileName = TempName;
					FileName.Replace("\\", "/");
					FileName = FileName.Mid( FileName.ReverseFind('/') + 1);


					CString NewName = IncrementName(FileName);
					TempName.Replace(FileName, NewName);
				
					m_FilePath = m_pPrefs->m_DownloadPath + "\\" + NewName;				
				}
				else
				{
					if(CopyFile(m_PartialPath, m_FilePath, false))
						m_FileMoved = true;

					break;
				}

			m_FileMoved = true;
		}

		// Save file meta
		CString Path = m_FilePath;

		int SlashPos = Path.ReverseFind('\\');
		if(SlashPos != -1)
		{
			CString Dirpath = Path.Left(SlashPos) + "\\Meta";
			CreateDirectory(Dirpath, NULL);
			
			Path.Insert(SlashPos, "\\Meta");
			Path += ".xml";

			CStdioFile MetaFile;
			if(MetaFile.Open(Path, CFile::modeWrite | CFile::modeCreate))
				MetaFile.WriteString( GetMetaXML(true) );
		}

		// Check for viruses
		if( m_pPrefs->m_AntivirusEnabled )
            _spawnl(_P_WAIT, m_pPrefs->m_AntivirusPath, "%s", m_FilePath, NULL);
          

		// Signal completed
		m_UpdatedInSecond = true;

		// Save hash computed/verified values
		BackupHosts();


		// Update shared
		m_pCore->m_pShare->m_UpdateShared = true;
		m_pCore->m_pShare->m_TriggerThread.SetEvent(); 
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
	Backup += "FilePath="		+ m_FilePath + "\n";
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
		Path = m_FilePath;
	else
		Path = m_PartialPath;

	return Path;
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

	// research done at 15m, 30m, 1h, 2h, 4h, 8h, 8h...
	ASSERT(m_ReSearchInterval);
	if(m_ReSearchInterval == 0)
		m_ReSearchInterval = 15;

	if(m_ReSearchInterval < 8 * 60)
		m_ReSearchInterval *= 2;
	else
		m_ReSearchInterval = 8 * 60;

	m_NextReSearch = time(NULL) + (60 * m_ReSearchInterval); 
}


CString CGnuDownloadShell::AvailableRangesCommaSeparated()
{
	//Returns a comma separated list of all downloaded ranges (partial file and chunks)

	CString list;

	bool chain = false;
	int  startByte = 0;
	int  endByte   = 0;

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

int CGnuDownloadShell::GetRange(int pos, unsigned char *buf, int len)
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
					m_File.Seek(pos, CFileLock::begin);
					return m_File.Read(buf, cpylen);
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
			m_File.Seek(pPart->StartByte + FilePos, CFile::begin);
			BytesRead = m_File.Read(Buffer, ReadSize);
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
	for(int i = 0; i < m_FileLength; i += m_TreeRes, NodeOffset++)
		if(i == pPart->StartByte)
			break;

	int TreeOffset = m_TreeSize - (BaseNodes * 24) + (NodeOffset * 24);
	
	if(TreeOffset > 0 && TreeOffset <= m_TreeSize - 24)
		if(memcmp(m_TigerTree + TreeOffset, Tiger_Digest, 24) == 0)
		{
			for(i = PartNumber; i < m_PartList.size(); i++)
				if( m_PartList[i].StartByte < pPart->StartByte + m_TreeRes)
					m_PartList[i].Verified = true;
			
			tt2_init(&Tiger_Context); // frees tree memory
			return true;
		}
	

	// Tag corrupt hosts
	std::map<int, int>::iterator itHost = m_HostMap.find(pPart->SourceHostID);
	if(itHost != m_HostMap.end())
		m_Queue[itHost->second].Status = FileSource::eCorrupt;

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
	in_addr* p = (in_addr*) pHost->h_addr_list[0];
	WebSource.Address.Host = StrtoIP( inet_ntoa(*p) );

	// Set Path
	WebSource.HostStr = WebSite;
	WebSource.Path    = strURL.Mid(EndPos);

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