#pragma once


class CGnuTransfers;
class CGnuUpload;
class CGnuNetworks;
class CGnuShare;
class CGnuPreferences;


class CGnuUploadShell  
{
public:
	CGnuUploadShell(CGnuTransfers*);
	virtual ~CGnuUploadShell();

	void Timer();
	void StatusUpdate(DWORD);

	void PushFile();
	void ParseRequest(CString);

	int  GetStatus();
	UINT GetFileLength();
	bool ParseRangeHeader(CString);
	bool LoadTigerTree();

	int GetBytesPerSec();
	int GetETD();

	CString GetFilePath();
	void RunFile();

	int m_UploadID;

	IP   m_Host;
	UINT m_Port;
	int  m_Attempts;

	CString   m_Name;
	int       m_Index;
	int		  m_Network;
	CFileLock m_File;
	//CFile   m_CheckFile;
	//CFile	  m_MirrorFile;
	CString   m_Sha1Hash;
	
	CString m_TigerHash;
	bool    m_TigerTreeRequest;
	bool    m_TigerThexRequest;
	byte*   m_TigerTree;
	int     m_TreeSize;
	
	CString m_RequestURI;
	CString m_HTTPMethod;
	CString m_HTTPVersion;
	CString m_RemoteClient;

	bool    m_IsPartial;
	int     m_PartialID;

	CTime   m_ChangeTime;
	DWORD   m_Status;
	CString m_Error;
	bool	m_UpdatedInSecond;
	
	bool	m_KeepAlive;
	bool	m_RequsetPending;

	int		m_StartPos;
	int		m_CurrentPos;
	int		m_StopPos;
	int     m_FileLength;
	int		m_BytesSent;

	CString m_Challenge;
	CString m_ChallengeAnswer;
	// Queue values
	bool m_QueueRequest;
	int  m_QueuePos;

	CString m_Handshake;

	CGnuUpload* m_Socket;

	CGnuTransfers* m_pTrans;
	CGnuNetworks*  m_pNet;
	CGnuShare*     m_pShare;
	CGnuPrefs*	   m_pPrefs;

	// Bandwidth Limits
	int    m_AllocBytesTotal;   // Total assigned bytes in second
	int    m_AllocBytes;		// Bytes left to use in a second
	
	// Bandwidth
	CRangeAvg m_AvgSentBytes;
	DWORD     m_dwSecBytes;     // Bytes sent in second

	int    m_nSecsUnderLimit;
	int	   m_nSecsDead;
};


bool operator > (const CGnuUploadShell &Transfer1, const CGnuUploadShell &Transfer2);

