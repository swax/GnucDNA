#pragma once

#include "G2Packets.h"

#define MAX_WRITE_SIZE   32768
#define MAX_FINAL_SIZE   65536
#define MAX_FRAMES        3000   // G2_Frame around 100b
#define FRAME_BUFF_SIZE 524228

class CG2Control;

class CG2Protocol // Instances of class can only be used by one thread
{
public:
	CG2Protocol(CG2Control* pG2Comm);
	~CG2Protocol(void);

	// Reading packets
	G2ReadResult ReadNextPacket( G2_Header &Packet, byte* &stream, uint32 &length );
	G2ReadResult ReadNextChild( G2_Header &Root, G2_Header &Child);
	bool		 ReadPayload(G2_Header &Packet);
	void		 ResetPacket(G2_Header &Packet);

	void ReadNodeInfo(G2_Header Packet, G2NodeInfo &ReadHub);

	// Writing packets
	G2_Frame* WritePacket(G2_Frame* Root, char* Name, void* payload=NULL, uint32 length=0);
	void      WriteFinish();

	int  m_FrameCount;
	int  m_FrameSize;
	byte m_Frames[FRAME_BUFF_SIZE];

	int  m_WriteOffset;
	byte m_WriteData[MAX_WRITE_SIZE];

	byte m_FinalPacket[MAX_FINAL_SIZE];
	int  m_FinalSize;

	// Decoding
	bool Decode_TO(G2_Header PacketTO, GUID &TargetID);

	void Decode_PI(G2_Header PacketPI, G2_PI &Ping);
	void Decode_PO(G2_Header PacketPO, G2_PO &Pong);
	void Decode_LNI(G2_Header PacketLNI, G2_LNI &LocalNodeInfo);
	void Decode_KHL(G2_Header PacketKHL, G2_KHL &KnownHubList);
	void Decode_QHT(G2_Header PacketQHT, G2_QHT &QueryHashTable);
	void Decode_QKR(G2_Header PacketQKR, G2_QKR &QueryKeyRequest);
	void Decode_QKA(G2_Header PacketQKA, G2_QKA &QueryKeyAnswer);
	void Decode_Q2(G2_Header PacketQ2, G2_Q2 &Query);
	void Decode_QA(G2_Header PacketQA, G2_QA &QueryAcknowledgement);
	void Decode_QH2(G2_Header PacketQH2, G2_QH2 &QueryHit);
	void Decode_PUSH(G2_Header PacketPUSH, G2_PUSH &Push);
	void Decode_MCR(G2_Header PacketMCR, G2_MCR &ModeChangeRequest);
	void Decode_MCA(G2_Header PacketMCA, G2_MCA &ModeChangeAck);
	void Decode_PM(G2_Header PacketPM, G2_PM &PrivateMessage);
	void Decode_CLOSE(G2_Header PacketCLOSE, G2_CLOSE &Close);
	void Decode_CRAWLR(G2_Header PacketCRAWLR, G2_CRAWLR &CrawlRequest);
	
	void Decode_URN(std::vector<CString> &URNs, byte* urn, int length);
	
	// Encoding
	void Encode_PI(G2_PI &Ping);
	void Encode_PO(G2_PO &Pong);
	void Encode_LNI(G2_LNI &LocalInfo);
	void Encode_KHL(G2_KHL &KnownHubList);
	void Encode_QHT(G2_QHT &QueryHitTable);
	void Encode_QKR(G2_QKR &QueryKeyRequest);
	void Encode_QKA(G2_QKA &QueryKeyAnswer);
	void Encode_Q2(G2_Q2 &Query);
	void Encode_QA(G2_QA &QueryAck);
	void Encode_QH2(G2_QH2 &QueryHit);
	void Encode_PUSH(G2_PUSH &Push);
	void Encode_MCR(G2_MCR &ModeChangeRequest);
	void Encode_MCA(G2_MCA &ModeChangeAck);
	void Encode_PM(G2_PM &PrivateMessage);
	void Encode_CLOSE(G2_CLOSE &Close);
	void Encode_CRAWLA(G2_CRAWLA &CrawlAck);

	void Encode_G2CrawlInfo(G2_Frame* pNode,  G2NodeInfo &G2Node,   G2_CRAWLA &CrawlAck);
	void Encode_GnuCrawlInfo(G2_Frame* pNode, GnuNodeInfo &GnuNode, G2_CRAWLA &CrawlAck, bool Self);
	
	void Encode_URN(CString strUrn, byte* urn, int &length);

	CG2Control* m_pG2Comm;
};

#pragma pack (push, 1)

struct G2_QHT_Reset
{ 
   byte   command;          // = 0 (reset) 
   uint32 table_size;		// = 2^(size_in_bits) 
   byte   infinity;			// = 1 
};

struct G2_QHT_Patch
{ 
   byte  command;          // = 1 (patch) 
   byte  fragment_no;      // = number of this patch fragment 
   byte  fragment_count;   // = number of patch fragments 
   byte  compression;      // = 0 for none, = 1 for deflate 
   byte  bits;             // = 1 for Gnutella2 
   //(data) 
};

#pragma pack (pop)
