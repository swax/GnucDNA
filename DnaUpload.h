
class CDnaCore;
class CGnuTransfers;

class GNUC_API CDnaUpload  
{
public:
	CDnaUpload();
	 ~CDnaUpload();

	void InitClass(CDnaCore* dnaCore);

	CDnaCore*		m_dnaCore;
	CGnuTransfers*  m_gnuTrans;

	LONG GetStatus(LONG UploadID);
	CString GetName(LONG UploadID);
	DATE GetChangeTime(LONG UploadID);
	ULONGLONG GetBytesCompleted(LONG UploadID);
	ULONGLONG GetFileLength(LONG UploadID);
	LONG GetBytesPerSec(LONG UploadID);
	LONG GetSecETD(LONG UploadID);
	std::vector<int> GetUploadIDs(void);
	void RunFile(LONG UploadID);
	void Remove(LONG UploadID);
	CString GetErrorStr(LONG UploadID);
	LONG GetIndex(LONG UploadID);
	ULONG GetIP(LONG UploadID);
	LONG GetPort(LONG UploadID);
	CString GetHandshake(LONG UploadID);
	LONG GetAttempts(LONG UploadID);
	LONG GetQueuePos(LONG UploadID);
	CString GetFilePath(LONG UploadID);
	void SendChallenge(LONG UploadID, LPCTSTR Challenge, LPCTSTR Answer );
	CString GetFileHash(LONG UploadID, LONG HashID);
};


