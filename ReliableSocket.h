#pragma once

#include "GnuControl.h"

#define MAX_WINDOW_SIZE     25
#define RECEIVE_BUFFER_SIZE (MAX_CHUNK_SIZE * MAX_WINDOW_SIZE)
#define SEND_BUFFER_SIZE    (64 * 1024)
#define MAX_CHUNK_SIZE		2048
#define CHUNK_SIZE			512

enum RudpState
{
	RUDP_NONE,
	RUDP_CONNECTING,
	RUDP_CONNECTED,
	RUDP_CLOSED
};

enum CloseReason
{
	NORMAL_CLOSE	 = 0x0,
	YOU_CLOSED		 = 0x1,
	TIMEOUT			 = 0x2,
	LARGE_PACKET	 = 0x3,
	TOO_MANY_RESENDS = 0x4
};

class  CGnuNetworks;
struct RudpConnMsg;
struct RudpSynMsg;
struct RudpAckMsg;
struct RudpDataMsg;
struct RudpFinMsg;
struct RudpSendPacket;

class CReliableSocket : public CAsyncSocketEx
{
public:
	CReliableSocket(CSocketEvents* pEvents);
	~CReliableSocket(void);

	// Socket Functions
	BOOL Attach( SOCKET hSocket);
	BOOL Create();
	BOOL Connect(LPCTSTR lpszHostAddress, UINT nHostPort);
	int Receive(void* lpBuf, int nBufLen);
	int Send(const void* lpBuf, int nBufLen);
	void Close();
	BOOL GetPeerName(CString& rPeerAddress, UINT& rPeerPort);
	int  GetLastError();

	void OnSecond();

	// rudp
	void RudpConnect(IPv4 Address, CGnuNetworks* pNet, bool Listening);
	void RudpReceive(Gnu_RecvdPacket &Packet);
	
	bool m_RudpMode;
	bool m_RudpSendBlock; // use to tell if safe to delete rudp socket without losing data

private:
	// Socket Events
	virtual void OnAccept(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	virtual void OnClose(int nErrorCode);

	CSocketEvents* m_pEventSink;

	// internal functions
	void SetConnected(int nErrorCode);
	void RudpClose(CloseReason code);
	void RudpSend(byte* packet, int length);
	int  FinishReceive(void* lpBuf, int nBufLen);

	// udp receive
	void ReceiveSyn(RudpSynMsg* pSyn);
	void ReceiveAck(RudpAckMsg* pAck);
	bool ReceiveData(RudpDataMsg* pData);
	void ReceiveFin(RudpFinMsg* pFin);

	void ManageRecvWindow();

	byte	m_RemotePeerID;
	uint16	m_HighestSeqRecvd;
	uint16	m_NextSeq;
	byte    m_RecvBuff[RECEIVE_BUFFER_SIZE];
	int     m_RecvBuffLength;
	std::map<uint16, std::pair<byte*,int> > m_RecvPacketMap;
	
	time_t m_LastRecv;
	time_t m_LastSend;

	std::map<uint16, bool> m_AckMap;
	std::list<uint16>      m_AckOrder;

	// udp send
	void SendSyn();
	void SendAck(RudpConnMsg* pConn);
	void SendKeepAlive();
	void SendFin(CloseReason code);

	void ManageSendWindow();

	byte m_PeerID;
	int  m_CurrentSeq;
	std::map<uint16, RudpSendPacket*> m_SendPacketMap;

	CCriticalSection m_SendSection;
	byte m_SendBuff[SEND_BUFFER_SIZE];
	int  m_SendBuffLength;
	int  m_SendWindowSize;

	CMovingAvg m_AvgLatency;

	//DWORD m_RTT;
	//bool  m_CalcRTT;
	
	int   m_InOrderAcks;
	int   m_ReTransmits;

	CMovingAvg m_AvgBytesSent;

	// other udp
	void InitHeader(packet_Header* pHeader);

	void Log(CString Entry);

	bool      m_Listening;
	RudpState m_State;
	time_t    m_ConnectTimeout;
	int		  m_LastError;

	bool m_SynAckReceieved;
	bool m_SynAckSent;

	IPv4 m_Address;

	CGnuNetworks* m_pNet;
};


#define MAX_GUID_DATA 12

enum RudpPacketType
{
	OP_SYN       = 0x0,
	OP_ACK       = 0x1,
	OP_KEEPALIVE = 0x2,
	OP_DATA      = 0x3,
	OP_FIN       = 0x4
};


#pragma pack (push, 1)

struct RudpConnMsg
{
	byte   PeerConnID;
	byte   DataSize   : 4;
	byte   OpCode     : 4;
	uint16 SeqNum;
};

struct RudpSynMsg : RudpConnMsg
{
	byte   ConnID;
	uint16 Version;
};

struct RudpAckMsg : RudpConnMsg
{
	uint16 WindowStart;
	uint16 WindowSpace;
};

struct RudpDataMsg : RudpConnMsg
{
	byte Data[MAX_GUID_DATA];
};

struct RudpFinMsg : RudpConnMsg
{
	byte ReasonCode;
};

struct RudpSendPacket
{
	byte*  Packet;
	int    Size;
	bool   Acked;
	uint32 TimeSent;
	int    Retries;

	RudpSendPacket(int size)
	{
		Packet   = new byte[size];
		Size     = size;
		Acked    = false;
		TimeSent = 0;
		Retries  = 0;
	}

	~RudpSendPacket()
	{
		delete [] Packet;
		Packet = NULL;
	}
};

#pragma pack (pop)


