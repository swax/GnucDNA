#pragma once


class CDnaCore;
class CGnuNetworks;

class GNUC_API CDnaSearch 
{
public:
	CDnaSearch();
	 ~CDnaSearch();

	void InitClass(CDnaCore* dnaCore);

	CDnaCore*    m_dnaCore;
	CGnuNetworks* m_gnuNetwork;

	LONG StartSearch(LPCTSTR Query);
	void EndSearch(LONG SearchID);
	void RefineSearch(LONG SearchID, LPCTSTR RefinedQuery);
	void SetFiltering(LONG SearchID, BOOL Enabled);
	void FilterSize(LONG SearchID, LONG Mode, LONG Value);
	void FilterSpeed(LONG SearchID, LONG Mode, LONG Value);
	void PauseSearch(LONG SearchID);
	LONG CountGoodResults(LONG SearchID);
	LONG CountTotalResults(LONG SearchID);
	CString GetResultName(LONG SearchID, LONG ResultID);
	ULONGLONG GetResultSize(LONG SearchID, LONG ResultID);
	LONG GetResultSpeed(LONG SearchID, LONG ResultID);
	LONG GetResultHostCount(LONG SearchID, LONG ResultID);
	CString GetResultHash(LONG SearchID, LONG ResultID, LONG HashID);
	std::vector<int> GetResultIDs(LONG SearchID);
	LONG DownloadResult(LONG SearchID, LONG ResultID);
	std::vector<int> GetHostIDs(LONG SearchID, LONG ResultID);
	ULONG GetHostIP(LONG SearchID, LONG ResultID, LONG HostID);
	LONG GetHostPort(LONG SearchID, LONG ResultID, LONG HostID);
	LONG GetHostSpeed(LONG SearchID, LONG ResultID, LONG HostID);
	LONG GetHostDistance(LONG SearchID, LONG ResultID, LONG HostID);
	BOOL GetHostFirewall(LONG SearchID, LONG ResultID, LONG HostID);
	BOOL GetHostStable(LONG SearchID, LONG ResultID, LONG HostID);
	BOOL GetHostBusy(LONG SearchID, LONG ResultID, LONG HostID);
	CString GetHostVendor(LONG SearchID, LONG ResultID, LONG HostID);
	std::vector<CString> GetHostExtended(LONG SearchID, LONG ResultID, LONG HostID);
	LONG GetResultState(LONG SearchID, LONG ResultID);
	LONG StartMetaSearch(LPCTSTR Query, LONG MetaID,  std::vector<CString> &AttributeList);
	LONG StartHashSearch(LPCTSTR Query, LONG HashID, LPCTSTR Hash);
	LONG GetResultMetaID(LONG SearchID, LONG ResultID);
	CString GetResultAttributeValue(LONG SearchID, LONG ResultID, LONG AttributeID);
	LONG GetHostMetaID(LONG SearchID, LONG ResultID, LONG HostID);
	CString GetHostAttributeValue(LONG SearchID, LONG ResultID, LONG HostID, LONG AttributeID);
	LONG SendBrowseRequest(LPCTSTR Host, LONG Port);
	LONG CountHostsSearched(LONG SearchID);
	void ContinueSearch(LONG SearchID);
};


