#pragma once


class CGnuCore;
class CGnuNetworks;
class CGnuPrefs;


#define ALIVE		1
#define DEAD		2
#define UNTESTED	3
#define GWCERROR	4

#define GWC_VERSION1	1 
#define	GWC_VERSION2	2

#define RECENT_SIZE 60


struct AltWebCache
{
	CString URL;
	int		State;
	CTime	LastRef;
	int		ErrCount;
	int		GWCVer;

	AltWebCache(CString nURL = "", 
		int nState = UNTESTED,
		CTime nLastRef = CTime::GetCurrentTime(),
		int nErrCount = 0,
		int nGWCVer = GWC_VERSION1)
	{ 
		URL   = nURL; 
		State = nState;
		LastRef = nLastRef;
		ErrCount = nErrCount;
		GWCVer = nGWCVer;
	};
};

class CGnuCache  
{
public:
	CGnuCache(CGnuNetworks*);
	virtual ~CGnuCache();
	void endThreads();

	void LoadCache(CString);
	void SaveCache(CString);
	void WriteCache(CStdioFile&, std::list<Node>&);

	void AddKnown(Node ActiveNode);
	void AddWorking(Node WorkingNode);

	bool IsRecent(IP);
	void RemoveIP(CString, int);

	// Web Cache funtions
	void LoadWebCaches(CString);
	void SaveWebCaches(CString);

	void WebCacheRequest(bool HostFileOnly=false);
	void WebCacheGetRequest(CString network = "gnutella");
	void WebCacheUpdate();
	bool WebCacheAddCache(CString);

	CString WebCacheDoRequest(CString);
	bool	WebCacheParseResponse(CString, CString);
	void	MarkWebCache(CString URL, bool isAlive, int GWCVer = GWC_VERSION1);
	void	DebugDumpWebCaches();

	bool    ValidURL(CString);
	CString EscapeEncode(CString &what);
	CString GetRandWebCache(bool);

	void Timer();

	int m_MaxCacheSize;
	int m_MaxWebCacheSize;

	std::list<Node> m_GnuPerm;
	std::list<Node> m_GnuReal;

	std::list<Node> m_G2Perm;
	std::list<Node> m_G2Real;

	std::list<IP>   m_RecentIPs;


	bool m_AllowPrivateIPs;

	UINT	m_WebMode;
	CString m_WebCommand;
	CString m_WebNetwork;
	CString m_NewSite;

	int m_TriedSites;
	int m_TotalSites;

	int m_SecsLastRequest;

	std::vector<AltWebCache> m_AltWebCaches;

	bool m_ThreadEnded;
	bool m_StopThread;
	CWinThread* m_pWebCacheThread;
	
	CCriticalSection m_TransferAccess;
	std::vector<CString> m_WebTransferList;

	CGnuCore*	   m_pCore;
	CGnuNetworks*  m_pNet;
	CGnuPrefs*	   m_pPrefs;
};
