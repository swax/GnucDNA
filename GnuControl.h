#pragma once


#include "GnuRouting.h"
#include "GnuPackets.h"
#include "GnuShare.h"

struct key_Value;
struct DynQuery;

class CGnuNetworks;
class CGnuLocal;
class CGnuPrefs;
class CGnuCore;
class CGnuSock;
class CGnuNode;
class CGnuCache;
class CGnuTransfers;
class CGnuShare;

#define GNU_ULTRAPEER 1
#define GNU_LEAF	  2

#define MAX_TTL 3

class CGnuControl
{
public:
	CGnuControl(CGnuNetworks*);
	~CGnuControl();


	// Other threads always have to lock before touching list
	CCriticalSection m_NodeAccess;
	
	std::vector<CGnuNode*>	 m_NodeList;
	std::map<int, CGnuNode*> m_NodeIDMap;

	std::map<uint32, CGnuNode*> m_GnuNodeAddrMap;

	std::vector<CGnuNode*>	m_NodesBrowsing;

	CGnuLocal* m_LanSock;


	// Traffic control
	void Broadcast_Ping(packet_Ping*,   int, CGnuNode*);
	void Broadcast_Query(packet_Query*, int, CGnuNode*);
	void Broadcast_LocalQuery(byte*, int);

	void Route_Pong(packet_Pong*, int, int);
	void Route_UltraPong(packet_Pong*, int, int);
	void Route_QueryHit(packet_QueryHit *, DWORD, int);
	void Route_LocalQueryHit(GnuQuery &FileQuery, byte* pQueryHit, DWORD ReplyLength, byte ReplyCount, CString &MetaTail);
	void Route_Push(packet_Push*, int, int);
	void Route_LocalPush(FileSource);

	void Encode_QueryHit(GnuQuery &FileQuery, std::list<UINT> &MatchingIndexes, byte* QueryReply);
	void Forward_Query(GnuQuery &FileQuery, std::list<int> &MatchingNodes);

	// Node control
	void AddNode(CString, UINT);
	void RemoveNode(CGnuNode*);
	CGnuNode* FindNode(CString, UINT);

	CGnuNode* GetRandUltrapeer();

	// Socket Counts
	int	 CountSuperConnects();
	int  CountNormalConnects();
	int  CountLeafConnects();

	bool UltrapeerAble();
	void DowngradeClient();


	// Communications
	void NodeUpdate(CGnuNode* pNode);
	void PacketIncoming(int NodeID, byte* packet, int size, int ErrorCode, bool Local);
	void PacketOutgoing(int NodeID, byte* packet, int size, bool Local);

	// Searching
	void StopSearch(GUID SearchGuid);

	void Timer();
	void HourlyTimer();

	void ManageNodes();
	void CleanDeadSocks();
	
	void AddConnect();
	void DropNode();
	void DropLeaf();
	
	void ShareUpdate();

	// Dynamic Queries
	void AddDynQuery(DynQuery* pQuery);
	void DynQueryTimer();

	std::map<uint32, DynQuery*> m_DynamicQueries;

	// Network
	CString  m_NetworkName;
	

	// Local Client Data
	CTime   m_ClientUptime;

	DWORD   m_dwFiles;
	DWORD   m_dwFilesSize;


	// SuperNodes
	void SwitchGnuClientMode(int GnuMode);

	int     m_GnuClientMode;
	bool	m_ForcedUltrapeer;

	int m_NormalConnectsApprox;

	int	m_ExtPongBytes;
	int m_UltraPongBytes;
	int m_SecCount;


	// Hash tables
	CGnuRouting m_TableRouting;
	CGnuRouting m_TablePush;
	CGnuRouting m_TableLocal;


	// Bandwidth
	double m_NetSecBytesDown;
	double m_NetSecBytesUp;

	int m_Minute;

	CGnuNetworks*  m_pNet;
	CGnuCore*	   m_pCore;
	CGnuPrefs*	   m_pPrefs;
	CGnuCache*     m_pCache;
	CGnuTransfers* m_pTrans;
	CGnuShare*	   m_pShare;
};


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

	std::map<int, bool> NodesQueried;

	int Secs;
	int Hits;

	DynQuery(int nodeID, byte* packet, int length)
	{
		NodeID = nodeID;

		Packet = new byte[length];
		memcpy(Packet, packet, length);

		PacketLength = length;

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