#pragma once

enum G2ReadResult { PACKET_GOOD, PACKET_INCOMPLETE, PACKET_ERROR, STREAM_END };

#define MAX_NAME_SIZE 64

struct G2_Header
{
	uint32 ParentPos;

	byte*  Packet;
	uint32 PacketSize;

	char Name[MAX_NAME_SIZE];
	byte NameLen;
	
	bool HasChildren;

	byte*  Internal;
	uint32 InternalSize;

	byte*  Payload;
	uint32 PayloadSize;

	byte*  NextBytePos;
	uint32 NextBytesLeft;

	G2_Header()
	{
		Packet = NULL;
		PacketSize = 0;

		Name[0] = NULL;
		NameLen = 0;

		HasChildren = false;
		
		Internal     = NULL;
		InternalSize = 0;

		Payload     = NULL;
		PayloadSize = 0;

		NextBytePos   = NULL;
		NextBytesLeft = 0;
	};
};

#pragma pack (push, 1)

struct G2_Frame
{
	G2_Frame* Parent;

	uint32  HeaderLength;
	uint32  InternalLength;
	
	char Name[MAX_NAME_SIZE];
	byte NameLen;

	byte LenLen;
	byte Compound;

	byte* NextChild;

	byte* Payload;
	uint32 PayloadLength;
	uint32 PayloadOffset;

	G2_Frame()
	{
		Parent  = NULL;
		HeaderLength = 0;
		InternalLength   = 0;
		Name[0] = NULL;
		NameLen = 0;
		LenLen  = 0;
		Compound = 0;
		NextChild = NULL;
		Payload = 0;
		PayloadLength = 0;
		PayloadOffset = 0;
	};
};

#pragma pack (pop)

#define GLOBAL_HUB_EXPIRE (60*60) // 1 hour

struct G2NodeInfo
{
	// Network Address (NA)
	IPv4 Address;

	// GUID
	GUID NodeID;

	// Vendor Code
	char Vendor[4];
	CString Client;

	// Library Statistics
	uint32 LibraryCount;
	uint32 LibrarySizeKB;

	// Hub Status
	uint16 LeafCount;
	uint16 LeafMax;

	// Node stats
	int    Mode;
	bool   HubAble;
	bool   Firewall;
	bool   Router;

	int    NetBpsIn;   // Bytes per sec
	int    NetBpsOut;  // Bytes per sec
	int	   UdpBpsIn;
	int	   UdpBpsOut;

	int    Cpu;
	int	   Mem;

	uint64 UpSince;
	uint32 ConnectUptime;

	// Timestamps
	uint32 LastSeen;
	
	// Location
	uint16 Latitude;
	uint16 Longitude;

	// Avg Query Packets
	uint16 PacketsQKR[2];
	uint16 PacketsQ2[2];
		
	G2NodeInfo()
	{
		memset(&NodeID, 0, 16);

		memset(Vendor, 0, 4);

		LibraryCount  = 0;
		LibrarySizeKB = 0;

		LeafCount = 0;
		LeafMax   = 0;

		Mode     = 0;
		HubAble  = false;
		Firewall = false;
		Router   = false;

		NetBpsIn  = 0;
		NetBpsOut = 0;
		UdpBpsIn  = 0;
		UdpBpsOut = 0;

		Cpu		 = 0;
		Mem      = 0;

		UpSince	 = 0;
		ConnectUptime = 0;

		LastSeen = 0;

		Latitude  = 0;
		Longitude = 0;

		PacketsQKR[0] = PacketsQKR[1] = 0;
		PacketsQ2[0]  = PacketsQ2[1]  = 0;
	};
};


struct G2_PI // Ping
{
	bool   Relay;
	IPv4   UdpAddress;
	uint32 Ident;
	bool   TestFirewall;

	G2_PI()
	{
		Relay = false;
		Ident = 0;
		TestFirewall = false;
	};
};

struct G2_PO // Pong
{
	bool Relay;

	G2_PO()
	{
		Relay = false;
	};
};

struct G2_LNI // Local Node Info
{
	G2NodeInfo Node;
};

struct G2_KHL // Known Hub List
{
	uint32 RefTime;

	std::vector<G2NodeInfo> Neighbours;
	std::vector<G2NodeInfo> Cached;

	G2_KHL()
	{
		RefTime = 0;
	};
};

struct G2_QHT
{
	bool Reset;
	bool Compressed;
	int  TableSize;
	int  Infinity;
	int  Bits;

	byte PartNum;
	byte PartTotal;
	
	byte* Part;
	int   PartSize;

	G2_QHT()
	{
		Reset      = false;
		Compressed = false;
		TableSize  = 0;
		Infinity   = 0;
		Bits	   = 0;

		PartNum   = 0;
		PartTotal = 0;

		Part     = NULL;
		PartSize = 0;
	};
};

struct G2_QKR
{
	IPv4 RequestingAddress;

	bool dna;

	G2_QKR()
	 {
		dna = false;
	 };
};

struct G2_QKA
{
	uint32 QueryKey;

	IPv4 SendingAddress;
	IPv4 QueriedAddress;

	 G2_QKA()
	 {
		QueryKey = 0;
	 };
};

struct G2_Q2
{
	GUID SearchGuid;

	IPv4   ReturnAddress;
	uint32 QueryKey;

	std::vector<CString> URNs;

	CString DescriptiveName;

	CString Metadata;

	uint64 MinSize;
	uint64 MaxSize;

	std::vector<CString> Interests;

	bool dna;

	G2_Q2()
	{
		QueryKey = 0;

		MinSize  = 0;
		MaxSize  = 0;

		dna = false;
	};
};

struct G2_QA
{
	GUID SearchGuid;

	uint32 Timestamp;

	std::vector<G2NodeInfo> DoneHubs;
	std::vector<G2NodeInfo> AltHubs;

	uint32 RetryAfter;

	IPv4 FromAddress;

	G2_QA()
	{
		Timestamp  = 0;
		RetryAfter = 0;
	};
};

struct G2_QH2_HG
{
	byte   GroupID;
	uint16 QueueLength;
	byte   Capacity;
	uint32 Speed;   // kilobits per sec

	G2_QH2_HG()
	{	
		GroupID     = 0;
		QueueLength = 0;
		Capacity    = 0;
		Speed       = 0;
	};
};

struct G2_QH2_H
{
	byte   GroupID;
	uint32 ObjectID;
	uint32 Index;

	CString DescriptiveName;
	CString Metadata;
	CString URL;

	uint64 ObjectSize;

	std::vector<CString> URNs;

	bool   Partial;
	uint32 PartialSize;

	uint16 CachedSources;

	bool Preview;
	CString PreviewURL;
	
	CString Comment;

	G2_QH2_H()
	{
		GroupID  = 0;
		ObjectID = 0;
		Index    = 0;

		ObjectSize = 0;

		Partial     = false;
		PartialSize = 0;

		CachedSources = 0;

		Preview = false;
	};
};

struct G2_QH2
{
	GUID SearchGuid;

	IPv4 Address;
	GUID NodeID;
	char Vendor[4];
	int  HopCount;
	bool Firewalled;

	std::vector<IPv4> NeighbouringHubs;

	std::vector<G2_QH2_HG> HitGroups;
	std::vector<G2_QH2_H>  Hits;
	CString UnifiedMetadata;

	bool Profile;
	bool Chat;
	CString Nickname;

	G2_QH2()
	{
		memset(Vendor, 0, 4);

		HopCount   = 0;
		Firewalled = false;

		Profile = false;
		Chat    = false;
	};
};

struct G2_PUSH
{
	GUID Destination;
	IPv4 BackAddress;
};

struct G2_MCR
{
	bool Hub;

	G2_MCR()
	{
		Hub = false;
	}
};

struct G2_MCA
{
	bool Hub;
	bool Leaf;
	bool Deny;

	G2_MCA()
	{
		Hub  = false;
		Leaf = false;
		Deny = false;
	}
};

struct G2_PM
{
	uint32 UniqueID;

	IPv4 Destination;
	IPv4 SendingAddress;
	std::vector<IPv4> Neighbours;

	bool Firewall;

	CString Message;

	G2_PM()
	{
		UniqueID = rand() * rand();
		Firewall = false;
	}
};

struct G2_CLOSE
{
	CString Reason;

	std::vector<IPv4> CachedHubs;

	G2_CLOSE()
	{
	}
};

struct GnuNodeInfo
{
	// Network Address (NA)
	IPv4 Address;

	int Mode;

	CString Client;

	// Library Statistics
	uint32 LibraryCount;
	uint32 LibrarySizeKB;

	// Hub Status
	uint16 LeafCount;
	uint16 LeafMax;

	int    NetBpsIn;   // Bytes per sec
	int    NetBpsOut;  // Bytes per sec

	uint64 UpSince;       
	uint32 ConnectUptime;
		
	GnuNodeInfo()
	{
		Mode = 0;

		LibraryCount  = 0;
		LibrarySizeKB = 0;

		LeafCount = 0;
		LeafMax   = 0;

		NetBpsIn  = 0;
		NetBpsOut = 0;

		UpSince = 0;
		ConnectUptime = 0;
	};
};

struct G2_CRAWLR // Crawl Request
{
	bool ReqLeaves;
	bool ReqNames;
	bool ReqGPS;

	bool   ReqG1;
	bool   ReqExt;
	uint32 ReqID;

	G2_CRAWLR()
	{
		ReqLeaves	= false;
		ReqNames	= false;
		ReqGPS		= false;

		ReqG1	= false;
		ReqExt	= false;
		ReqID	= 0;
	};
};

struct G2_CRAWLA // Crawl Ack
{
	G2NodeInfo G2Self;
	std::vector<G2NodeInfo> G2Hubs;
	std::vector<G2NodeInfo> G2Leaves;
	
	GnuNodeInfo GnuSelf;
	std::vector<GnuNodeInfo> GnuUPs;
	std::vector<GnuNodeInfo> GnuLeaves;

	G2_CRAWLR OrigRequest;

	G2_CRAWLA()
	{
	};
};
