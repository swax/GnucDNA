#pragma once

class CGnuCore;
class CGnuShare;

struct IPRule;
struct BlockedHost;
struct ProxyAddr;

class CGnuPrefs
{
public:
	CGnuPrefs(CGnuCore* pCore);
	~CGnuPrefs();

	void LoadDefaults();

	void LoadConfig(CString);
	void SaveConfig(CString);

	void LoadBlocked(CString);
	void SaveBlocked(CString);

	void LoadProxies(CString);
	void SaveProxies(CString);

	bool	AllowedIP(IP);
	bool	BlockedIP(IP);
	bool	MatchIP(IP, IPRule&);

	int CalcMaxLeaves();
	ProxyAddr GetRandProxy();


	// Local	
	IP    m_ForcedHost;		  // IP of node assigned by user
	UINT  m_ForcedPort;		  // Port of node assigned by user
	
	UINT  m_SpeedStatic;	  // Node speed determined by the user
	int   m_SpeedDown; // KBs
	int   m_SpeedUp;   // KBs
	
	int   m_Update;			  // Auto-Update Mode
	GUID  m_ClientID;
	bool  m_Sp2Override;
	
	// Local network
	bool	m_SupernodeAble;
	int		m_MaxLeaves;

	bool	m_LanMode;
	CString m_LanName;

	CString m_NetworkName;
	
	// Local Firewall
	bool  m_BehindFirewall;

	// Connect
	int		m_LeafModeConnects;
	int     m_MinConnects;
	int     m_MaxConnects;
	
	// G2 Connect Prefs
	int m_G2ChildConnects;
	int m_G2MinConnects;
	int m_G2MaxConnects;

	// Connect Servers
	std::vector<Node> m_HostServers;

	// Connect Screen
	std::vector<IPRule> m_ScreenedNodes;

	// Search
	CString m_DownloadPath;
	bool    m_DoubleCheck;
	bool    m_ScreenNodes; 

	// Search Screen
	std::vector<CString>     m_ScreenedWords;
	std::vector<BlockedHost> m_BlockList;

	// Share
	bool m_ReplyFilePath;
	int  m_MaxReplies;
	bool m_SendOnlyAvail;

	// Transfer
	CString m_PartialDir;
	int     m_MaxDownloads;
	int     m_MaxUploads;
	bool    m_Multisource;
	bool    m_AntivirusEnabled;
	CString m_AntivirusPath;

	// Bandwidth
	float m_BandwidthUp;   // KBs
	float m_BandwidthDown; // KBs
	float m_MinDownSpeed;  // KBs
	float m_MinUpSpeed;    // KBs

	// Proxy setup
	std::vector<ProxyAddr> m_ProxyList;
	
	// Geo location
	uint16 m_GeoLatitude;
	uint16 m_GeoLongitude;

	CGnuCore*  m_pCore;	// Parent object
	CGnuShare* m_pShare;
};

struct IPRule
{
	int a; // 0 - 255 is ip num, -1 is wildcard
	int b;
	int c;
	int d;
	bool mode;       // false for deny, true for allow

	IPRule()
	{
		a = 0;
		b = 0;
		c = 0;
		d = 0;
		mode = false;
	}
};

struct BlockedHost
{
	IP StartIP;
	IP EndIP;

	CString Reason;
};

struct ProxyAddr
{
	CString host;
	uint16  port;

	ProxyAddr()
	{
		port = 80;
	};
};



IPRule    StrtoIPRule(CString in);	// String to extended IP
CString IPRuletoStr(IPRule in);		// ExtendedIP to String	

BlockedHost StrtoBlocked(CString strBlocked);
CString		BlockedtoStr(BlockedHost badHost);

ProxyAddr StrtoProxy(CString strProxy);