#pragma once


class CGnuControl;
class CGnuNetworks;
class CGnuCore;
class CGnuPrefs;

struct LanNode;

class CGnuLocal : public CAsyncSocket
{
public:
	CGnuLocal(CGnuControl*);
	virtual ~CGnuLocal();

	void Init();

	void LanModeOn();
	void LanModeOff();
	void JoinLan(CString LanName);

	void SendPing();


	std::map<int, LanNode> m_LanNodeIDMap;

	int	    m_NextLanID;


	//{{AFX_VIRTUAL(CGnuLocal)
	public:
	virtual void OnReceive(int nErrorCode);
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CGnuLocal)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG


protected:
	CAsyncSocket m_OutSock;

	int  m_Broadcasted;

	CGnuControl*  m_pComm;
	CGnuNetworks* m_pNet;
	CGnuCore*	  m_pCore;
	CGnuPrefs*    m_pPrefs;

};

struct LanNode /*  : public Node */
{
	CString Host;
	UINT    Port;
	UINT	Leaves;

	CString Name;
	CString IRCserver;
	//CString UpdateServer;
	CString InfoPage;

};
