#pragma once

class CDnaCore;
class CGnuPrefs;

class GNUC_API CDnaPrefs 
{
public:
	CDnaPrefs();
	 ~CDnaPrefs();


	void InitClass(CDnaCore* dnaCore);

	CDnaCore*  m_dnaCore;
	CGnuPrefs* m_gnuPrefs;

	// Local
	void LoadConfig(LPCTSTR FilePath);
	void SaveConfig(LPCTSTR FilePath);
	void LoadBlocked(LPCTSTR FilePath);
	void SaveBlocked(LPCTSTR FilePath);

	ULONG GetForcedHost(void);
	void SetForcedHost(ULONG newVal);
	LONG GetForcedPort(void);
	void SetForcedPort(LONG newVal);
	LONG GetSpeedStat(void);
	void SetSpeedStat(LONG newVal);
	LONG GetUpdate(void);
	void SetUpdate(LONG newVal);
	void GetClientID(byte ClientID[16]);
	void SetClientID(byte ClientID[16]);

	// Local Network
	BOOL GetSuperNodeAble(void);
	void SetSuperNodeAble(BOOL newVal);
	LONG GetMaxLeaves(void);
	void SetMaxLeaves(LONG newVal);
	BOOL GetLanMode(void);
	void SetLanMode(BOOL newVal);
	CString GetLanName(void);
	void SetLanName(LPCTSTR newVal);

	// Local Firewall
	BOOL GetBehindFirewall(void);
	void SetBehindFirewall(BOOL newVal);

	// Connect
	LONG GetLeafModeConnects(void);
	void SetLeafModeConnects(LONG newVal);
	LONG GetMinConnects(void);
	void SetMinConnects(LONG newVal);
	LONG GetMaxConnects(void);
	void SetMaxConnects(LONG newVal);

	// Connect Servers
	std::vector<CString> GetHostServers(void);
	void SetHostServers(std::vector<CString> &HostServers);

	// Connect Screen
	std::vector<CString> GetScreenedNodes(void);
	void SetScreenedNodes(std::vector<CString> &ScreenedNodes);

	// Search
	CString GetDownloadPath(void);
	void SetDownloadPath(LPCTSTR newVal);
	BOOL GetDoubleCheck(void);
	void SetDoubleCheck(BOOL newVal);
	BOOL GetScreenNodes(void);
	void SetScreenNodes(BOOL newVal);

	// Search Screen
	std::vector<CString> GetScreenedWords(void);
	void SetScreenedWords(std::vector<CString> &ScreenedWords);
	std::vector<CString> GetBlockList(void);
	void SetBlockList(std::vector<CString> &BlockList);

	// Share
	BOOL GetReplyFilePath(void);
	void SetReplyFilePath(BOOL newVal);
	LONG GetMaxReplies(void);
	void SetMaxReplies(LONG newVal);
	BOOL GetSendOnlyAvail(void);
	void SetSendOnlyAvail(BOOL newVal);
	
	/// Transfer
	LONG GetMaxDownloads(void);
	void SetMaxDownloads(LONG newVal);
	LONG GetMaxUploads(void);
	void SetMaxUploads(LONG newVal);
	BOOL GetMultisource(void);
	void SetMultisource(BOOL newVal);
	
	// Bandwidth
	FLOAT GetBandwidthUp(void);
	void SetBandwidthUp(FLOAT newVal);
	FLOAT GetBandwidthDown(void);
	void SetBandwidthDown(FLOAT newVal);
	FLOAT GetMinDownSpeed(void);
	void SetMinDownSpeed(FLOAT newVal);
	FLOAT GetMinUpSpeed(void);
	void SetMinUpSpeed(FLOAT newVal);
	

	CString GetPartialsDir(void);
	void SetPartialsDir(LPCTSTR newVal);
	void LoadProxies(LPCTSTR FilePath);
	void SaveProxies(LPCTSTR FilePath);
	std::vector<CString> GetProxyList(void);
	void SetProxyList(std::vector<CString> &ProxyList);
	BOOL GetAntivirusEnabled(void);
	void SetAntivirusEnabled(BOOL newVal);
	CString GetAntivirusPath(void);
	void SetAntivirusPath(LPCTSTR newVal);

	DOUBLE GetLatitude(void);
	void SetLatitude(DOUBLE newVal);
	DOUBLE GetLongitude(void);
	void SetLongitude(DOUBLE newVal);

	CString GetNetworkName(void);
	void SetNetworkName(LPCTSTR newVal);
};


