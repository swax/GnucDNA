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
#include "GnuCore.h"

#include "GnuShare.h"
#include "GnuWordHash.h"

#include "hash/Sha1.h"
#include "hash/md5.h"
#include "hash/ed2k_md4.h"
#include "hash/tigertree2.h"

#include "GnuFileHash.h"


#define BUFF_SIZE 32768

UINT HashWorker(LPVOID pVoidHash);


CGnuFileHash::CGnuFileHash(CGnuShare* pShare)
{
	m_pCore   = pShare->m_pCore;
	m_pShare  = pShare;

	m_NextIndex = 1;

	m_pHashThread = NULL;

	m_StopThread  = false;
	m_StopHashing = false;

	m_EverythingHashed = true;
	m_HashSetModified  = false;

	m_SaveHashFile = false;
	m_SaveInterval = 0;

	m_CpuUsage = 0.10;

	// Begin share thread
	GnuStartThread(m_pHashThread, HashWorker, this);
}

CGnuFileHash::~CGnuFileHash()
{
	for(int i = 0; i < m_HashedFiles.size(); i++) 
		if(m_HashedFiles[i].TigerTree)
			delete [] m_HashedFiles[i].TigerTree;
}

void CGnuFileHash::endThreads()
{
	m_StopThread = true;
	m_HashEvent.SetEvent();
	GnuEndThread(m_pHashThread);
}

UINT HashWorker(LPVOID pVoidHash)
{
	TRACE0("*** Hash Thread Started\n");

	CGnuFileHash* pHash  = (CGnuFileHash*) pVoidHash;
	CGnuShare*    pShare = (CGnuShare*) pHash->m_pShare;
	
	// load saved hashes 

	try
	{
		pHash->LoadShareHashes(pHash->m_pCore->m_RunPath + "GnuHashes.ini");
	}
	catch(...)
	{
		DeleteFile(pHash->m_pCore->m_RunPath + "GnuHashes.ini");
		pHash->LoadShareHashes(pHash->m_pCore->m_RunPath + "GnuHashes.ini");
	}

	// trigger share thread to load files
	pShare->m_HashReady.SetEvent();

	// hash files
	LARGE_INTEGER StartTime, EndTime;

	CFileLock HashFile;
	CString   FileName;
	int	      FileIndex = 0;

	sha_ctx     Sha1_Context;
	MD5Context  MD5_Context;
	ED2K_CTX    MD4_Context;
	tt2_context Tiger_Context;
	Tiger_Context.tree = NULL;
	
	byte Sha1_Digest[20];
	byte MD5_Digest[16];
	byte MD4_Digest[16];
	byte Tiger_Digest[24];


	UINT		ReadSize = 0;
	static char Buffer[BUFF_SIZE];


	for(;;)
	{
		pHash->m_HashEvent.ResetEvent();
			WaitForSingleObject(pHash->m_HashEvent, INFINITE);

		if(pHash->m_StopThread)
			return 0;

		if(pHash->m_SaveHashFile && pHash->m_SaveInterval > HASH_SAVE_INTERVAL)
		{
			pHash->SaveShareHashes(pHash->m_pCore->m_RunPath + "GnuHashes.ini");

			pHash->m_SaveInterval = 0;
			pHash->m_SaveHashFile = false;
		}

		if(pHash->m_EverythingHashed || pHash->m_StopHashing)
			continue;


		QueryPerformanceCounter(&StartTime);
		double MaxTicks = pHash->m_pShare->m_Freq * pHash->m_CpuUsage;


		// If a file isnt in the process of being hashed..
		if(HashFile.m_hFile == CFile::hFileNull)
		{
			pShare->m_FilesAccess.Lock();

			// Find files to hashed
			for (int i = 0; i < pShare->m_SharedFiles.size(); i++)
			{
				bool needHash = false; 
				
				for(int j = 0; j < HASH_TYPES; j++ ) 
					if( pShare->m_SharedFiles[i].HashValues[j] == "" ) 
						needHash = true; 

				if(pShare->m_SharedFiles[i].TreeSize == 0 || pShare->m_SharedFiles[i].TigerTree == NULL)
					needHash = true;

				if(needHash && !pShare->m_SharedFiles[i].HashError && !pShare->m_SharedFiles[i].Dir.empty()) 
				{
					sha_init(&Sha1_Context);
					MD5Init(&MD5_Context);
					ED2KInit(&MD4_Context);
					tt2_init(&Tiger_Context);

					memset(Sha1_Digest,  0, 20);
					memset(MD5_Digest,   0, 16);
					memset(MD4_Digest,   0, 16);
					memset(Tiger_Digest, 0, 24);
					
					FileIndex = pShare->m_SharedFiles[i].Index;

				
					if(HashFile.Open(pShare->m_SharedFiles[i].Dir.c_str(), CFile::modeRead | CFile::shareDenyWrite, true))
					{
						tt2_initTree(&Tiger_Context, HashFile.GetLength());		

						break;
					}
					else
					{
						HashFile.Abort();
						pShare->m_SharedFiles[i].HashError = true;
					}	
				}
			}

			pShare->m_FilesAccess.Unlock();


			// If everything hashed return
			if(HashFile.m_hFile == CFile::hFileNull)
				pHash->m_EverythingHashed = true;
		}


		while(HashFile.m_hFile != CFile::hFileNull)
		{
			if(pHash->m_StopThread)
			{
				tt2_init(&Tiger_Context);
				return 0;
			}
			
			// Read data
			try
			{
				ReadSize = HashFile.Read((void*) Buffer, BUFF_SIZE);	
			}
			catch(...)
			{
				HashFile.Abort();
				pHash->SetFileHash(FileIndex, HASH_SHA1, ""); // Error
				continue;
			}

			// Hash data
			if(ReadSize > 0)
			{
				sha_update(&Sha1_Context, (byte*) Buffer,  ReadSize);
				MD5Update(&MD5_Context,   (byte*) Buffer,  ReadSize);
				ED2KUpdate(&MD4_Context,   (byte*) Buffer,  ReadSize);
				tt2_update(&Tiger_Context, (byte*) Buffer,  ReadSize);
			}

			// Check if finished
			if(ReadSize != BUFF_SIZE)
			{
				int FileSize = HashFile.GetLength();

				HashFile.Abort();

				sha_final(&Sha1_Context);
				sha_digest(&Sha1_Context,	Sha1_Digest);
				MD5Final(&MD5_Context,		MD5_Digest);
				ED2KFinal(&MD4_Context,		MD4_Digest);
				tt2_digest(&Tiger_Context,	Tiger_Digest);

				
				CString Sha1_String     = EncodeBase32(Sha1_Digest,  20);
				CString MD5_String      = EncodeBase16(MD5_Digest,   16);
				CString MD4_String      = EncodeBase16(MD4_Digest,   16);
				CString Tiger_String    = EncodeBase32(Tiger_Digest, 24);
				CString Bitprint_String = Sha1_String + "." + Tiger_String;


				// Add hashes to hash lookup list
				pHash->m_HashAccess.Lock();
					for(int i = 0; i < pHash->m_HashedFiles.size(); i++)
						if(pHash->m_HashedFiles[i].Index == FileIndex)
						{
							pHash->m_HashedFiles[i].Size = FileSize;

							pHash->m_HashedFiles[i].HashValues[HASH_SHA1]      = Sha1_String;
							pHash->m_HashedFiles[i].HashValues[HASH_MD5]       = MD5_String;
							pHash->m_HashedFiles[i].HashValues[HASH_MD4_ED2K]  = MD4_String;
							pHash->m_HashedFiles[i].HashValues[HASH_TIGERTREE] = Tiger_String;
							pHash->m_HashedFiles[i].HashValues[HASH_BITPRINT]  = Bitprint_String;

							if(pHash->m_HashedFiles[i].TigerTree)
								delete [] pHash->m_HashedFiles[i].TigerTree;

							pHash->m_HashedFiles[i].TigerTree = new byte[Tiger_Context.treeSize];				
							pHash->m_HashedFiles[i].TreeSize  = Tiger_Context.treeSize;
							pHash->m_HashedFiles[i].TreeDepth = Tiger_Context.treeDepth;
							memcpy(pHash->m_HashedFiles[i].TigerTree, Tiger_Context.tree, Tiger_Context.treeSize);

							break;
						}
				pHash->m_HashAccess.Unlock();
				

				// Add hashes to shared file
				int FileID = 0;
				pShare->m_FilesAccess.Lock();
					for(i = 0; i < pShare->m_SharedFiles.size(); i++)
						if(pShare->m_SharedFiles[i].Index == FileIndex)
						{
							FileID = pShare->m_SharedFiles[i].FileID;
							pShare->m_SharedFiles[i].Size = FileSize;
							
							pShare->m_SharedFiles[i].HashValues[HASH_SHA1]      = Sha1_String;
							pShare->m_SharedFiles[i].HashValues[HASH_MD5]       = MD5_String;
							pShare->m_SharedFiles[i].HashValues[HASH_MD4_ED2K]  = MD4_String;
							pShare->m_SharedFiles[i].HashValues[HASH_TIGERTREE] = Tiger_String;
							pShare->m_SharedFiles[i].HashValues[HASH_BITPRINT]  = Bitprint_String;
							
							if(pShare->m_SharedFiles[i].TigerTree)
								delete [] pShare->m_SharedFiles[i].TigerTree;

							pShare->m_SharedFiles[i].TigerTree = new byte[Tiger_Context.treeSize];						
							pShare->m_SharedFiles[i].TreeSize  = Tiger_Context.treeSize;
							pShare->m_SharedFiles[i].TreeDepth = Tiger_Context.treeDepth;
							memcpy(pShare->m_SharedFiles[i].TigerTree, Tiger_Context.tree, Tiger_Context.treeSize);

							// Insert hash value into QRP table
							pShare->m_pWordTable->InsertString((LPCTSTR) ( "urn:" + HashIDtoTag(HASH_SHA1)      + Sha1_String),     i, false);
							//pShare->m_pWordTable->InsertString((LPCTSTR) ( "urn:" + HashIDtoTag(HASH_MD5)       + MD5_String),      i, false);
							//pShare->m_pWordTable->InsertString((LPCTSTR) ( "urn:" + HashIDtoTag(HASH_MD4_ED2K)  + MD4_String),      i, false);
							//pShare->m_pWordTable->InsertString((LPCTSTR) ( "urn:" + HashIDtoTag(HASH_TIGERTREE) + Tiger_String),    i, false);
							//pShare->m_pWordTable->InsertString((LPCTSTR) ( "urn:" + HashIDtoTag(HASH_BITPRINT)  + Bitprint_String), i, false);

							pShare->m_SharedHashMap[Sha1_String] = i;
							
							pHash->m_HashSetModified = true;

							break;
						}

						
					
					pShare->m_FilesAccess.Unlock();
									
				
				// File hashed send notification
				if(FileID)
				{
					pHash->m_QueueAccess.Lock();
					pHash->m_HashQueue.push_back(FileID);
					pHash->m_QueueAccess.Unlock();
				}
			}


			// Check cpu usage
			QueryPerformanceCounter(&EndTime);
			
			if( EndTime.QuadPart - StartTime.QuadPart > MaxTicks)
				break;
		}
	
		if (pHash->m_StopThread)
			return 0;
	}

	tt2_init(&Tiger_Context);

	HashFile.Abort();

	TRACE0("*** Hash Thread Ended\n");

	return 0;
}


CString CGnuFileHash::GetFileHash(CString FileName)
{
	CFileLock HashFile;

	// Open the File, only called from download so dont ignore ID3
	if( !HashFile.Open(FileName, CFile::modeRead | CFile::shareDenyWrite))
		return "";

	// Initialise ready to start
	sha_ctx	Sha1_Context;
	sha_init(&Sha1_Context);

	uint64 FilePos  = 0;
	uint64 FileSize = HashFile.GetLength();
	byte   Buffer[BUFF_SIZE];

	while(FilePos < FileSize)
	{
		uint32 BytesRead = 0;
		uint64 ReadSize  = FileSize - FilePos;

		if( ReadSize > BUFF_SIZE)
			ReadSize = BUFF_SIZE;

		// Read data
		try
		{
			BytesRead = HashFile.Read(Buffer, ReadSize);
		}
		catch(...)
		{
			return "";
		}

		// Hash data
		sha_update(&Sha1_Context, (byte*) Buffer,  BytesRead);
		FilePos += BytesRead;
	}

	HashFile.Abort();

	sha_final(&Sha1_Context);

	byte Sha1_Digest[20];
	sha_digest(&Sha1_Context, Sha1_Digest);

	return EncodeBase32(Sha1_Digest, 20);
}
 



//
// return a file hash for the file 'Name' with the timestamp
//   first look in the loaded hashesfile
//   if timestamps don't match recalc the hash and save it
//   if no entry found create a new entry
//
void CGnuFileHash::LookupFileHash(CString FilePath, CString TimeStamp, SharedFile &File) 
{
	m_HashAccess.Lock();

	std::map<CString, int>::iterator itPos = m_HashMap.find(FilePath);

	if(itPos != m_HashMap.end())
	{
		int i = itPos->second;

		if (m_HashedFiles[i].TimeStamp == TimeStamp)
		{
			File.Index = m_HashedFiles[i].Index;
			File.Size  = m_HashedFiles[i].Size;
			
			for(int j = 0; j < HASH_TYPES; j++)
				File.HashValues[j]  = m_HashedFiles[i].HashValues[j];

			if(File.TigerTree)
				delete [] File.TigerTree;

			File.TreeSize  = m_HashedFiles[i].TreeSize;
			File.TreeDepth = m_HashedFiles[i].TreeDepth;
			File.TigerTree = new byte[m_HashedFiles[i].TreeSize];
			memcpy(File.TigerTree, m_HashedFiles[i].TigerTree, m_HashedFiles[i].TreeSize);

			for(j = 0; j < m_HashedFiles[i].AltHosts.size(); j++)
				File.AltHosts.push_back( m_HashedFiles[i].AltHosts[j] );
				
			m_HashAccess.Unlock();
			return;
		}
	}
	

	for(int j = 0; j < HASH_TYPES; j++)
		File.HashValues[j]  = "";
	

	HashedFile hf;
	hf.FilePath	 = FilePath;
	hf.TimeStamp = TimeStamp;
	hf.Index	 = m_NextIndex++;
	
	File.Index = hf.Index;
	
	m_HashMap[hf.FilePath] = m_HashedFiles.size();
	m_HashedFiles.push_back(hf);

	m_HashAccess.Unlock();


	// Actual hashing of a file is done in seperate thread
}

void CGnuFileHash::SetFileHash(int FileIndex, int HashID, CString Hash)
{	
	m_HashAccess.Lock();

	// Find files to hashed
	for (int i = 0; i < m_HashedFiles.size(); i++)
		if(m_HashedFiles[i].Index == FileIndex)
		{
			m_HashedFiles[i].HashValues[HashID] = Hash;

			break;
		}

	m_HashAccess.Unlock();
	
	UINT FileID = 0;
	
	m_pShare->m_FilesAccess.Lock();
	
	for ( i = 0; i < m_pShare->m_SharedFiles.size(); i++)
		if(m_pShare->m_SharedFiles[i].Index == FileIndex)
		{
			m_pShare->m_SharedFiles[i].HashValues[HashID] = Hash;

			// Insert hash value into QRP table
			if(Hash.IsEmpty())
				m_pShare->m_SharedFiles[i].HashError = true;
			else
				m_pShare->m_pWordTable->InsertString((LPCTSTR) ( "urn:" + HashIDtoTag(HashID) + Hash), i, false);

			FileID = m_pShare->m_SharedFiles[i].FileID;

			break;
		}

	m_pShare->m_FilesAccess.Unlock();
			
	

	// Notify of change
	if(FileID)
	{
		m_QueueAccess.Lock();
		m_HashQueue.push_back(FileID);
		m_QueueAccess.Unlock();
	}
}

//
// return a file index for a matching Hash
//
int CGnuFileHash::GetHashIndex(int HashID, CString Hash)
{
	m_pShare->m_FilesAccess.Lock();
	
	for (int i = 0; i < m_pShare->m_SharedFiles.size(); i++)
		if(Hash.CompareNoCase(m_pShare->m_SharedFiles[i].HashValues[HashID].c_str()) == 0)
		{
			m_pShare->m_FilesAccess.Unlock();
			return m_pShare->m_SharedFiles[i].Index;
		}

	m_pShare->m_FilesAccess.Unlock();


	return 0;
}

// 
// Load saved file hashes so we don't have to keep recalculating them
//
void CGnuFileHash::LoadShareHashes(CString HashFileName)
{
	CStdioFile HashFile;

	if (HashFile.Open(HashFileName, CFile::modeRead | CFile::shareExclusive) == false)
		return;

	// Check version of hash file
	CString Version; 
	HashFile.ReadString(Version);

	int ColonPos = Version.Find(":");
	if(ColonPos == -1)
		return;
	
	CString Ident = Version.Left(ColonPos);
	CString Value = Version.Mid(ColonPos + 1);

	if(Ident != "Version")
		return;

	if( atoi(Value) < HASH_FILE_VERSION)
		return;


	// Load file hashes
	m_HashAccess.Lock();
	HashedFile hf;
		
	for(;;)
	{
		CString ReadString;

		if(HashFile.ReadString(ReadString) == false)
			break;

		if(ReadString == "End")
		{
			// Push back hash stuct
			if(hf.Index)
			{
				m_HashMap[hf.FilePath] = m_HashedFiles.size();
				m_HashedFiles.push_back(hf);
			}

			// Reset
			hf.FilePath  = "";
			hf.Index     = 0;
			hf.TimeStamp = "";
			for(int i = 0; i < HASH_TYPES; i++)
				hf.HashValues[i] = "";
			hf.AltHosts.clear();
		}

		ColonPos = ReadString.Find(":");

		if(ColonPos != -1)
		{
			Ident = ReadString.Left(ColonPos);
			Value = ReadString.Mid(ColonPos + 1);

			if(Ident == "Name")
				hf.FilePath = Value;

			if(Ident == "Index")
			{
				hf.Index = atoi(Value);

				if(hf.Index >= m_NextIndex)
					m_NextIndex = hf.Index + 1; 
			}

			if(Ident == "Size")
				hf.Size = atoi(Value);

			if(Ident == "Time")
				hf.TimeStamp = Value;

			if(Ident == "urn")
			{
				int BackColon = ReadString.ReverseFind(':');

				if(BackColon != -1)
				{
					int HashID = TagtoHashID( ReadString.Mid(ColonPos + 1, BackColon - ColonPos));

					if(HashID != HASH_UNKNOWN)
						hf.HashValues[HashID] = ReadString.Mid(BackColon + 1);
				}
			}

			if(Ident == "TreeSize")
			{
				hf.TreeSize = atoi(Value);

				if(hf.TreeSize % 24 != 0)
				{
					hf.TreeSize = 0;
					continue;
				}

				hf.TigerTree = new byte[hf.TreeSize];
				memset(hf.TigerTree, 0, hf.TreeSize);
			}
			
			if(Ident == "TreeDepth")
				hf.TreeDepth = atoi(Value);

			if(Ident == "TigerTree" && hf.TigerTree)
			{
				int buffPos = 0;
				int dotPos  = Value.Find(".");

				while(dotPos != -1 && buffPos < hf.TreeSize)
				{
					DecodeBase32( Value.Mid(dotPos - 39, 39), 39, hf.TigerTree + buffPos, hf.TreeSize-buffPos);

					buffPos += 24;
					dotPos = Value.Find(".", dotPos + 1);
				}
			}

			if(Ident == "Alt-Loc")
			{
				CString AltLocs = ReadString.Mid(ColonPos + 1);

				while( !AltLocs.IsEmpty() )
					hf.AltHosts.push_back( StrtoIPv4(ParseString(AltLocs, ',')) );
			}
		}
			
	}

	m_HashAccess.Unlock();
	
	HashFile.Close();
}

//
// Save the Shared files hashes
//
void CGnuFileHash::SaveShareHashes(CString HashFileName)
{
	CStdioFile HashFile;
	CString Buffer;

	if (HashFile.Open(HashFileName, CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive) == false)
		return;

	HashFile.WriteString("Version:" + NumtoStr(HASH_FILE_VERSION) + "\n");

	m_pShare->m_FilesAccess.Lock();
	
	for (int i = 0; i < m_pShare->m_SharedFiles.size(); i++)
	{	
		HashFile.WriteString("Name:" + CString(m_pShare->m_SharedFiles[i].Dir.c_str()) + "\n");
		HashFile.WriteString("Index:" + NumtoStr(m_pShare->m_SharedFiles[i].Index) + "\n");
		HashFile.WriteString("Size:" + NumtoStr(m_pShare->m_SharedFiles[i].Size) + "\n");
		HashFile.WriteString("Time:" + CString(m_pShare->m_SharedFiles[i].TimeStamp.c_str()) + "\n");
		
		for(int j = 0; j < HASH_TYPES; j++)
			HashFile.WriteString("urn:" + HashIDtoTag(j) + CString(m_pShare->m_SharedFiles[i].HashValues[j].c_str()) + "\n");

		if(m_pShare->m_SharedFiles[i].TreeSize % 24 != 0)
			ASSERT(0);

		HashFile.WriteString("TreeSize:"  + NumtoStr(m_pShare->m_SharedFiles[i].TreeSize)  + "\n");
		HashFile.WriteString("TreeDepth:" + NumtoStr(m_pShare->m_SharedFiles[i].TreeDepth) + "\n");
		HashFile.WriteString("TigerTree:");
		for(j = 0; j < m_pShare->m_SharedFiles[i].TreeSize; j += 24)
			HashFile.WriteString(EncodeBase32(m_pShare->m_SharedFiles[i].TigerTree + j, 24) + ".");
		HashFile.WriteString("\n");

		CString AltLocs;
		for(j = 0; j < m_pShare->m_SharedFiles[i].AltHosts.size(); j++)
			AltLocs += IPv4toStr(m_pShare->m_SharedFiles[i].AltHosts[j]) + ", ";
		
		AltLocs.Trim(", ");
		if( !AltLocs.IsEmpty() )
			HashFile.WriteString("Alt-Loc:" + AltLocs + "\n");

		HashFile.WriteString("End\n");
	}
	m_pShare->m_FilesAccess.Unlock();

	HashFile.Close();
}

void CGnuFileHash::Timer()
{
	if( m_EverythingHashed )
	{
		if( m_HashSetModified )
		{
			// Signal that connected nodes to update hit tables
			m_pShare->HitTableRefresh();

			m_HashSetModified = false;
		}
	}

	else if( !m_StopHashing )
		m_HashEvent.SetEvent();
	


	if(m_HashQueue.size())
	{
		m_QueueAccess.Lock();
		m_pShare->ShareUpdate(m_HashQueue.back());
		m_HashQueue.pop_back();
		m_QueueAccess.Unlock();

		m_SaveHashFile = true;
	}


	if(m_SaveHashFile)
	{
		if(m_SaveInterval > HASH_SAVE_INTERVAL)
			m_HashEvent.SetEvent();
		else
			m_SaveInterval++;
	}
}

