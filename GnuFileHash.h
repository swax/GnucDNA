#pragma once

#include "GnuShare.h"

class CGnuShare;
class CGnuCore;

#define HASH_FILE_VERSION 2
#define HASH_SAVE_INTERVAL (1 * 60)

struct HashedFile;

class CGnuFileHash  
{
public:
	CGnuFileHash(CGnuShare* pShare);
	virtual ~CGnuFileHash();

	void LoadShareHashes(CString);
	void SaveShareHashes(CString);

	void    SetFileHash(int FileIndex, int HashID, CString Hash);
	CString GetFileHash(CString FileName);
	void    LookupFileHash(CString, CString, SharedFile &);
	int     GetHashIndex(int HashID, CString);

	void Timer();

	void endThreads();

	CCriticalSection m_HashAccess;

	int m_NextIndex;

	std::map<CString, int>  m_HashMap;
	std::vector<HashedFile> m_HashedFiles;		// Stored Hashes of Files

	CEvent		  m_HashEvent;
	CWinThread*   m_pHashThread;
	bool		  m_StopThread;
	bool		  m_StopHashing;

	bool		  m_EverythingHashed;
	bool		  m_HashSetModified;

	bool		 m_SaveHashFile;
	int			 m_SaveInterval;

	double	m_CpuUsage;

	// Messaging
	CCriticalSection m_QueueAccess;
	std::vector<UINT> m_HashQueue;


	CGnuShare*    m_pShare;
	CGnuCore* m_pCore;
};

struct HashedFile
{
	CString FilePath;
	int     Index;
	int		Size;
	CString TimeStamp;
	std::vector<IPv4> AltHosts;
	
	CString HashValues[HASH_TYPES];
	byte* TigerTree;
	int   TreeSize;
	int   TreeDepth;


	HashedFile()
	{
		Index     = 0;
		Size	  = 0;
		TigerTree = 0;
		TreeSize  = 0;
		TreeDepth = 0;
	};

};

