
#pragma once


class CGnuCore;
class CDnaPrefs;
class CDnaNetwork;
class CDnaCache;
class CDnaShare;
class CDnaSearch;
class CDnaDownload;
class CDnaUpload;
class CDnaUpdate;
class CDnaMeta;
class CDnaChat;
class CDnaEvents;

class GNUC_API CDnaCore
{
public:
	CDnaCore();
	 ~CDnaCore();

	void Load(void);
	void Unload(void);
	
	bool m_Loaded;
	
	UINT m_SecTimerID;
	UINT m_HourTimerID;
	UINT m_LastTickCount;

	CString GetRunPath(void);
	void SetRunPath(LPCTSTR newVal);
	void Connect(void);
	void Disconnect(void);
	BOOL IsConnecting(void);
	DATE GetUptime(void);
	LONG GetBytesPerSecDown(void);
	LONG GetBytesPerSecUp(void);
	CString GetClientName(void);
	void SetClientName(LPCTSTR newVal);
	CString GetClientVersion(void);
	void SetClientVersion(LPCTSTR newVal);
	CString GetCoreVersion(void);
	std::vector<CString> GetCoreCredits(void);
	CString GetCoreLicense(void);
	CString GetClientCode(void);
	void SetClientCode(LPCTSTR newVal);
	void Connect2(LONG NetworkID);
	void Disconnect2(LONG NetworkID);
	BOOL IsConnecting2(LONG NetworkID);
	DATE GetUptime2(LONG NetworkID);

	CGnuCore*  m_gnuCore; // Main Gnuc core

	CDnaEvents*   m_dnaEvents;
	CDnaPrefs*    m_dnaPrefs;
	CDnaNetwork*  m_dnaNetwork;
	CDnaCache*	  m_dnaCache;
	CDnaShare*	  m_dnaShare;
	CDnaSearch*   m_dnaSearch;
	CDnaDownload* m_dnaDownload;
	CDnaUpload*   m_dnaUpload;
	CDnaUpdate*	  m_dnaUpdate;
	CDnaMeta*     m_dnaMeta;
	CDnaChat*	  m_dnaChat;

	CDnaPrefs*		GetPrefs()	 { return m_dnaPrefs; } 
	CDnaSearch*		GetSearch()	 { return m_dnaSearch; } 
	CDnaDownload*	GetDownload(){ return m_dnaDownload; } 
	CDnaUpload*		GetUpload()  { return m_dnaUpload; } 
	CDnaUpdate*		GetUpdate()  { return m_dnaUpdate; }
	CDnaNetwork*	GetNetwork() { return m_dnaNetwork; } 
	CDnaCache*		GetCache()	 { return m_dnaCache; } 
	CDnaShare*		GetShare()	 { return m_dnaShare; } 
	CDnaMeta*		GetMeta()	 { return m_dnaMeta; } 
	CDnaChat*		GetChat()	 { return m_dnaChat; } 


};

