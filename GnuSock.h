#pragma once


class CGnuCore;
class CGnuNetworks;
class CGnuTransfers;
class CGnuShare;
class CGnuPrefs;


class CGnuSock : public CAsyncSocket
{
public:
	CGnuSock(CGnuNetworks*);
	virtual ~CGnuSock();

	void Timer();

	CString m_RemoteHost;
	UINT	m_RemotePort;

	int   m_SecsAlive;
	bool  m_bDestroy;

	//{{AFX_VIRTUAL(CGnuSock)
	virtual void OnReceive(int nErrorCode);
	virtual int Send(const void* lpBuf, int nBufLen, int nFlags = 0);
	virtual void OnSend(int nErrorCode);
	virtual void Close();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CGnuSock)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

protected:
	void ParseConnectRequest();
	void ParseBrowseRequest();
	void ParseUploadRequest();
	void ParseDownloadRequest(int Network);
	void ParsePushProxyRequest();

	void SendGetGnucleus();

	CGnuCore*	   m_pCore;
	CGnuNetworks*  m_pNet;
	CGnuTransfers* m_pTrans;
	CGnuPrefs*     m_pPrefs;
	CGnuShare*	   m_pShare;

	CString m_Handshake;

	byte m_pBuff[4096];
	int  m_BuffLength;
};
