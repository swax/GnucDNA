#pragma once

#define TRANSFER_TIMEOUT  6
#define RECEIVE_BUFF	32768


class CGnuNetworks;
class CGnuNode;
class CGnuDownloadShell;
class CGnuPrefs;
class CGnuTransfers;

class CGnuDownload : public CAsyncSocketEx
{
public:
	CGnuDownload(CGnuDownloadShell*, int);
	virtual ~CGnuDownload();

	bool StartDownload();
	void StopDownload();

	bool GetStartPos();
	bool GetStartPosPartial();
	bool ByteIsInRanges(int StartByte);
	bool LoadTigerTree();

	
	void SendRequest();
	void SendTigerRequest();
	void SendPushRequest();
	
	void DoPushProxy();
	void SendPushProxyRequest();
	
	void DownloadBytes(byte*, int);

	void StatusUpdate(DWORD);

	void SetError(CString);
	void Timer();

	void AddHosttoMesh(IPv4 Address);

	FileSource* HostInfo();


	// File info
	int m_HostID;
	
	bool m_PartActive;
	int  m_PartNumber;

	int m_StartPos;
	int m_PausePos;

	bool m_DoHead;
	bool m_HeadNotSupported;

	bool m_KeepAlive;
	
	bool m_TigerRequest;
	int  m_TigerLength;
	int  m_TigerPos;
	byte* m_TigerReqBuffer;
	byte* m_tempTigerTree;
	int	  m_tempTreeSize;
	int	  m_tempTreeRes;
	

	// Queue info
	int m_QueuePos;
	int m_QueueLength;
	int m_QueueLimit;
	int m_RetryMin;
	int m_RetryMax;

	// Download Properties
	IPv4      m_ConnectAddress;

	CString   m_Request;
	CString   m_Header;
	CString   m_ServerName;
	int       m_Status;
	bool      m_Push;
	ProxyAddr m_LocalProxy;
	
	CString   m_RemoteChallenge;
	CString   m_RemoteChallengeAnswer;

	// Bandwidth
	CRangeAvg m_AvgRecvBytes;
	DWORD     m_dwSecBytes;      // Bytes sent in second

	// Proxy
	bool m_PushProxy;
	IPv4 m_ProxyAddress;

	//{{AFX_VIRTUAL(CGnuDownload)
	public:
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual int Send(const void* lpBuf, int nBufLen, int nFlags = 0);
	virtual void OnSend(int nErrorCode);
	virtual void Close();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CGnuDownload)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG


public:

	byte m_pBuff[RECEIVE_BUFF];
	
	
	CGnuDownloadShell* m_pShell;
	CGnuPrefs*         m_pPrefs;
	CGnuNetworks*      m_pNet;
	CGnuTransfers*	   m_pTrans;

	int    m_nSecsUnderLimit;
	int	   m_nSecsDead;

	FileSource dumbResult;
};
