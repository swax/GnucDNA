#pragma once

// Actual mem 720k

//byte InflateBuff[ZSTREAM_BUFF]; // 16k
//byte DeflateBuff[ZSTREAM_BUFF]; // 16k
//byte m_pBuff[PACKET_BUFF];      // 32k
//byte m_BackBuff[PACKET_BUFF];   // 32k
// inflateInit					  // 56k
// deflateInit					  // 272k

// Total 420k

#define REQUERY_WAIT 30

#include "GnuPackets.h"
#include "GnuShare.h"

#define PACKET_BUFF	32768
#define ZSTREAM_BUFF 16384
#define PACKETCACHE_SIZE 1000

#define LEAF_THROTTLE_IN  1000
#define LEAF_THROTTLE_OUT 1000

class CGnuNetworks;
class CGnuTransfers;
class CGnuPrefs;
class CGnuCore;
class CGnuCache;
class CGnuShare;
class CGnuControl;

class  PriorityPacket;
struct MapNode;
struct RecentQuery;

class CGnuNode : public CAsyncSocket
{
public:
	CGnuNode(CGnuControl* pComm, CString Host, UINT Port);
	virtual ~CGnuNode();


	// New connections
	void	ParseHandshake04(CString, byte*, int);
	void	ParseIncomingHandshake06(CString, byte*, int);
	void	ParseOutboundHandshake06(CString, byte*, int);
	
	void	ParseBrowseHandshakeRequest(CString);
	void	ParseBrowseHandshakeResponse(CString, byte*, int);
	
	CString FindHeader(CString);
	void	ParseTryHeader(CString TryHeader);
	void    ParseHubsHeader(CString HubsHeader);

	void SetConnected();
	void Send_ConnectOK(bool);
	void Send_ConnectError(CString Reason);

 
	// Sending packets
	void SendPacket(void*, int, int, int, bool thread=false);
	void SendPacket(PriorityPacket* OutPacket);

	void Send_Ping(int);
	void Send_Pong(GUID, int);
	void Send_QueryHit(GnuQuery &, byte*, DWORD, byte, CString &);
	void Send_ForwardQuery(GnuQuery &);
	void Send_Bye(CString Reason);
	void Send_PatchTable();
	

	// Misc functions
	bool GetAlternateHostList(CString &);
	bool GetAlternateSuperList(CString &);
	void AvoidTriangles();

	void Refresh();
	void Timer();

	int  UpdateStats(int type);
	void AddGoodStat(int type);
	void RemovePacket(int);

	bool ValidAgent(CString Agent);

	// Socket vars
	int m_NodeID;
	int	m_Status;
	
	int	m_SecsTrying;
	int m_SecsAlive;
	int	m_SecsDead;
	int m_CloseWait;

	int m_IntervalPing;
	int m_NextRequeryWait;


	// Connection vars
	CString m_StatusText;
	CString m_HostIP;
	CString m_HostName;
	int		m_GnuNodeMode;
	UINT	m_Port;
	CString m_NetworkName;
	bool    m_Inbound;
	CString m_InitData;

	CString m_WholeHandshake;
	CString m_Handshake;
	CString m_lowHandshake;
	CString m_RemoteAgent;

	// Compression
	bool m_dnapressionOn;
	bool m_InflateRecv;
	bool m_DeflateSend;

	z_stream InflateStream;
	z_stream DeflateStream;

	byte InflateBuff[ZSTREAM_BUFF];
	//byte DeflateBuff[ZSTREAM_BUFF];

	int  m_DeflateStreamSize;
	int  m_ZipStat;

	// Authentication
	CString m_Challenge;
	CString m_ChallengeAnswer;

	CString m_RemoteChallenge;
	CString m_RemoteChallengeAnswer;


	// Ultrapeers
	CTime     m_HostUpSince;
	CTime     m_ConnectTime;
	UINT	  m_NodeFileCount;
	int		  m_NodeLeafMax;

	bool	m_DowngradeRequest; // Is true if we request node to become child, or remote node requests us to become a child node
	

	bool	m_UltraPongSent;

	// QRP
	bool	m_PatchUpdateNeeded;

	UINT	m_TableInfinity;
	UINT	m_TableLength;

	char*	m_PatchTable;
	UINT	m_TableNextPos;

	byte*	m_dnapressedTable;
	UINT	m_dnapressedSize;

	UINT	m_CurrentSeq;


	// Host Browsing
	int   m_BrowseID;
	int   m_BrowseSize;
	std::vector<byte*> m_BrowseHits;
	std::vector<int>   m_BrowseHitSizes;
	byte* m_BrowseBuffer;
	int   m_BrowseBuffSize;
	int   m_BrowseSentBytes;
	int   m_BrowseRecvBytes;
	bool  m_BrowseCompressed;


	// Packet Stats
	char  m_StatPackets[PACKETCACHE_SIZE][2]; // Last 1000 packet, can be set either bad(0) or good(1)
	int   m_StatPos;			  // Position in array
	int   m_StatElements;
	int   m_Efficeincy;
	int   m_StatPings[2],     m_StatPongs[2], m_StatQueries[2],
		  m_StatQueryHits[2], m_StatPushes[2];  // Total received during last 1000 packets and total that were good


	// Mapping
	void MapPong(packet_Pong* Pong);
	std::vector<MapNode> NearMap[2];

	
	// Bandwidth, [0] is Received, [1] is Sent, [2] is Dropped
	CRangeAvg m_AvgPackets[3];   // Packets received/sent in second
	CRangeAvg m_AvgBytes[3];     // Bytes received/sent in second
	
	DWORD m_dwSecPackets[3];
	DWORD m_dwSecBytes[3];


	// CAsyncSocket Overrides
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	
	virtual int Send(const void* lpBuf, int nBufLen, int nFlags = 0);
	virtual void Close();

	void CloseWithReason(CString Reason, bool RemoteClosed=false);


protected:
	// Packet handlers
	void  FinishReceive(int BuffLength);
	void  SplitBundle(byte*, DWORD);
	void  HandlePacket(packet_Header*, DWORD);
	bool  InspectPacket(packet_Header*);


	// Receiving packets
	void Receive_Ping(packet_Ping*,   int);
	void Receive_Pong(packet_Pong*,   int);
	void Receive_Push(packet_Push*,   int);
	void Receive_Query(packet_Query*, int);
	void Receive_QueryHit(packet_QueryHit*, DWORD);
	void Receive_Bye(packet_Bye*,	  int);

	void Decode_QueryHit( std::vector<FileSource> &Sources, packet_QueryHit* QueryHit, uint32 length);

	void Receive_RouteTableReset(packet_RouteTableReset*, UINT);
	void Receive_RouteTablePatch(packet_RouteTablePatch*, UINT);
		
	void Receive_Unknown(byte*, DWORD);

	DWORD GetSpeed();
	void  NodeManagement();
	void  CompressionStats();


	// Receiving data
	byte  m_pBuff[PACKET_BUFF];
	int   m_ExtraLength;


	// Sending data, packet prioritization
	CCriticalSection m_TransferPacketAccess;
	std::vector<PriorityPacket*> m_TransferPackets;
	void FlushSendBuffer(bool FullFlush=false);

public:
	std::list<PriorityPacket*> m_PacketList[MAX_TTL];
	int m_PacketListLength[MAX_TTL];

	//bool m_SendReady;
	byte m_BackBuff[PACKET_BUFF];
	int  m_BackBuffLength;

protected:
	
	// Bandwidth 
	int m_LeafBytesIn;
	int m_LeafBytesOut;


	CGnuNetworks*  m_pNet;
	CGnuPrefs*     m_pPrefs;
	CGnuCore*	   m_pCore;
	CGnuControl*   m_pComm;
	CGnuCache*	   m_pCache;
	CGnuShare*	   m_pShare;
	CGnuTransfers* m_pTrans;
};

class PriorityPacket
{
public:
	
	byte* m_Packet;
	int   m_Length;
	
	int   m_Type;
	int   m_Hops;

	PriorityPacket(byte* packet, int length, int type, int hops)
	{
		m_Packet = new byte[length];
		memcpy(m_Packet, packet, length);
		
		m_Length = length;

		m_Type   = type;
		m_Hops	 = hops;
	};

	~PriorityPacket()
	{
		if(m_Packet)
		{
			delete [] m_Packet;
			m_Packet = NULL;
		}
	};
};

struct MapNode
{
	UINT Age;

	IP		Host;
	WORD    Port;
	DWORD	FileCount;
	DWORD	FileSize;	
	char    Client[4];
};

struct RecentQuery
{
	GUID Guid; 
	int  SecsOld;
	
	RecentQuery();

	RecentQuery(GUID cGuid)
	{
		Guid    = cGuid;
		SecsOld = 0;
	}
};

