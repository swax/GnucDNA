#pragma once

class CDnaCore;
class CGnuCache;

class GNUC_API CDnaCache 
{
public:
	CDnaCache();
	~CDnaCache();

	void InitClass(CDnaCore* dnaCore);


	CDnaCore*  m_dnaCore;
	CGnuCache* m_gnuCache;

	void LoadCache(LPCTSTR FilePath);
	void LoadUltraCache(LPCTSTR FilePath);
	void LoadWebCache(LPCTSTR FilePath);
	void AddWebCache(LPCTSTR WebAddress);
	void SaveCache(LPCTSTR FilePath);
	void SaveUltraCache(LPCTSTR FilePath);
	void SaveWebCache(LPCTSTR FilePath);
	LONG GetNodeCacheSize(void);
	LONG GetNodeCacheMaxSize(void);
	LONG GetUltraNodeCacheSize(void);
	LONG GetUltraNodeCacheMaxSize(void);
	LONG GetWebCacheSize(void);
	LONG GetWebCacheMaxSize(void);
	void AddNode(LPCTSTR HostPort, BOOL SuperNode);
	void RemoveIP(LPCTSTR Host, int Port);
};


