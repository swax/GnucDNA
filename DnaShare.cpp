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
	license your contribution.

	For support, questions, commercial use, etc...
	E-Mail: swabby@c0re.net

********************************************************************************/


#include "stdafx.h"

#include "DnaCore.h"

#include "GnuCore.h"
#include "GnuShare.h"
#include "GnuWordHash.h"
#include "GnuFileHash.h"
#include "GnuMeta.h"
#include "GnuSchema.h"

#include "DnaShare.h"


CDnaShare::CDnaShare()
{
	m_dnaCore   = NULL;
	m_gnuShare  = NULL;
	m_gnuMeta   = NULL;

}

void CDnaShare::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;

	m_gnuShare = dnaCore->m_gnuCore->m_pShare;
	m_gnuMeta  = dnaCore->m_gnuCore->m_pMeta;
}

CDnaShare::~CDnaShare()
{
	// To terminate the application when all objects created with
	// 	with OLE automation, the destructor calls AfxOleUnlockApp.
	
	 
}


// CDnaShare message handlers

std::vector<int> CDnaShare::GetFileIDs(void)
{
	std::vector<int> FileIDs;
	
	m_gnuShare->m_FilesAccess.Lock();
	
	for(int i = 0; i < m_gnuShare->m_SharedFiles.size(); i++)
		FileIDs.push_back( m_gnuShare->m_SharedFiles[i].FileID );

	m_gnuShare->m_FilesAccess.Unlock();


	return FileIDs;
}

LONG CDnaShare::GetFileIndex(LONG FileID)
{
	

	UINT retIndex = 0;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			retIndex = m_gnuShare->m_SharedFiles[itFile->second].Index;

	m_gnuShare->m_FilesAccess.Unlock();


	return retIndex;
}

CString CDnaShare::GetFileDir(LONG FileID)
{
	


	CString strResult;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			strResult = m_gnuShare->m_SharedFiles[itFile->second].Dir.c_str();

	m_gnuShare->m_FilesAccess.Unlock();


	return strResult;
}

CString CDnaShare::GetFileName(LONG FileID)
{
	

	CString strResult;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			strResult = m_gnuShare->m_SharedFiles[itFile->second].Name.c_str();

	m_gnuShare->m_FilesAccess.Unlock();


	return strResult;
}

LONG CDnaShare::GetFileSize(LONG FileID)
{
	

	UINT retSize = 0;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			retSize = m_gnuShare->m_SharedFiles[itFile->second].Size;

	m_gnuShare->m_FilesAccess.Unlock();


	return retSize;
}

LONG CDnaShare::GetFileMatches(LONG FileID)
{
	

	UINT retMatches = 0;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			retMatches = m_gnuShare->m_SharedFiles[itFile->second].Matches;

	m_gnuShare->m_FilesAccess.Unlock();


	return retMatches;
}

LONG CDnaShare::GetFileUploads(LONG FileID)
{
	

	UINT retUploads = 0;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			retUploads = m_gnuShare->m_SharedFiles[itFile->second].Uploads;

	m_gnuShare->m_FilesAccess.Unlock();


	return retUploads;
}

CString CDnaShare::GetFileHash(LONG FileID, LONG HashID)
{
	

	CString strResult;
	
	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			if(HashID >= 0 && HashID < HASH_TYPES)
				strResult = m_gnuShare->m_SharedFiles[itFile->second].HashValues[HashID].c_str();

	m_gnuShare->m_FilesAccess.Unlock();


	return strResult;
}

void CDnaShare::StartHashing(void)
{
	

	m_gnuShare->m_pHash->m_StopHashing = false;
	m_gnuShare->m_pHash->m_HashEvent.SetEvent();
}

void CDnaShare::StopHashing(void)
{
	

	m_gnuShare->m_pHash->m_StopHashing = true;
}

BOOL CDnaShare::IsEverythingHashed(void)
{
	

	if(m_gnuShare->m_pHash->m_EverythingHashed)
		return TRUE;


	return FALSE;
}

BOOL CDnaShare::IsHashingStopped(void)
{
	

	if(m_gnuShare->m_pHash->m_StopHashing)
		return TRUE;


	return FALSE;
}



void CDnaShare::StopSharingFile(LONG FileID)
{
	

	m_gnuShare->StopShare(FileID);
}

std::vector<CString> CDnaShare::GetFileKeywords(LONG FileID)
{
	std::vector<CString> FileKeywords;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);
	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
		{
			int i = itFile->second;

			CString Keyword;

			for(int j = 0; j < m_gnuShare->m_SharedFiles[i].Keywords.size(); j++)
			{
				Keyword = m_gnuShare->m_SharedFiles[i].Keywords[j].c_str();
				Keyword += ":" + NumtoStr(m_gnuShare->m_SharedFiles[i].HashIndexes[j]);

				FileKeywords.push_back( Keyword );
			}
		}

	m_gnuShare->m_FilesAccess.Unlock();

	
	return FileKeywords;
}

std::vector<CString> CDnaShare::GetFileAltLocs(LONG FileID)
{
	std::vector<CString> AltLocs;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
		{
			int i = itFile->second;

			for(int j = 0; j < m_gnuShare->m_SharedFiles[i].AltHosts.size(); j++)
				AltLocs.push_back( IPv4toStr(m_gnuShare->m_SharedFiles[i].AltHosts[j]) );
		}

	m_gnuShare->m_FilesAccess.Unlock();


	return AltLocs;
}

std::vector<int> CDnaShare::GetSharedDirIDs(void)
{
	std::vector<int> SharedDirIDs;

	m_gnuShare->m_FilesAccess.Lock();

	for(int i = 0; i < m_gnuShare->m_SharedDirectories.size(); i++)
		SharedDirIDs.push_back( m_gnuShare->m_SharedDirectories[i].DirID );

	m_gnuShare->m_FilesAccess.Unlock();


	return SharedDirIDs;
}

CString CDnaShare::GetDirName(LONG DirID)
{
	

	CString strResult;
	
	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itDir = m_gnuShare->m_DirIDMap.find(DirID);

	if(itDir != m_gnuShare->m_DirIDMap.end())
		if(itDir->second < m_gnuShare->m_SharedDirectories.size())
			strResult = m_gnuShare->m_SharedDirectories[itDir->second].Name;

	m_gnuShare->m_FilesAccess.Unlock();


	return strResult;
}

BOOL CDnaShare::GetDirRecursive(LONG DirID)
{
	

	BOOL retRecur = FALSE;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itDir = m_gnuShare->m_DirIDMap.find(DirID);

	if(itDir != m_gnuShare->m_DirIDMap.end())
		if(itDir->second < m_gnuShare->m_SharedDirectories.size())
			if(m_gnuShare->m_SharedDirectories[itDir->second].Recursive)
				retRecur = TRUE;

	m_gnuShare->m_FilesAccess.Unlock();


	return retRecur;
}

LONG CDnaShare::GetDirFileCount(LONG DirID)
{
	

	UINT retCount = 0;

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itDir = m_gnuShare->m_DirIDMap.find(DirID);

	if(itDir != m_gnuShare->m_DirIDMap.end())
		if(itDir->second < m_gnuShare->m_SharedDirectories.size())
			retCount = m_gnuShare->m_SharedDirectories[itDir->second].FileCount;

	m_gnuShare->m_FilesAccess.Unlock();

	return retCount;
}

void CDnaShare::SetSharedDirs(std::vector<CString> &DirPaths)
{
	m_gnuShare->m_FilesAccess.Lock();

		m_gnuShare->m_DirIDMap.clear();
		m_gnuShare->m_SharedDirectories.clear();

		for(int i = 0; i < DirPaths.size(); i++)
		{
			CString RealName  = DirPaths[i];
			CString LowName   = RealName;

			LowName.MakeLower();

			bool Recurse = false;
			if(LowName.Find("recursive") != -1)
			{
				LowName  = LowName.Left( LowName.ReverseFind(','));
				RealName = RealName.Left( RealName.ReverseFind(','));
				Recurse = true;
			}

			// Insert directory
			SharedDirectory Directory;
			Directory.Name		= RealName;
			Directory.Recursive = Recurse;
			Directory.Size		= 0;
			Directory.FileCount = 0;

			if(!RealName.IsEmpty())
			{
				Directory.DirID = m_gnuShare->m_NextDirID++;
				m_gnuShare->m_DirIDMap[Directory.DirID] = m_gnuShare->m_SharedDirectories.size();

				m_gnuShare->m_SharedDirectories.push_back(Directory);
			}
			else
				continue;

		}
		
	m_gnuShare->m_FilesAccess.Unlock();

		
	m_gnuShare->m_UpdateShared = true;
	m_gnuShare->m_TriggerThread.SetEvent();
}

BOOL CDnaShare::IsLoading(void)
{
	

	if(m_gnuShare->m_LoadingActive)
		return TRUE;

	return FALSE;
}

LONG CDnaShare::GetFileCount(void)
{
	

	return m_gnuShare->m_TotalLocalFiles;
}

LONG CDnaShare::GetFileMetaID(LONG FileID)
{
	

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			return m_gnuShare->m_SharedFiles[itFile->second].MetaID;


	return 0;
}

CString CDnaShare::GetFileAttributeValue(LONG FileID, LONG AttributeID)
{
	

	CString strResult;

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
		{
			std::map<int, CString>::iterator itAttr = m_gnuShare->m_SharedFiles[itFile->second].AttributeMap.find(AttributeID);

			if(itAttr != m_gnuShare->m_SharedFiles[itFile->second].AttributeMap.end())
				strResult = itAttr->second;
		}

	return strResult;
}

void CDnaShare::SetFileAttributeValue(LONG FileID, LONG AttributeID, LPCTSTR Value)
{
	


	m_gnuShare->m_BlockUpdate = true;


	// Find file
	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
		{
			//m_dnaCore->m_gnuCore->DebugLog("File Found");

			int MetaID = m_gnuShare->m_SharedFiles[itFile->second].MetaID;

			std::map<int, CGnuSchema*>::iterator itMeta = m_gnuMeta->m_MetaIDMap.find(MetaID);

			if(itMeta != m_gnuMeta->m_MetaIDMap.end())
			{
				CGnuSchema* pSchema = itMeta->second;
				
				pSchema->SaveFileAttribute(&m_gnuShare->m_SharedFiles[itFile->second], AttributeID, Value);
				
				// Insert meta attribute value into QRP table
				m_gnuShare->m_pWordTable->InsertString(Value, itFile->second, true, (LPCTSTR) (pSchema->m_Name + "." + pSchema->GetAttributeName(AttributeID)));


				// Rehash file
				m_gnuShare->m_pHash->m_EverythingHashed = false;
			}

			//m_dnaCore->m_gnuCore->DebugLog("File Set");
		}
}

LONG CDnaShare::GetTotalFileSize(void)
{
	

	return m_gnuShare->m_TotalLocalSize;
}

LONG CDnaShare::GetHashSpeed(void)
{
	

	return m_gnuShare->m_pHash->m_CpuUsage * 100;
}

void CDnaShare::SetHashSpeed(LONG newVal)
{
	

	if( newVal < 0)
		newVal = 0;

	if( newVal > 100)
		newVal = 100;

	m_gnuShare->m_pHash->m_CpuUsage = (double) newVal / (double) 100;
}

void CDnaShare::SetFileMetaID(LONG FileID, LONG MetaID)
{
	

	m_gnuShare->m_FilesAccess.Lock();

	std::map<UINT, UINT>::iterator itFile = m_gnuShare->m_FileIDMap.find(FileID);

	if(itFile != m_gnuShare->m_FileIDMap.end())
		if(itFile->second < m_gnuShare->m_SharedFiles.size())
			m_gnuShare->m_SharedFiles[itFile->second].MetaID = MetaID;

	m_gnuShare->m_FilesAccess.Unlock();
}
