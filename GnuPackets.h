#pragma once

#pragma pack (push, 1)

struct packet_Header;
struct packet_Ping;
struct packet_Pong;
struct packet_Push;
struct packet_Query;
struct packet_QueryHit;
struct packet_Bye;

struct packet_QueryHitItem;
struct packet_QueryHitEx;

struct packet_RouteTableReset;
struct packet_RouteTablePatch;

struct packet_Log;

struct packet_Header		// Size 23
{
	GUID  Guid;					// 0  - 15							
	BYTE  Function;				// 16
	BYTE  TTL;					// 17
	BYTE  Hops;					// 18							
	UINT  Payload;				// 19 - 22
};


struct packet_Ping			// Size 23
{
	packet_Header Header;		// 0  - 22	
};


struct packet_Pong			// Size 37
{
	packet_Header Header;		// 0  - 22							
	WORD Port;					// 23 - 24
	IP Host;					// 25 - 28
	UINT FileCount;				// 29 - 32
	UINT FileSize;				// 33 - 36
};


struct packet_Push			// Size 49
{
	packet_Header Header;		// 0  - 22;							
	GUID ServerID;				// 23 - 38
	UINT Index;				    // 39 - 42
	IP Host;					// 43 - 46
	WORD Port;					// 47 - 48
};

struct packet_QueryFlags	    // Size 1
{
	byte None		: 2;
	byte OobHits	: 1;
	byte GGEP_H		: 1;
	byte Guidance	: 1;
	byte XML		: 1;
	byte Firewalled	: 1;
	byte Set		: 1;
	
	packet_QueryFlags()
	{
		None		= 0;
		OobHits		= 0;
		GGEP_H		= 0;
		Guidance	= 0;
		XML			= 0;
		Firewalled	= 0;
		Set			= 0;
	};
};

struct packet_Query			// Size 26+
{		
	packet_Header Header;		// 0  - 22						
	packet_QueryFlags Flags;	// 23	
	byte Reserved;				// 24 
	// Search					// 25+
};

struct packet_QueryHit		// Size 35+
{
	packet_Header Header;		// 0  - 22
	byte TotalHits;				// 23
	WORD Port;					// 24 - 25
	IP   Host;					// 26 - 29
	UINT Speed;					// 30 - 33
	// QueryHitItems			// 34+
	
	// QueryHit Descriptor

	// ClientGuid				// Last 16 bytes
};

struct packet_Bye				// 23+
{
	packet_Header Header;		// 0 - 22
	WORD Code;					// 23 - 24

	// Reason					// 25+
};

#define BYE_MANUAL   201
#define BYE_REMOTE   400
#define BYE_TIMEOUT  405
#define BYE_INTERNAL 500

struct packet_QueryHitItem	// Size 9+
{
	UINT Index;					// 0  - 3
	UINT Size;					// 4  - 7
	
	// FileName					// 8+	
};

struct packet_QueryHitEx	    // Size 6+
{
	byte VendorID[4];			// 0  - 3
	byte Length;				// 4

	// Public Sector
	byte Push		 : 1; // 5
	byte FlagBad 	 : 1;
	byte FlagBusy	 : 1;
	byte FlagStable  : 1;
	byte FlagSpeed	 : 1;
	byte FlagGGEP    : 1;
	byte FlagTrash   : 2;

	byte FlagPush	 : 1; // 6
	byte Bad		 : 1;
	byte Busy		 : 1;
	byte Stable  	 : 1;
	byte Speed		 : 1;
	byte GGEP		 : 1;
	byte Trash		 : 2;

	WORD MetaSize;

	// Private Sector

};

struct packet_RouteTableReset	// Size 29
{
	packet_Header Header;		// 0  - 22

	byte PacketType;			// 23
	UINT TableLength;			// 24 - 27
	byte Infinity;				// 28
};

struct packet_RouteTablePatch	// Size 29+
{
	packet_Header Header;		// 0  - 22

	byte PacketType;			// 23
	byte SeqNum;				// 24
	byte SeqSize;				// 25

	byte Compression;		    // 26
	byte EntryBits;				// 27

	// Patch Table...			// 28+
};

struct packet_VendIdent // Size 8
{
	char VendorID[4];			// 0 - 3
	uint16 Type;			    // 4 - 5
	uint16 Version;				// 6 - 7

	packet_VendIdent()
	{ };

	packet_VendIdent(char vendor[4], uint16 type, uint16 version)
	{
		memcpy(VendorID, vendor, 4);
		Type    = type;
		Version = version;
	};

	bool operator == (packet_VendIdent &Compare)
	{
		if( memcmp(Compare.VendorID, VendorID, 4) == 0 && 
			Compare.Type    == Type  && 
			Compare.Version == Version  )
			return true;

		return false;
	};
};

struct packet_VendMsg // Size 31+
{
	packet_Header Header;		// 0  - 22

	packet_VendIdent Ident;	// 23 - 30

	// Message Payload...
};

struct packet_StatsMsg	// 13
{
	uint16 LeafMax;				// 0 - 1
	uint16 LeafCount;			// 2 - 3
	uint32 Uptime;				// 4 - 7
	uint16 Cpu;					// 8 - 9
	uint16 Mem;					// 10 - 11
	
	byte FlagUltraAble	 : 1;	// 12
	byte FlagRouter		 : 1;   
	byte FlagTcpFirewall : 1;
	byte FlagUdpFirewall : 1;
	byte FlagEmpty		 : 4;

	packet_StatsMsg()
	{
		LeafMax		= 0;
		LeafCount	= 0;
		Uptime		= 0;
		Cpu		= 0;
		Mem		= 0;
	
		FlagUltraAble	= 0;
		FlagRouter		= 0;
		FlagTcpFirewall = 0;
		FlagUdpFirewall = 0;
		FlagEmpty		= 0;
	};	
};

struct packet_GGEPHeaderFlags
{
	byte NameLength	 : 4;
	byte Reserved	 : 1;
	byte Compression : 1;
	byte Encoding	 : 1;
	byte Last		 : 1;

	packet_GGEPHeaderFlags()
	{
		NameLength	= 0;
		Reserved	= 0;
		Compression = 0;
		Encoding	= 0;
		Last		= 0;
	}
};


enum GGEPReadResult { BLOCK_GOOD, BLOCK_INCOMPLETE, BLOCK_ERROR };

struct packet_GGEPBlock
{
	char Name[15];

	bool Compression;
	bool Encoded;
	bool Last;

	byte* Payload;
	int   PayloadSize;

	bool Cleanup;

	packet_GGEPBlock()
	{
		memset(Name, 0, 15);

		Compression = false;
		Encoded		= false;
		Last		= false;

		Payload     = NULL;
		PayloadSize = 0;

		Cleanup = false;
	};

	~packet_GGEPBlock()
	{
		if(Cleanup)
			delete [] Payload;
	}
};

#pragma pack (pop)

#define CRAWL_UPTIME	0x1 // total connection time to node in minutes
#define CRAWL_LOCAL		0x2 // local pref info
#define CRAWL_NEW		0x4 // return only nodes that support udp pong
#define CRAWL_AGENT		0x8 // return zipped useragen list with self at end


