#pragma once

class CDnaCore;

class CGnuPrefs;
class CGnuShare;
class CGnuMeta;
class CGnuNetworks;
class CGnuTransfers;
class CGnuUpdate;
class CGnuChat;

class CGnuCore
{
public:
	CGnuCore(CDnaCore* dnaCore);
	~CGnuCore();

	void SetLicense();
	void SetCredits();

	void Connect(int NetworkID);
	void Disconnect(int NetworkID);

	void SecondTimer();
	void HourlyTimer();

	CString GetUserAgent();
	void ScanPerformance();
	int  _GetCPUSpeed();

	// Client properties
	CString m_ClientName;
	CString m_ClientVersion;
	CString m_ClientCode;
	
	CString  m_RunPath;
	
	bool    m_IsKernalNT;
	int     m_SysMemory;
	int     m_SysSpeed;
	
	
	CString				 m_DnaVersion;
	std::vector<CString> m_Credits;
	CString              m_License;
	CTime				 m_StartTime;

	CDnaCore*  m_dnaCore;	// Parent object

	CGnuPrefs*     m_pPrefs;  // Preferences core
	CGnuShare*     m_pShare;
	CGnuMeta*	   m_pMeta;
	CGnuTransfers* m_pTrans;
	CGnuNetworks*  m_pNet;
	CGnuUpdate*    m_pUpdate;
	CGnuChat*	   m_pChat;


	// Debugging
	void DebugLog(CString Section, CString Entry);
	void DebugTrigger(bool details);
	void LogError(CString);	

	CString m_DebugFilename;

	#ifdef _DEBUG
		CMemoryState m_OldMemState;
		CMemoryState m_NewMemState;
		CMemoryState m_DiffMemState;
	#endif

};


