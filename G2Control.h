#pragma once

#include "G2Packets.h"
#include "GnuShare.h"

class CGnuNetworks;
class CGnuPrefs;
class CGnuCache;
class CGnuShare;
class CGnuTransfers;
class CGnuCore;
class CG2Datagram;
class CG2Protocol;
class CG2Node;


#define G2_UNKNOWN 1
#define G2_CHILD   2
#define G2_HUB     3 

#define PACKETS_PER_SEC  8
#define ALT_HOST_MAX     5

#define ROUTE_EXPIRE	(3 * 60)
#define QA_QUERY_RETRY  (5 * 60) // Each node with 10 connects, 30 secs between search of cluster

#define CLEAN_ROUTES  (3 * 60)
#define CLEAN_KEYS    (1 * 60)
#define CLEAN_GLOBAL  (5 * 60)

#define MAX_ROUTES	 20000
#define MAX_KEYS	 50000
#define MAX_GLOBAL	 30000

#define HIGH_HUB_CAPACITY 90
#define LOW_HUB_CAPACITY  70

#define OPT_BANDWIDTH_DOWN 512 // 512 kilobits // use ping?
#define OPT_BANDWIDTH_UP   512 // 512 kilobits
#define OPT_LEAFMAX		   500   // 500 Children
#define OPT_UPTIME		   (6 * 60 * 60) // 6 hours

#define AVG_DNA   0
#define AVG_TOTAL 1


struct G2_RecvdPacket;
struct G2_Search;
struct G2_QueryKey;
struct G2_Route;
struct G2HubInfo;

class CG2Control
{
public:
	CG2Control(CGnuNetworks* pNet);
	~CG2Control();

	// Events
	void Timer();
	void HourlyTimer();

	void ShareUpdate();

	// Connections
	void ManageNodes();
	
	void TryConnect(bool PrefDna);
	void DropNode(int G2Mode, bool NeedDna);
	bool ConnectFromCache(std::list<Node> &Cache, bool Perm);

	void CreateNode(Node HostInfo);
	void RemoveNode(CG2Node*);

	std::map<uint32, bool> m_TriedConnects;

	time_t m_LastConnect;


	// Hub Balancing
	void HubBalancing();
	int  ScoreNode(CG2Node* pNode);
	void UpdateGlobal(G2NodeInfo &HubNode);

	int    m_HubBalanceCheck;
	uint32 m_NextUpgrade;
	uint32 m_ModeChangeTimeout;
	
	int m_MinsBelow10;
	int m_MinsBelow70;
	int m_NoConnections;

	// Counting
	int CountHubConnects();
	int CountChildConnects();

	
	// Other
	void TrimMaps();
	void CleanDeadSocks();

	CG2Node* FindNode(CString Host, UINT Port, bool Connected=true);
	CG2Node* GetRandHub();
	bool	 GetAltHubs(CString &HostList, CG2Node* NodeExclude, bool DnaOnly);

	void G2NodeUpdate(CG2Node* updated);

	void SwitchG2ClientMode(int G2Mode, bool DownG1=false);

	void TestEncodeDecode();

	void ApplyPatchTable(CG2Node* pNode);
	void SetQHTBit(int &remotePos, double &Factor, CG2Node* pNode);

	

	// Receiving
	void ReceivePacket(G2_RecvdPacket &Packet);

	void Receive_PI(G2_RecvdPacket &PacketPI);
	void Receive_PO(G2_RecvdPacket &PacketPO);
	void Receive_LNI(G2_RecvdPacket &PacketLNI);
	void Receive_KHL(G2_RecvdPacket &PacketKHL);
	void Receive_QHT(G2_RecvdPacket &PacketQHT);
	void Receive_QKR(G2_RecvdPacket &PacketQKR);
	void Receive_QKA(G2_RecvdPacket &PacketQKA);
	void Receive_Q1(G2_RecvdPacket &PacketQ1);
	void Receive_Q2(G2_RecvdPacket &PacketQ2);
	void Receive_QA(G2_RecvdPacket &PacketQA);
	void Receive_QH2(G2_RecvdPacket &PacketQH2);
	void Receive_PUSH(G2_RecvdPacket &PacketPUSH);
	void Receive_MCR(G2_RecvdPacket &PacketMCR);
	void Receive_MCA(G2_RecvdPacket &PacketMCA);
	void Receive_PM(G2_RecvdPacket &PacketPM);
	void Receive_CLOSE(G2_RecvdPacket &PacketCLOSE);
	void Receive_CRAWLR(G2_RecvdPacket &PacketCRAWLR);

	void GetLocalNodeInfo(G2NodeInfo &LocalNode);

	// Sending 
	void RoutePacket(G2_RecvdPacket &Packet, GUID &TargetID);

	void Send_PI(IPv4 Target, G2_PI &Ping, CG2Node* pTCP=NULL);
	void Send_PO(G2_PI &Ping, CG2Node* pTCP=NULL);
	void Send_LNI(CG2Node* pTCP);
	void Send_KHL(CG2Node* pTCP);
	void Send_QHT(CG2Node* pTCP, bool Reset=false);
	void Send_QKR(IPv4 Target);
	void Send_QKA(IPv4 Target, G2_QKA &QueryKeyAnswer, CG2Node* pTCP=NULL);
	void Send_Q2(G2HubInfo* pHub, G2_Q2 &Query, CG2Node* pTCP=NULL);
	void Send_Q2(GnuQuery &FileQuery, std::list<int> &MatchingNodes);
	void Send_QA(IPv4 Target, G2_QA &QueryAck, CG2Node* pTCP=NULL);
	void Send_QH2(GnuQuery &FileQuery, std::list<UINT> &MatchingIndexes);
	void Send_QH2(GnuQuery &FileQuery, G2_QH2 &QueryHit);
	void Send_QH2(IPv4 Target, G2_QH2 &QueryHit);
	void Send_PUSH(FileSource* HostSource);
	void Send_MCR(CG2Node* pTCP);
	void Send_MCA(CG2Node* pTCP, bool Accept);
	void Send_PM(IPv4 Target, G2_PM &PrivateMessage, CG2Node* pTCP=NULL);
	void Send_CLOSE(CG2Node* pTCP, G2_CLOSE &Close);
	void Send_CRAWLA(IPv4 Target, G2_CRAWLA &CrawlAck);


	// Nodes
	CCriticalSection	m_G2NodeAccess;
	std::vector<CG2Node*>	 m_G2NodeList;
	std::map<int, CG2Node*>  m_G2NodeIDMap;
	std::map<uint32, CG2Node*> m_G2NodeAddrMap;

	uint32 m_CleanRoutesNext;
	std::map<uint32, G2_Route> m_RouteMap;

	uint32 GenerateQueryKey(uint32 Address);
	byte RandKeyBlock[64];
	std::map<uint32, G2_QueryKey> m_KeyInfo;
	uint32 m_CleanKeysNext;

	// Client Info
	int    m_ClientMode;
	CTime  m_ClientUptime;
	uint32 m_ClientIdent;

	bool m_ForcedHub;

	// Searching
	void StartSearch(G2_Search *pSearch);
	void StepSearch(GUID SearchGuid);
	void EndSearch(GUID SearchGuid);

	uint32 m_CleanGlobalNext;
	std::map<uint32, G2HubInfo> m_GlobalHubs;
	uint32 m_GlobalUnique; // Keeps the hub table in a different order for each client

	std::list<G2_Search*> m_G2Searches;

	int m_ActiveSearches;
	int m_ActiveSearchCount;

	int m_SearchReport;

	int m_SearchPacketsRecvd;

	// Bandwidth
	double m_NetSecBytesDown;
	double m_NetSecBytesUp;

	// Avg Query Packets
	CRangeAvg m_PacketsAvgQKR[2];
	CRangeAvg m_PacketsAvgQ2[2];
	
	uint16    m_PacketsQKR[2];
	uint16    m_PacketsQ2[2];

	// Links
	CGnuNetworks*  m_pNet;
	CGnuCache*     m_pCache;
	CGnuPrefs*     m_pPrefs;
	CGnuShare*	   m_pShare;
	CGnuTransfers* m_pTrans;
	CGnuCore*	   m_pCore;

	CG2Datagram*  m_pDispatch;
	CG2Protocol*  m_pProtocol; // Only access with main thread
	CG2Protocol*  m_pThreadProtocol;
};

struct G2_RecvdPacket
{
	IPv4      Source;
	CG2Node*  pTCP;
	G2_Header Root;

	G2_RecvdPacket(IPv4 Address, G2_Header Packet, CG2Node* pNode=NULL)
	{
		Source = Address;
		Root   = Packet;
		pTCP   = pNode;
	};
};

struct G2_Search
{
	G2_Q2 Query;

	int SearchedHubs;
	int SearchedChildren;
	int SearchedDupes;

	std::map<uint32, bool> TriedHubs;

	std::vector<IP> Routers;

	G2_Search()
	{
		SearchedHubs = 0;
		SearchedChildren = 0;
		SearchedDupes = 0;
	};
};

struct G2_QueryKey
{
	uint32 RetryAfter;
	bool   Banned;

	G2_QueryKey()
	{
		RetryAfter = 0;
		Banned = false;
	};
};

struct G2_Route
{
	IPv4   Address;
	GUID   RouteID;
	uint32 ExpireTime;

	G2_Route()
	{
		ExpireTime = 0;
	}
};

struct G2HubInfo
{
	IPv4 Address;
	IPv4 Router;

	uint32 QueryKey;

	uint32 ExpireTime;
	uint32 NextTry;
	uint32 TryCount;

	G2HubInfo()
	{
		Init();
	};

	G2HubInfo(IPv4 address)
	{
		Init();

		Address = address;
	};

	void Init()
	{
		ExpireTime = time(NULL) + GLOBAL_HUB_EXPIRE;
		NextTry    = 0;
		TryCount   = 0;

		QueryKey   = 0;
	};
};
