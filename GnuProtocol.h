#pragma once

#define GGEP_H_SHA1     0x01
#define GGEP_H_BITPRINT 0x02

#include "GnuControl.h"

class CGnuControl;
class CGnuCore;
class CGnuNetworks;
class CGnuShare;
class CGnuPrefs;
class CGnuCache;
class CGnuTransfers;
class CGnuDatagram;

class CGnuProtocol
{
public:
	CGnuProtocol(CGnuControl* pComm);
	~CGnuProtocol();

	void Init();

	// Receiving
	void ReceivePacket(Gnu_RecvdPacket &Packet);

	void Receive_Ping(Gnu_RecvdPacket &Packet);
	void Receive_Pong(Gnu_RecvdPacket &Packet);
	void Receive_Query(Gnu_RecvdPacket &Packet);
	void Receive_QueryHit(Gnu_RecvdPacket &Packet);
	void Receive_Push(Gnu_RecvdPacket &Packet);
	void Receive_VendMsg(Gnu_RecvdPacket &Packet);
	void Receive_Bye(Gnu_RecvdPacket &Packet);
	void Receive_Unknown(Gnu_RecvdPacket &Packet);

	void ReceiveVM_Supported(Gnu_RecvdPacket &Packet);

	void ReceiveVM_TCPConnectBack(Gnu_RecvdPacket &Packet);
	void ReceiveVM_UDPConnectBack(Gnu_RecvdPacket &Packet);
	void ReceiveVM_UDPRelayConnect(Gnu_RecvdPacket &Packet);

	void ReceiveVM_QueryStatusReq(Gnu_RecvdPacket &Packet);
	void ReceiveVM_QueryStatusAck(Gnu_RecvdPacket &Packet);

	void ReceiveVM_QueryHitNum(Gnu_RecvdPacket &Packet);
	void ReceiveVM_QueryHitReq(Gnu_RecvdPacket &Packet);

	void ReceiveVM_PushProxyReq(Gnu_RecvdPacket &Packet);
	void ReceiveVM_PushProxyAck(Gnu_RecvdPacket &Packet);

	void ReceiveVM_NodeStats(Gnu_RecvdPacket &Packet);

	void ReceiveVM_ModeChangeReq(Gnu_RecvdPacket &Packet);
	void ReceiveVM_ModeChangeAck(Gnu_RecvdPacket &Packet);

	void ReceiveVM_CrawlReq(Gnu_RecvdPacket &Packet);

	void PacketError(CString Type, CString Error, byte* packet, int length);

	void WriteCrawlResult(byte* buffer, CGnuNode* pNode, byte Flags);
	
	// Recv QHT
	void Receive_RouteTableReset(Gnu_RecvdPacket &Packet);
	void Receive_RouteTablePatch(Gnu_RecvdPacket &Packet);

	// Sending
	void Send_Ping(CGnuNode* pTCP, int TTL, bool NeedHosts, GUID* pGuid=NULL, IPv4 Target=IPv4());
	void Send_Pong(CGnuNode* pTCP, packet_Pong &Pong);
	void Send_PatchReset(CGnuNode* pTCP);
	void Send_PatchTable(CGnuNode* pTCP);
	void Send_Query(byte* Packet, int length);
	void Send_Query(GnuQuery &FileQuery, std::list<int> &MatchingNodes);
	void Send_QueryHit(GnuQuery &FileQuery, byte* pQueryHit, DWORD ReplyLength, byte ReplyCount, CString &MetaTail);
	void Send_Push(FileSource Download, IPv4 Proxy=IPv4());
	void Send_VendMsg(CGnuNode* pTCP, packet_VendMsg VendMsg, void* payload=NULL, int length=0, IPv4 Target=IPv4());
	void Send_Bye(CGnuNode* pTCP, CString Reason, int ErrorCode);
	void Send_StatsMsg(CGnuNode* pTCP);
	
	// Other
	void Decode_QueryHit( std::vector<FileSource> &Sources, Gnu_RecvdPacket &QHPacket);
	void Encode_QueryHit(GnuQuery &FileQuery, std::list<UINT> &MatchingIndexes, byte* QueryReply);
	
	GGEPReadResult Decode_GGEPBlock(packet_GGEPBlock &Block, byte* &stream, uint32 &length);
	int            Encode_GGEPBlock(packet_GGEPBlock &Block, byte* stream, byte* payload, uint32 length);
	void		   CheckGgepSize(int value);
	void		   TestGgep();

	uint64 DecodeLF(byte* data, int length);
	int    EncodeLF(uint64 filesize, byte* data);
	void   TestLF();
	
	int  EncodeCobs(byte* src, int length, byte* dst);
	int  DecodeCobs(byte* src, int length, byte* dst);
	void TestCobs();

	int	ParsePayload(byte* &pPayload, int &BytesLeft, byte Break, byte* pBuffer, int BufferSize);
	byte m_QueryBuffer[1024];

	byte m_PacketBuffer[1024];

	CGnuControl*   m_pComm;
	CGnuCore*      m_pCore;
	CGnuNetworks*  m_pNet;
	CGnuShare*     m_pShare;
	CGnuPrefs*     m_pPrefs;
	CGnuCache*	   m_pCache;
	CGnuTransfers* m_pTrans;
	CGnuDatagram*  m_pDatagram;
};
