/********************************************************************************

	GnucDNA - The Gnucleus Library
	Copyright (C) 2000-2005 John Marshall Group

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	By contributing code you grant John Marshall Group an unlimited, non-exclusive
	license to your contribution.

	For support, questions, commercial use, etc...
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

	//TestCobs();
	//TestGgep();
	//TestLF();
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

	// Build a pong
	packet_Pong* pPong      = (packet_Pong*) m_PacketBuffer;
	pPong->Header.Guid		= Ping->Header.Guid;
	pPong->Header.Function	= 0x01;
	pPong->Header.TTL		= 1;
	pPong->Header.Hops		= 0;
	pPong->Port				= m_pNet->m_CurrentPort;
	pPong->Host				= m_pNet->m_CurrentIP;
	pPong->FileCount			= m_pShare->m_TotalLocalFiles;

	if(m_pPrefs->m_ForcedHost.S_addr)
		pPong->Host = m_pPrefs->m_ForcedHost;

	// If we are an ultrapeer, the size field is used as a marker send that info
	if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		pPong->FileSize = m_pShare->m_UltrapeerSizeMarker;
	else
		pPong->FileSize = m_pShare->m_TotalLocalSize;


	int packetLength = 37;

	bool isBig = false;
	bool scp   = false;
	bool isDna = false;

	// if ggep ping
	if(Ping->Header.Payload > 0)
	{
		byte*  PingBuffer = ((byte*) Ping) + 23;
		uint32 BytesRead  = Ping->Header.Payload;

		// check if ggep
		if(PingBuffer[0] == 0xC3 && BytesRead > 0)
		{
			byte* ggepBuff = PingBuffer;
			ggepBuff  += 1;
			BytesRead -= 1;

			isBig = true;

			GGEPReadResult Status = BLOCK_GOOD;
			
			while(Status == BLOCK_GOOD)
			{
				packet_GGEPBlock Block;
				Status = Decode_GGEPBlock(Block, ggepBuff, BytesRead);

				if( strcmp(Block.Name, "DNA") == 0)
					isDna = true;

				if( strcmp(Block.Name, "SCP") == 0)
					scp = true;

				if(Block.Last)
					break;
			}
		}
	}

	// create big pong
	if(isBig)
	{
		// create big pong
		byte* pGGEPStart = m_PacketBuffer + packetLength;

		*pGGEPStart   = 0xC3; // ggep magic
		packetLength += 1;

		// add ultrapeer status
		packet_GGEPBlock UpBlock;
		memcpy(UpBlock.Name, "UP", 2);

		byte upStatus[3];
		upStatus[0] = (m_pComm->m_GnuClientMode == GNU_ULTRAPEER);
		upStatus[1] = MAX_LEAVES - m_pComm->CountLeafConnects();
		upStatus[2] = m_pPrefs->m_MaxConnects - m_pComm->CountUltraConnects();
		UpBlock.Last = (scp == false && isDna == false);

		packetLength += Encode_GGEPBlock(UpBlock, m_PacketBuffer + packetLength, upStatus, 3);


		if(scp)
		{
			// add cache ips
			packet_GGEPBlock IppBlock;
			memcpy(IppBlock.Name, "IPP", 3);
			IppBlock.Last = (isDna == false);
			
			std::vector<IPv4> UltraIPs;

			for(int i = 0; i < m_pComm->m_NodeList.size() && i < 5; i++)
			{
				CGnuNode* pNode = m_pComm->GetRandNode(GNU_ULTRAPEER);

				if(pNode)
					UltraIPs.push_back( pNode->m_Address );
			}

			if( UltraIPs.size() )
			{
				int size = UltraIPs.size() * 6;
				byte* pIPs = new byte[size];

				for(i = 0; i < UltraIPs.size(); i++)
					memcpy(pIPs + (i * 6), &UltraIPs[i], 6);
				
				packetLength += Encode_GGEPBlock(IppBlock, m_PacketBuffer + packetLength, pIPs, size);
				delete [] pIPs;
			}
		}

		if(isDna)
		{
			std::vector<IPv4> UltraIPs;

			for(int i = 0; i < m_pComm->m_NodeList.size() && i < 5; i++)
			{
				CGnuNode* pNode = m_pComm->GetRandNode(GNU_ULTRAPEER, true);

				if(pNode)
					UltraIPs.push_back( pNode->m_Address );
			}

			int size  = UltraIPs.size() * 6;

			packet_GGEPBlock DnaBlock;
			memcpy(DnaBlock.Name, "DNA", 3);
			DnaBlock.Last = (size == 0);
			packetLength += Encode_GGEPBlock(DnaBlock, m_PacketBuffer + packetLength, NULL, 0);

			if( UltraIPs.size() )
			{
				// add dna ips
				packet_GGEPBlock DIppBlock;
				memcpy(DIppBlock.Name, "DIPP", 3);
				DIppBlock.Last = true;

				int size = UltraIPs.size() * 6;
				byte* pIPs = new byte[size];

				for(i = 0; i < UltraIPs.size(); i++)
					memcpy(pIPs + (i * 6), &UltraIPs[i], 6);
				
				packetLength += Encode_GGEPBlock(DIppBlock, m_PacketBuffer + packetLength, pIPs, size);
				delete [] pIPs;
			}
		}
	}


	pPong->Header.Payload = packetLength - 23;
	
	
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

		m_pDatagram->SendPacket(Packet.Source, m_PacketBuffer, packetLength);

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

	pNode->SendPacket(m_PacketBuffer, packetLength, PACKET_PONG, pPong->Header.TTL - 1);
}


void CGnuProtocol::Receive_Pong(Gnu_RecvdPacket &Packet)
{
	packet_Pong* Pong = (packet_Pong*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

	if(Pong->Header.Payload < 14)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Pong, Length " + NumtoStr(Pong->Header.Payload));
		return;
	}

	bool isDna = false;
	bool connectSource = false;
	
	// if ggep pong
	if(Pong->Header.Payload > 14)
	{
		byte*  PongBuffer = ((byte*) Pong) + 23 + 14;
		uint32 BytesRead  = Pong->Header.Payload - 14;	
		
		// check if ggep
		if(*PongBuffer == 0xC3 && BytesRead > 0)
		{
			byte* ggepBuff = PongBuffer;
			ggepBuff  += 1;
			BytesRead -= 1;

			GGEPReadResult Status = BLOCK_GOOD;
			
			while(Status == BLOCK_GOOD)
			{
				packet_GGEPBlock Block;
				Status = Decode_GGEPBlock(Block, ggepBuff, BytesRead);

				if( strcmp(Block.Name, "DNA") == 0)
					isDna = true;


				// dna ips
				if( strcmp(Block.Name, "DIPP") == 0 && Block.PayloadSize % 6 == 0)
				{
					for(int i = 0; i < Block.PayloadSize; i += 6)
					{
						IPv4 cacheIP;
						memcpy(&cacheIP, Block.Payload + i, 6);
						m_pCache->AddKnown( Node(IPtoStr(cacheIP.Host), cacheIP.Port, NETWORK_GNUTELLA, CTime::GetCurrentTime(), true));
					}
				}

				// real ips
				if(	strcmp(Block.Name, "IPP") == 0 && Block.PayloadSize % 6 == 0) 
				{
					for(int i = 0; i < Block.PayloadSize; i += 6)
					{
						IPv4 cacheIP;
						memcpy(&cacheIP, Block.Payload + i, 6);
						m_pCache->AddKnown( Node(IPtoStr(cacheIP.Host), cacheIP.Port, NETWORK_GNUTELLA, CTime::GetCurrentTime(), false));
					}
				}

				// utlrapeer status
				if(	strcmp(Block.Name, "UP") == 0 && Block.PayloadSize == 3) 
				{
					if(Block.Payload[0])
					{
						// if we are leaf and remote node has space
						if(m_pComm->m_GnuClientMode == GNU_LEAF && Block.Payload[1])
							if(m_pPrefs->m_LeafModeConnects - m_pComm->CountUltraConnects() > 0 || (isDna && m_pComm->NeedDnaUltras))
								connectSource = true;

						// if we are ultra and remote node has space
						if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER && Block.Payload[2])
							if(m_pPrefs->m_MaxConnects - m_pComm->CountUltraConnects() > 0 || (isDna && m_pComm->NeedDnaUltras))
								connectSource = true;
					}
				}

				if(Block.Last)
					break;
			}
		}
	}

	// if received udp
	if(pNode == NULL)
	{
		if(connectSource && m_pComm->m_TryingConnect)
			m_pComm->AddNode(IPtoStr(Packet.Source.Host), Packet.Source.Port);
		else if(isDna)
			m_pCache->AddKnown( Node(IPtoStr(Packet.Source.Host), Packet.Source.Port, NETWORK_GNUTELLA, CTime::GetCurrentTime(), true));
		else
			m_pCache->AddKnown( Node(IPtoStr(Packet.Source.Host), Packet.Source.Port, NETWORK_GNUTELLA, CTime::GetCurrentTime(), false));


		if(m_pNet->m_UdpFirewall == UDP_BLOCK)
			m_pNet->m_UdpFirewall = UDP_NAT;

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

				if(Query->Header.TTL > 1) 
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
		}

		memcpy(G1Query.Packet, (byte*) Query, Packet.Length);
		G1Query.PacketSize = Packet.Length;

		
		byte* pPayload = (byte*) Query + 25;
		int   BytesLeft = Query->Header.Payload - 2;


		// Get Query string
		uint32 BytesRead = ParsePayload(pPayload, BytesLeft, NULL, m_QueryBuffer, 1024);
		if(BytesRead)
			G1Query.Terms.push_back( CString((char*) m_QueryBuffer, BytesRead) );

		// read extended info in standard format
		if(pPayload[0] != 0xC3)
		{	
			BytesRead = ParsePayload(pPayload, BytesLeft, 0x1C, m_QueryBuffer, 1024);
		
			while(BytesRead)
			{
				if( m_QueryBuffer[0] == '<' || 
					(BytesRead >= 4 && memcmp(m_QueryBuffer, "urn:", 4) == 0) )
				{
					// prevent extra null from  being copied into new string, screwing up comparison
					if(m_QueryBuffer[BytesRead - 1] == NULL)
						BytesRead--;

					G1Query.Terms.push_back( CString((char*) m_QueryBuffer, BytesRead) );
					
				}

				if(pPayload[0] == 0xC3)
					break;

				BytesRead = ParsePayload(pPayload, BytesLeft, 0x1C, m_QueryBuffer, 1024);
			}
		}

		// check if extended info in ggep format
		if(BytesLeft > 0 && pPayload[0] == 0xC3)
		{
			byte*  ggepBuff = pPayload + 1;
			uint32 ggepSize = BytesLeft - 1;

			GGEPReadResult Status = BLOCK_GOOD;
			
			while(Status == BLOCK_GOOD)
			{
				packet_GGEPBlock Block;
				Status = Decode_GGEPBlock(Block, ggepBuff, ggepSize);

				if( strcmp(Block.Name, "M") == 0 && Block.PayloadSize)
				{
					byte flags = Block.Payload[0]; // no idea what these are
				}

				if(Block.Last)
					break;
			}
		}

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

int	CGnuProtocol::ParsePayload(byte* &pPayload, int &BytesLeft, byte Break, byte* pBuffer, int BufferSize)
{
	if(BytesLeft == 0)
		return 0;

	int BytesRead = memfind(pPayload, BytesLeft, Break);

	bool BreakFound = true;
	if( BytesRead == -1)
	{
		BreakFound = false;
		BytesRead  = BytesLeft;
	}
	
	if(BytesRead > BufferSize)
	{
		BreakFound = false;
		BytesRead  = BufferSize;
	}

	memcpy(pBuffer, pPayload, BytesRead);
	
	int NextPos = BreakFound ? BytesRead + 1 : BytesRead;
	
	pPayload  += NextPos;
	BytesLeft -= NextPos;

	return BytesRead;
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


		std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(RouteID);
		if(itNode != m_pComm->m_NodeIDMap.end() && itNode->second->m_Status == SOCK_CONNECTED)
		{
			// Route to another ultrapeer
			if(itNode->second->m_GnuNodeMode == GNU_ULTRAPEER && QueryHit->Header.TTL > 0)
				itNode->second->SendPacket(QueryHit, Packet.Length, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, false);

			// Send if meant for child
			if(itNode->second->m_GnuNodeMode == GNU_LEAF)
			{
				if(QueryHit->Header.TTL == 0)
					QueryHit->Header.TTL++;

				itNode->second->SendPacket(QueryHit, Packet.Length, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, false);

				// Update dyn query
				std::map<uint32, DynQuery*>::iterator itDyn = m_pComm->m_DynamicQueries.find( HashGuid(QueryHit->Header.Guid) );
				if( itDyn != m_pComm->m_DynamicQueries.end() )
					itDyn->second->Hits += QueryHit->TotalHits;
			}
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
		for(int i = 0; i < DirectUltrapeers.size(); i++)
			Item.DirectHubs.push_back( DirectUltrapeers[i] );

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


		// If packet has extended block after result between nulls
		if(Packet + pos + 1 != NULL)
		{
			// set postion and size of extended block
			byte* extStart  = Packet + pos + 1;
			int   extLength = 0;

			for(int i = pos + 1; i < 23 + QueryHit->Header.Payload; i++)
			{
				if( Packet[i] == 0)
					break;
				else
					extLength++;
			}

			pos += extLength + 1;

			// check for data between nulls in standard format
			if(extStart[0] != 0xC3 && extLength > 0)
			{
				byte extBuffer[256];
				
				int readSize = ParsePayload(extStart, extLength, 0x1C, extBuffer, 256);

				while(readSize)
				{
					CString extString( (char*) extBuffer, readSize);

					if( !extString.IsEmpty() )
						Item.GnuExtraInfo.push_back( extString );

					if(extStart[0] == 0xC3)
						break;
					
					readSize = ParsePayload(extStart, extLength, 0x1C, extBuffer, 256);
				}
			}

			// Check extended block in GGEP format
			if(extStart[0] == 0xC3 && extLength > 0)
			{
				byte*  nextBlock   = extStart  + 1;
				uint32 blockLength = extLength - 1;

				GGEPReadResult Status = BLOCK_GOOD;
				
				while(Status == BLOCK_GOOD)
				{
					packet_GGEPBlock block;
					Status = Decode_GGEPBlock(block, nextBlock, blockLength);
					
					// Hashes
					if( strcmp(block.Name, "H") == 0 && block.PayloadSize)
					{
						if( block.Payload[0] == GGEP_H_SHA1 && block.PayloadSize == 1 + 20)
							Item.Sha1Hash = EncodeBase32(block.Payload + 1, 20);

						else if( block.Payload[0] == GGEP_H_BITPRINT && block.PayloadSize == 1 + 20 + 24)
						{
							Item.Sha1Hash  = EncodeBase32(block.Payload + 1,      20);
							Item.TigerHash = EncodeBase32(block.Payload + 1 + 20, 24);
						}
					}

					// Large File
					if( strcmp(block.Name, "LF") == 0 && block.PayloadSize)
					{
						Item.Size = DecodeLF(block.Payload, block.PayloadSize);
					}

					if(block.Last)
						break;
				}
			}
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
			ReceiveVM_Supported(Packet);

		// TCP Connect Back
		if(VendMsg->Ident == packet_VendIdent("BEAR", 7, 1) )
			ReceiveVM_TCPConnectBack(Packet);
	
		// UDP Connect Back
		if(VendMsg->Ident == packet_VendIdent("GTKG", 7, 2))
			ReceiveVM_UDPConnectBack(Packet);

		// UDP Relay Connect Back
		if( VendMsg->Ident == packet_VendIdent("GNUC", 7, 1))
			ReceiveVM_UDPRelayConnect(Packet);

		// Leaf Guided Dyanic Query: Query Status Request
		if(VendMsg->Ident == packet_VendIdent("BEAR", 11, 1) )
			ReceiveVM_QueryStatusReq(Packet);

		// Leaf Guided Dyanic Query: Query Status Response
		if(VendMsg->Ident == packet_VendIdent("BEAR", 12, 1) )
			ReceiveVM_QueryStatusAck(Packet);

		// Push Proxy Request
		if(VendMsg->Ident == packet_VendIdent("LIME", 21, 1) )
			ReceiveVM_PushProxyReq(Packet);

		// Push Proxy Acknowledgement
		if(VendMsg->Ident == packet_VendIdent("LIME", 22, 2) )
			ReceiveVM_PushProxyAck(Packet);

		// Node Stats
		if(VendMsg->Ident == packet_VendIdent("GNUC", 60, 1) )
			ReceiveVM_NodeStats(Packet);

		// Mode Change Request
		if(VendMsg->Ident == packet_VendIdent("GNUC", 61, 1) )
			ReceiveVM_ModeChangeReq(Packet);

		// Mode Change Ack
		if(VendMsg->Ident == packet_VendIdent("GNUC", 62, 1) )
			ReceiveVM_ModeChangeAck(Packet);
	}

	// Packet received UDP
	else
	{
		// Out of band - Reply Num
		if(VendMsg->Ident == packet_VendIdent("LIME", 12, 1) )
			ReceiveVM_QueryHitNum(Packet);

		// Out of band - Reply Ack
		if(VendMsg->Ident == packet_VendIdent("LIME", 11, 2) )
			ReceiveVM_QueryHitReq(Packet);

		// Crawl Req
		if(VendMsg->Ident == packet_VendIdent("LIME", 5, 1) )
			ReceiveVM_CrawlReq(Packet);
	}
}

void CGnuProtocol::Receive_Bye(Gnu_RecvdPacket &Packet)
{
	packet_Bye* Bye = (packet_Bye*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;
	if(pNode == NULL)
		return;

	byte* ByeData = (byte*) Bye;

	CString Reason;
	if( Packet.Length > 25)
		Reason = CString( (char*) &ByeData[25], Packet.Length - 25);

	pNode->CloseWithReason( Reason, 0, true );
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

void CGnuProtocol::ReceiveVM_Supported(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

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

			if(*MsgSupported == packet_VendIdent("LIME", 6, 1))
				pNode->m_SupportsUdpCrawl = true;

			if(*MsgSupported == packet_VendIdent("GNUC", 6, 1))
				pNode->m_SupportsUdpConnect = true;

			//TRACE0(Msg + " ");
		}

		//TRACE0("\n");
	}
	else
		m_pCore->DebugLog("Gnutella", "Vendor Msg, Messages Support, VS:" + NumtoStr(VectorSize) + ", SL:" + NumtoStr(sublength));
}

void CGnuProtocol::ReceiveVM_TCPConnectBack(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

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

void CGnuProtocol::ReceiveVM_UDPConnectBack(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

	if(Packet.Length - 31 != 2)
		return;

	uint16 ConnectPort = 0;
	memcpy(&ConnectPort, ((byte*) VendMsg) + 31, 2);

	IPv4 Target;
	Target.Host = pNode->m_Address.Host;
	Target.Port = ConnectPort;
	Send_Ping(NULL, 1, false, &VendMsg->Header.Guid, Target);		
}

void CGnuProtocol::ReceiveVM_UDPRelayConnect(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

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

		Send_Ping(NULL, 1, false, &VendMsg->Header.Guid, Target);
	}
}

void CGnuProtocol::ReceiveVM_QueryStatusReq(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

	if(m_pComm->m_GnuClientMode != GNU_LEAF)
	{
		m_pCore->DebugLog("Gnutella", "Query Status Request Received from " + pNode->m_RemoteAgent + " while in Ultrapeer Mode");
		return;
	}

	// Find right search by using the GUID
	for(int i = 0; i < m_pNet->m_SearchList.size(); i++)
		if(VendMsg->Header.Guid == m_pNet->m_SearchList[i]->m_QueryID)
		{
			TRACE0("VMS " + pNode->m_RemoteAgent + " " + IPtoStr(pNode->m_Address.Host) + ": Query Status Request\n");
			CGnuSearch* pSearch = m_pNet->m_SearchList[i];
			
			packet_VendMsg ReplyMsg;
			ReplyMsg.Header.Guid = VendMsg->Header.Guid;
			ReplyMsg.Ident = packet_VendIdent("BEAR", 12, 1);
			
			uint16 Hits = pSearch->m_WholeList.size();

			Send_VendMsg(pNode, ReplyMsg, &Hits, 2 );
		}
}

void CGnuProtocol::ReceiveVM_QueryStatusAck(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

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


void CGnuProtocol::ReceiveVM_QueryHitNum(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
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

void CGnuProtocol::ReceiveVM_QueryHitReq(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
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


void CGnuProtocol::ReceiveVM_PushProxyReq(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

	if(Packet.Length != 31)
		return;

	if(Packet.pTCP->m_GnuNodeMode != GNU_LEAF)
		return;

	m_pComm->m_PushProxyHosts[ Packet.pTCP->m_NodeID ] = VendMsg->Header.Guid;

	IPv4 LocalAddr;
	LocalAddr.Host = m_pNet->m_CurrentIP;
	LocalAddr.Port = m_pNet->m_CurrentPort;

	packet_VendMsg ProxyAck;
	ProxyAck.Header.Guid = VendMsg->Header.Guid;
	ProxyAck.Ident = packet_VendIdent("LIME", 22, 2);
	Send_VendMsg(Packet.pTCP, ProxyAck, &LocalAddr, 6);
}

void CGnuProtocol::ReceiveVM_PushProxyAck(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

	byte* message   = ((byte*) VendMsg) + 31;
	int	  sublength = Packet.Length - 31;

	if(sublength != 6)
		return;

	memcpy(&pNode->m_PushProxy, message, 6);
}


void CGnuProtocol::ReceiveVM_NodeStats(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

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


void CGnuProtocol::ReceiveVM_ModeChangeReq(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

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
		AckMsg.Header.Guid = VendMsg->Header.Guid;
		AckMsg.Ident       = packet_VendIdent("GNUC", 62, 1);
		
		if( Packet.pTCP->m_GnuNodeMode != GNU_ULTRAPEER || // Make sure remote is ultrapeer
			m_pComm->m_GnuClientMode != GNU_LEAF ||		   // Make sure we are a leaf
			!m_pComm->UltrapeerAble())					   // Make sure we are ultrapeer able
		{
			byte NoAck = 0x02;
			Send_VendMsg(Packet.pTCP, AckMsg, &NoAck, 1);
			return;
		}
	
		byte YesAck = 0x01;
		// Send both tcp and udp to ensure it gets there	
		Send_VendMsg(Packet.pTCP, AckMsg, &YesAck, 1);
		Send_VendMsg(NULL, AckMsg, &YesAck, 1);
			
		m_pComm->SwitchGnuClientMode(GNU_ULTRAPEER);
		return;
	}
}

void CGnuProtocol::ReceiveVM_ModeChangeAck(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	CGnuNode* pNode = Packet.pTCP;

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

void CGnuProtocol::ReceiveVM_CrawlReq(Gnu_RecvdPacket &Packet)
{
	packet_VendMsg* VendMsg = (packet_VendMsg*) Packet.Header;
	
	byte* message   = ((byte*) VendMsg) + 31;
	int	  sublength = Packet.Length - 31;

	if(sublength < 3)
		return;

	int  UltraMax = message[0];
	int  LeafMax  = message[1];
	bool Time     = message[2] & CRAWL_UPTIME;
	bool Local    = message[2] & CRAWL_LOCAL;
	bool NewOnly  = message[2] & CRAWL_NEW;
	bool Agents   = message[2] & CRAWL_AGENT;

	// build list of nodes to send back
	std::vector<CGnuNode*> Ultras;
	std::vector<CGnuNode*> Leaves;

	for(int i = 0; i < m_pComm->m_NodeList.size(); i++)
		if(m_pComm->m_NodeList[i]->m_Status == SOCK_CONNECTED)
		{
			CGnuNode* pNode = m_pComm->m_NodeList[i];
			
			if(NewOnly && !pNode->m_SupportsUdpCrawl)
				continue;

			if(pNode->m_GnuNodeMode == GNU_ULTRAPEER)
				Ultras.push_back(pNode);

			if(pNode->m_GnuNodeMode == GNU_LEAF)
				Leaves.push_back(pNode);
		}

	// trim node lists to size remote host requested
	while(UltraMax != 255 && Ultras.size() > UltraMax)
		Ultras.pop_back();

	while(LeafMax != 255 && Leaves.size() > LeafMax)
		Leaves.pop_back();

	// put togther user agents
	std::string UserAgents;

	if( Agents )
	{	
		for(i = 0; i < Ultras.size(); i++)
			UserAgents += Ultras[i]->m_RemoteAgent + ";" ;
		
		for(i = 0; i < Leaves.size(); i++)
			UserAgents += Leaves[i]->m_RemoteAgent + ";" ;

		UserAgents += m_pCore->GetUserAgent();

		uint16 StringSize = UserAgents.size();

		// write file to compress
		gzFile gz = gzopen("udpcrawl.tmp", "wb+");
		gzwrite(gz, &StringSize, 2);
		gzwrite(gz, (void*) UserAgents.c_str(), UserAgents.size());
		gzclose(gz);
	}

	// packet size = 3 + ultras & leaves + (2 + compressed data)
	int   ackLength = 0;

	int BytesPerResult = 6;
	BytesPerResult += (Time)  ? 2 : 0;
	BytesPerResult += (Local) ? 2 : 0;

	ackLength += 3 + (Ultras.size() + Leaves.size()) * BytesPerResult;

	// add compressed size of result
	CFile  GzFile;
	uint16 CompressSize = 0;

	if(UserAgents.size() && GzFile.Open("udpcrawl.tmp", CFile::modeRead) )
		CompressSize = GzFile.GetLength();

	if(CompressSize)
		ackLength += 2 + CompressSize;

	// create payload for packet
	byte* payload = new byte[ackLength];

	payload[0] = Ultras.size();
	payload[1] = Leaves.size();
	
	byte Flags = 0;
	Flags |= (Time)    ? CRAWL_UPTIME : 0;
	Flags |= (Local)   ? CRAWL_LOCAL  : 0;
	Flags |= (NewOnly) ? CRAWL_NEW	  : 0;
	Flags |= (Agents)  ? CRAWL_AGENT  : 0;

	payload[2] = Flags;

	// add ultrapeers
	int CurrentPos = 3;

	for(i = 0; i < Ultras.size(); i++)
	{
		WriteCrawlResult(payload + CurrentPos, Ultras[i], Flags);
		CurrentPos += BytesPerResult;		
	}

	for(i = 0; i < Leaves.size(); i++)
	{
		WriteCrawlResult(payload + CurrentPos, Leaves[i], Flags);
		CurrentPos += BytesPerResult;		
	}

	if(CompressSize)
	{
		memcpy(payload + CurrentPos, &CompressSize, 2);

		GzFile.Read(payload + CurrentPos + 2, CompressSize);

		GzFile.Close();
	}

	// Build Packet
	packet_VendMsg CrawlAck;
	memcpy(&CrawlAck.Header.Guid, &VendMsg->Header.Guid, 16);
	CrawlAck.Ident = packet_VendIdent("LIME", 6, 1);

	Send_VendMsg( NULL, CrawlAck, payload, ackLength, Packet.Source );
}

void CGnuProtocol::WriteCrawlResult(byte* buffer, CGnuNode* pNode, byte Flags)
{
	memcpy(buffer, &pNode->m_Address, 6);
		
	int pos = 6;

	if(Flags | CRAWL_UPTIME)
	{
		uint16 minutes = time(NULL) - pNode->m_ConnectTime;
		minutes /= 60;

		memcpy(buffer + pos, &minutes, 2);
		pos += 2;
	}

	if(Flags | CRAWL_LOCAL)
	{
		if(pNode->m_LocalPref.GetLength() == 2)
			memcpy(buffer + pos, pNode->m_LocalPref, 2);
		else
			memcpy(buffer + pos, "en", 2);

		pos += 2;
	}
}

////////////////////////////////////////////////////////////////////////

void CGnuProtocol::Send_Ping(CGnuNode* pTCP, int TTL, bool NeedHosts, GUID* pGuid, IPv4 Target)
{
	GUID Guid;

	if(pGuid)
		memcpy(&Guid, pGuid, 16);
	else
		GnuCreateGuid(&Guid);

	packet_Ping* pPing = (packet_Ping*) m_PacketBuffer; 
	
	pPing->Header.Guid	   = Guid;
	pPing->Header.Function = 0;
	pPing->Header.Hops	   = 0;
	pPing->Header.TTL	   = TTL;
	pPing->Header.Payload  = 0;

	int packetLength = 23;

	// if udp or tcp support ggep, add dna marker
	if(NeedHosts && (pTCP == NULL || pTCP->m_SupportsVendorMsg))
	{
		byte* pGGEPStart = m_PacketBuffer + packetLength;
	
		*pGGEPStart   = 0xC3; // ggep magic
		packetLength += 1;
	
		packet_GGEPBlock ScpBlock;
		memcpy(ScpBlock.Name, "SCP", 3);

		packetLength += Encode_GGEPBlock(ScpBlock, m_PacketBuffer + packetLength, NULL, 0);


		packet_GGEPBlock DnaBlock;
		memcpy(DnaBlock.Name, "DNA", 3);
		DnaBlock.Last = true;

		packetLength += Encode_GGEPBlock(DnaBlock, m_PacketBuffer + packetLength, NULL, 0);
	}
	
	pPing->Header.Payload = packetLength - 23;

	m_pComm->m_TableLocal.Insert(Guid, 0);
	if(pTCP)
		pTCP->SendPacket(m_PacketBuffer, packetLength, PACKET_PING, pPing->Header.Hops);
	else
		m_pDatagram->SendPacket(Target, m_PacketBuffer, packetLength);

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

	pTCP->SendPacket(&Reset, 29, PACKET_PATCH, Reset.Header.Hops);
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
					FourBitPatch[pos / 2]   = 10 << 4; // high -6 works with LW
				else
					//FourBitPatch[pos / 2] |= 15; // low -1
					FourBitPatch[pos / 2]   |= 10; // low -6 works with LW
			}
			// Patch turning off ( set positive value)
			else if( (PatchTable[i] & mask) > 0 && (pTCP->m_LocalHitTable[i] & mask) == 0)
			{
				if(pos % 2 == 0)
					//FourBitPatch[pos / 2] = 1 << 4; // high 1
					FourBitPatch[pos / 2]   = 6 << 4; // high 6 works with LW
				else
					//FourBitPatch[pos / 2] |= 1; // low 1
					FourBitPatch[pos / 2]   |= 6; // low 6 works with LW
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
	pTCP->SendPacket(PacketBuff, NextPos, PACKET_PATCH, PatchPacket->Header.Hops);

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
	QueryHit->Header.TTL	  = pQuery->Header.Hops + 1; // plus 1 to get through lime ultrapeer
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
	QHD->FlagGGEP   = true;
	QHD->FlagTrash  = 0;

	QHD->Push	= m_pNet->m_TcpFirewall;
	QHD->Bad	= 0;
	QHD->Busy	= Busy;
	QHD->Stable	= m_pNet->m_HaveUploaded;
	QHD->Speed	= m_pNet->m_RealSpeedUp ? true : false;
	QHD->GGEP   = false;
	QHD->Trash	= 0;


	// Get list of push proxies
	std::vector<IPv4> PushProxies;

	for(int i = 0; i < m_pComm->m_NodeList.size(); i++)
		if( m_pComm->m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER && m_pComm->m_NodeList[i]->m_PushProxy.Host.S_addr != 0)
			PushProxies.push_back(m_pComm->m_NodeList[i]->m_PushProxy);

	if( PushProxies.size() )
	{
		QHD->GGEP = true;

		byte* pGGEPStart = pQueryHit + packetLength;
	
		*pGGEPStart   = 0xC3; // ggep magic
		packetLength += 1;

		packet_GGEPBlock Block;
		memcpy(Block.Name, "PUSH", 4);
		//Block.Encoded = true;
		Block.Last    = true;

		int   GgepPayloadLength = PushProxies.size() * 6;
		byte* GgepPayload       = new byte[GgepPayloadLength];

		int x = 0;
		for(i = 0; i < PushProxies.size(); i++, x += 6)
			memcpy(GgepPayload + x, &PushProxies[i], 6);

		// Write ggep block
		packetLength += Encode_GGEPBlock(Block, pQueryHit + packetLength, GgepPayload, GgepPayloadLength);

		delete [] GgepPayload;
	}

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
	if(FileQuery.DirectAddress.Host.S_addr && m_pNet->m_UdpFirewall != UDP_BLOCK)
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
	for( i = 0; i < m_pComm->m_NodesBrowsing.size(); i++)
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
void CGnuProtocol::Send_Push(FileSource Download, IPv4 Proxy)
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

	if(Proxy.Host.S_addr)
	{
		Push.Host = Proxy.Host;
		Push.Port = Proxy.Port;
	}
	else
	{
		Push.Host = m_pNet->m_CurrentIP;
		Push.Port = m_pNet->m_CurrentPort;

		if(m_pPrefs->m_ForcedHost.S_addr)
			Push.Host = m_pPrefs->m_ForcedHost;
	}

	
	// Send Push
	for(int i = 0; i < m_pComm->m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_pComm->m_NodeList[i];

		if(p->m_NodeID == Download.GnuRouteID && p->m_Status == SOCK_CONNECTED)
		{
			if(Proxy.Host.S_addr == 0)
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

void CGnuProtocol::Send_Bye(CGnuNode* pTCP, CString Reason, int ErrorCode)
{
	GUID Guid;
	GnuCreateGuid(&Guid);
	
	int PacketSize = 23 + 2 + Reason.GetLength() + 1;
	byte* PacketData = new byte[PacketSize];
	
	packet_Bye* Bye =  (packet_Bye*) PacketData;
	Bye->Header.Guid		= Guid;
	Bye->Header.Function	= 0x02;
	Bye->Header.TTL			= 1;
	Bye->Header.Hops		= 0;
	Bye->Header.Payload		= PacketSize - 23;

	Bye->Code = ErrorCode;

	strcpy((char*) &PacketData[25], (LPCSTR) Reason);
	PacketData[PacketSize - 1] = NULL;

	pTCP->SendPacket(PacketData, PacketSize, PACKET_BYE, Bye->Header.Hops);

	delete [] PacketData;
}

void CGnuProtocol::Send_StatsMsg(CGnuNode* pTCP)
{
	packet_StatsMsg Stats;

	Stats.LeafMax	= MAX_LEAVES;
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
		SharedFile* pFile = &m_pShare->m_SharedFiles[*itIndex];

		if(pFile->Name.size() == 0)
			continue;

		if(MaxReplies && MaxReplies <= TotalReplyCount)	
			break;

		pFile->Matches++;

		// File Index
		memcpy(QueryReplyNext, &pFile->Index, 4);
		QueryReplyNext   += 4;
		QueryReplyLength += 4;

		// File Size
		if(pFile->Size < 0xFFFFFFFF)
			memcpy(QueryReplyNext, &pFile->Size, 4);
		else
			memset(QueryReplyNext, 0xFF, 4);

		QueryReplyNext   += 4;
		QueryReplyLength += 4;
		
		// File Name
		strcpy ((char*) QueryReplyNext, pFile->Name.c_str());
		QueryReplyNext   += pFile->Name.size() + 1;
		QueryReplyLength += pFile->Name.size() + 1;

		// If a large file, put all info in GGEP
		if(pFile->Size > 0xFFFFFFFF)
		{
			// ggep magic
			QueryReplyNext[0] = 0xC3; 
			QueryReplyNext++;
			QueryReplyLength++;
			
			// Hash Block
			if( !pFile->HashValues[HASH_SHA1].empty() )
			{
				packet_GGEPBlock H_Block;
				memcpy(H_Block.Name, "H", 1);
				H_Block.Encoded = true;

				byte hBuff[21];
				hBuff[0] = GGEP_H_SHA1;
				DecodeBase32( pFile->HashValues[HASH_SHA1].c_str(), 32, hBuff + 1, 20);
				
				int blockSize = Encode_GGEPBlock(H_Block, QueryReplyNext, hBuff, 21);

				QueryReplyNext   += blockSize;
				QueryReplyLength += blockSize;
			}

			// Large File Block
			packet_GGEPBlock LF_Block;
			memcpy(LF_Block.Name, "LF", 2);
			LF_Block.Encoded = true;
			LF_Block.Last    = true;

			byte lfBuff[8];
			int buffSize = EncodeLF(pFile->Size, lfBuff);
			
			int blockSize = Encode_GGEPBlock(LF_Block, QueryReplyNext, lfBuff, buffSize);

			QueryReplyNext   += blockSize;
			QueryReplyLength += blockSize;
		}

		// File Hash
		else if( !pFile->HashValues[HASH_SHA1].empty() )
		{
			strcpy ((char*) QueryReplyNext, "urn:sha1:");
			QueryReplyNext   += 9;
			QueryReplyLength += 9;

			strcpy ((char*) QueryReplyNext, pFile->HashValues[HASH_SHA1].c_str());
			QueryReplyNext   += pFile->HashValues[HASH_SHA1].size();
			QueryReplyLength += pFile->HashValues[HASH_SHA1].size();
		}

		

		// Add end null
		QueryReplyNext[0] = NULL;
		QueryReplyNext++;
		QueryReplyLength++;


		// File Meta
		if(pFile->MetaID)
		{
			std::map<int, CGnuSchema*>::iterator itMeta = m_pCore->m_pMeta->m_MetaIDMap.find(pFile->MetaID);
			if(itMeta != m_pCore->m_pMeta->m_MetaIDMap.end())
			{
				int InsertPos = MetaTail.Find("</" + itMeta->second->m_NamePlural + ">");

				if(InsertPos == -1)
				{
					MetaTail += "<" + itMeta->second->m_NamePlural + "></" + itMeta->second->m_NamePlural + ">";
					InsertPos = MetaTail.Find("</" + itMeta->second->m_NamePlural + ">");
				}
			
				MetaTail.Insert(InsertPos, itMeta->second->AttrMaptoNetXML(pFile->AttributeMap, ReplyCount));
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

	// decode cobs
	if(Block.Encoded && Block.PayloadSize && Block.PayloadSize < 1000)
	{
		byte* cobsBuff = new byte[1024];
		
		int cobslen = DecodeCobs(Block.Payload, Block.PayloadSize, cobsBuff);
		
		Block.Payload     = cobsBuff;
		Block.PayloadSize = cobslen - 1; // cobs decoder adds null to end of all data

		Block.Cleanup = true;
	}

	// decompress block
	if(Block.Compression && Block.PayloadSize)
	{
		byte* uncompressedBuff = new byte[1024];
		DWORD uncompressedSize = 1024;

		if( uncompress(uncompressedBuff, &uncompressedSize, Block.Payload, Block.PayloadSize) == Z_OK)
		{
			if(Block.Cleanup)
				delete [] Block.Payload;

			Block.Payload     = uncompressedBuff;
			Block.PayloadSize = uncompressedSize;

			Block.Cleanup = true;
		}
		else
			delete [] uncompressedBuff;
	}

	return BLOCK_GOOD;
}

int CGnuProtocol::Encode_GGEPBlock(packet_GGEPBlock &Block, byte* stream, byte* payload, uint32 length)
{
	int len = 0;

	// Set flags
	packet_GGEPHeaderFlags* pFlags = (packet_GGEPHeaderFlags*) stream;

	pFlags->Compression	= Block.Compression;
	pFlags->Encoding	= Block.Encoded;
	pFlags->Last		= Block.Last;
	pFlags->Reserved    = 0;
	pFlags->NameLength  = strlen( Block.Name );

	len++;

	bool deletePayload = false;

	// compress
	if(Block.Compression)
	{
		DWORD compressedLength  = length * 1.2 + 12;
		byte* compressedPayload = new byte[compressedLength];

		if( compress(compressedPayload, &compressedLength, payload, length) == Z_OK)
		{
			payload = compressedPayload;
			length  = compressedLength;

			deletePayload = true;
		}
		else
			delete [] compressedPayload;
	}

	// apply cobs
	if(Block.Encoded)
	{
		byte* cobsPayload = new byte[length + 10];

		int cobslen = EncodeCobs(payload, length, cobsPayload);

		if(deletePayload)
			delete [] payload;

		payload = cobsPayload;
		length  = cobslen;

		deletePayload = true;
	}

	// Set name
	memcpy(stream + len, Block.Name, pFlags->NameLength );

	len += pFlags->NameLength;

	// Set size
	int pos = 0;

	if(length & 0x3F000) // value in 12 - 17?
	{
		(stream + len)[pos]      = (length & 0x3F000) >> 12; // set cccccc
		(stream + len)[pos]     |= 0x80; // set another flag
		(stream + len)[pos + 1] |= 0x80; // set another flag
		(stream + len)[pos + 2] |= 0x40; // set last flag
		pos++;
	}

	if(length & 0xFC0 || pos > 0) // value in 6 - 11?
	{
		(stream + len)[pos]      = (length & 0xFC0) >> 6; // set bbbbbb
		(stream + len)[pos]     |= 0x80; // set another flag
		(stream + len)[pos + 1] |= 0x40; // set last flag
		pos++;
	}

	(stream + len)[pos]  = length & 0x3F; // set aaaaaa
	(stream + len)[pos] |= 0x40; // set last flag
	pos++;

	len += pos;

	// Set Payload
	memcpy(stream + len, payload, length);

	len += length;

	if(deletePayload)
		delete [] payload;

	return len;
}

void CGnuProtocol::TestGgep()
{
	byte* buffer = new byte[4024];

	for(int j = 0; j < 10000; j++)
	{
		// Encode ggep
		packet_GGEPBlock EncBlock;
		memcpy(EncBlock.Name, "Test", 4);
		EncBlock.Compression = true;
		EncBlock.Encoded = true;
		EncBlock.Last = true;

		byte payload[500];
		for(int i = 0; i < 500; i++)
			payload[i] = rand() % 3;

		uint32 buffsize = Encode_GGEPBlock(EncBlock, buffer, payload, 500);

		// Decode Ggep
		byte* decBuffer = buffer;
		packet_GGEPBlock DecBlock;
		GGEPReadResult result = Decode_GGEPBlock(DecBlock, decBuffer, buffsize);

		if(memcmp(payload, DecBlock.Payload, DecBlock.PayloadSize) != 0)
		{
			int x = 0;
			x++;
		}
	}
	
	delete [] buffer;
}

void CGnuProtocol::CheckGgepSize(int value)
{
	// Check building deconstructing packet
	
	// Encode ggep
	uint32 buffsize = 1024;
	byte* buffer = new byte[buffsize];
	
	packet_GGEPBlock EncBlock;
	memcpy(EncBlock.Name, "Test", 4);
	EncBlock.Last = true;

	char* message = "HELLO!!!";
	
	int blocksize = Encode_GGEPBlock(EncBlock, buffer, (byte*) message, strlen(message));

	// Decode ggep
	packet_GGEPBlock DecBlock;
	GGEPReadResult result = Decode_GGEPBlock(DecBlock, buffer, buffsize);

	////////////////////////////////////////////////////////////////////////////////////////

	// Checks encoding and decoding size value of ggep

	int len = value;
	uint32 final = 0;

	int pos = 0;

	if(len & 0x3F000) // value in 12 - 17?
	{
		((byte*) &final)[pos]      = (len & 0x3F000) >> 12; // set cccccc
		((byte*) &final)[pos]     |= 0x80; // set another flag
		((byte*) &final)[pos + 1] |= 0x80; // set another flag
		((byte*) &final)[pos + 2] |= 0x40; // set last flag
		pos++;
	}

	if(len & 0xFC0 || pos > 0) // value in 6 - 11?
	{
		((byte*) &final)[pos]      = (len & 0xFC0) >> 6; // set bbbbbb
		((byte*) &final)[pos]     |= 0x80; // set another flag
		((byte*) &final)[pos + 1] |= 0x40; // set last flag
		pos++;
	}

	((byte*) &final)[pos]  = len & 0x3F; // set aaaaaa
	((byte*) &final)[pos] |= 0x40; // set last flag
	pos++;

	// Read Payload Length
	byte b;
	int PayloadSize = 0;
	int finpos = 0;

	do 
	{
		b = ((byte*) &final)[finpos];
		finpos++;
		PayloadSize = (PayloadSize << 6) | (b & 0x3f);

	} while (0x40 != (b & 0x40));	

	ASSERT(len == PayloadSize);
}

#define FinishBlock(X) \
	(*code_ptr = (X),  \
	code_ptr = dst++,  \
	code = 0x01,	   \
	dstlen++)

int CGnuProtocol::EncodeCobs(byte* src, int length, byte* dst)
{
	int dstlen = 0;

	byte* end = src + length;

	byte* code_ptr = dst++;
	byte  code = 0x01;

	while( src < end )
	{
		if( *src == 0 )
			FinishBlock(code);
		else
		{
			*dst++ = *src;
			dstlen++;
			code++;
			if( code == 0xFF)
				FinishBlock(code);
		}

		src++;
	}

	FinishBlock(code);

	return dstlen;
}

int CGnuProtocol::DecodeCobs(byte* src, int length, byte* dst)
{
	int dstlen = 0;

	byte* end = src + length;

	while( src < end)
	{
		int code = *src++;

		for(int i = 1; i < code; i++)
		{
			*dst++ = *src++;
			dstlen++;
		}

		if( code < 0xFF)
		{
			*dst++ = 0;
			dstlen++;
		}
	}

	return dstlen;
}

void CGnuProtocol::TestCobs()
{
	byte source[10];
	memset(source, 1, 10);
	//for(int i = 0; i < 10; i++)
	//	source[i] = rand() % 3;

	byte dest[20];	
	memset(dest, 0xFF, 20);

	int dstlen = EncodeCobs(source, 10, dest);

	byte check[15];
	memset(check, 0xFF, 15);
	int chklen = DecodeCobs(dest, dstlen, check);

	int x = 0; 
	x++;
}

uint64 CGnuProtocol::DecodeLF(byte* data, int length)
{
	uint64 filesize, b;
	int i, j;

	if (length < 1 || length > 8)
		return 0;

	j = i = filesize = 0;
	
	do 
	{
		b = data[i];
		filesize |= b << j;
		j += 8;
	} while (++i < length);

	if (0 == b)
		return 0;

	return filesize;
}

int CGnuProtocol::EncodeLF(uint64 filesize, byte* data)
{
	byte* p = data;

	if (0 == filesize)
		return 0;

	do 
	{
		*p++ = filesize;
	} while (0 != (filesize >>= 8));

	return p - data;
}

void CGnuProtocol::TestLF()
{
	for(int i = 0; i < 100; i++)
	{
		uint64 origNum = rand();

		for(int y = 0; y < 2; y++)
			origNum *= rand();

		byte buffer[8];
		
		int buffSize = EncodeLF(origNum, buffer);

		uint64 checkNum = DecodeLF(buffer, buffSize);

		if(origNum != checkNum)
		{
			ASSERT(0);
		}
	}
}