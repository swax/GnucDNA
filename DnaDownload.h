#pragma once


class CDnaCore;
class CGnuTransfers;

class GNUC_API CDnaDownload 
{	
public:
	CDnaDownload();
	 ~CDnaDownload();

	void InitClass(CDnaCore* dnaCore);

	CDnaCore*		m_dnaCore;
	CGnuTransfers*  m_gnuTrans;
	
	std::vector<int> GetDownloadIDs();
	LONG GetStatus(LONG DownloadID);
	BOOL IsCompleted(LONG DownloadID);
	CString GetName(LONG DownloadID);
	ULONGLONG GetBytesCompleted(LONG DownloadID);
	ULONGLONG GetFileLength(LONG DownloadID);
	LONG GetSourceCount(LONG DownloadID);
	BOOL IsSearching(LONG DownloadID);
	BOOL IsRetrying(LONG DownloadID);
	LONG GetCoolingCount(LONG DownloadID);
	LONG GetActiveSourceCount(LONG DownloadID);
	CString GetReasonClosedStr(LONG DownloadID);
	LONG GetBytesPerSec(LONG DownloadID);
	LONG GetSecETA(LONG DownloadID);
	LONG GetSourcePos(LONG DownloadID);
	void RemoveCompleted();
	void ForceStart(LONG DownloadID);
	void Stop(LONG DownloadID);
	void Remove(LONG DownloadID);
	void RunFile(LONG DownloadID);
	void ReSearch(LONG DownloadID);
	CString GetHash(LONG DownloadID, LONG HashID);
	std::vector<int> GetSourceIDs(LONG DownloadID);
	ULONG GetSourceIP(LONG DownloadID, LONG SourceID);
	LONG GetSourcePort(LONG DownloadID, LONG SourceID);
	CString GetSourceName(LONG DownloadID, LONG SourceID);
	LONG GetSourceSpeed(LONG DownloadID, LONG SourceID);
	CString GetSourceStatusStr(LONG DownloadID, LONG SourceID);
	CString GetSourceVendor(LONG DownloadID, LONG SourceID);
	CString GetSourceHandshake(LONG DownloadID, LONG SourceID);
	std::vector<int> GetChunkIDs(LONG DownloadID);
	ULONGLONG GetChunkStart(LONG DownloadID, LONG ChunkID);
	LONG GetChunkCompleted(LONG DownloadID, LONG ChunkID);
	LONG GetChunkSize(LONG DownloadID, LONG ChunkID);
	LONG GetChunkFamily(LONG DownloadID, LONG ChunkID);
	LONG GetSourceBytesPerSec(LONG DownloadID, LONG SourceID);
	LONG DownloadFile(LPCTSTR Name, ULONGLONG Size, LONG HashID, LPCTSTR Hash);
	CString GetFilePath(LONG DownloadID);
	void AddSource(LONG DownloadID, LONG NetworkID, LPCTSTR URL);
	void Proxy(LONG DownloadID, BOOL Enabled, LPCTSTR Default);
	LONG GetMetaID(LONG DownloadID);
	CString GetAttributeValue(LONG DownloadID, LONG AttributeID);
	void SetAttributeValue(LONG DownloadID, LONG AttributeID, LPCTSTR Value);
	void SetMetaID(LONG DownloadID, LONG MetaID);
	CString GetReasonClosed(LONG DownloadID);
	void AnswerChallenge(LONG DownloadID, LONG SourceID, LPCTSTR Answer);
	void OverrideDownloadPath(LONG DownloadID, LPCTSTR Path);
};


