#pragma once

#include "GnuWordHash.h"

// Actual mem 720k

//byte InflateBuff[ZSTREAM_BUFF]; // 16k
//byte DeflateBuff[ZSTREAM_BUFF]; // 16k
//byte m_pBuff[PACKET_BUFF];      // 32k
//byte m_BackBuff[PACKET_BUFF];   // 32k
// inflateInit					  // 56k
// deflateInit					  // 272k

// Total 420k



#include "GnuPackets.h"
#include "GnuShare.h"
#include "GnuControl.h"

#define REQUERY_WAIT 30

#define PACKET_BUFF	 32768
#define ZSTREAM_BUFF 16384
#define PACKETCACHE_SIZE 1000

#define LEAF_THROTTLE_IN  1000
#define LEAF_THROTTLE_OUT 1000

#define PATCH_TIMEOUT  (3*60)
#define PATCH_PART_MAXSIZE 2048

class CGnuNetworks;
class CGnuTransfers;
class CGnuPrefs;
class CGnuCore;
class CGnuCache;
class CGnuShare;
class CGnuControl;
class CGnuProtocol;

class  PriorityPacket;
struct MapNode;
struct RecentQuery;

class CGnuNode : public CAsyncSocket
{
public:
	CGnuNode(CGnuControl* pComm, CString Host, UINT Port);
	virtual ~CGnuNode();


	// Connecting
	void	ParseIncomingHandshake06(CString, byte*, int);
	void	ParseOutboundHandshake06(CString, byte*, int);
	
	void	ParseBrowseHandshakeRequest(CString);
	void	ParseBrowseHandshakeResponse(CString, byte*, int);
	
	CString FindHeader(CString);
	void	ParseTryHeader(CString TryHeader);
	void    ParseHubsHeader(CString HubsHeader);
	
	void Send_ConnectOK(bool);
	void Send_ConnectError(CString Reason);

	void SetConnected();


	// Receiving
	void  FinishReceive(int BuffLength);
	void  SplitBundle(byte*, DWORD);

	byte  m_pBuff[PACKET_BUFF];
	int   m_ExtraLength;


	// Sending packets
	void SendPacket(void* packet, int length, int type, int distance, bool thread=false);
	void SendPacket(PriorityPacket* OutPacket);

	CCriticalSection m_TransferPacketAccess;
	std::vector<PriorityPacket*> m_TransferPackets;
	void FlushSendBuffer(bool FullFlush=false);

	std::list<PriorityPacket*> m_PacketList[MAX_TTL];
	int m_PacketListLength[MAX_TTL];

	byte m_BackBuff[PACKET_BUFF];
	int  m_BackBuffLength;


	// CAsyncSocket Overrides
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual int  Send(const void* lpBuf, int nBufLen, int nFlags = 0);
	virtual void Close();

	void CloseWithReason(CString Reason, bool RemoteClosed=false);


	// Other
	bool GetAlternateHostList(CString &);
	bool GetAlternateSuperList(CString &);

	void  Timer();
	void  NodeManagement();
	void  CompressionStats();

	int  UpdateStats(int type);
	void AddGoodStat(int type);
	void RemovePacket(int);

	bool ValidAgent(CString Agent);

	
	// Node vars
	int m_NodeID;
	int	m_Status;
	
	int	m_SecsTrying;
	int m_SecsAlive;
	int	m_SecsDead;
	int m_CloseWait;

	int m_NextRequeryWait;
	int m_NextStatUpdate;

	// Connection vars
	CString m_StatusText;
	IPv4    m_Address;
	int		m_GnuNodeMode;
	CString m_NetworkName;
	bool    m_Inbound;
	bool	m_ConnectBack;
	CString m_InitData;

	CString m_WholeHandshake;
	CString m_Handshake;
	CString m_lowHandshake;
	CString m_RemoteAgent;

	bool m_SupportsVendorMsg;
	bool m_SupportsLeafGuidance;
	bool m_SupportsDynQuerying;
	bool m_SupportsStats;
	bool m_SupportsModeChange;
	
	IPv4 m_PushProxy;

	int  m_RemoteMaxTTL;
	

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
	CTime     m_ConnectTime;
	UINT	  m_NodeFileCount;
	bool	  m_TriedUpgrade;

	// In stat msg (stats for crawler)
	bool   m_StatsRecvd;
	int    m_LeafMax;
	int	   m_LeafCount;
	uint64 m_UpSince;
	int    m_Cpu;
	int    m_Mem;
	bool   m_UltraAble;
	bool   m_Router;
	bool   m_FirewallTcp;
	bool   m_FirewallUdp;

	// QRP - Recv
	void ApplyPatchTable();
	void SetPatchBit(int &remotePos, double &Factor, byte value);

	UINT	m_CurrentSeq;

	byte* m_PatchBuffer;
	int   m_PatchOffset;
	int   m_PatchSize;
	bool  m_PatchCompressed;
	int   m_PatchBits;

	bool  m_PatchReady;
	int   m_PatchTimeout;

	UINT  m_RemoteTableInfinity;
	int   m_RemoteTableSize;
	byte  m_RemoteHitTable[GNU_TABLE_SIZE];

	// QRP - Sending
	bool m_SendDelayPatch;
	int  m_PatchWait;

	bool  m_SupportInterQRP;
	byte* m_LocalHitTable; // Dynamic, sent leaf->ultrapeer and ultrapeer->ultrapeer, not ultrapeer->leaf

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

	
	// Bandwidth, [0] is Received, [1] is Sent, [2] is Dropped
	CRangeAvg m_AvgPackets[3];   // Packets received/sent in second
	CRangeAvg m_AvgBytes[3];     // Bytes received/sent in second
	
	DWORD m_dwSecPackets[3];
	DWORD m_dwSecBytes[3];

	int m_QueryThrottle;

	CGnuNetworks*  m_pNet;
	CGnuPrefs*     m_pPrefs;
	CGnuCore*	   m_pCore;
	CGnuControl*   m_pComm;
	CGnuCache*	   m_pCache;
	CGnuShare*	   m_pShare;
	CGnuTransfers* m_pTrans;
	CGnuProtocol*  m_pProtocol;
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

