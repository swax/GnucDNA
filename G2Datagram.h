#pragma once

// Need bound because udp doesnt detect threshhold
// Too high can mean many dropped packets
#define UDP_HUB_LIMIT   (10 * 1024) 
#define UDP_CHILD_LIMIT ( 4 * 1024)

#define GND_PACKET_BUFF 65536

#define GND_MTU 500
#define GND_SEND_RETRY   10
#define GND_SEND_TIMEOUT 26
#define GND_RECV_TIMEOUT 30

#define ACK_LENGTH 8

#define FLUSH_PERIOD 10 

class CG2Control;
class CG2Protocol;

struct GND_Header;
struct GND_Packet;
struct GND_Fragment;
struct GND_Ack;

class CG2Datagram
{
public:
	CG2Datagram(CG2Control* pG2Comm);
	virtual ~CG2Datagram();
	void Timer();

	// Receiving
	virtual void OnReceive(IPv4 Address, byte* pRecvBuff, int RecvLength);
	void Decode_GND(IPv4 Address, GND_Header* packet, int length);
	void ProcessACK(GND_Header* Packet);

	byte m_pRecvPacketBuff[GND_PACKET_BUFF];
	byte m_pSendPacketBuff[GND_PACKET_BUFF];

	std::list<GND_Packet*> m_RecvCache;


	// Sending
	void DispatchACK(IPv4 Address, GND_Header* Packet);
	void SendPacket(IPv4 Address, byte* packet, uint32 length, bool Thread=false, bool ReqAck=true, bool Priority=false);
	void SendPacket(GND_Packet* OutPacket);
	void FlushSendBuffer();

	std::list<GND_Ack>     m_AckCache;
	std::list<GND_Packet*> m_SendCache; // Packets sit in here waiting to be acked

	

	CCriticalSection m_TransferPacketAccess;
	std::vector<GND_Packet*> m_TransferPackets;

	std::map<uint16, GND_Packet*> m_SequenceMap;
	uint16 m_NextSequence;

	int m_SendBytesAvail;
	int m_FlushCounter; // Cross between flushing immediately and flushing at a timer interval

	// Bandwidth
	CRangeAvg m_AvgUdpDown;
	CRangeAvg m_AvgUdpUp;

	int m_UdpSecBytesDown;
	int m_UdpSecBytesUp;

	CG2Control*  m_pG2Comm;
	CG2Protocol* m_pProtocol;
};


#pragma pack (push, 1)

struct GND_Header
{ 
   char   Tag[3]; 
   byte   Flags; 
   uint16 Sequence; 
   byte   Part; 
   byte   Count; 
};

#pragma pack (pop)

struct GND_Fragment
{
	int Part;

	byte* Data;
	int   Length;

	bool Sent;
	bool Acked;
	int  Wait;

	GND_Fragment( int part, byte* orgData=NULL, int length=0)
	{
		Part = part;

		Sent  = false;
		Acked = false;
		Wait  = 0;

		if(orgData && length)
		{
			Data = new byte[length];
			memcpy(Data, orgData, length);
			Length = length;
		}
		else
		{
			Data   = NULL;
			Length = 0;
		}
	};

	~GND_Fragment()
	{
		if( Data )
			delete [] Data;
	};
};

struct GND_Packet
{
	IPv4   Address;
	uint16 Sequence;
	
	bool Compressed;
	bool Priority;

	byte FragemntCount;
	std::list<GND_Fragment*> Fragments;

	bool Processed;
	bool AckRequired;
	int  Timeout;

	GND_Packet(IPv4 address, uint16 seq, int count, bool deflate, bool priority=false)
	{
		Address.Host = address.Host;
		Address.Port = address.Port;

		Sequence = seq;
	
		Compressed = deflate;
		Priority   = priority;

		FragemntCount = count;

		Processed   = false;
		AckRequired = false;
		Timeout     = 0;
	};

	~GND_Packet()
	{
		std::list<GND_Fragment*>::iterator itFrag;
		for(itFrag = Fragments.begin(); itFrag != Fragments.end(); itFrag++)
			delete *itFrag;

		Fragments.clear();
	};
};

struct GND_Ack
{
	IPv4       Address;
	GND_Header Packet;

	GND_Ack(IPv4 address)
	{
		Address.Host = address.Host;
		Address.Port = address.Port;
	};
};

