/********************************************************************************

	GnucDNA - The Gnucleus Library
    Copyright (C) 2000-2004 John Marshall Group

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
#include "DnaCore.h"

#include "GnuCore.h"
#include "GnuPrefs.h"
#include "GnuTransfers.h"
#include "GnuNetworks.h"
#include "GnuMeta.h"
#include "GnuSchema.h"
#include "GnuSearch.h"
#include "GnuDownloadShell.h"
#include "GnuDownload.h"

#include "DnaDownload.h"


CDnaDownload::CDnaDownload()
{
	m_dnaCore  = NULL;
	m_gnuTrans = NULL;

}

CDnaDownload::~CDnaDownload()
{
}

void CDnaDownload::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore  = dnaCore;
	m_gnuTrans = dnaCore->m_gnuCore->m_pTrans;
}


// CDnaDownload message handlers

std::vector<int> CDnaDownload::GetDownloadIDs() 
{
	std::vector<int> DownloadIDs;
	
	for(int i = 0; i < m_gnuTrans->m_DownloadList.size(); i++)
		DownloadIDs.push_back( m_gnuTrans->m_DownloadList[i]->m_DownloadID );

	return DownloadIDs;
}

LONG CDnaDownload::GetStatus(LONG DownloadID)
{
	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->GetStatus();

	return 0;
}

BOOL CDnaDownload::IsCompleted(LONG DownloadID)
{

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->CheckCompletion();

	return TRUE;
}

CString CDnaDownload::GetName(LONG DownloadID)
{
	

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		strResult = itDL->second->m_Name;
		

	return strResult;
}

LONG CDnaDownload::GetBytesCompleted(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->GetBytesCompleted();

	return 0;
}

LONG CDnaDownload::GetFileLength(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->m_FileLength;

	return 0;
}

LONG CDnaDownload::GetSourceCount(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->m_Queue.size();

	return 0;
}

BOOL CDnaDownload::IsSearching(LONG DownloadID)
{

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		std::map<int, CGnuSearch*>::iterator itSearch = m_gnuTrans->m_pNet->m_SearchIDMap.find(itDL->second->m_SearchID);
		if(itSearch !=  m_gnuTrans->m_pNet->m_SearchIDMap.end())
		{
			CGnuSearch* pSearch = itSearch->second;

			if( !pSearch->m_SearchPaused )
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CDnaDownload::IsRetrying(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->m_Retry;

	return TRUE;
}

LONG CDnaDownload::GetCoolingCount(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->m_Cooling;

	return 0;
}

LONG CDnaDownload::GetActiveSourceCount(LONG DownloadID)
{
	

	int Sources = 0;
	
	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		
		for(int i = 0; i < itDL->second->m_Sockets.size(); i++)
			if(itDL->second->m_Sockets[i]->m_Status == TRANSFER_RECEIVING)
				Sources++;
	}

	return Sources;
}

CString CDnaDownload::GetReasonClosedStr(LONG DownloadID)
{
	

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		strResult = itDL->second->m_ReasonDead;

	return strResult;
}

LONG CDnaDownload::GetBytesPerSec(LONG DownloadID)
{
	

	int BytesPerSec = 0;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* p = itDL->second;

		// Add up all the b/s
		for(int i = 0; i < p->m_Sockets.size(); i++)
			if(p->m_Sockets[i]->m_Status == TRANSFER_RECEIVING)
				BytesPerSec += p->m_Sockets[i]->m_AvgRecvBytes.GetAverage();
	}

	return BytesPerSec;
}

LONG CDnaDownload::GetSecETA(LONG DownloadID)
{
	

	UINT Seconds;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* p = itDL->second;

		int BytesPerSec = 0;

		// Add up all the b/s
		for(int j = 0; j < p->m_Sockets.size(); j++)
			if(p->m_Sockets[j]->m_Status == TRANSFER_RECEIVING)
				BytesPerSec += p->m_Sockets[j]->m_AvgRecvBytes.GetAverage();
		
		int BytesLeft  = p->m_FileLength - p->GetBytesCompleted();
		
		Seconds = (double) BytesLeft / (double) BytesPerSec;
	}
	
	return Seconds;
}

LONG CDnaDownload::GetSourcePos(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->m_HostTryPos;

	return 0;
}

void CDnaDownload::RemoveCompleted()
{
	

	m_gnuTrans->RemoveCompletedDownloads();
}

void CDnaDownload::ForceStart(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		itDL->second->Start();
}

void CDnaDownload::Stop(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		itDL->second->Stop();
}

void CDnaDownload::Remove(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		m_gnuTrans->RemoveDownload(itDL->second);
}

void CDnaDownload::RunFile(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		itDL->second->RunFile();
}

void CDnaDownload::ReSearch(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		itDL->second->ReSearch();
}

CString CDnaDownload::GetHash(LONG DownloadID, LONG HashID)
{
	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		if(HashID == HASH_SHA1)
			strResult = itDL->second->m_Sha1Hash;

		if(HashID == HASH_TIGERTREE)
			strResult = itDL->second->m_TigerHash;
	}

	return strResult;
}

std::vector<int> CDnaDownload::GetSourceIDs(LONG DownloadID)
{
	std::vector<int> SourceIDs;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		for(int i = 0; i < itDL->second->m_Queue.size(); i++)
			SourceIDs.push_back( itDL->second->m_Queue[i].SourceID );
	}

	return SourceIDs;
}

ULONG CDnaDownload::GetSourceIP(LONG DownloadID, LONG SourceID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			return pDown->m_Queue[itSource->second].Address.Host.S_addr;
	}

	return 0;
}

LONG CDnaDownload::GetSourcePort(LONG DownloadID, LONG SourceID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			return pDown->m_Queue[itSource->second].Address.Port;
	}

	return 0;
}

CString CDnaDownload::GetSourceName(LONG DownloadID, LONG SourceID)
{
	

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			strResult = pDown->m_Queue[itSource->second].Name;
	}

	return strResult;
}

LONG CDnaDownload::GetSourceSpeed(LONG DownloadID, LONG SourceID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			return pDown->m_Queue[itSource->second].Speed / 8;
	}

	return 0;
}

CString CDnaDownload::GetSourceStatusStr(LONG DownloadID, LONG SourceID)
{
	

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			strResult = pDown->m_Queue[itSource->second].Error;
	}

	return strResult;
}

CString CDnaDownload::GetSourceVendor(LONG DownloadID, LONG SourceID)
{
	

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			strResult = pDown->m_Queue[itSource->second].Vendor;
	}

	return strResult;
}

CString CDnaDownload::GetSourceHandshake(LONG DownloadID, LONG SourceID)
{
	

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			strResult = pDown->m_Queue[itSource->second].Handshake;
	}

	return strResult;
}

std::vector<int> CDnaDownload::GetChunkIDs(LONG DownloadID) // can be used direcly int aray :) but next time
{
	std::vector<int> ChunkIDs;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		for(int i = 0; i < itDL->second->m_PartList.size(); i++)
			ChunkIDs.push_back( i );
	}

	return ChunkIDs;
}

LONG CDnaDownload::GetChunkStart(LONG DownloadID, LONG ChunkID)
{

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		if(ChunkID < pDown->m_PartList.size()) 
			return pDown->m_PartList[ChunkID].StartByte;
	}

	return 0;
}

LONG CDnaDownload::GetChunkCompleted(LONG DownloadID, LONG ChunkID)
{

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		if(ChunkID < pDown->m_PartList.size()) 
			return pDown->m_PartList[ChunkID].BytesCompleted;
	
	}

	return 0;
}

LONG CDnaDownload::GetChunkSize(LONG DownloadID, LONG ChunkID)
{

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		if(ChunkID < pDown->m_PartList.size()) 
			return pDown->m_PartList[ChunkID].EndByte - pDown->m_PartList[ChunkID].StartByte;
	}

	return 0;
}

LONG CDnaDownload::GetChunkFamily(LONG DownloadID, LONG ChunkID)
{

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

	}

	return 0x00FF00;
}

LONG CDnaDownload::GetSourceBytesPerSec(LONG DownloadID, LONG SourceID)
{

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		std::map<int, int>::iterator itSource = pDown->m_HostMap.find(SourceID);
		if(itSource != pDown->m_HostMap.end())
			return pDown->m_Queue[itSource->second].RealBytesPerSec;
	}

	return 0;
}

LONG CDnaDownload::DownloadFile(LPCTSTR Name, LONG Size, LONG HashID, LPCTSTR Hash)
{
	// Create new download
	CGnuDownloadShell* Download = new CGnuDownloadShell(m_gnuTrans);

	CString FileName = Name;
	
	// Fix name if it is in direcory format
	int DirPos = FileName.ReverseFind('/');
	if(DirPos != -1)
		FileName = FileName.Mid(DirPos + 1);

	// Change download name if there's a duplicate
	bool dups = true;
	while(dups)
	{
		dups = false;

		for(int i = 0; i < m_gnuTrans->m_DownloadList.size(); i++)
			if(m_gnuTrans->m_DownloadList[i]->m_Name == FileName)
			{
				FileName = IncrementName(FileName);

				dups = true;
				break;
			}
	}

	Download->Init(FileName, Size, HashID, Hash);

	m_gnuTrans->m_DownloadAccess.Lock();
	m_gnuTrans->m_DownloadList.push_back(Download);
	m_gnuTrans->m_DownloadAccess.Unlock();

	// Go through search list to look for hosts
	Download->Start();
	Download->ReSearch();
	Download->BackupHosts();
	Download->BackupParts();
	m_gnuTrans->DownloadUpdate(Download->m_DownloadID);

	return Download->m_DownloadID;
}

CString CDnaDownload::GetFilePath(LONG DownloadID)
{

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		strResult = itDL->second->GetFilePath();

	return strResult;
}

void CDnaDownload::AddSource(LONG DownloadID, LONG NetworkID, LPCTSTR URL)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pShell = itDL->second;

		if(NetworkID == NETWORK_WEB)
		{
			FileSource WebSource;
			pShell->URLtoSource(WebSource, URL);
			
			pShell->AddHost(WebSource);
		}
	}
}

void CDnaDownload::Proxy(LONG DownloadID, BOOL Enabled, LPCTSTR Default)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pShell = itDL->second;

		if( m_gnuTrans->m_pPrefs->m_ProxyList.size() == 0 && CString(Default).IsEmpty() )
			return;

		pShell->m_UseProxy = (Enabled) ? true : false;
		pShell->m_DefaultProxy = Default;

	}
}

LONG CDnaDownload::GetMetaID(LONG DownloadID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
		return itDL->second->m_MetaID;

	return 0;
}

CString CDnaDownload::GetAttributeValue(LONG DownloadID, LONG AttributeID)
{
	

	CString strResult;

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		std::map<int, CString>::iterator itAttr = itDL->second->m_AttributeMap.find(AttributeID);

		if(itAttr != itDL->second->m_AttributeMap.end())
			strResult = itAttr->second;
	}

	return strResult;
}

void CDnaDownload::SetAttributeValue(LONG DownloadID, LONG AttributeID, LPCTSTR Value)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{	
		int MetaID = itDL->second->m_MetaID;

		CGnuMeta* gnuMeta = m_dnaCore->m_gnuCore->m_pMeta;
		std::map<int, CGnuSchema*>::iterator itMeta = gnuMeta->m_MetaIDMap.find(MetaID);
		if(itMeta != gnuMeta->m_MetaIDMap.end())
		{
			CGnuSchema* pSchema = itMeta->second;
			
			// Check if file has attribute
			std::map<int, CString>::iterator itAttr = itDL->second->m_AttributeMap.find(AttributeID);

			if(itAttr != itDL->second->m_AttributeMap.end())
				itAttr->second = Value;
			else
				itDL->second->m_AttributeMap[AttributeID] = Value;

			itDL->second->m_MetaXml = itDL->second->GetMetaXML(false);
	
			itDL->second->BackupHosts();
		}
	}

}

void CDnaDownload::SetMetaID(LONG DownloadID, LONG MetaID)
{
	

	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);

	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		itDL->second->m_MetaID = MetaID;
		itDL->second->m_MetaXml = itDL->second->GetMetaXML(false);
	}
}

CString CDnaDownload::GetReasonClosed(LONG DownloadID)
{
	return GetReasonClosedStr(DownloadID);
}

void CDnaDownload::AnswerChallenge(LONG DownloadID, LONG SourceID, LPCTSTR Answer)
{
	std::map<int, CGnuDownloadShell*>::iterator itDL = m_gnuTrans->m_DownloadMap.find(DownloadID);
	if(itDL != m_gnuTrans->m_DownloadMap.end())
	{
		CGnuDownloadShell* pDown = itDL->second;

		for(int i = 0; i < pDown->m_Sockets.size(); i++)
			if(pDown->m_Sockets[i]->m_HostID == SourceID)
				pDown->m_Sockets[i]->m_RemoteChallengeAnswer = Answer;
	}

}