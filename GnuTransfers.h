#pragma once

#include "GnuUploadQueue.h"



class CGnuCore;
class CGnuPrefs;
class CGnuNetworks;
class CGnuDownloadShell;
class CGnuUploadShell;

struct GnuPush;

class CGnuTransfers
{
public:
	CGnuTransfers();
	~CGnuTransfers(void);

	void InitTransfers(CGnuCore* pCore);
	
	void LoadDownloads();
	CGnuDownloadShell* LoadDownloadHosts(CString FilePath);
	void LoadDownloadParts(CString FilePath, CGnuDownloadShell* pDownload);

	CString GetBackupString(CString Property, int &StartPos, CString &Backup);
	
	void Timer();
	void ManageDownloads();
	void ManageUploads();

	int  CountUploading();
	int  CountDownloading();

	void RemoveDownload(CGnuDownloadShell*);
	void RemoveUpload(CGnuUploadShell*);

	void RemoveCompletedDownloads();

	void DownloadUpdate(int DownloadID);
	void UploadUpdate(int UploadID);

	void DoPush(GnuPush &Push);

	void TransferLoadMeta();

	double m_DownloadSecBytes;
	double m_UploadSecBytes;


	CCriticalSection m_DownloadAccess;
	std::vector<CGnuDownloadShell*>	      m_DownloadList;
	std::map<int, CGnuDownloadShell*>     m_DownloadMap;
	std::map<CString, CGnuDownloadShell*> m_DownloadHashMap;  // Key Hash, Value DownloadID
	int m_NextDownloadID;


	CCriticalSection m_UploadAccess;
	std::vector<CGnuUploadShell*>	m_UploadList;
	std::map<int, CGnuUploadShell*> m_UploadMap;
	int m_NextUploadID;


	//Upload queuing
	CUploadQueue  m_UploadQueue;	// Queue of pending uploads


	CGnuCore*     m_pCore;
	CGnuPrefs*    m_pPrefs;
	CGnuNetworks* m_pNet;

};

struct GnuPush
{
	int	 Network;
	IPv4 Address;
	int	 FileID;

	GnuPush()
	{
		Network = 0;
		FileID  = 0;
	};
};