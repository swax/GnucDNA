#pragma once

#define SEND_BUFF	32768

class CGnuTransfers;
class CGnuUploadShell;
class CGnuNetworks;
class CGnuShare;
class CGnuPrefs;


class CGnuUpload : public CSocketEvents
{
public:
	CGnuUpload(CGnuUploadShell*);
	virtual ~CGnuUpload();

	void Send_HttpOK();
	void Send_TigerTree();
	void Send_HttpBusy();
	void Send_HttpFailed();
	void Send_HttpNotFound();
	void Send_BrowserBlock();
	void Send_HttpInternalError();
	void Send_HttpBadRequest();
	void Send_ClientBlock(CString ClientName);
	void Send_HttpRangeNotAvailable();

	void StartUpload();

	void Timer();

	CString m_GetRequest;
	CString m_ListenIP;
	
	bool m_Push;
	
	bool m_ThreadRunning;
	CWinThread*  m_pUploadThread;
	
	CEvent		 m_CanWrite;
	CEvent		 m_MoreBytes;

	CCriticalSection m_SendSection;

	bool m_Authorized;

	CGnuUploadShell* m_pShell;
	CGnuTransfers*   m_pTrans;
	CGnuNetworks*    m_pNet;
	CGnuShare*       m_pShare;
	CGnuPrefs*       m_pPrefs;

	// Socket
	CReliableSocket* m_pSocket;

	void OnClose(int nErrorCode);
	void OnConnect(int nErrorCode);
	void OnReceive(int nErrorCode);
	void OnSend(int nErrorCode);
	 
	int  Send(const void* lpBuf, int nBufLen, int nFlags = 0);
	void Close();
};


