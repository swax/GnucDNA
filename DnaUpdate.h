#pragma once

 

class CDnaCore;
class CGnuUpdate;

class GNUC_API CDnaUpdate  
{
public:
	CDnaUpdate();
	 ~CDnaUpdate();

	void InitClass(CDnaCore* dnaCore);

	CDnaCore*	m_dnaCore;
	CGnuUpdate* m_gnuUpdate;

	void AddServer(LPCTSTR Server);
	void Check(void);
	void StartDownload(void);
	void CancelUpdate(void);
	std::vector<int> GetFileIDs(void);
	LONG GetTotalCompleted(void);
	LONG GetTotalSize(void);
	CString GetFileName(LONG FileID);
	LONG GetFileSize(LONG FileID);
	LONG GetFileCompleted(LONG FileID);
	void LaunchUpdate(void);
};


