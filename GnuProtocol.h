#pragma once

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

	void PacketError(CString Type, CString Error, byte* packet, int length);

	// Recv QHT
	void Receive_RouteTableReset(Gnu_RecvdPacket &Packet);
	void Receive_RouteTablePatch(Gnu_RecvdPacket &Packet);

	// Sending
	void Send_Ping(CGnuNode* pTCP, int TTL, GUID* pGuid=NULL, IPv4 Target=IPv4());
	void Send_Pong(CGnuNode* pTCP, packet_Pong &Pong);
	void Send_PatchReset(CGnuNode* pTCP);
	void Send_PatchTable(CGnuNode* pTCP);
	void Send_Query(byte* Packet, int length);
	void Send_Query(GnuQuery &FileQuery, std::list<int> &MatchingNodes);
	void Send_QueryHit(GnuQuery &FileQuery, byte* pQueryHit, DWORD ReplyLength, byte ReplyCount, CString &MetaTail);
	void Send_Push(FileSource Download);
	void Send_VendMsg(CGnuNode* pTCP, packet_VendMsg VendMsg, void* payload=NULL, int length=0, IPv4 Target=IPv4());
	void Send_Bye(CGnuNode* pTCP, CString Reason);
	void Send_StatsMsg(CGnuNode* pTCP);
	// Other
	void Decode_QueryHit( std::vector<FileSource> &Sources, Gnu_RecvdPacket &QHPacket);
	void Encode_QueryHit(GnuQuery &FileQuery, std::list<UINT> &MatchingIndexes, byte* QueryReply);
	
	GGEPReadResult Decode_GGEPBlock(packet_GGEPBlock &Block, byte* &stream, uint32 &length);


	CGnuControl*   m_pComm;
	CGnuCore*      m_pCore;
	CGnuNetworks*  m_pNet;
	CGnuShare*     m_pShare;
	CGnuPrefs*     m_pPrefs;
	CGnuCache*	   m_pCache;
	CGnuTransfers* m_pTrans;
	CGnuDatagram*  m_pDatagram;
};
