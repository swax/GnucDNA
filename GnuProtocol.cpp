/********************************************************************************

	GnucDNA - The Gnucleus Library
    Copyright (C) 2000-2004 John Marshall Group

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


	For support, questions, comments, etc...
	E-Mail: swabby@c0re.net

********************************************************************************/
#include "StdAfx.h"

#include "GnuCore.h"
#include "GnuNode.h"
#include "GnuNetworks.h"
#include "GnuShare.h"
#include "GnuPrefs.h"
#include "GnuCache.h"
#include "GnuSearch.h"
#include "GnuWordHash.h"
#include "GnuMeta.h"
#include "GnuSchema.h"
#include "GnuTransfers.h"
#include "GnuDatagram.h"

#include "DnaCore.h"
#include "DnaEvents.h"

#include "GnuProtocol.h"

CGnuProtocol::CGnuProtocol(CGnuControl* pComm)
{
	m_pComm  = pComm;
}

CGnuProtocol::~CGnuProtocol()
{
	
}

void CGnuProtocol::Init()
{
	m_pCore		= m_pComm->m_pCore;
	m_pNet		= m_pComm->m_pNet;
	m_pShare	= m_pComm->m_pShare;
	m_pPrefs	= m_pComm->m_pPrefs;
	m_pCache	= m_pComm->m_pCache;
	m_pTrans	= m_pComm->m_pTrans;
	m_pDatagram = m_pComm->m_pDatagram;
}

void CGnuProtocol::ReceivePacket(Gnu_RecvdPacket &Packet)
{
	if(m_pCore->m_dnaCore->m_dnaEvents)	
		m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketIncoming(NETWORK_GNUTELLA, Packet.pTCP != 0, Packet.Source.Host.S_addr, Packet.Source.Port, (byte*) Packet.Header, Packet.Length, false, ERROR_NONE );


	switch(Packet.Header->Function)
	{
	case 0x00:
		Receive_Ping(Packet);
		break;

	case 0x01:
		Receive_Pong(Packet);
		break;

	case 0x30:
		if( ((packet_RouteTableReset*) Packet.Header)->PacketType == 0x0)
			Receive_RouteTableReset(Packet);
		else if(((packet_RouteTableReset*) Packet.Header)->PacketType == 0x1)
			Receive_RouteTablePatch(Packet);

		break;

	case 0x31:
		Receive_VendMsg(Packet);
		break;
	case 0x32:
		Receive_VendMsg(Packet);
		break;

	case 0x40:
		Receive_Push(Packet);
		break;

	case 0x80:
		Receive_Query(Packet);
		break;

	case 0x81:
		Receive_QueryHit(Packet);
		break;

	case 0x02:
		Receive_Bye(Packet);
		break;

	default:
		// Disable unknowns
		// Receive_Unknown(Packet);
		break;
	}
}
void CGnuProtocol::PacketError(CString Type, CString Error, byte* packet, int length)
{
	//m_pCore->DebugLog("Gnutella", Type + " Packet " + Error + " Error");
}

void CGnuProtocol::Receive_Ping(Gnu_RecvdPacket &Packet)
{
	packet_Ping* Ping = (packet_Ping*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
	{
		// If received udp from not-local connection, full udp support

		bool Local = false;
		for(int i = 0; i < m_pComm->m_NodeList.size(); i++)
			if(m_pComm->m_NodeList[i]->m_Address.Host.S_addr == Packet.Source.Host.S_addr)
				Local = true;

		if( !Local )
		{
			std::map<uint32, bool>::iterator itHost = m_pNet->m_NatDetectMap.find(Packet.Source.Host.S_addr);
			if(itHost == m_pNet->m_NatDetectMap.end())
				m_pNet->m_UdpFirewall = UDP_FULL;	
		}

		return;
	}

	// Packet stats
	pNode->UpdateStats(Ping->Header.Function);

	// Ping not useful anymore except for keep alives
	if(Ping->Header.Hops != 0)
	{
		PacketError("Ping", "Hops", (byte*) Ping, Packet.Length);
		return;
	}

	// Build a pong
	packet_Pong Pong;
	Pong.Header.Guid		= Ping->Header.Guid;
	Pong.Port				= m_pNet->m_CurrentPort;
	Pong.Host				= m_pNet->m_CurrentIP;
	Pong.FileCount			= m_pShare->m_TotalLocalFiles;

	if(m_pPrefs->m_ForcedHost.S_addr)
		Pong.Host = m_pPrefs->m_ForcedHost;

	// If we are an ultrapeer, the size field is used as a marker send that info
	if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		Pong.FileSize = m_pShare->m_UltrapeerSizeMarker;
	else
		Pong.FileSize = m_pShare->m_TotalLocalSize;

	Send_Pong(pNode, Pong);
}


void CGnuProtocol::Receive_Pong(Gnu_RecvdPacket &Packet)
{
	packet_Pong* Pong = (packet_Pong*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
		return;

	if(Pong->Header.Payload < 14)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Pong, Length " + NumtoStr(Pong->Header.Payload));
		return;
	}

	// Packet stats
	pNode->UpdateStats(Pong->Header.Function);

	if(Pong->Header.Hops != 0)
	{
		PacketError("Pong", "Hops", (byte*) Pong, Packet.Length);
		return;
	}

	// Detect if this pong is from an ultrapeer
	bool Ultranode = false;

	UINT Marker = 8;
	while(Marker <= Pong->FileSize && Marker)
		if(Marker == Pong->FileSize)
		{
			Ultranode = true;
			break;
		}
		else
			Marker *= 2;

	// Add to host cache
	if(Ultranode)
		m_pCache->AddKnown( Node(IPtoStr(Pong->Host), Pong->Port ) );
	
	// Pong for us
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(Pong->Header.Guid);
	if(LocalRouteID == 0)
	{
		// Nodes file count
		ASSERT(Pong->Header.Hops == 0);
		pNode->m_NodeFileCount = Pong->FileCount;

		pNode->AddGoodStat(Pong->Header.Function);
		
		return;
	}
	else
	{
		PacketError("Pong", "Routing", (byte*) Pong, Packet.Length);	
		return;
	}  
}

void CGnuProtocol::Receive_Query(Gnu_RecvdPacket &Packet)
{
	packet_Query* Query = (packet_Query*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
		return;

	int SourceNodeID = pNode->m_NodeID;

	if(Query->Header.Payload < 2)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Query, Length " + NumtoStr(Query->Header.Payload));
		return;
	}
	
	// Packet stats
	pNode->UpdateStats(Query->Header.Function);


	// Inspect
	int QuerySize  = Query->Header.Payload - 2;
	int TextSize   = strlen((char*) Query + 25) + 1;

	// Bad packet, text bigger than payload
	if (TextSize > QuerySize)
	{
		PacketError("Query", "Routing", (byte*) Query, Packet.Length);
		//TRACE0("Text Query too big " + CString((char*) Query + 25) + "\n");
		return;
	}
	
	int RouteID		 = m_pComm->m_TableRouting.FindValue(Query->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(Query->Header.Guid);

	if(LocalRouteID != -1)
	{
		PacketError("Query", "Loopback", (byte*) Query, Packet.Length);
		return;
	}

	// Fresh Query?
	if(RouteID == -1)
	{
		m_pComm->m_TableRouting.Insert(Query->Header.Guid, SourceNodeID);

		pNode->AddGoodStat(Query->Header.Function);

		if(*((char*) Query + 25) == '\\')
			return;

		// Client in Ultrapeer Mode
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			Query->Header.Hops++;
			
			if(Query->Header.TTL > MAX_TTL) 
				Query->Header.TTL = MAX_TTL;   

			// Received from Leaf
			if( pNode->m_GnuNodeMode == GNU_LEAF )
			{
				// Keep TTL the Same from leaf to UP to dyn query list
				m_pComm->AddDynQuery( new DynQuery(SourceNodeID, (byte*) Query, Packet.Length) );
				
		
				// Change ttl after its added to dyn query list
				// So query initially sent to other chilren and immediate UPs based on QRTs
				Query->Header.TTL = 1;
			}

			// Received from Ultrapeer
			if( pNode->m_GnuNodeMode == GNU_ULTRAPEER)
			{
				if(Query->Header.TTL != 0)
					Query->Header.TTL--;

				pNode->m_QueryThrottle += Packet.Length;

				if(pNode->m_QueryThrottle > 1024)
					TRACE0("Query Throttle Tripped at " + NumtoStr(pNode->m_QueryThrottle) + " by " + pNode->m_RemoteAgent + "\n");

				if(Query->Header.TTL > 1 && pNode->m_QueryThrottle < 1024) // Throttle broadcasting of incoming queries at 1kb/s
					for(int i = 0; i < m_pComm->m_NodeList.size(); i++)	
					{
						CGnuNode *p = m_pComm->m_NodeList[i];

						if(p->m_Status == SOCK_CONNECTED && p->m_GnuNodeMode == GNU_ULTRAPEER && p != pNode)
							p->SendPacket(Query, Packet.Length, PACKET_QUERY, Query->Header.Hops);
					}				
			}
		}

		// Test too see routed last hop queries are correct
/*#ifdef _DEBUG
		if(Query->Header.TTL == 0 && pNode->m_SupportInterQRP)
		{
			CString Text((char*) Query + 25);
			TRACE0("INTER-QRP:" + Text + "\n");
		}
#endif*/

		// Queue to be compared with local files
		GnuQuery G1Query;
		G1Query.Network    = NETWORK_GNUTELLA;
		G1Query.OriginID   = SourceNodeID;
		G1Query.SearchGuid = Query->Header.Guid;

		if(Query->Flags.Set && Query->Flags.OobHits && Query->Header.Hops > 1)
		{
			// Host requests UDP response, get address from Guid
			memcpy(&G1Query.DirectAddress.Host, &Query->Header.Guid, 4);
			memcpy(&G1Query.DirectAddress.Port, ((byte*) &Query->Header.Guid) + 13, 2);
		}

		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			G1Query.Forward = true;

			if(Query->Header.TTL == 1)
				G1Query.UltraForward = true;

			memcpy(G1Query.Packet, (byte*) Query, Packet.Length);
			G1Query.PacketSize = Packet.Length;
		}

		G1Query.Terms.push_back( CString((char*) Query + 25, TextSize) );

		
		CString ExtendedQuery;

		if (TextSize < QuerySize)
		{
			int ExtendedSize = strlen((char*) Query + 25 + TextSize);
		
			if(ExtendedSize)
			{
				ExtendedQuery = CString((char*) Query + 25 + TextSize, ExtendedSize);
		
				/*int WholeSize = TextSize + HugeSize + 1;
				
				TRACE0("Huge Query, " + NumtoStr(WholeSize) + " bytes\n");
				TRACE0("     " + CString((char*)Query + 25 + TextSize) + "\n");

				if(WholeSize > QuerySize)
					TRACE0("   Huge Query too big " + CString((char*) Query + 25 + TextSize) + "\n");

				if(WholeSize < QuerySize)
				{
					TRACE0("   Huge Query too small " + CString((char*) Query + 25 + TextSize) + "\n");

					byte* j = 0;
					for(int i = WholeSize; i < QuerySize; i++)
						j = (byte*) Query + 25 + i;
				}*/
			}
			else
			{
				// Query with double nulls, wtf
			}
		}

		while(!ExtendedQuery.IsEmpty())
			G1Query.Terms.push_back( ParseString(ExtendedQuery, 0x1C) );


		m_pShare->m_QueueAccess.Lock();
			m_pShare->m_PendingQueries.push_front(G1Query);	
		m_pShare->m_QueueAccess.Unlock();


		m_pShare->m_TriggerThread.SetEvent();
		
		return;
	}
	else
	{
		if(RouteID == SourceNodeID)
		{
			PacketError("Query", "Duplicate", (byte*) Query, Packet.Length);
			return;
		}
		else
		{
			PacketError("Query", "Routing", (byte*) Query, Packet.Length);
			return;
		}
	}
}

void CGnuProtocol::Receive_QueryHit(Gnu_RecvdPacket &Packet)
{
	packet_QueryHit* QueryHit = (packet_QueryHit*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	
	int SourceNodeID = 0;
	if(pNode)
		SourceNodeID = pNode->m_NodeID;


	if(QueryHit->Header.Payload < 27)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Query Hit, Length " + NumtoStr(QueryHit->Header.Payload));
		return;
	}

	// Packet stats
	if(pNode)
		pNode->UpdateStats(QueryHit->Header.Function);

	// Host Cache
	m_pCache->AddKnown( Node(IPtoStr(QueryHit->Host), QueryHit->Port) );
	
	
	// Check if OOB guid used to send dyn queries for leaves
	std::map<uint32, GUID>::iterator itGuid = m_pComm->m_OobtoRealGuid.find( HashGuid(QueryHit->Header.Guid) );
	if(itGuid != m_pComm->m_OobtoRealGuid.end())
		QueryHit->Header.Guid = itGuid->second;


	// Look up route info for queryhit
	int RouteID		 = m_pComm->m_TableRouting.FindValue(QueryHit->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(QueryHit->Header.Guid);

	if(pNode && pNode->m_BrowseID)
		LocalRouteID = 0;


	// Queryhit for us, or Queryhit coming in from same path we sent one out
	if(LocalRouteID == 0 || LocalRouteID == SourceNodeID)
	{
		// Check for query hits we sent out
		if(Packet.pTCP && LocalRouteID == SourceNodeID)
		{
			PacketError("QueryHit", "Loopback", (byte*) QueryHit, Packet.Length);
			return;
		}

		else
		{	
			if(pNode)
				pNode->AddGoodStat(QueryHit->Header.Function);

			std::vector<FileSource> Sources;
			Decode_QueryHit(Sources, Packet);

			// Send to searches / downloads
			for(int i = 0; i < Sources.size(); i++)
				m_pNet->IncomingSource(QueryHit->Header.Guid, Sources[i]);
			
		
			// Update dyn query
			std::map<uint32, DynQuery*>::iterator itDyn = m_pComm->m_DynamicQueries.find( HashGuid(QueryHit->Header.Guid) );
			if( itDyn != m_pComm->m_DynamicQueries.end() )
				itDyn->second->Hits += QueryHit->TotalHits;

			return;
		}
	}

	if(RouteID != -1)
	{	
		if(m_pComm->m_GnuClientMode != GNU_ULTRAPEER) // only ultrapeers can route
			return;

		// Add ClientID of packet to push table
		if(m_pComm->m_TablePush.FindValue( *((GUID*) ((byte*)QueryHit + (Packet.Length - 16)))) == -1)
			m_pComm->m_TablePush.Insert( *((GUID*) ((byte*)QueryHit + (Packet.Length - 16))) , SourceNodeID);
		
		QueryHit->Header.Hops++;
		if(QueryHit->Header.TTL != 0)
			QueryHit->Header.TTL--;

		// Route to another ultrapeer
		if(QueryHit->Header.TTL > 0)
		{
			std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(RouteID);
			if(itNode != m_pComm->m_NodeIDMap.end() && itNode->second->m_Status == SOCK_CONNECTED)
				itNode->second->SendPacket(QueryHit, Packet.Length, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, false);
		}

		// Send if meant for child
		std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(RouteID);
		if(itNode != m_pComm->m_NodeIDMap.end())
			if(itNode->second->m_Status == SOCK_CONNECTED && itNode->second->m_GnuNodeMode == GNU_LEAF)
			{	
				if(QueryHit->Header.TTL == 0)
					QueryHit->Header.TTL++;

				std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(RouteID);
				if(itNode != m_pComm->m_NodeIDMap.end() && itNode->second->m_Status == SOCK_CONNECTED)
					itNode->second->SendPacket(QueryHit, Packet.Length, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, false);

				// Update dyn query
				std::map<uint32, DynQuery*>::iterator itDyn = m_pComm->m_DynamicQueries.find( HashGuid(QueryHit->Header.Guid) );
				if( itDyn != m_pComm->m_DynamicQueries.end() )
					itDyn->second->Hits += QueryHit->TotalHits;
			}

		if(pNode)
			pNode->AddGoodStat(QueryHit->Header.Function);

		return;
	}
	else
	{
		PacketError("QueryHit", "Routing", (byte*) QueryHit, Packet.Length);
		return;
	}  
}

void CGnuProtocol::Decode_QueryHit( std::vector<FileSource> &Sources, Gnu_RecvdPacket &QHPacket)
{
	byte* Packet = (byte*) QHPacket.Header;

	packet_QueryHit* QueryHit = (packet_QueryHit*) QHPacket.Header;
	
	CGnuNode* pNode = QHPacket.pTCP;
	
	int SourceNodeID = 0;
	if(pNode)
		SourceNodeID = pNode->m_NodeID;

	bool ExtendedPacket = false;
	bool Firewall		= false;
	bool Busy			= false;
	bool Stable			= false;
	bool ActualSpeed	= false;

	std::vector<IPv4> DirectUltrapeers;

	int   pos = 0;
	int   i   = 0;
	int   HitsLeft    = QueryHit->TotalHits;
	UINT  NextPos     = 34;
	UINT  Length      = QueryHit->Header.Payload + 23;
	int   ClientIDPos = Length - 16;
	
	// Find start of QHD
	int ItemCount = 0;
	for(i = 42; i < ClientIDPos; i++)
		if(Packet[i] == 0)
		{
			while(Packet[++i] != 0)
				if(i > Length - 16)
					break;

			ItemCount++;
		
			if(ItemCount != QueryHit->TotalHits)
				i += 9;
			else
				break;
		}


	// i should either now be at front of ClientID or QHD
	CString Vendor;
	std::map<int, int>     MetaIDMap;
	std::map<int, CString> MetaValueMap;
	
	if(i < ClientIDPos)
	{
		ExtendedPacket = true;
		
		packet_QueryHitEx* QHD = (packet_QueryHitEx*) &Packet[i + 1];
	
		Vendor = CString((char*) QHD->VendorID, 4);		

		if(QHD->Length == 1)
			if(QHD->Push == 1)
				Firewall = true;

		bool EmbeddedGGEP = false;
		
		if(QHD->Length > 1)
		{
			if(QHD->FlagPush)
				Firewall = QHD->Push;

			if(QHD->FlagBusy)
				Busy = QHD->Busy;
			
			if(QHD->FlagStable)
				Stable = QHD->Stable;

			if(QHD->FlagSpeed)
				ActualSpeed = QHD->Speed;

			if(QHD->FlagGGEP && QHD->GGEP)
				EmbeddedGGEP = true;
		}

		int QHDSize = 4 + 1 + QHD->Length; // Vendor, Length, Payload
		int NextPos = i + 1 + QHDSize;

		// Check for XML Metadata
		if(QHD->Length == 4 && QHD->MetaSize > 1)
			if(QHD->MetaSize <= ClientIDPos - NextPos)
			{
				CString MetaLoad = CString((char*) &Packet[ClientIDPos - QHD->MetaSize], QHD->MetaSize);

				// Decompress, returns pure xml response
				if(m_pShare->m_pMeta->DecompressMeta(MetaLoad, (byte*) &Packet[ClientIDPos - QHD->MetaSize], QHD->MetaSize))
					m_pShare->m_pMeta->ParseMeta(MetaLoad, MetaIDMap, MetaValueMap);
			}

		if(QHD->Length != 4)
			QHD->MetaSize = 0;
		
		// Get GGEP block
		int BlockSpace = ClientIDPos - QHD->MetaSize - NextPos;

		if(EmbeddedGGEP && BlockSpace > 0)
		{
			byte*  NextBlock   = &Packet[NextPos];
			uint32 BlockLength = BlockSpace;

			// Clients still using private area of QHD so search for first occurance of magic byte
			while(NextBlock[0] != 0xC3 && BlockLength > 0)
			{
				NextBlock   += 1;
				BlockLength -= 1;
			}

			if(NextBlock[0] == 0xC3 && BlockLength > 0)
			{
				NextBlock   += 1;
				BlockLength -= 1;

				GGEPReadResult Status = BLOCK_GOOD;
				
				while(Status == BLOCK_GOOD)
				{
					packet_GGEPBlock Block;
					Status = Decode_GGEPBlock(Block, NextBlock, BlockLength);
					
					// Push Proxies
					if( strcmp(Block.Name, "PUSH") == 0 && Block.PayloadSize % 6 == 0)
					{
						for(int i = 0; i < Block.PayloadSize; i += 6)
						{
							IPv4 Proxy;
							memcpy(&Proxy, Block.Payload + i, 6);
							DirectUltrapeers.push_back(Proxy);
						}	
					}

					if(Block.Last)
						break;
				}
			}
		}
	}

	// Extract results from the packet
	while(HitsLeft > 0 && NextPos < ClientIDPos)
	{	
		FileSource Item;
		
		memcpy(&Item.FileIndex, &Packet[NextPos], 4);
		memcpy(&Item.Size, &Packet[NextPos + 4], 4);

		Item.Address.Host = QueryHit->Host;
		Item.Address.Port = QueryHit->Port;
		Item.Network	  = NETWORK_GNUTELLA;
		Item.Speed        = QueryHit->Speed;

		Item.Firewall	 = Firewall;
		Item.Busy		 = Busy;
		Item.Stable		 = Stable;
		Item.ActualSpeed = ActualSpeed;
		
		if(ExtendedPacket)
			Item.Vendor = GetVendor( Vendor );

		// Push Proxy off for now
		//for(int i = 0; i < DirectUltrapeers.size(); i++)
		//	Item.DirectHubs.push_back( DirectUltrapeers[i] );

		Item.GnuRouteID = SourceNodeID;
		memcpy(&Item.PushID, &Packet[ClientIDPos], 16);
		Item.Distance = QueryHit->Header.Hops;


		// Get Filename
		Item.Name = (char*) &Packet[NextPos + 8];
		
		Item.NameLower = Item.Name;
		Item.NameLower.MakeLower();
		//Item.Icon = m_pDoc->GetIconIndex(Item.NameLower);
		
		pos = NextPos + 8 + Item.Name.GetLength();


		// Add extracted metadata from end of packet
		Item.MetaID = 0;
		std::map<int, int>::iterator itMetaID = MetaIDMap.find(QueryHit->TotalHits - HitsLeft);

		if(itMetaID != MetaIDMap.end())
		{
			Item.MetaID = itMetaID->second;

			std::map<int, CGnuSchema*>::iterator itSchema = m_pShare->m_pMeta->m_MetaIDMap.find(Item.MetaID);
			std::map<int, CString>::iterator itMetaValue = MetaValueMap.find(QueryHit->TotalHits - HitsLeft);
			
			if(itSchema != m_pShare->m_pMeta->m_MetaIDMap.end() && itMetaValue != MetaValueMap.end())
			{
				itSchema->second->SetResultAttributes(Item.AttributeMap, itMetaValue->second);

				itMetaValue->second.Replace("&apos;", "'");
				Item.GnuExtraInfo.push_back(itMetaValue->second);
			}
		}


		// Get Extended file info
		if(Packet + pos + 1 != NULL)
		{
			// Check for GGEP block

			CString ExInfo = (char*) (Packet + pos + 1);
			int ExLength = ExInfo.GetLength();

			while(!ExInfo.IsEmpty())
			{
				CString SubExInfo = ParseString(ExInfo, 0x1C);

				Item.GnuExtraInfo.push_back(SubExInfo);
			}

			pos += ExLength + 1;
		}

		// Add Hash info
		for(int i = 0; i < Item.GnuExtraInfo.size(); i++)
		{
			// Sha1
			int hashpos = Item.GnuExtraInfo[i].Find("urn:sha1:");

			if(hashpos != -1 && Item.GnuExtraInfo[i].GetLength() == 9 + 32)
				Item.Sha1Hash = Item.GnuExtraInfo[i].Right(32);

			// Bitprint
			hashpos = Item.GnuExtraInfo[i].Find("urn:bitprint:");

			if(hashpos != -1 && Item.GnuExtraInfo[i].GetLength() == 13 + 32 + 1 + 39)
			{
				Item.Sha1Hash  = Item.GnuExtraInfo[i].Mid(13, 32);
				Item.TigerHash = Item.GnuExtraInfo[i].Right(39);
			}
		}
		
		// Add to source list
		Sources.push_back(Item);


		// Check for end of reply packet
		if(pos + 1 >= Length - 16)
			HitsLeft = 0;
		else
		{
			HitsLeft--;
			NextPos = pos + 1;
		}
	}
}

void CGnuProtocol::Receive_Push(Gnu_RecvdPacket &Packet)
{
	packet_Push* Push = (packet_Push*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
		return;

	if(Push->Header.Payload < 26)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Push, Length " + NumtoStr(Push->Header.Payload));
		return;
	}

	// Packet stats
	pNode->UpdateStats(Push->Header.Function);
	
	// Host Cache
	m_pCache->AddKnown( Node(IPtoStr(Push->Host), Push->Port) );


	// Find packet in hash tables
	int RouteID		 = m_pComm->m_TableRouting.FindValue(Push->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(Push->Header.Guid);
	int PushRouteID  = m_pComm->m_TablePush.FindValue(Push->ServerID);

	if(LocalRouteID != -1)
	{
		PacketError("Push", "Loopback", (byte*) Push, Packet.Length);
		return;
	}

	int i = 0;

	// Check ServerID of Push with ClientID of the client
	if(m_pPrefs->m_ClientID == Push->ServerID)
	{
		GnuPush G1Push;
		G1Push.Network		= NETWORK_GNUTELLA;
		G1Push.Address.Host = Push->Host;
		G1Push.Address.Port	= Push->Port;
		G1Push.FileID		= Push->Index;

		m_pTrans->DoPush( G1Push );

		pNode->AddGoodStat(Push->Header.Function);
		
		return;
	}

	if(RouteID == -1)
	{
		m_pComm->m_TableRouting.Insert(Push->Header.Guid, pNode->m_NodeID);
	}
	else
	{
		if(RouteID == pNode->m_NodeID)
		{
			PacketError("Push", "Duplicate", (byte*) Push, Packet.Length);
			return;
		}
		else
		{
			PacketError("Push", "Routing", (byte*) Push, Packet.Length);
			return;
		}
	}

	if(PushRouteID != -1)
	{	
		Push->Header.Hops++;
		if(Push->Header.TTL != 0)
			Push->Header.TTL--;

		if(Push->Header.TTL > 0)
		{
			std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(PushRouteID);
			if(itNode != m_pComm->m_NodeIDMap.end() && itNode->second->m_Status == SOCK_CONNECTED)
				itNode->second->SendPacket(Push, Packet.Length, PACKET_PUSH, Push->Header.TTL - 1);
		}
		
		pNode->AddGoodStat(Push->Header.Function);
		
		return;	
	}
	else
	{
		PacketError("Push", "Routing", (byte*) Push, Packet.Length);
		return;
	}
}

void CGnuProtocol::Receive_VendMsg(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	

	if(VendMsg->Header.Payload < 8)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad VendMsg, Length " + NumtoStr(VendMsg->Header.Payload));
		return;
	}

	// Packet received TCP
	if(Packet.pTCP)
	{
		// Check if a 'Messages Supported' message
		if(VendMsg->Ident == packet_VendIdent("\0\0\0\0", 0, 0) )
		{
			byte* message   = ((byte*) VendMsg) + 31;
			int	  sublength = Packet.Length - 31;

			uint16 VectorSize = 0;
			if(sublength >= 2)
				memcpy(&VectorSize, message, 2);

			if( sublength == 2 + VectorSize * 8)
			{
				// VMS Gnucleus 12.33.43.13: BEAR/11v1 
				//TRACE0("VMS " + pNode->m_RemoteAgent + " " + IPtoStr(pNode->m_Address.Host) + ": ");

				for(int i = 2; i < sublength; i += 8)
				{
					packet_VendIdent* MsgSupported = (packet_VendIdent*) (message + i);

					CString Msg =  CString(MsgSupported->VendorID, 4) + "/" + NumtoStr(MsgSupported->Type) + "v" + NumtoStr(MsgSupported->Version);
					
					if(*MsgSupported == packet_VendIdent("BEAR", 11, 1))
						pNode->m_SupportsLeafGuidance = true;

					if(*MsgSupported == packet_VendIdent("GNUC", 60, 1))
						pNode->m_SupportsStats = true;

					if(*MsgSupported == packet_VendIdent("GNUC", 61, 1))
						pNode->m_SupportsModeChange = true;

					//TRACE0(Msg + " ");
				}

				//TRACE0("\n");
			}
			else
				m_pCore->DebugLog("Gnutella", "Vendor Msg, Messages Support, VS:" + NumtoStr(VectorSize) + ", SL:" + NumtoStr(sublength));
				
		}

		// TCP Connect Back
		if(VendMsg->Ident == packet_VendIdent("BEAR", 7, 1) )
		{
			if(Packet.Length - 31 != 2)
				return;

			uint16 ConnectPort = 0;
			memcpy(&ConnectPort, ((byte*) VendMsg) + 31, 2);

			CGnuNode* ConnectNode = new CGnuNode(m_pComm, IPtoStr(pNode->m_Address.Host), ConnectPort);
			ConnectNode->m_ConnectBack = true;

			if( !ConnectNode->Create() )
			{
				delete ConnectNode;
				return;
			}
			
			if( !ConnectNode->Connect(IPtoStr(pNode->m_Address.Host), ConnectPort) )
				if (ConnectNode->GetLastError() != WSAEWOULDBLOCK)
				{
					delete ConnectNode;
					return;
				}
			
			m_pComm->m_NodeAccess.Lock();
				m_pComm->m_NodeList.push_back(ConnectNode);
			m_pComm->m_NodeAccess.Unlock();
		}
	
		// UDP Connect Back
		if(VendMsg->Ident == packet_VendIdent("GTKG", 7, 2))
		{
			if(Packet.Length - 31 != 2)
				return;

			uint16 ConnectPort = 0;
			memcpy(&ConnectPort, ((byte*) VendMsg) + 31, 2);

			IPv4 Target;
			Target.Host = pNode->m_Address.Host;
			Target.Port = ConnectPort;
			Send_Ping(NULL, 1, &VendMsg->Header.Guid, Target);		
		}

		// UDP Relay Connect Back
		if( VendMsg->Ident == packet_VendIdent("GNUC", 7, 1))
		{
			if(Packet.Length - 31 != 6)
				return;

			// A UDP reply needs to be indirect to verify full udp support

			// Relay request to 10 leaves
			if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
			{
				int Relays = 0;
				for(int i = 0; i < m_pComm->m_NodeList.size() && Relays < 10; i++)
					if(m_pComm->m_NodeList[i] != pNode && m_pComm->m_NodeList[i]->m_Status == SOCK_CONNECTED && m_pComm->m_NodeList[i]->m_GnuNodeMode == GNU_LEAF)
					{
						m_pComm->m_NodeList[i]->SendPacket(VendMsg, Packet.Length, PACKET_VENDMSG, 1);
						Relays++;
					}
			}
			
			// Respond udp
			if(m_pComm->m_GnuClientMode == GNU_LEAF)
			{
				IPv4 Target;
				memcpy(&Target, ((byte*) VendMsg) + 31, 6);

				Send_Ping(NULL, 1, &VendMsg->Header.Guid, Target);
			}
		}

		// Leaf Guided Dyanic Query: Query Status Request
		if(VendMsg->Ident == packet_VendIdent("BEAR", 11, 1) )
		{
			if(m_pComm->m_GnuClientMode != GNU_LEAF)
			{
				m_pCore->DebugLog("Gnutella", "Query Status Request Received from " + pNode->m_RemoteAgent + " while in Ultrapeer Mode");
				return;
			}

			// Find right search by using the GUID
			for(int i = 0; i < m_pNet->m_SearchList.size(); i++)
				if(VendMsg->Header.Guid == m_pNet->m_SearchList[i]->m_QueryID)
				{
					TRACE0("VMS " + pNode->m_RemoteAgent + " " + IPtoStr(pNode->m_Address.Host) + ": Query Status Request");
					CGnuSearch* pSearch = m_pNet->m_SearchList[i];
					
					packet_VendMsg ReplyMsg;
					ReplyMsg.Header.Guid = VendMsg->Header.Guid;
					ReplyMsg.Ident = packet_VendIdent("BEAR", 12, 1);
					
					uint16 Hits = pSearch->m_WholeList.size();

					Send_VendMsg(pNode, ReplyMsg, &Hits, 2 );
				}
		}

		// Leaf Guided Dyanic Query: Query Status Response
		if(VendMsg->Ident == packet_VendIdent("BEAR", 12, 1) )
		{
			byte* message   = ((byte*) VendMsg) + 31;
			int	  sublength = Packet.Length - 31;

			if(sublength != 2)
				return;

			uint16 HitCount = 0;
			memcpy(&HitCount, message, 2);

			std::map<uint32, DynQuery*>::iterator itDyn = m_pComm->m_DynamicQueries.find( HashGuid(VendMsg->Header.Guid) );
			if( itDyn != m_pComm->m_DynamicQueries.end() )
			{
				// End Query if 0xFFFF received
				if(HitCount == 0xFFFF)
				{
					delete itDyn->second;
					m_pComm->m_DynamicQueries.erase(itDyn);
				}

				// Otherwise update hit count if its greater than what we've seen
				else if(HitCount > itDyn->second->Hits)
					itDyn->second->Hits = HitCount;
			}
		}

		// Push Proxy Acknowledgement
		if(VendMsg->Ident == packet_VendIdent("LIME", 22, 2) )
		{
			byte* message   = ((byte*) VendMsg) + 31;
			int	  sublength = Packet.Length - 31;

			if(sublength != 6)
				return;

			memcpy(&pNode->m_PushProxy, message, 6);
		}

		// Node Stats
		if(VendMsg->Ident == packet_VendIdent("GNUC", 60, 1) )
		{
			byte* message   = ((byte*) VendMsg) + 31;
			int	  sublength = Packet.Length - 31;

			if(sublength < 13)
				return;

			packet_StatsMsg StatsMsg;
			memcpy(&StatsMsg, message, 13);

			// Values sometimes mess up in debugger for StatsMsg
			// Once assigned everything looks right

			Packet.pTCP->m_StatsRecvd  = true;
			Packet.pTCP->m_LeafMax	   = StatsMsg.LeafMax;
			Packet.pTCP->m_LeafCount   = StatsMsg.LeafCount;
			Packet.pTCP->m_UpSince	   = time(NULL) - StatsMsg.Uptime;
			Packet.pTCP->m_Cpu		   = StatsMsg.Cpu;
			Packet.pTCP->m_Mem		   = StatsMsg.Mem;
			Packet.pTCP->m_UltraAble   = StatsMsg.FlagUltraAble;
			Packet.pTCP->m_Router	   = StatsMsg.FlagRouter;
			Packet.pTCP->m_FirewallTcp = StatsMsg.FlagTcpFirewall;
			Packet.pTCP->m_FirewallUdp = StatsMsg.FlagUdpFirewall;
		}

		// Mode Change Request
		if(VendMsg->Ident == packet_VendIdent("GNUC", 61, 1) )
		{
			byte* message   = ((byte*) VendMsg) + 31;
			int	  sublength = Packet.Length - 31;

			if(sublength < 1)
				return;

			byte Request = 0;
			memcpy(&Request, message, 1);

			// Upgrade Request
			if(Request == 0x01)
			{
				packet_VendMsg AckMsg;
				GnuCreateGuid(&AckMsg.Header.Guid);
				AckMsg.Ident = packet_VendIdent("GNUC", 62, 1);
				
				if( Packet.pTCP->m_GnuNodeMode != GNU_ULTRAPEER || // Make sure remote is ultrapeer
					m_pComm->m_GnuClientMode != GNU_LEAF ||		   // Make sure we are a leaf
					!m_pComm->UltrapeerAble())					   // Make sure we are ultrapeer able
				{
					byte NoAck = 0x02;
					Send_VendMsg(Packet.pTCP, AckMsg, &NoAck, 1);
					return;
				}
			
				byte YesAck = 0x01;
				Send_VendMsg(Packet.pTCP, AckMsg, &YesAck, 1);
					
				m_pComm->SwitchGnuClientMode(GNU_ULTRAPEER);
				return;
			}
		}

		// Mode Change Ack
		if(VendMsg->Ident == packet_VendIdent("GNUC", 62, 1) )
		{
			byte* message   = ((byte*) VendMsg) + 31;
			int	  sublength = Packet.Length - 31;

			if(sublength < 1)
				return;

			byte Ack = 0;
			memcpy(&Ack, message, 1);

			// If we sent node a request and Ack signals a yes
			if(Packet.pTCP->m_TriedUpgrade && Ack == 0x01)
			{
				m_pComm->m_NextUpgrade = time(NULL) + 40*60; // Node upgrade do not upgrade another 40 mins
				Packet.pTCP->CloseWithReason("Leaf Upgraded");

				// Reset 'tried' children so next time they're eligible
				for(int i = 0; i < m_pComm->m_NodeList.size(); i++)
					m_pComm->m_NodeList[i]->m_TriedUpgrade = false;
			}
		}
	}

	// Packet received UDP
	else
	{
		// Out of band - Reply Num
		if(VendMsg->Ident == packet_VendIdent("LIME", 12, 1) )
		{
			bool SendAck = false;

			// Check if query active
			for(int i = 0; i < m_pNet->m_SearchList.size(); i++)
				if(VendMsg->Header.Guid == m_pNet->m_SearchList[i]->m_QueryID)
				{
					SendAck = true;
					break;
				}

			// Check if OOB guid used to send dyn queries for leaves
			std::map<uint32, GUID>::iterator itGuid = m_pComm->m_OobtoRealGuid.find( HashGuid(VendMsg->Header.Guid) );
			if(itGuid != m_pComm->m_OobtoRealGuid.end())
				SendAck = true;


			if(SendAck)
			{
				packet_VendMsg Ack;
				Ack.Header.Guid = VendMsg->Header.Guid;
				Ack.Ident = packet_VendIdent("LIME", 11, 2);
				
				if(Packet.Length - 31 != 1)
					return;

				byte Results = 0;
				memcpy(&Results, ((byte*) VendMsg) + 31, 1);

				Send_VendMsg(NULL, Ack, &Results, 1, Packet.Source );
			}
		}

		// Out of band - Reply Ack
		if(VendMsg->Ident == packet_VendIdent("LIME", 11, 2) )
		{
			m_pComm->m_OobHitsLock.Lock();

			// Lookup structure that has all the hits for this host
			std::map<uint32, OobHit*>::iterator itHit = m_pComm->m_OobHits.find( HashGuid(VendMsg->Header.Guid) );
			if(itHit != m_pComm->m_OobHits.end())
			{
				OobHit* pHit = itHit->second;

				if(Packet.Length - 31 != 1)
				{
					m_pComm->m_OobHitsLock.Unlock();
					return;
				}

				byte RequestNum = 0;
				memcpy(&RequestNum, ((byte*) VendMsg) + 31, 1);

				int HitsSent = 0;

				// Send hits over UDP and only the amount the host asked for
				for(int i = 0; i < pHit->QueryHits.size(); i++)
					if(RequestNum == 255 || HitsSent < RequestNum)
					{
						m_pDatagram->SendPacket(pHit->Target, pHit->QueryHits[i], pHit->QueryHitLengths[i]);

						HitsSent += ((packet_QueryHit*) pHit->QueryHits[i])->TotalHits;
					}
					else
						break;

				delete pHit;
				m_pComm->m_OobHits.erase(itHit);
			}

			m_pComm->m_OobHitsLock.Unlock();
		}
	}
}

void CGnuProtocol::Receive_Bye(Gnu_RecvdPacket &Packet)
{
	packet_Bye* Bye = (packet_Bye*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
		return;

	byte* ByeData = (byte*) Bye;

	pNode->CloseWithReason( CString( (char*) &ByeData[23]), true );
}

void CGnuProtocol::Receive_Unknown(Gnu_RecvdPacket &Packet)
{
}

void CGnuProtocol::Receive_RouteTableReset(Gnu_RecvdPacket &Packet)
{	
	packet_RouteTableReset* TableReset = (packet_RouteTableReset*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
		return;

	if(TableReset->Header.Payload < 6)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Table Reset, Length " + NumtoStr(TableReset->Header.Payload));
		return;
	}

	if(m_pComm->m_GnuClientMode != GNU_ULTRAPEER)	   		 
	{
		m_pCore->DebugLog("Gnutella", "Table Reset Received while in Leaf Mode");
		return;
	}

	if(TableReset->Header.Hops > 0)
	{
		m_pCore->DebugLog("Gnutella", "Table Reset Hops > 0");
		return;
	}

	pNode->m_RemoteTableInfinity = TableReset->Infinity;
	pNode->m_RemoteTableSize     = TableReset->TableLength / 8;
	memset( pNode->m_RemoteHitTable, 0xFF, GNU_TABLE_SIZE );

	pNode->m_CurrentSeq = 1;
}

void CGnuProtocol::Receive_RouteTablePatch(Gnu_RecvdPacket &Packet)
{
	packet_RouteTablePatch* TablePatch = (packet_RouteTablePatch*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
		return;

	if(TablePatch->Header.Payload < 5)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Table Patch, Length " + NumtoStr(TablePatch->Header.Payload));
		return;
	}

	if(m_pComm->m_GnuClientMode != GNU_ULTRAPEER)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Received while in Leaf Mode");
		return;
	}

	if(TablePatch->Header.Hops > 0)
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Hops > 0");
		return;
	}

	if( TablePatch->SeqNum == 0 || TablePatch->SeqNum > TablePatch->SeqSize || pNode->m_CurrentSeq != TablePatch->SeqNum)
	{
		pNode->CloseWithReason("Table Patch Sequence Error");
		return;
	}

	if(TablePatch->EntryBits != 4 && TablePatch->EntryBits != 8)
	{
		pNode->CloseWithReason("Table Patch Invalid Entry Bits");
		return;
	}

	// Make sure table length and infinity have been set
	if(pNode->m_RemoteTableSize == 0 || pNode->m_RemoteTableInfinity == 0)
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Received Before Reset");
		return;
	}

	// If first patch in sequence, reset table
	if(TablePatch->SeqNum == 1)
	{
		if(pNode->m_PatchBuffer)
			delete [] pNode->m_PatchBuffer;

		pNode->m_PatchSize    = pNode->m_RemoteTableSize * TablePatch->EntryBits;
		pNode->m_PatchBuffer  = new byte[pNode->m_PatchSize];
		pNode->m_PatchOffset  = 0;
	}
	
	// Check patch not received out of sync and buff not created
	if(pNode->m_PatchBuffer == NULL)
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Received Out of Sync");
		return;
	}


	if(TablePatch->SeqNum <= TablePatch->SeqSize)
	{
		int PartSize = TablePatch->Header.Payload - 5;

		// As patches come in, build buffer of data
		if(pNode->m_PatchOffset + PartSize <= pNode->m_PatchSize)
		{
			memcpy(pNode->m_PatchBuffer + pNode->m_PatchOffset, (byte*) TablePatch + 28, PartSize);
			pNode->m_PatchOffset += PartSize;
		}
		else
		{
			pNode->CloseWithReason("Patch Exceeded Specified Size");
			m_pCore->DebugLog("Gnutella", "Table Patch Too Large");
		}
	}

	// Final patch received
	if(TablePatch->SeqNum == TablePatch->SeqSize)
	{
		if(TablePatch->Compression == 0x1)
			pNode->m_PatchCompressed = true;

		pNode->m_PatchBits = TablePatch->EntryBits;

		pNode->m_PatchReady = true;
	}
	else
		pNode->m_CurrentSeq++;
	
}

////////////////////////////////////////////////////////////////////////

void CGnuProtocol::Send_Ping(CGnuNode* pTCP, int TTL, GUID* pGuid, IPv4 Target)
{
	GUID Guid;

	if(pGuid)
		memcpy(&Guid, pGuid, 16);
	else
		GnuCreateGuid(&Guid);

	packet_Ping Ping;
	
	Ping.Header.Guid	 = Guid;
	Ping.Header.Function = 0;
	Ping.Header.Hops	 = 0;
	Ping.Header.TTL		 = TTL;
	Ping.Header.Payload  = 0;

	
	m_pComm->m_TableLocal.Insert(Guid, 0);
	if(pTCP)
		pTCP->SendPacket(&Ping, 23, PACKET_PING, Ping.Header.Hops);
	else
		m_pDatagram->SendPacket(Target, (byte*) &Ping, 23);

}

void CGnuProtocol::Send_Pong(CGnuNode* pTCP, packet_Pong &Pong)
{
	Pong.Header.Function	= 0x01;
	Pong.Header.TTL			= 1;
	Pong.Header.Hops		= 0;
	Pong.Header.Payload		= 14;

	pTCP->SendPacket(&Pong, 37, PACKET_PONG, Pong.Header.TTL - 1);
}

void CGnuProtocol::Send_PatchReset(CGnuNode* pTCP)
{
	GUID Guid;
	GnuCreateGuid(&Guid);

	// Build the packet
	packet_RouteTableReset Reset;

	Reset.Header.Guid		= Guid;
	Reset.Header.Function	= 0x30;
	Reset.Header.TTL		= 1;
	Reset.Header.Hops		= 0;
	Reset.Header.Payload	= 6;

	Reset.PacketType	= 0x0;
	Reset.TableLength	= 1 << GNU_TABLE_BITS;
	Reset.Infinity		= TABLE_INFINITY;

	pTCP->SendPacket(&Reset, 29, PACKET_QUERY, Reset.Header.Hops);
}

void CGnuProtocol::Send_PatchTable(CGnuNode* pTCP)
{
	byte PatchTable[GNU_TABLE_SIZE];

	// Get local table
	memcpy(PatchTable, m_pShare->m_pWordTable->m_GnutellaHitTable, GNU_TABLE_SIZE);
	

	// Sending inter-ultrapeer qrp table
	if( m_pComm->m_GnuClientMode == GNU_ULTRAPEER )
	{
		// Build aggregate table of leaves
		for(int i = 0; i < m_pComm->m_NodeList.size(); i++)
			if(m_pComm->m_NodeList[i]->m_Status == SOCK_CONNECTED && m_pComm->m_NodeList[i]->m_GnuNodeMode == GNU_LEAF)
			{
				for(int k = 0; k < GNU_TABLE_SIZE; k++)
					PatchTable[k] &= m_pComm->m_NodeList[i]->m_RemoteHitTable[k];
			}
	}

	// Create local table if not created yet (needed to save qht info if needed to send again)
	if( pTCP->m_LocalHitTable == NULL)
	{
		pTCP->m_LocalHitTable = new byte [GNU_TABLE_SIZE];
		memset( pTCP->m_LocalHitTable,  0xFF, GNU_TABLE_SIZE );
	}

	// create 4 bit patch table to send to remote host
	byte* FourBitPatch = new byte [GNU_TABLE_SIZE * 4];
	memset(FourBitPatch, 0, GNU_TABLE_SIZE * 4);

	int pos = 0;
	for(int i = 0; i < GNU_TABLE_SIZE; i++)
	{
		// Find what changed and build a 4 bit patch table for it
		for(byte mask = 1; mask != 0; mask *= 2)
		{ 
			
			// No change
			if( (PatchTable[i] & mask) == (pTCP->m_LocalHitTable[i] & mask) )
			{
				
			}
			// Patch turning on ( set negetive value)
			else if( (PatchTable[i] & mask) == 0 && (pTCP->m_LocalHitTable[i] & mask) > 0)
			{
				if(pos % 2 == 0) 
					//FourBitPatch[pos / 2] = 15 << 4; // high -1
					FourBitPatch[pos / 2] = 10 << 4; // high -6 works with LW
				else
					//FourBitPatch[pos / 2] |= 15;  // low -1
					FourBitPatch[pos / 2] = 10 << 4; // low -6 works with LW
			}
			// Patch turning off ( set positive value)
			else if( (PatchTable[i] & mask) > 0 && (pTCP->m_LocalHitTable[i] & mask) == 0)
			{
				if(pos % 2 == 0)
					//FourBitPatch[pos / 2] = 1 << 4;// high 1
					FourBitPatch[pos / 2] = 6 << 4;// high 6 works with LW
				else
					//FourBitPatch[pos / 2] |= 1;  // low 1
					FourBitPatch[pos / 2] = 6 << 4;// low 6 works with LW
			}
			
			pos++;
		}

		pTCP->m_LocalHitTable[i] = PatchTable[i];
	}

	// Compress patch table
	DWORD CompSize	= GNU_TABLE_SIZE * 4 * 1.2;
	byte* CompBuff	= new byte[CompSize];
	
	if(compress(CompBuff, &CompSize, FourBitPatch, GNU_TABLE_SIZE * 4) != Z_OK)
	{
		delete [] FourBitPatch;
		FourBitPatch = NULL;

		m_pCore->LogError("Patch Compression Error");
		return;
	}

	delete [] FourBitPatch;
	FourBitPatch = NULL;

	 // Determine how many 2048 byte packets to send 
	int SeqSize = (CompSize + (PATCH_PART_MAXSIZE - 1)) >> 11; 
	
	int CopyPos  = 0;
	int CopySize = 0;

	byte* RawPacket = new byte[PATCH_PART_MAXSIZE + 28];
	packet_RouteTablePatch* PatchPacket = (packet_RouteTablePatch*) RawPacket;	

	byte* PacketBuff   = new byte[GNU_TABLE_SIZE * 4 + 896]; // Used so everything is sent in the correct order enough space for packet headers
	UINT  NextPos	   = 0;

	for(int SeqNum = 1; SeqNum <= SeqSize; SeqNum++)
	{
		if(CompSize - CopyPos < PATCH_PART_MAXSIZE)
			CopySize = CompSize - CopyPos;
		else
			CopySize = PATCH_PART_MAXSIZE;

		// Build packet
		GUID Guid;
		GnuCreateGuid(&Guid);

		PatchPacket->Header.Guid		= Guid;
		PatchPacket->Header.Function	= 0x30;
		PatchPacket->Header.TTL			= 1;
		PatchPacket->Header.Hops		= 0;
		PatchPacket->Header.Payload		= 5 + CopySize;
		
		PatchPacket->PacketType = 0x1;
		PatchPacket->SeqNum		= SeqNum;
		PatchPacket->SeqSize	= SeqSize;

		PatchPacket->Compression = 0x1;
		PatchPacket->EntryBits	 = 4;

		memcpy(RawPacket + 28, CompBuff + CopyPos, CopySize);
		CopyPos += PATCH_PART_MAXSIZE;
	
		memcpy(PacketBuff + NextPos, RawPacket, 28 + CopySize);
		NextPos += 28 + CopySize;
	}

	// This mega packet includes the reset and all patches
	pTCP->SendPacket(PacketBuff, NextPos, PACKET_QUERY, PatchPacket->Header.Hops);

	delete [] PacketBuff;
	delete [] CompBuff;
	delete [] RawPacket;
}

void CGnuProtocol::Send_Query(byte* Packet, int length)
{
	packet_Query* Query = (packet_Query*) Packet;
	
	//Query->Header.Guid	= // Already added before this is called
	Query->Header.Function	= 0x80;
	Query->Header.Hops		= 0;
	Query->Header.TTL		= MAX_TTL;
	Query->Header.Payload	= length - 23;


	// New MinSpeed Field
	Query->Reserved = 0;	
	
	if(m_pNet->m_UdpFirewall == UDP_FULL) 
		Query->Flags.OobHits = true;   
	
	//Query->Flags.GGEP_H = true;
	Query->Flags.Guidance =	true;
	Query->Flags.XML	  = true;	 
		
	if(m_pNet->m_TcpFirewall) 
		Query->Flags.Firewalled = true;

	Query->Flags.Set = true;		


	m_pComm->m_TableLocal.Insert(Query->Header.Guid, 0);

	// If leaf mode, send to all ultrapeers
	if(m_pComm->m_GnuClientMode == GNU_LEAF)
		for(int i = 0; i < m_pComm->m_NodeList.size(); i++)	
		{
			CGnuNode *p = m_pComm->m_NodeList[i];
		
			if(p->m_Status == SOCK_CONNECTED)
			{
				if(p->m_RemoteMaxTTL)
					Query->Header.TTL = p->m_RemoteMaxTTL;
				
				p->SendPacket(Packet, length, PACKET_QUERY, Query->Header.Hops);
			}
		}

	// If in utlrapeer mode do dynamic query
	if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
	{
		m_pComm->AddDynQuery( new DynQuery(0, Packet, length) );

		// Send immediately to all connected hosts based on QHT			
		GnuQuery G1Query;
		G1Query.Network    = NETWORK_GNUTELLA;
		G1Query.OriginID   = 0;
		G1Query.SearchGuid = Query->Header.Guid;

		// Forward to connected ultrapeers
		G1Query.Forward = true;
		memcpy(G1Query.Packet, Packet, length);
		G1Query.PacketSize = length;

		// Add text in query to search terms
		int TextSize   = strlen((char*) Query + 25) + 1;
		G1Query.Terms.push_back( CString((char*) Query + 25, TextSize) );

		CString ExtendedQuery;
		int ExtendedSize = strlen((char*) Query + 25 + TextSize);
		if(ExtendedSize)
			ExtendedQuery = CString((char*) Query + 25 + TextSize, ExtendedSize);
		
		while(!ExtendedQuery.IsEmpty())
			G1Query.Terms.push_back( ParseString(ExtendedQuery, 0x1C) );

		m_pShare->m_QueueAccess.Lock();
			m_pShare->m_PendingQueries.push_front(G1Query);	
		m_pShare->m_QueueAccess.Unlock();


		m_pShare->m_TriggerThread.SetEvent();
	}
}

// Called from share thread
void CGnuProtocol::Send_Query(GnuQuery &FileQuery, std::list<int> &MatchingNodes)
{
	// Forward query to child nodes that match the query

	packet_Query* pQuery = (packet_Query*) FileQuery.Packet;
	if(pQuery->Header.TTL == 0)
		pQuery->Header.TTL++;
	
	// Hops already increased in packet handling

	std::list<int>::iterator  itNodeID;

	for(itNodeID = MatchingNodes.begin(); itNodeID != MatchingNodes.end(); itNodeID++)
		if(*itNodeID != FileQuery.OriginID)
		{
			m_pComm->m_NodeAccess.Lock();

				std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(*itNodeID);

				if(itNode != m_pComm->m_NodeIDMap.end())
				{
					CGnuNode* pNode = itNode->second;

					if(pNode->m_Status == SOCK_CONNECTED)
					{
						packet_Query* pQuery = (packet_Query*) FileQuery.Packet;

						if( pNode->m_GnuNodeMode == GNU_LEAF)
							pNode->SendPacket(FileQuery.Packet, FileQuery.PacketSize, PACKET_QUERY, pQuery->Header.Hops, true);

						if( pNode->m_GnuNodeMode == GNU_ULTRAPEER && pNode->m_SupportInterQRP)
							pNode->SendPacket(FileQuery.Packet, FileQuery.PacketSize, PACKET_QUERY, pQuery->Header.Hops, true);
					}
				}

			m_pComm->m_NodeAccess.Unlock();
		}
}

void CGnuProtocol::Send_QueryHit(GnuQuery &FileQuery, byte* pQueryHit, DWORD ReplyLength, byte ReplyCount, CString &MetaTail)
{
	packet_QueryHit*    QueryHit = (packet_QueryHit*)   pQueryHit;
	packet_QueryHitEx*  QHD      = (packet_QueryHitEx*) (pQueryHit + 34 + ReplyLength);

	// Build Query Packet
	int packetLength = 34 + ReplyLength;

	QueryHit->Header.Guid = FileQuery.SearchGuid;

	packet_Query* pQuery = (packet_Query*) FileQuery.Packet;

	QueryHit->Header.Function = 0x81;
	QueryHit->Header.TTL	  = pQuery->Header.Hops;
	QueryHit->Header.Hops	  = 0;

	QueryHit->TotalHits	= ReplyCount;
	QueryHit->Port		= (WORD) m_pNet->m_CurrentPort;
	QueryHit->Speed		= m_pComm->GetSpeed();
	QueryHit->Host		= m_pNet->m_CurrentIP;

	if(m_pPrefs->m_ForcedHost.S_addr)
		QueryHit->Host = m_pPrefs->m_ForcedHost;


	// Add Query Hit Descriptor
	packetLength += sizeof(packet_QueryHitEx);

	bool Busy = false;
	if(m_pPrefs->m_MaxUploads)
		if(m_pTrans->CountUploading() >= m_pPrefs->m_MaxUploads)
			Busy = true;

	memcpy( QHD->VendorID, (LPCSTR) m_pCore->m_ClientCode, 4);
	QHD->Length		= 4;
	QHD->FlagPush	= true;
	QHD->FlagBad	= true;
	QHD->FlagBusy	= true;
	QHD->FlagStable	= true;
	QHD->FlagSpeed	= true;
	QHD->FlagTrash  = 0;

	QHD->Push	= m_pNet->m_TcpFirewall;
	QHD->Bad	= 0;
	QHD->Busy	= Busy;
	QHD->Stable	= m_pNet->m_HaveUploaded;
	QHD->Speed	= m_pNet->m_RealSpeedUp ? true : false;
	QHD->Trash	= 0;
	

	// Add Metadata to packet
	strcpy((char*) pQueryHit + packetLength, "{deflate}");
	packetLength += 9;

	DWORD MetaSize  = MetaTail.GetLength() + 1; // Plus one for null
	DWORD CompSize	= MetaSize * 1.2 + 12;

	if(compress(pQueryHit + packetLength, &CompSize, (byte*) MetaTail.GetBuffer(), MetaSize) == Z_OK)
	{
		packetLength += CompSize;

		QHD->MetaSize = 9 + CompSize;
	}
	MetaTail.ReleaseBuffer();

	// Add ClientID of this node
	memcpy(pQueryHit + packetLength, &m_pPrefs->m_ClientID, 16);

	packetLength += 16;

	
	// Send the packet
	QueryHit->Header.Payload  = packetLength - 23;


	// DirectAddress set signal node wants hits out of band
	if(FileQuery.DirectAddress.Host.S_addr)
	{	
		std::map<uint32, OobHit*>::iterator itHit = m_pComm->m_OobHits.find( HashGuid(QueryHit->Header.Guid) );
	
		if(itHit != m_pComm->m_OobHits.end())
		{
			OobHit* pHit = itHit->second;

			byte* HitCopy = new byte[packetLength];
			memcpy( HitCopy, pQueryHit, packetLength);

			pHit->QueryHits.push_back(HitCopy);
			pHit->QueryHitLengths.push_back(packetLength);
		}

		return;
	}

	// FilesAccess must be unlocked before calling this
	m_pComm->m_NodeAccess.Lock();

		std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(FileQuery.OriginID);

		if(itNode != m_pComm->m_NodeIDMap.end())
			if(itNode->second->m_Status == SOCK_CONNECTED)
			{
				itNode->second->m_pComm->m_TableLocal.Insert(FileQuery.SearchGuid, itNode->second->m_NodeID);
				itNode->second->SendPacket(pQueryHit, packetLength, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, true);
				m_pComm->m_NodeAccess.Unlock();
				return;
			}

	m_pComm->m_NodeAccess.Unlock();

	// Node not found, check if its a node browsing files
	for(int i = 0; i < m_pComm->m_NodesBrowsing.size(); i++)
		if(m_pComm->m_NodesBrowsing[i]->m_NodeID == FileQuery.OriginID)
		{
			byte* BrowsePacket = new byte[packetLength];
			memcpy(BrowsePacket, pQueryHit, packetLength);
			
			m_pComm->m_NodesBrowsing[i]->m_BrowseHits.push_back(BrowsePacket);
			m_pComm->m_NodesBrowsing[i]->m_BrowseHitSizes.push_back(packetLength);

			m_pComm->m_NodesBrowsing[i]->SendPacket(pQueryHit, packetLength, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, true);
			break;
		}
}

// Called from share thread
void CGnuProtocol::Send_Push(FileSource Download)
{
	GUID Guid;
	GnuCreateGuid(&Guid);

	// Create packet
	packet_Push Push;

	Push.Header.Guid		= Guid;
	Push.Header.Function	= 0x40;
	Push.Header.TTL			= Download.Distance;
	Push.Header.Hops		= 0;
	Push.Header.Payload		= 26;
	Push.ServerID			= Download.PushID;
	Push.Index				= Download.FileIndex;
	Push.Host				= m_pNet->m_CurrentIP;
	Push.Port				= m_pNet->m_CurrentPort;

	if(m_pPrefs->m_ForcedHost.S_addr)
		Push.Host = m_pPrefs->m_ForcedHost;

	// Send Push
	for(int i = 0; i < m_pComm->m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_pComm->m_NodeList[i];

		if(p->m_NodeID == Download.GnuRouteID && p->m_Status == SOCK_CONNECTED)
		{
			m_pComm->m_TableLocal.Insert(Guid, p->m_NodeID);

			p->SendPacket(&Push, 49, PACKET_PUSH, Push.Header.TTL - 1);
		}
	}
}

void CGnuProtocol::Send_VendMsg(CGnuNode* pTCP, packet_VendMsg VendMsg, void* payload, int length, IPv4 Target )
{
	if(pTCP)
	{
		ASSERT(pTCP->m_SupportsVendorMsg);
	}

	byte* FinalPacket = NULL;
	int   FinalLength = 0;

	// Build the packet
	//VendMsg.Header.Guid		= Guid; // Should be created before function is called
	VendMsg.Header.Function	= 0x31;
	VendMsg.Header.TTL		= 1;
	VendMsg.Header.Hops		= 0;
	VendMsg.Header.Payload	= 8 + length;

	// Build packet
	FinalLength = 31 + length;
	FinalPacket = new byte[FinalLength];

	memcpy(FinalPacket, &VendMsg, 31);

	if(payload)
		memcpy(FinalPacket + 31, payload, length);

	if(pTCP)
		pTCP->SendPacket(FinalPacket, FinalLength, PACKET_VENDMSG, VendMsg.Header.Hops);
	else
		m_pDatagram->SendPacket(Target, FinalPacket, FinalLength);
	
	delete [] FinalPacket;
	FinalPacket = NULL;
}

void CGnuProtocol::Send_Bye(CGnuNode* pTCP, CString Reason)
{
	GUID Guid;
	GnuCreateGuid(&Guid);
	
	int PacketSize = 23 + Reason.GetLength() + 1;
	byte* PacketData = new byte[PacketSize];
	
	packet_Bye* Bye =  (packet_Bye*) PacketData;
	Bye->Header.Guid		= Guid;
	Bye->Header.Function	= 0x02;
	Bye->Header.TTL			= 1;
	Bye->Header.Hops		= 0;
	Bye->Header.Payload		= PacketSize - 23;

	strcpy((char*) &PacketData[23], (LPCSTR) Reason);

	byte test[255];
	memcpy(test, PacketData, PacketSize);

	PacketData[PacketSize - 1] = NULL;

	pTCP->SendPacket(PacketData, PacketSize, PACKET_BYE, Bye->Header.Hops);

	delete [] PacketData;
}

void CGnuProtocol::Send_StatsMsg(CGnuNode* pTCP)
{
	packet_StatsMsg Stats;

	Stats.LeafMax	= m_pPrefs->m_MaxLeaves;
	Stats.LeafCount = m_pComm->CountLeafConnects();
	Stats.Uptime	= time(NULL) - m_pComm->m_ClientUptime.GetTime();
	Stats.Cpu		= m_pCore->m_SysSpeed;
	Stats.Mem		= m_pCore->m_SysMemory;
	
	Stats.FlagUltraAble   = m_pComm->UltrapeerAble();
	Stats.FlagRouter      = m_pNet->m_BehindRouter;
	Stats.FlagTcpFirewall = m_pNet->m_TcpFirewall;
	Stats.FlagUdpFirewall = m_pNet->m_UdpFirewall;

	packet_VendMsg StatsMsg;
	GnuCreateGuid(&StatsMsg.Header.Guid);
	StatsMsg.Ident = packet_VendIdent("GNUC", 60, 1);
	Send_VendMsg( pTCP, StatsMsg, &Stats, 13);
}

// Called from share thread
void CGnuProtocol::Encode_QueryHit(GnuQuery &FileQuery, std::list<UINT> &MatchingIndexes, byte* QueryReply)
{	
	byte*	 QueryReplyNext		= &QueryReply[34];
	DWORD	 QueryReplyLength	= 0;
	UINT	 TotalReplyCount	= 0;
	byte	 ReplyCount			= 0;
	int		 MaxReplies			= m_pPrefs->m_MaxReplies;
	CString  MetaTail			= "";

	m_pShare->m_FilesAccess.Lock();


	if(FileQuery.DirectAddress.Host.S_addr)
	{
		m_pComm->m_OobHitsLock.Lock();
		
		std::map<uint32, OobHit*>::iterator itHit = m_pComm->m_OobHits.find( HashGuid(FileQuery.SearchGuid) );
		if(itHit == m_pComm->m_OobHits.end())
			m_pComm->m_OobHits[ HashGuid(FileQuery.SearchGuid) ] = new OobHit(FileQuery.DirectAddress, MatchingIndexes.size(), FileQuery.OriginID);
	}


	std::list<UINT>::iterator itIndex;
	for(itIndex = MatchingIndexes.begin(); itIndex != MatchingIndexes.end(); itIndex++)
	{	
		// Add to Search Reply
		int pos = *itIndex;

		if(m_pShare->m_SharedFiles[pos].Name.size() == 0)
			continue;

		if(MaxReplies && MaxReplies <= TotalReplyCount)	
			break;

		m_pShare->m_SharedFiles[pos].Matches++;

		// File Index
		* (UINT*) QueryReplyNext = m_pShare->m_SharedFiles[pos].Index;
		QueryReplyNext   += 4;
		QueryReplyLength += 4;

		// File Size
		* (UINT*) QueryReplyNext = m_pShare->m_SharedFiles[pos].Size;
		QueryReplyNext   += 4;
		QueryReplyLength += 4;
		
		// File Name
		strcpy ((char*) QueryReplyNext, m_pShare->m_SharedFiles[pos].Name.c_str());
		QueryReplyNext   += m_pShare->m_SharedFiles[pos].Name.size() + 1;
		QueryReplyLength += m_pShare->m_SharedFiles[pos].Name.size() + 1;

		// File Hash
		if( !m_pShare->m_SharedFiles[pos].HashValues[HASH_SHA1].empty() )
		{
			strcpy ((char*) QueryReplyNext, "urn:sha1:");
			QueryReplyNext   += 9;
			QueryReplyLength += 9;

			strcpy ((char*) QueryReplyNext, m_pShare->m_SharedFiles[pos].HashValues[HASH_SHA1].c_str());
			QueryReplyNext   += m_pShare->m_SharedFiles[pos].HashValues[HASH_SHA1].size() + 1;
			QueryReplyLength += m_pShare->m_SharedFiles[pos].HashValues[HASH_SHA1].size() + 1;
		}
		else
		{
			*QueryReplyNext = '\0';

			QueryReplyNext++;
			QueryReplyLength++;
		}

		// File Meta
		if(m_pShare->m_SharedFiles[pos].MetaID)
		{
			std::map<int, CGnuSchema*>::iterator itMeta = m_pCore->m_pMeta->m_MetaIDMap.find(m_pShare->m_SharedFiles[pos].MetaID);
			if(itMeta != m_pCore->m_pMeta->m_MetaIDMap.end())
			{
				int InsertPos = MetaTail.Find("</" + itMeta->second->m_NamePlural + ">");

				if(InsertPos == -1)
				{
					MetaTail += "<" + itMeta->second->m_NamePlural + "></" + itMeta->second->m_NamePlural + ">";
					InsertPos = MetaTail.Find("</" + itMeta->second->m_NamePlural + ">");
				}
			
				MetaTail.Insert(InsertPos, itMeta->second->AttrMaptoNetXML(m_pShare->m_SharedFiles[pos].AttributeMap, ReplyCount));
			}
		}

		ReplyCount++;
		TotalReplyCount++;

		// Udp has a max size of around a kilobyte make sure packet doesnt go over
		int EstSize = QueryReplyLength + 6 + MetaTail.GetLength() + 16; // QH, Ext, Meta, ClientID
		if(EstSize > 512 || ReplyCount == 255)
		{
			Send_QueryHit(FileQuery, QueryReply, QueryReplyLength, ReplyCount, MetaTail);
			
			QueryReplyNext	 = &QueryReply[34];
			QueryReplyLength = 0;
			ReplyCount		 = 0;
			MetaTail		 = "";
		}
	}


	if(ReplyCount > 0)
	{
		Send_QueryHit(FileQuery, QueryReply, QueryReplyLength, ReplyCount, MetaTail);
	}

	if(FileQuery.DirectAddress.Host.S_addr)
		m_pComm->m_OobHitsLock.Unlock();

	m_pShare->m_FilesAccess.Unlock();
}

GGEPReadResult CGnuProtocol::Decode_GGEPBlock(packet_GGEPBlock &Block, byte* &stream, uint32 &length)
{
	if(length == 0)
		return BLOCK_INCOMPLETE;

	// Read Flags
	packet_GGEPHeaderFlags* pFlags = (packet_GGEPHeaderFlags*) stream;

	Block.Compression = pFlags->Compression;
	Block.Encoded	  = pFlags->Encoding;
	Block.Last		  = pFlags->Last;

	stream += 1;
	length -= 1;

	// Read Name
	if(length <	pFlags->NameLength)
		return BLOCK_INCOMPLETE;

	memcpy(Block.Name, stream, pFlags->NameLength);
	
	stream += pFlags->NameLength;
	length -= pFlags->NameLength;
	
	// Read Payload Length
	byte b;
	Block.PayloadSize = 0;
	
	do 
	{
		if(length == 0)
			return BLOCK_INCOMPLETE;

		b = stream[0];
		Block.PayloadSize = (Block.PayloadSize << 6) | (b & 0x3f);

		stream += 1;
		length -= 1;
	} while (0x40 != (b & 0x40));	

	// Set Payload
	Block.Payload = stream;

	if(Block.PayloadSize && length < Block.PayloadSize)
		return BLOCK_INCOMPLETE;

	stream += Block.PayloadSize;
	length -= Block.PayloadSize;

	return BLOCK_GOOD;
}