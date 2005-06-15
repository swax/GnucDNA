#pragma once

#define TRANSFER_TIMEOUT  6
#define RECEIVE_BUFF	32768


class CGnuNetworks;
class CGnuNode;
class CGnuDownloadShell;
class CGnuPrefs;
class CGnuTransfers;
class CReliableSocket;

class CGnuDownload : public CSocketEvents
{
public:
	CGnuDownload(CGnuDownloadShell*, int);
	virtual ~CGnuDownload();

	bool StartDownload();
	void StopDownload();

	bool GetStartPos();
	bool GetStartPosPartial();
	bool ByteIsInRanges(uint64 StartByte);
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

	uint64 m_StartPos;
	uint64 m_PausePos;

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
	
	// Features
	bool m_SendAltF2Fs;

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
	bool	  m_PushF2F;
	ProxyAddr m_LocalProxy;
	
	CString   m_RemoteChallenge;
	CString   m_RemoteChallengeAnswer;

	// Bandwidth
	CMovingAvg m_AvgRecvBytes;

	// Proxy
	bool m_PushProxy;
	IPv4 m_ProxyAddress;

	// Socket
	CReliableSocket* m_pSocket;

	void OnAccept(int nErrorCode);
	void OnConnect(int nErrorCode);
	void OnReceive(int nErrorCode);
	void OnClose(int nErrorCode);
	
	void Close();	

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
