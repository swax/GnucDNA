#pragma once


class CGnuCore;
class CGnuPrefs;
class CGnuTransfers;
class CGnuUpdateSock;

struct UpdateServer
{
	CString Host;
	IP		HostIP;
	int		Port;
	CString Path;
};

struct UpdateFile
{
	int FileID;
	int DownloadID;

	CString Name;
	CString Source;
	int		Size;
	CString Hash;

	bool Completed;
	CGnuUpdateSock* Socket;
};


class CGnuUpdate
{
public:
	CGnuUpdate(CGnuCore* pCore);
	~CGnuUpdate(void);
	void endThreads();

	void AddServer(CString Server);
	void Check();
	void ParseUpdateFile(CString FilePath);
	void ScanComponent(CString Component, CString LocalVersion, CString File);
	void AddFile(CString FileTag);
	void StartDownload();
	bool DownloadProgress();
	bool DownloadComplete();
	CGnuUpdateSock* ServerDownload(UpdateFile &File);
	void CancelUpdate();
	void LaunchUpdate();

	int  TotalUpdateSize();
	int  TotalUpdateCompleted();
	int  GetFileCompleted(UpdateFile &File);

	void Timer();
	
	bool m_CheckPending;
	bool m_DownloadMode;
	int  m_TryingNetwork;

	CCriticalSection m_ServerLock;
	std::vector<UpdateServer> m_ServerList;

	int m_NextFileID;
	std::vector<UpdateFile> m_FileList;

	CWinThread* m_pResolveThread;
	
	CString m_NewVers;

	CGnuUpdateSock* m_Socket;

	CGnuCore*      m_pCore;
	CGnuPrefs*     m_pPrefs;
	CGnuTransfers* m_pTrans;
};
