#pragma once


class CGnuUpdate;
class CGnuPrefs;

class CGnuUpdateSock : public CAsyncSocket
{
public:
	CGnuUpdateSock(CGnuUpdate* pUpdate);
	virtual ~CGnuUpdateSock();

	void ReadyFile();
	void Download(byte* pBuff, int nSize);

	CString m_Host;
	int		m_Port;
	CString m_Path;

	CString m_Header;
	int     m_Status;
	CString m_Error;

	CFile   m_File;
	int     m_BytesCompleted;
	int     m_FileSize;
	CString m_DownloadPath;

	CGnuUpdate* m_pUpdate;
	CGnuPrefs*  m_pPrefs;


	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual void Close();
};


