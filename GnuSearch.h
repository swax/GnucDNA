#pragma once


class CGnuCore;
class CGnuNetworks;
class CGnuTransfers;
class CGnuNode;
class CGnuShare;
class CGnuPrefs;
class CGnuMeta;

#define SEARCH_RESULT_STEP 100
#define SEARCH_TIMOUT_STEP (5 * 60)

#define BUILD_GNUTELLA 1 // If G1 network activated, packets built and ready to go

#define SUBNET_LIMIT 3

class CGnuSearch
{
public:
	CGnuSearch(CGnuNetworks* pNet);
	~CGnuSearch(void);

	void SendQuery(CString Query);
	void SendMetaQuery(CString Query, int MetaID, std::vector<CString> Metadata);
	void SendHashQuery(CString Query, int HashID, CString Hash);
	void SendBrowseRequest(CString Node, int Port);

	void IncomingHost(FileSource &Source);
	bool Inspect(FileSource &);
	bool ResultDoubleCheck(CString, CString);
	bool ResultDoubleCheckMeta(FileSource &);
	bool CheckLimit(int, DWORD, DWORD);
	
	void Timer();
	void TransferUpdate(int);
	void IncomingGnuNode(CGnuNode*);

	ResultGroup* AddtoGroup(FileSource &);

	void Rebuild();

	int Download(UINT ResultID);

	int UpdateResultState(CString Hash);

	int     m_SearchID;
	UINT    m_NextResultID;
	
	CString m_Search;
	CString m_Hash;
	GUID	m_QueryID;
	int		m_MetaID;
	CString m_MetaSearch;

	std::map<int, CString> m_SearchAttributes;
	
	int    m_ResultStep;
	uint32 m_NextTimeout;
	bool   m_SearchPaused;
	int    m_Minute;

	CGnuNode* m_BrowseNode;
	int       m_BrowseWaiting;

	byte m_GnuPacket[4096];
	int  m_GnuPacketLength;

	bool m_BrowseSearch;

	std::map<UINT, ResultGroup*>    m_ResultMap;
	std::map<CString, ResultGroup*> m_ResultHashMap;

	std::list<ResultGroup>	m_GroupList;    // Groups from current list by hash and size
	std::vector<FileSource>	m_CurrentList;  // Filtered results 
	std::vector<FileSource>	m_WholeList;    // All results

	CString RefinedQuery;
	bool    m_FilteringActive;

	int  m_SizeFilterMode;
	int  m_SizeFilterValue;

	int  m_SpeedFilterMode;
	int  m_SpeedFilterValue;

	CGnuCore*	   m_pCore;
	CGnuNetworks*  m_pNet;
	CGnuPrefs*     m_pPrefs;
	CGnuShare*	   m_pShare;
	CGnuTransfers* m_pTrans;
	CGnuMeta*	   m_pMeta;
};
