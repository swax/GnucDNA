#pragma once

#include "G2Packets.h"
#include "GnuWordHash.h"

/*
Mem Usage

Receive Buffer 64k
Send Buffer    64k

LocalHitTable  128k
RemoteHitTable 128k

Inflate Buffer 16k

Inflate Init	56k
Deflate Init	272k

Node   408k
Comp   332k
Total  740k
*/


#define G2_PACKET_BUFF  (65536+1024)
#define G2_ZSTREAM_BUFF 16384

#define QHT_TIMEOUT       (3*60)
#define LNI_TIMEOUT       60
#define KHL_TIMEOUT_HUB   60
#define KHL_TIMEOUT_CHILD (5*60)


class CGnuCore;
class CGnuNetworks;
class CGnuCache;
class CGnuPrefs;
class CG2Control;
class CG2Protocol;
class CG2Datagram;

class QueuedPacket;

class CG2Node : public CAsyncSocketEx
{
public:
	CG2Node(CG2Control* pG2Comm, CString Host, uint32 Port);
	virtual ~CG2Node();

	// New connections
	void ParseIncomingHandshake(CString, byte*, int);
	void ParseOutboundHandshake(CString, byte*, int);

	void Send_ConnectOK(bool Reply);
	void Send_ConnectError(CString Reason);
	void SetConnected();

	CString FindHeader(CString);
	void	ParseTryHeader(CString TryHeader);
	void    ParseG1TryHeader(CString TryHeader);
	bool    ValidAgent(CString Agent);

	// Receiving
	void FinishReceive(int BuffLength);

	void SplitPackets(byte* stream, uint32 length);


	byte  m_pRecvBuff[G2_PACKET_BUFF];
	int   m_ExtraLength;


	// Sending
	void SendPacket(byte* packet, uint32 length, bool thread=false);
	void SendPacket(QueuedPacket* OutPacket);
	void FlushSendQueue(bool FullFlush=false);

	std::list<QueuedPacket*> m_OutboundPackets;
	
	CCriticalSection m_TransferPacketAccess;
	std::vector<QueuedPacket*> m_TransferPackets;
	

	byte m_SendBuff[G2_PACKET_BUFF];
	int  m_SendBuffLength;

	// Misc
	void Timer();
	
	int m_SecsTrying;
	int m_SecsDead;
	int m_SecsAlive;
	int m_CloseWait;

	// Socket
	int m_G2NodeID;

	IPv4 m_Address;

	bool m_Inbound;
	bool m_TriedUpgrade;

	int m_Status;
	CString m_StatusText;

	int m_NodeMode;

	CTime m_ConnectTime;

	CString m_WholeHandshake;
	CString m_Handshake;
	CString m_lowHandshake;
	CString m_InitData;

	CString m_RemoteAgent;
	uint32  m_RemoteIdent;
	
	// Authentication
	CString m_Challenge;
	CString m_ChallengeAnswer;

	CString m_RemoteChallenge;
	CString m_RemoteChallengeAnswer;

	// Info
	G2NodeInfo m_NodeInfo;
	std::vector<G2NodeInfo> m_HubNeighbors;

	// Delayed packets
	bool m_SendDelayQHT;
	bool m_SendDelayLNI;

	int  m_QHTwait;
	int  m_LNIwait;
	int  m_KHLwait;



	// QHT buffers
	int   m_CurrentPart;
	bool  m_PatchReady;
	int   m_PatchTimeout;

	bool  m_PatchCompressed;
	byte  m_PatchBits;

	byte* m_PatchBuffer;
	int   m_PatchOffset;
	int   m_PatchSize;
	int   m_RemoteTableSize;
	byte  m_RemoteHitTable[G2_TABLE_SIZE];
	
	byte* m_LocalHitTable; // Dynamic, sent child->hub and hub->hub, not hub->child

	// Compression
	bool m_dnapressionOn;
	bool m_InflateRecv;
	bool m_DeflateSend;

	z_stream InflateStream;
	z_stream DeflateStream;

	byte InflateBuff[G2_ZSTREAM_BUFF];

	int  m_DeflateStreamSize;
	int  m_ZipStat;

	
	// Bandwidth, [0] is Received, [1] is Sent, [2] is Dropped
	CRangeAvg m_AvgBytes[3];     
	DWORD     m_dwSecBytes[3];


	// CAsyncSocket Overrides
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual void Close();
	virtual void OnSend(int nErrorCode);

	void Recv_Close(G2_CLOSE &Close);
	void Send_Close(CString Reason);

	void CloseWithReason(CString Reason, bool RemoteClosed=false, bool SendBye=true);


	CG2Control*   m_pG2Comm;
	CG2Datagram*  m_pDispatch;
	CG2Protocol*  m_pProtocol;
	
	CGnuNetworks* m_pNet;
	CGnuCore*     m_pCore;
	CGnuCache*    m_pCache;
	CGnuPrefs*    m_pPrefs;

};


class QueuedPacket
{
public:

	byte* m_Packet;
	int   m_Length;

	QueuedPacket(byte* packet, int length)
	{
		m_Packet = new byte[length];
		memcpy(m_Packet, packet, length);
		
		m_Length = length;
	};

	~QueuedPacket()
	{
		if(m_Packet)
		{
			delete [] m_Packet;
			m_Packet = NULL;
		}
	};	
};
