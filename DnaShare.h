#pragma once

class CDnaCore;
class CGnuShare;
class CGnuMeta;

class GNUC_API CDnaShare  
{
public:
	CDnaShare();
	 ~CDnaShare();

    void InitClass(CDnaCore* dnaCore);

	CDnaCore*    m_dnaCore;
	CGnuShare*   m_gnuShare;
	CGnuMeta*    m_gnuMeta;

	std::vector<int> GetFileIDs(void);
	LONG GetFileIndex(LONG FileID);
	CString GetFileDir(LONG FileID);
	CString GetFileName(LONG FileID);
	ULONGLONG GetFileSize(LONG FileID);
	LONG GetFileMatches(LONG FileID);
	LONG GetFileUploads(LONG FileID);
	void StartHashing(void);
	void StopHashing(void);
	BOOL IsEverythingHashed(void);
	BOOL IsHashingStopped(void);
	CString GetFileHash(LONG FileID, LONG HashID);
	void StopSharingFile(LONG FileID);
	std::vector<CString> GetFileKeywords(LONG FileID);
	std::vector<CString> GetFileAltLocs(LONG FileID);
	std::vector<int> GetSharedDirIDs(void);
	CString GetDirName(LONG DirID);
	BOOL GetDirRecursive(LONG DirID);
	LONG GetDirFileCount(LONG DirID);
	void SetSharedDirs(std::vector<CString> &DirPaths);
	BOOL IsLoading(void);
	LONG GetFileCount(void);
	LONG GetFileMetaID(LONG FileID);
	CString GetFileAttributeValue(LONG FileID, LONG AttributeID);
	void SetFileAttributeValue(LONG FileID, LONG AttributeID, LPCTSTR Value);
	ULONGLONG GetTotalFileSize(void);
	LONG GetHashSpeed(void);
	void SetHashSpeed(LONG newVal);
	void SetFileMetaID(LONG FileID, LONG MetaID);
};


