#pragma once


#include "GnuRouting.h"
#include "GnuPackets.h"
#include "G2Packets.h"
#include "GnuShare.h"

struct key_Value;
struct Gnu_RecvdPacket;
struct DynQuery;
struct OobHit;

class CGnuNetworks;
class CGnuLocal;
class CGnuPrefs;
class CGnuCore;
class CGnuSock;
class CGnuNode;
class CGnuCache;
class CGnuTransfers;
class CGnuShare;
class CGnuDatagram;
class CGnuProtocol;

#define GNU_ULTRAPEER 1
#define GNU_LEAF	  2

#define MAX_TTL     3
#define MAX_LEAVES 32

class CGnuControl
{
public:
	CGnuControl(CGnuNetworks*);
	~CGnuControl();


	void Timer();
	void MinuteTimer();
	void HourlyTimer();

	void ManageNodes();
	void CleanDeadSocks();


	// Other threads always have to lock before touching list
	CCriticalSection m_NodeAccess;
	
	std::vector<CGnuNode*>	    m_NodeList;
	std::map<int, CGnuNode*>    m_NodeIDMap;
	std::map<uint32, CGnuNode*> m_GnuNodeAddrMap;
	std::vector<CGnuNode*>	    m_NodesBrowsing;

	CGnuLocal* m_LanSock;

	void SendUdpConnectRequest(CString, UINT);
	void AddNode(CString, UINT);
	void RemoveNode(CGnuNode*);
	CGnuNode* FindNode(CString Host, UINT Port, bool Connected=true);

	CGnuNode* GetRandNode(int Type, bool dnaOnly=false);

	void AddConnect(bool PrefDna=false);
	void DropNode(int GnuMode, bool NeedDna);
	bool ConnectFromCache(std::list<Node> &Cache, bool Perm);

	int	 CountUltraConnects();
	int  CountLeafConnects();
	int  CountConnecting();

	void NodeUpdate(CGnuNode* pNode);


	// Other
	bool UltrapeerAble();
	void DowngradeClient();	
	void ShareUpdate();
	DWORD GetSpeed();
	void GetLocalNodeInfo(GnuNodeInfo &LocalNode);

	CString GetPushProxyHeader();

	// Network
	CString  m_NetworkName;

	std::map<uint32, bool> m_TriedConnects;

	time_t m_LastConnect;
	bool   m_TryingConnect;

	// Local Client Data
	CTime   m_ClientUptime;
	uint32  m_LastSearchTime;

	DWORD   m_dwFiles;
	DWORD   m_dwFilesSize;


	// Ultrapeers
	void UltrapeerBalancing();
	int  ScoreNode(CGnuNode* pNode);
	void SwitchGnuClientMode(int GnuMode);

	int     m_GnuClientMode;
	bool	m_ForcedUltrapeer;
	bool    NeedDnaUltras;

	uint32 m_NextUpgrade;
	uint32 m_ModeChangeTimeout;
	uint32 m_AutoUpgrade;
	
	int m_MinsBelow10;
	int m_MinsBelow70;
	int m_NoConnections;

	// Hash tables
	CGnuRouting m_TableRouting;
	CGnuRouting m_TablePush;
	CGnuRouting m_TableLocal;


	// Dynamic Queries
	void AddDynQuery(DynQuery* pQuery);
	void DynQueryTimer();

	std::map<uint32, DynQuery*> m_DynamicQueries;
	std::map<uint32, GUID> m_OobtoRealGuid;

	void StopSearch(GUID SearchGuid);
	

	// Out of Band Hits
	void OobHitsTimer();

	CCriticalSection m_OobHitsLock;
	std::map<uint32, OobHit*> m_OobHits;

	// Push Proxying
	std::map<int, GUID> m_PushProxyHosts; // NodeID, ClientID

	// Bandwidth
	double m_TcpSecBytesDown;
	double m_TcpSecBytesUp;

	double m_NetSecBytesDown;
	double m_NetSecBytesUp;

	int m_Minute;


	CGnuNetworks*  m_pNet;
	CGnuCore*	   m_pCore;
	CGnuPrefs*	   m_pPrefs;
	CGnuCache*     m_pCache;
	CGnuTransfers* m_pTrans;
	CGnuShare*	   m_pShare;
	CGnuDatagram*  m_pDatagram;
	CGnuProtocol*  m_pProtocol;
};


struct Gnu_RecvdPacket
{
	IPv4		   Source;
	packet_Header* Header;
	int			   Length;
	CGnuNode*	   pTCP;

	Gnu_RecvdPacket(IPv4 Address, packet_Header* header, int length, CGnuNode* pNode=NULL)
	{
		Source = Address;
		Header = header;
		Length = length;
		pTCP   = pNode;
	};
};

// Dynamic Queries
#define DQ_TARGET_HITS		50		// Number of hits to obtain for leaf
#define DQ_QUERY_TIMEOUT	(3*60)  // Time a dyn query lives for
#define DQ_QUERY_INTERVAL	7       // Interval to send next query out
#define DQ_UPDATE_INTERVAL	5       // Interval to ask leaf for a hit update
#define DQ_MAX_QUERIES      4		// Simultaneous qeueries by 1 node

struct DynQuery
{
	int NodeID;

	byte* Packet;
	int   PacketLength;
	GUID  RealGuid;

	std::map<int, bool> NodesQueried;

	int Secs;
	int Hits;

	DynQuery(int nodeID, byte* packet, int length)
	{
		NodeID = nodeID;

		Packet = new byte[length];
		memcpy(Packet, packet, length);
		
		PacketLength = length;

		memcpy(&RealGuid, Packet, 16);

		Secs = 0;
		Hits = 0;
	};

	~DynQuery()
	{
		// Dynamic Query ID:15 Destroyed, Secs:30, Hits:15
		TRACE0("Dynamic Query ID:" + NumtoStr(NodeID) + " Destroyed, Secs:" + NumtoStr(Secs) + ", Hits:" + NumtoStr(Hits) + "\n");

		if(Packet)
			delete [] Packet;

		Packet = NULL;
	};
};


// Out of band Hits
#define OOB_TIMEOUT 10

struct OobHit
{
	IPv4 Target;
	int  TotalHits;
	int  OriginID;
	
	int Secs;

	bool SentReplyNum;

	std::vector<byte*> QueryHits;
	std::vector<int>   QueryHitLengths;


	OobHit(IPv4 target, int hits, int origin)
	{
		Target    = target;
		TotalHits = hits;
		OriginID  = origin;

		Secs = 0;

		SentReplyNum = false;
	};

	~OobHit()
	{
		for(int i = 0; i < QueryHits.size(); i++)
			delete [] QueryHits[i];
	}
};