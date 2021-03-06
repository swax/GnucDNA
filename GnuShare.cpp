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
#include "DnaCore.h"
#include "DnaShare.h"
#include "DnaEvents.h"

#include "GnuCore.h"
#include "GnuPrefs.h"
#include "GnuNetworks.h"
#include "GnuRouting.h"
#include "GnuControl.h"
#include "G2Control.h"
#include "G2Node.h"
#include "GnuNode.h"
#include "GnuTransfers.h"
#include "GnuFileHash.h"
#include "GnuWordHash.h"
#include "GnuMeta.h"
#include "GnuSchema.h"
#include "GnuProtocol.h"

#include "GnuShare.h"


#define AVG_QUERIES_PER_SEC 50

CGnuShare::CGnuShare(CGnuCore* pCore)
{	
	m_pCore		 = pCore;
	m_pHash		 = NULL;
	m_pWordTable = NULL;
	m_pMeta		 = NULL;
	m_StopThread = false;

	m_UpdateShared = false;
	m_BlockUpdate  = false;

	m_TotalLocalFiles = 0;
	m_TotalLocalSize  = 0;
	m_UltrapeerSizeMarker = 8;

	m_LoadingActive = false;
	m_ShareReload	= false;
	
	m_Minute		= 0;
	m_Freq			= 0;

	m_pShareThread = NULL;

	m_NextFileID = 1;
	m_NextDirID  = 1;
}

CGnuShare::~CGnuShare()
{	
	TRACE0("*** CGnuShare Deconstructing\n");

	if(m_pHash)
		delete m_pHash;
	m_pHash = NULL;


	m_FilesAccess.Lock();
		
		m_FileIDMap.clear();

		// Clear local fiels
		for(int i = 0; i < m_SharedFiles.size(); i++)
		{
			m_SharedFiles[i].Name			= "";
			m_SharedFiles[i].NameLower		= "";
			m_SharedFiles[i].Dir			= "";

			for(int j = 0; j < HASH_TYPES; j++)
				m_SharedFiles[i].HashValues[j] = "";

			m_SharedFiles[i].TimeStamp      = "";

			if(m_SharedFiles[i].TigerTree)
				delete [] m_SharedFiles[i].TigerTree;
		}
 

		m_SharedDirectories.clear();	
		m_SharedFiles.clear(); 


	m_FilesAccess.Unlock();


	m_QueueAccess.Lock();

	while(m_PendingQueries.size())
	{
		delete m_PendingQueries.back();
		m_PendingQueries.pop_back();
	}

	m_QueueAccess.Unlock();


	if(m_pWordTable)
		delete m_pWordTable;
	m_pWordTable = NULL;
}

bool CGnuShare::AddSharedDir(CString strPath, bool bRecursive)
{
	strPath.MakeLower();
	
	SharedDirectory Directory;
	Directory.Name		= strPath;
	Directory.Recursive = bRecursive;
	Directory.Size		= 0;
	Directory.FileCount = 0;

	bool IsNewDirectory = true;
	
	for (int i = 0; i < m_SharedDirectories.size(); ++i)
		if (m_SharedDirectories[i].Name == Directory.Name)
			IsNewDirectory = false;

	if (IsNewDirectory)
	{
		Directory.DirID = m_NextDirID++;
		m_DirIDMap[Directory.DirID] = m_SharedDirectories.size();

		m_SharedDirectories.push_back(Directory);
	}

	return IsNewDirectory;
}

void CGnuShare::endThreads()
{
	if(m_pHash)
		m_pHash->endThreads();

	m_StopThread = true ;
	m_TriggerThread.SetEvent();
	GnuEndThread(m_pShareThread);
}

void CGnuShare::InitShare()
{
	m_pNet  	 = m_pCore->m_pNet;
	m_pMeta		 = m_pCore->m_pMeta;
	m_pHash		 = new CGnuFileHash(this);
	m_pWordTable = new CGnuWordHash(this);


	// Get cpu frequency, used to analyze share thread
	LARGE_INTEGER FreqTime;
	if(QueryPerformanceFrequency(&FreqTime))
		m_Freq = FreqTime.QuadPart;
	else
	{
		//AfxMessageBox("Analyzing CPU Frequency not supported");
		return;
	}	
	

	// Begin share thread
	GnuStartThread(m_pShareThread, ShareWorker, this);
}

void CGnuShare::ReleaseEventList(DWORD &EventCount, LPHANDLE EventList)
{
	// Close wait events on all directories
	for(int i = 1; i < EventCount; i++)
		FindCloseChangeNotification( EventList[i]);

	EventCount = 1;
}

void CGnuShare::ResetDirectories(DWORD &EventCount, LPHANDLE EventList)
{
	m_FilesAccess.Lock();

		ReleaseEventList(EventCount, EventList);

		// Reset partial watch
		EventList[1] = FindFirstChangeNotification(m_pCore->m_pPrefs->m_PartialDir, false, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);

		if(EventList[1] != INVALID_HANDLE_VALUE)
			EventCount++;

		// Reset wait events on all shared directories
		for(int i = 0; i < m_SharedDirectories.size(); i++)
		{
			if(EventCount >= MAX_EVENTS)
				break;

			EventList[EventCount] = FindFirstChangeNotification(m_SharedDirectories[i].Name, m_SharedDirectories[i].Recursive, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);

			if(EventList[EventCount] != INVALID_HANDLE_VALUE)
				EventCount++;
		}

	m_FilesAccess.Unlock();
}

void CGnuShare::LoadFiles()
{
	TRACE0("Loading Shared Files...\n");

	m_FilesAccess.Lock();

		m_pWordTable->ClearLocalTable();

		for(int i = 0; i < m_SharedFiles.size(); i++)
		{
			m_SharedFiles[i].Name		= "";
			m_SharedFiles[i].NameLower	= "";
			m_SharedFiles[i].Dir		= "";

			for(int j = 0; j < HASH_TYPES; j++)
				m_SharedFiles[i].HashValues[j] = "";

			m_SharedFiles[i].TimeStamp	= "";

			if(m_SharedFiles[i].TigerTree)
			{
				delete [] m_SharedFiles[i].TigerTree;
				m_SharedFiles[i].TigerTree = NULL;
			}
		}


		// Load shared files
		m_SharedFiles.clear();
		m_FileIDMap.clear();
		m_SharedHashMap.clear();
	
	m_FilesAccess.Unlock();

	m_LoadingActive = true;

	m_TotalLocalFiles = 0;
	m_TotalLocalSize  = 0;

	for(i = 0; i < m_SharedDirectories.size(); i++)
	{
		if(m_SharedDirectories[i].Recursive)
			RecurseLoad(m_SharedDirectories[i].Name, "", true, m_SharedDirectories[i].FileCount, m_SharedDirectories[i].Size);
		else
			RecurseLoad(m_SharedDirectories[i].Name, "", false, m_SharedDirectories[i].FileCount, m_SharedDirectories[i].Size);
	}

	if(m_StopThread)
		return;


	// Fill word lookup table
	for(i = 0; i < m_SharedFiles.size(); i++)
	{
		m_pWordTable->InsertString(m_SharedFiles[i].Name, i);


		// Insert hash
		if( !m_SharedFiles[i].HashValues[HASH_SHA1].empty() )
		{
			CString Hash = "urn:" + HashIDtoTag(HASH_SHA1) + CString(m_SharedFiles[i].HashValues[HASH_SHA1].c_str());
			m_pWordTable->InsertString((LPCTSTR) Hash, i, false);
		}
		/*for(int j = 0; j < HASH_TYPES; j++)
			if( !m_SharedFiles[i].HashValues[j].empty() )
			{
				CString Hash = "urn:" + HashIDtoTag(j) + CString(m_SharedFiles[i].HashValues[j].c_str());
				m_pWordTable->InsertString((LPCTSTR) Hash, i, false);
			}*/


		// Insert meta
		std::map<int, CGnuSchema*>::iterator itMeta = m_pMeta->m_MetaIDMap.find(m_SharedFiles[i].MetaID);

		if(itMeta != m_pMeta->m_MetaIDMap.end())
		{
			CGnuSchema* pSchema = itMeta->second;

			m_pWordTable->InsertString((LPCTSTR) pSchema->m_Name, i);

			if( !pSchema->m_Namespace.IsEmpty() )
				m_pWordTable->InsertString((LPCTSTR) pSchema->m_Namespace, i, false);

			std::map<int, CString>::iterator itAttr = m_SharedFiles[i].AttributeMap.begin();
			for( ; itAttr != m_SharedFiles[i].AttributeMap.end(); itAttr++)
				m_pWordTable->InsertString((LPCTSTR) itAttr->second, i, true); // (LPCTSTR) (pSchema->m_Name + "." + pSchema->GetAttributeName(itAttr->first)));
		}
	}

	
	HitTableRefresh();


	m_LoadingActive = false;
	

	// Put ultrapeer marker on shared file size
	m_UltrapeerSizeMarker = 8;
	while(m_UltrapeerSizeMarker < m_TotalLocalSize && m_UltrapeerSizeMarker)
		m_UltrapeerSizeMarker *= 2;


	// Start hashing new files
	m_pHash->m_EverythingHashed = false;
}

void CGnuShare::RecurseLoad(CString FullPath, CString DirPath, bool doRecurse, DWORD &DirCount, uint64 &DirSize)
{
	// Add wild card to directory path
	CString strWildcard(FullPath);

	if(strWildcard.GetAt( strWildcard.GetLength() - 1) != '\\')
		strWildcard += "\\*";
	else
		strWildcard += "*";

	// Go through files in directory
	CFileFind Finder;

	BOOL bWorking = Finder.FindFile(strWildcard);

	while (bWorking)
	{
		if(m_StopThread)
		{
			bWorking = false;
			continue;
		}

		bWorking = Finder.FindNextFile();

		// skip . and .. files
		if (Finder.IsDots())
			continue;

		// if it's a directory, recursively search it
		if (Finder.IsDirectory())
		{
			CString str = Finder.GetFilePath();
			int     pos = str.ReverseFind('\\');

			if(pos != -1)
				if( str.Mid(pos + 1).CompareNoCase("Partials") != 0 && str.Mid(pos + 1).CompareNoCase("Meta") != 0)
					if(doRecurse)
						RecurseLoad(str, DirPath + str.Mid(pos), 1, DirCount, DirSize);
		}
		else
		{
			CString FilePath = Finder.GetFilePath();
			CString	FileName = FilePath.Mid( FilePath.ReverseFind('\\') + 1);
			uint64  FileSize = Finder.GetLength();

			// get right size for mp3s, ignoring id3
			if (FileName.Right(4).CompareNoCase(".mp3") == 0)
				FileSize = CFileLock::ScanFileSize(FilePath);
			
			if(m_pCore->m_pPrefs->m_ReplyFilePath && DirPath.GetLength() > 0)
				FileName = DirPath + "\\" + FileName;
	
			// Make network compatible
			FileName.Replace("\\", "/");
			if(FileName[0] == '/')
				FileName = FileName.Mid(1);
		
			//Skip partial files from other apps, etc.
			if (FileName.Right(4).CompareNoCase(".tmp") == 0) //General temp file
				continue;	
			if (FileName.Right(4).CompareNoCase(".swp") == 0) //Windows swap file
				continue;	
			if (FileName.Right(5).CompareNoCase(".part") == 0) //Edonkey partial file
				continue;	
			if (FileName.Left(8).CompareNoCase("download") == 0 && FileName.Right(4).CompareNoCase(".dat") == 0) //FastTrack partial file
				continue;	

			// Make a file item
			SharedFile NewShare;
			
			NewShare.FileID  = m_NextFileID++;
		
			NewShare.Size	= FileSize;
			NewShare.Dir	= FilePath;
			NewShare.Name	= FileName;
			FileName.MakeLower();
			NewShare.NameLower = FileName;

			CTime TimeStamp;
			Finder.GetLastWriteTime(TimeStamp);
			CString TimeStr = TimeStamp.Format("%Y %m %d %H %M %S");
			NewShare.TimeStamp   = TimeStr;

			m_pHash->LookupFileHash(FilePath, TimeStr, NewShare);

			m_pMeta->LoadFileMeta(NewShare);


			m_FilesAccess.Lock();
			
				CString Sha1Hash = NewShare.HashValues[HASH_SHA1].c_str();
				if( !Sha1Hash.IsEmpty() )
					m_SharedHashMap[Sha1Hash] = m_SharedFiles.size();

				m_FileIDMap[NewShare.FileID] = m_SharedFiles.size();
				m_SharedFiles.push_back(NewShare);
			
			m_FilesAccess.Unlock();


			m_TotalLocalFiles++;
			m_TotalLocalSize += FileSize / 1024;

			DirCount++;
			DirSize += Finder.GetLength();
		}

	}

	Finder.Close();
}

void CGnuShare::HitTableRefresh()
{
	if( m_pNet->m_pGnu )
		m_pNet->m_pGnu->ShareUpdate();

	if( m_pNet->m_pG2 )
		m_pNet->m_pG2->ShareUpdate();
}

void CGnuShare::StopShare(UINT FileID)
{
	m_FilesAccess.Lock();

		std::map<UINT, UINT>::iterator itFile = m_FileIDMap.find(FileID);

		if(itFile != m_FileIDMap.end())
			if(itFile->second < m_SharedFiles.size())
			{
				int i = itFile->second;

				m_SharedFiles[i].Index		= 0;
				m_SharedFiles[i].Name		= "";
				m_SharedFiles[i].NameLower  = "";
				m_SharedFiles[i].Dir		= "";
			}

	m_FilesAccess.Unlock();
}

CString CGnuShare::GetFilePath(int index)
{
	m_FilesAccess.Lock();

		std::vector<SharedFile>::iterator itFile;
		for (itFile = m_SharedFiles.begin(); itFile != m_SharedFiles.end(); itFile++)
			if(index == (*itFile).Index) 
			{
				CString Filepath = CString( (*itFile).Dir.c_str() );
				m_FilesAccess.Unlock();

				return Filepath;			
			}

	m_FilesAccess.Unlock();

	return "";
}

CString CGnuShare::GetFileName(int index)
{
	m_FilesAccess.Lock();

		std::vector<SharedFile>::iterator itFile;
		for (itFile = m_SharedFiles.begin(); itFile != m_SharedFiles.end(); itFile++)
			if(index == (*itFile).Index) 
			{
				CString Filename = CString( (*itFile).Name.c_str() );
				m_FilesAccess.Unlock();
					
				return Filename;			
			}

	m_FilesAccess.Unlock();

	return "";
}

CString CGnuShare::GetFileHash(int index, int HashType)
{
	m_FilesAccess.Lock();

		std::vector<SharedFile>::iterator itFile;
		for (itFile = m_SharedFiles.begin(); itFile != m_SharedFiles.end(); itFile++)
			if(index == (*itFile).Index) 
			{
					m_FilesAccess.Unlock();
					return CString( (*itFile).HashValues[HashType].c_str() );			
			}

	m_FilesAccess.Unlock();

	return "";
}


UINT CGnuShare::ShareWorker(LPVOID pVoidShare)
{
	TRACE0("*** Search Thread Started\n");
	srand((unsigned)time(NULL));

	CGnuShare*    pShare    = (CGnuShare*) pVoidShare;
	CGnuCore*	  pCore     = pShare->m_pCore;
	CGnuNetworks* pNet      = pCore->m_pNet;
	CGnuWordHash* pWordHash = pShare->m_pWordTable;
	
	// wait for saved hashes to load before loading files etc..
	WaitForSingleObject( (HANDLE) pShare->m_HashReady, INFINITE);

	// Search vars
	byte QueryReply[16384];
	
	// Set up event system
	HANDLE EventList[MAX_EVENTS];
	DWORD  EventCount = 1;
	DWORD  WhichEvent = 0;
	EventList[0]   = (HANDLE) pShare->m_TriggerThread;

	pShare->ResetDirectories(EventCount, EventList);


	for(;;)
	{
		// Waiting for an event to be triggered, once it is, reload
		WhichEvent = MsgWaitForMultipleObjects(EventCount, EventList, false, INFINITE, NULL) - WAIT_OBJECT_0;
		pShare->m_TriggerThread.ResetEvent();

		if(pShare->m_StopThread)
		{
			TRACE0("*** Search Thread Ended\n");
			return 0;
		}

		if(pShare->m_BlockUpdate)
		{
			pShare->ResetDirectories(EventCount, EventList);
			continue;
		}

		// Directory changed contents
		if(WhichEvent > 0)
			pShare->ResetDirectories(EventCount, EventList);

		// Partial dir changed contenets
		if(WhichEvent == 1)
		{
		}

		// Shared dir changed contents
		if(WhichEvent > 1)
		{
			if( !pCore->m_pPrefs->m_NoReload )		
				pShare->m_UpdateShared = true;
		}
		

		// Check for query look up requests
		while(pShare->m_UpdateShared || pShare->m_PendingQueries.size())
		{
			if(pShare->m_StopThread)
			{
				TRACE0("*** Search Thread Ended\n");
				return 0;
			}

			// Check if shared files need to be updated
			if(pShare->m_UpdateShared)
			{	
				pShare->LoadFiles();

				pShare->ResetDirectories(EventCount, EventList);
				
				pShare->m_UpdateShared = false;
				pShare->m_ShareReload  = true;
				
				continue;
			}

			GnuQuery* FileQuery = NULL;

			// Check for new searches
			pShare->m_QueueAccess.Lock();

				if(pShare->m_PendingQueries.size())
				{
					FileQuery = pShare->m_PendingQueries.front();
					pShare->m_PendingQueries.pop_front();
				}
				else
				{
					pShare->m_QueueAccess.Unlock();
					continue;
				}

				// Make sure query buffer isnt overflowing
				while(pShare->m_PendingQueries.size() > 100)
				{
					delete pShare->m_PendingQueries.back();
					pShare->m_PendingQueries.pop_back();
				}

			pShare->m_QueueAccess.Unlock();


			// Disabled because it causes queries not to be sent to children
			// Dont send results if no upload slots available
			//if(pCore->m_pPrefs->m_SendOnlyAvail)
			//	if(pCore->m_pPrefs->m_MaxUploads)
			//		if(pCore->m_pTrans->CountUploading() >= pCore->m_pPrefs->m_MaxUploads)
			//			continue;
			
			// A four space query is an index query
			/*if( FileQuery.Terms.size() && FileQuery.Terms[0].Compare("    ") == 0 && FileQuery.Hops == 1)
			{
				pShare->SendFileIndex(FileQuery, QueryReply);	
				continue;
			}*/

			
			// Look up query in shared files and remote indexes
			std::list<UINT>	 MatchingIndexes;
			std::list<int>	 MatchingNodes;

			pWordHash->LookupQuery(*FileQuery, MatchingIndexes, MatchingNodes);
				
			// Check for size criteria
			if(FileQuery->MinSize || FileQuery->MaxSize)
			{
				std::list<UINT>::iterator itIndex;
				for( itIndex = MatchingIndexes.begin(); itIndex != MatchingIndexes.end(); itIndex++)
				{
					if(FileQuery->MinSize)
						if(pShare->m_SharedFiles[*itIndex].Size < FileQuery->MinSize)
						{
							itIndex = MatchingIndexes.erase(itIndex);
							continue;
						}

					if(FileQuery->MaxSize)
						if(pShare->m_SharedFiles[*itIndex].Size > FileQuery->MaxSize)
						{
							itIndex = MatchingIndexes.erase(itIndex);
							continue;
						}
				}
			}

			// Gnutella
			if( FileQuery->Network == NETWORK_GNUTELLA && pNet->m_pGnu )
			{
				if( MatchingIndexes.size() )
					pNet->m_pGnu->m_pProtocol->Encode_QueryHit(*FileQuery, MatchingIndexes, QueryReply);

				if( FileQuery->Forward && MatchingNodes.size() )
					pNet->m_pGnu->m_pProtocol->Send_Query(*FileQuery, MatchingNodes);
			}
	

			// G2
			else if( FileQuery->Network == NETWORK_G2 && pNet->m_pG2 )
			{
				if( MatchingIndexes.size() )
					pNet->m_pG2->Send_QH2(*FileQuery, MatchingIndexes);

				if( FileQuery->Forward && MatchingNodes.size() )
					pNet->m_pG2->Send_Q2(*FileQuery, MatchingNodes);
			}
			
			delete FileQuery;
		}		
	}

	pShare->ReleaseEventList(EventCount, EventList);

	TRACE0("*** Search Thread Ended\n");
	return 0;
}

void CGnuShare::ShareUpdate(UINT FileID)
{
	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->ShareUpdate(FileID);
}

void CGnuShare::ShareReload()
{
	// Block update so ShareReload does not cause more ShareReloads	
	m_BlockUpdate = true;

	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->ShareReload();
	
	m_BlockUpdate = false;
}


void CGnuShare::Timer()
{
	if(m_ShareReload)
	{
		m_ShareReload = false;
		
		ShareReload();
	}

	if(m_BlockUpdate)
		m_BlockUpdate = false;

	m_pHash->Timer();


	/*m_Minute++;
	if(m_Minute == 300)
	{
		int TableSize = 0; // Only total remote qrp size, not total (1 << GNU_TABLE_BITS) * sizeof(WordKey);
		int TableEntries = 0;
		for(int i = 0; i < (1 << GNU_TABLE_BITS); i++)
			if(m_pWordTable->m_HashTable[i].RemoteKey)
			{
				TableEntries++;
				TableSize += m_pWordTable->m_HashTable[i].RemoteKey->size();
			}

		m_pCore->DebugLog( CommaIze(NumtoStr(TableEntries)) + " Slots, " + CommaIze(NumtoStr(TableSize)) + " Pointers, for " + NumtoStr(m_pComm->m_NodeList.size()) + " Nodes");

		m_Minute = 0; 
	}*/
}

void CGnuShare::AddShareAltLocation(CString Hash, IPv4 Location)
{
	ASSERT(Location.Port);
	if(Location.Port == 0)
		return;

	if( IsPrivateIP(Location.Host) )
		return;

	if( !m_pNet->NotLocal( Node( IPtoStr(Location.Host), Location.Port) ) )
		return;


	m_FilesAccess.Lock();

		int FileIndex = 0;
		std::map<CString, int>::iterator itShare = m_SharedHashMap.find(Hash);
		if(itShare != m_SharedHashMap.end())
			FileIndex = itShare->second;

		if(FileIndex == 0)
		{
			m_FilesAccess.Unlock();
			return;
		}


		bool found = false;

		for(int i = 0; i < m_SharedFiles[FileIndex].AltHosts.size(); i++)
			if (Location.Host.S_addr == m_SharedFiles[FileIndex].AltHosts[i].Host.S_addr)
				{
					found = true;
					break;
				}

		if(!found)
			m_SharedFiles[FileIndex].AltHosts.push_back(Location);


		if(m_SharedFiles[FileIndex].AltHosts.size() > 15)
			m_SharedFiles[FileIndex].AltHosts.pop_front();

	m_FilesAccess.Unlock();
}

CString CGnuShare::GetShareAltLocHeader(CString Hash, IP ToIP, int HostCount)
{
	m_FilesAccess.Lock();

		int FileIndex = 0;
		std::map<CString, int>::iterator itShare = m_SharedHashMap.find(Hash);
		if(itShare != m_SharedHashMap.end())
			FileIndex = itShare->second;

		if(FileIndex == 0)
		{
			m_FilesAccess.Unlock();
			return "";
		}

		CString Header = "X-Alt: ";

		int j = 0;

		if(m_SharedFiles[FileIndex].AltHosts.size() < HostCount)
			HostCount = m_SharedFiles[FileIndex].AltHosts.size(); 

		std::vector<int> HostIndexes;

		// Get random indexes to send to host
		while(HostCount > 0)
		{
			HostCount--;

			int NewIndex = rand() % m_SharedFiles[FileIndex].AltHosts.size() + 0;

			if(ToIP.S_addr == m_SharedFiles[FileIndex].AltHosts[NewIndex].Host.S_addr)
				continue;

			bool found = false;

			for(j = 0; j < HostIndexes.size(); j++)
				if(HostIndexes[j] == NewIndex)
					found = true;

			if(!found)
				HostIndexes.push_back(NewIndex);
		}

		for(j = 0; j < HostIndexes.size(); j++)
			Header += IPv4toStr(m_SharedFiles[FileIndex].AltHosts[ HostIndexes[j] ]) + ", ";

	m_FilesAccess.Unlock();
	

	if( HostIndexes.size() == 0)
		return "";

	Header.Trim(", ");
	
	Header += "\r\n";

	

	return Header;
}

void CGnuShare::RemoveShareAltLocation(CString Hash, IPv4 Location)
{
	m_FilesAccess.Lock();

		int FileIndex = 0;
		std::map<CString, int>::iterator itShare = m_SharedHashMap.find(Hash);
		if(itShare != m_SharedHashMap.end())
			FileIndex = itShare->second;

		if(FileIndex == 0)
		{
			m_FilesAccess.Unlock();
			return;
		}

		std::deque<IPv4>::iterator itAlt;
		for(itAlt = m_SharedFiles[FileIndex].AltHosts.begin(); itAlt != m_SharedFiles[FileIndex].AltHosts.end(); itAlt++)
			if (Location.Host.S_addr == itAlt->Host.S_addr)
				{
					m_SharedFiles[FileIndex].AltHosts.erase(itAlt);
					break;
				}

	m_FilesAccess.Unlock();
}


	
