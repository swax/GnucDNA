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


#include "stdafx.h"
#include "GnuCore.h"

#include "GnuNetworks.h"
#include "GnuPrefs.h"
#include "GnuTransfers.h"
#include "GnuRouting.h"
#include "GnuLocal.h"
#include "GnuCache.h"
#include "GnuSock.h"
#include "GnuNode.h"
#include "GnuShare.h"
#include "GnuMeta.h"
#include "GnuSchema.h"
#include "GnuUploadShell.h"
#include "GnuDownloadShell.h"
#include "GnuDownload.h"
#include "GnuSearch.h"
#include "G2Control.h"

#include "DnaCore.h"
#include "DnaNetwork.h"
#include "DnaSearch.h"
#include "DnaEvents.h"

#include "GnuControl.h"


CGnuControl::CGnuControl(CGnuNetworks* pNet)
{
	m_pNet		= pNet;
	m_pCore		= pNet->m_pCore;
	m_pPrefs	= m_pCore->m_pPrefs;
	m_pTrans	= m_pCore->m_pTrans;
	m_pCache	= pNet->m_pCache;
	m_pShare	= m_pCore->m_pShare;

	m_ClientUptime	 = CTime::GetCurrentTime();

	m_LanSock = new CGnuLocal(this);
	m_LanSock->Init();

	m_NetworkName		= "GNUTELLA";

//#ifdef _DEBUG
	m_GnuClientMode = GNU_ULTRAPEER;
//#else
//	m_GnuClientMode = GNU_LEAF;
//#endif

	m_ForcedUltrapeer = false;
	
	m_NormalConnectsApprox = 0;
	

	// Bandwidth
	m_NetSecBytesDown	= 0;
	m_NetSecBytesUp		= 0;

	m_ExtPongBytes		= 0;
	m_UltraPongBytes	= 0;
	m_SecCount			= 0;

	m_Minute = 0;
}

CGnuControl::~CGnuControl()
{
	TRACE0("*** CGnuControl Deconstructing\n");

	CGnuNode *deadNode = NULL;


	if( m_LanSock )
	{
		delete m_LanSock;
		m_LanSock = NULL;
	}

	// Destroy Browse List
	while( m_NodesBrowsing.size() )
	{
		delete m_NodesBrowsing.back();
		m_NodesBrowsing.pop_back();
	}


	// Destroy Node List
	m_NodeAccess.Lock();

		while( m_NodeList.size() )
		{
			delete m_NodeList.back();
			m_NodeList.pop_back();
		}

	m_NodeAccess.Unlock();
}


/////////////////////////////////////////////////////////////////////////////
// TRAFFIC CONTROL

void CGnuControl::Broadcast_Ping(packet_Ping* Ping, int length, CGnuNode* exception)
{
	if(m_GnuClientMode == GNU_LEAF)
		return;

	for(int i = 0; i < m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_NodeList[i];
	
		if(m_GnuClientMode == GNU_ULTRAPEER && p->m_GnuNodeMode == GNU_LEAF)
			continue;
			
		if(p != exception && p->m_Status == SOCK_CONNECTED)
			p->SendPacket(Ping, length, PACKET_PING, Ping->Header.Hops);
	}
}

void CGnuControl::Broadcast_Query(packet_Query* Query, int length, CGnuNode* exception)
{
	if(m_GnuClientMode == GNU_LEAF)
		return;

	for(int i = 0; i < m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_NodeList[i];

		if(m_GnuClientMode == GNU_ULTRAPEER && p->m_GnuNodeMode == GNU_LEAF)
			continue;

		if(Query->Header.TTL == 1 && p->m_SupportInterQRP)
			continue;

		if(p != exception && p->m_Status == SOCK_CONNECTED)
			p->SendPacket(Query, length, PACKET_QUERY, Query->Header.Hops);
	}
}

void CGnuControl::Broadcast_LocalQuery(byte* Packet, int length)
{
	/* See if any downloads have the same keywords and use same guid
	CString QueryText = (char*) Packet + 25;
	
	if(!QueryText.IsEmpty())
		for(i = 0; i < m_pTrans->m_DownloadList.size(); i++)
			if(m_pTrans->m_DownloadList[i]->m_Search.Find(QueryText) == 0)
				Guid = m_pTrans->m_DownloadList[i]->m_SearchGuid;*/


	packet_Query* Query = (packet_Query*) Packet;
	
	//Query->Header.Guid	= // Already added before this is called
	Query->Header.Function	= 0x80;
	Query->Header.Hops		= 0;
	Query->Header.TTL		= MAX_TTL;
	Query->Header.Payload	= length - 23;


	// New MinSpeed Field
	Query->Speed = 0;		 // bit 0 to 8, Was reserved to indicate the number of max query hits expected, 0 if no maximum   
	/*Query->Speed |= 1 << 10; // bit 10, I understand and desire Out Of Band queryhits via UDP   
	
	//Query->Speed |= 1 << 13; // bit 11 I understand the H GGEP extension in queryhits
	//Query->Speed |= 1 << 13; // bit 12 Leaf guided dynamic querying   

	Query->Speed |= 1 << 13; // bit 13, I understand and want XML metadata in query hits   
		
	//if(m_pNet->m_TcpFirewall)
		Query->Speed |= 1 << 14; // bit 14, I am firewalled, please reply only if not firewalled 

	Query->Speed |= 1 << 15; // bit 15, Special meaning of the minspeed field, has to be always set.  
*/

	m_TableLocal.Insert(Query->Header.Guid, 0);

	// Send Query
	for(int i = 0; i < m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_NodeList[i];
	
		if(p->m_Status == SOCK_CONNECTED)
			p->SendPacket(Packet, length, PACKET_QUERY, Query->Header.Hops);
	}

// Test sending query to leaf based on hash table
// Make sure debug in UP mode, above uncommented
/*#ifdef _DEBUG
	
	// Inspect
	int QuerySize  = Query->Header.Payload - 2;
	int TextSize   = strlen((char*) Query + 25) + 1;

	CString ExtendedQuery;

	if (TextSize < QuerySize)
	{
		int ExtendedSize = strlen((char*) Query + 25 + TextSize);
	
		if(ExtendedSize)
			ExtendedQuery = CString((char*) Query + 25 + TextSize, ExtendedSize);
	}

	// Queue to be compared with local files
	GnuQuery G1Query;
	G1Query.Network    = NETWORK_GNUTELLA;
	G1Query.OriginID   = 0;
	G1Query.SearchGuid = Query->Header.Guid;

	if(m_GnuClientMode == GNU_ULTRAPEER)
	{
		G1Query.Forward = true;

		memcpy(G1Query.Packet, (byte*) Query, length);
		G1Query.PacketSize = length;
	}

	G1Query.Terms.push_back( CString((char*) Query + 25, TextSize) );

	while(!ExtendedQuery.IsEmpty())
		G1Query.Terms.push_back( ParseString(ExtendedQuery, 0x1C) );


	m_pShare->m_QueueAccess.Lock();
		m_pShare->m_PendingQueries.push_front(G1Query);	
	m_pShare->m_QueueAccess.Unlock();


	m_pShare->m_TriggerThread.SetEvent();

#endif*/

}


void CGnuControl::Route_Pong(packet_Pong* Pong, int length, int RouteID)
{
	std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(RouteID);

	if(itNode != m_NodeIDMap.end())
		if(itNode->second->m_Status == SOCK_CONNECTED)
			itNode->second->SendPacket(Pong, length, PACKET_PONG, Pong->Header.TTL - 1);
}

void CGnuControl::Route_UltraPong(packet_Pong* Pong, int length, int RouteID)
{	
	if(m_GnuClientMode == GNU_LEAF)
		return;

	// Send a max of 2KB in pongs a sec to children
	if(m_UltraPongBytes > 2048)
		return;

	bool HostsFound = false;

	// Send pongs to children
	std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(RouteID);

	if(itNode != m_NodeIDMap.end())
	{
		CGnuNode* p = itNode->second;

		if(m_GnuClientMode == GNU_ULTRAPEER && p->m_GnuNodeMode == GNU_LEAF)
			if(p->m_Status == SOCK_CONNECTED && !p->m_UltraPongSent)
			{
				p->SendPacket(Pong, length, PACKET_PONG, Pong->Header.TTL - 1);
				m_UltraPongBytes += 37;

				p->m_UltraPongSent = true;
				HostsFound = true;
				
			}
	}

	// If all children sent ultra-pongs, reset list
	if(!HostsFound)
		for(int i = 0; i < m_NodeList.size(); i++)
			m_NodeList[i]->m_UltraPongSent = false;

}

void CGnuControl::Route_QueryHit(packet_QueryHit* QueryHit, DWORD length, int RouteID)
{
	std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(RouteID);

	if(itNode != m_NodeIDMap.end())
		if(itNode->second->m_Status == SOCK_CONNECTED)
			itNode->second->SendPacket(QueryHit, length, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, false);
}

// Called from share thread
void CGnuControl::Route_LocalQueryHit(GnuQuery &FileQuery, byte* pQueryHit, DWORD ReplyLength, byte ReplyCount, CString &MetaTail)
{	
	// FilesAccess must be unlocked before calling this

	m_NodeAccess.Lock();

		std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(FileQuery.OriginID);

		if(itNode != m_NodeIDMap.end())
			if(itNode->second->m_Status == SOCK_CONNECTED)
			{
				itNode->second->Send_QueryHit(FileQuery, pQueryHit, ReplyLength, ReplyCount, MetaTail);
				m_NodeAccess.Unlock();
				return;
			}

	m_NodeAccess.Unlock();

	// Node not found, check if its a node browsing files
	for(int i = 0; i < m_NodesBrowsing.size(); i++)
		if(m_NodesBrowsing[i]->m_NodeID == FileQuery.OriginID)
		{
			m_NodesBrowsing[i]->Send_QueryHit(FileQuery, pQueryHit, ReplyLength, ReplyCount, MetaTail);
			break;
		}
}

void CGnuControl::Route_Push(packet_Push* Push, int length, int RouteID)
{
	std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(RouteID);

	if(itNode != m_NodeIDMap.end())
		if(itNode->second->m_Status == SOCK_CONNECTED)
			itNode->second->SendPacket(Push, length, PACKET_PUSH, Push->Header.TTL - 1);
}

void CGnuControl::Route_LocalPush(FileSource Download)
{
	GUID Guid = GUID_NULL;
	GnuCreateGuid(&Guid);
	if (Guid == GUID_NULL)
	{
		//m_pCore->LogError("Failed to create a GUID to send.");
		return;
	}

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
	for(int i = 0; i < m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_NodeList[i];

		if(p->m_NodeID == Download.GnuRouteID && p->m_Status == SOCK_CONNECTED)
		{
			m_TableLocal.Insert(Guid, p->m_NodeID);

			p->SendPacket(&Push, 49, PACKET_PUSH, Push.Header.TTL - 1);
		}
	}
}

// Called from share thread
void CGnuControl::Encode_QueryHit(GnuQuery &FileQuery, std::list<UINT> &MatchingIndexes, byte* QueryReply)
{	
	byte*	 QueryReplyNext		= &QueryReply[34];
	DWORD	 QueryReplyLength	= 0;
	UINT	 TotalReplyCount	= 0;
	byte	 ReplyCount			= 0;
	int		 MaxReplies			= m_pPrefs->m_MaxReplies;
	CString  MetaTail			= "";

	m_pShare->m_FilesAccess.Lock();

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

		if(QueryReplyLength > 2048 || ReplyCount == 255)
		{
			//m_pShare->m_FilesAccess.Unlock();
			Route_LocalQueryHit(FileQuery, QueryReply, QueryReplyLength, ReplyCount, MetaTail);
			//m_pShare->m_FilesAccess.Lock();

			QueryReplyNext	 = &QueryReply[34];
			QueryReplyLength = 0;
			ReplyCount		 = 0;
			MetaTail		 = "";
		}
	}


	if(ReplyCount > 0)
	{
		//m_pShare->m_FilesAccess.Unlock();
		Route_LocalQueryHit(FileQuery, QueryReply, QueryReplyLength, ReplyCount, MetaTail);
		//m_pShare->m_FilesAccess.Lock();
	}

	m_pShare->m_FilesAccess.Unlock();
}

// Called from share thread
void CGnuControl::Forward_Query(GnuQuery &FileQuery, std::list<int> &MatchingNodes)
{
	// Forward query to child nodes that match the query

	packet_Query* pQuery = (packet_Query*) FileQuery.Packet;
	if(pQuery->Header.TTL == 0)
		pQuery->Header.TTL++;
	if(pQuery->Header.Hops == MAX_TTL)
		pQuery->Header.Hops--;

	std::list<int>::iterator  itNodeID;

	for(itNodeID = MatchingNodes.begin(); itNodeID != MatchingNodes.end(); itNodeID++)
		if(*itNodeID != FileQuery.OriginID)
		{
			m_NodeAccess.Lock();

				std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(*itNodeID);

				if(itNode != m_NodeIDMap.end())
				{
					CGnuNode* pNode = itNode->second;

					if(pNode->m_Status == SOCK_CONNECTED)
					{
						if( pNode->m_GnuNodeMode == GNU_LEAF)
							pNode->Send_ForwardQuery( FileQuery );

						if( pNode->m_GnuNodeMode == GNU_ULTRAPEER && pNode->m_SupportInterQRP)
							pNode->Send_ForwardQuery( FileQuery );
					}
				}

			m_NodeAccess.Unlock();
		}
}

////////////////////////////////////////////////////////////////////////////
// Node control

void CGnuControl::AddNode(CString Host, UINT Port)
{
	if(FindNode(Host, Port) != NULL)
		return;

	CGnuNode* Node = new CGnuNode(this, Host, Port);

	//Attempt to connect to node
	if(!Node->Create())
	{
		m_pCore->LogError("Node Create Error: " + NumtoStr(Node->GetLastError()));
	
		delete Node;
		return;
	}
	
	if( !Node->Connect(Host, Port) )
		if (Node->GetLastError() != WSAEWOULDBLOCK)
		{
			m_pCore->LogError("Node Connect Error : " + NumtoStr(Node->GetLastError()));
				
			delete Node;
			return;
		}
	

	// Add node to list
	m_NodeAccess.Lock();
	m_NodeList.push_back(Node);
	m_NodeAccess.Unlock();
		

	NodeUpdate(Node);
}

void CGnuControl::RemoveNode(CGnuNode* pNode)
{
	std::vector<CGnuNode*>::iterator itNode;
	for(itNode = m_NodeList.begin(); itNode != m_NodeList.end(); itNode++)
		if(*itNode == pNode)
			pNode->CloseWithReason("Manually Removed");

	NodeUpdate(pNode);
}

CGnuNode* CGnuControl::FindNode(CString Host, UINT Port)
{
	std::map<uint32, CGnuNode*>::iterator itNode = m_GnuNodeAddrMap.find( StrtoIP(Host).S_addr);

	if(itNode != m_GnuNodeAddrMap.end())
		return itNode->second;

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
// TRANSFER CONTROL

int CGnuControl::CountNormalConnects()
{
	int NormalConnects = 0;

	for(int i = 0; i < m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_NodeList[i];

		if(p->m_Status == SOCK_CONNECTED)
		{
			if(m_GnuClientMode ==GNU_ULTRAPEER)
			{
				if(p->m_GnuNodeMode != GNU_LEAF)
					NormalConnects++;
			}
			else
				NormalConnects++;
		}
	}

	m_NormalConnectsApprox = NormalConnects;

	return NormalConnects;
}

int CGnuControl::CountSuperConnects()
{
	int SuperConnects = 0;

	for(int i = 0; i < m_NodeList.size(); i++)	
	{
		CGnuNode *p = m_NodeList[i];

		if(p->m_Status == SOCK_CONNECTED)
			if(p->m_GnuNodeMode == GNU_ULTRAPEER)
				SuperConnects++;
	}

	return SuperConnects;
}

int CGnuControl::CountLeafConnects()
{
	int LeafConnects = 0;

	if(m_GnuClientMode == GNU_ULTRAPEER)
		for(int i = 0; i < m_NodeList.size(); i++)	
		{
			CGnuNode *p = m_NodeList[i];

			if(p->m_Status == SOCK_CONNECTED)			
				if(p->m_GnuNodeMode == GNU_LEAF)
					LeafConnects++;
		}

	return LeafConnects;
}

/////////////////////////////////////////////////////////////////////////////
// This is where the Network side of Gnucleus talks with the Interface side

void CGnuControl::NodeUpdate(CGnuNode* pNode)
{
	if(pNode->m_BrowseID)
	{
		int Completed = 0;

		if(pNode->m_BrowseSize)
			Completed = pNode->m_BrowseRecvBytes * 100 / pNode->m_BrowseSize;
	
		if(m_pCore->m_dnaCore->m_dnaEvents)
			m_pCore->m_dnaCore->m_dnaEvents->SearchBrowseUpdate(pNode->m_BrowseID, pNode->m_Status, Completed);
	}
	else
	{
		if(m_pCore->m_dnaCore->m_dnaEvents)
			m_pCore->m_dnaCore->m_dnaEvents->NetworkChange(pNode->m_NodeID);
	}

	for(int i = 0; i < m_pNet->m_SearchList.size(); i++)
		m_pNet->m_SearchList[i]->SockUpdate();
}

void CGnuControl::PacketIncoming(int NodeID, byte* packet, int size, int ErrorCode, bool Local)
{
	std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(NodeID);

	if(itNode != m_NodeIDMap.end() && m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketIncoming(NETWORK_GNUTELLA, true, StrtoIP(itNode->second->m_HostIP).S_addr, itNode->second->m_Port, packet, size, Local, ErrorCode );
}

void CGnuControl::PacketOutgoing(int NodeID, byte* packet, int size, bool Local)
{
	std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(NodeID);

	if(itNode != m_NodeIDMap.end() && m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketOutgoing(NETWORK_GNUTELLA, true, StrtoIP(itNode->second->m_HostIP).S_addr, itNode->second->m_Port, packet, size, Local );
}


void CGnuControl::Timer()
{
	CleanDeadSocks();
	
	ManageNodes();

	/*m_Minute++;

	if(m_Minute == 60)
	{
		int BufferSize = 0;
		int BackSize   = 0;

		for(i = 0; i < m_NodeList.size(); i++)
		{
			BackSize += m_NodeList[i]->m_BackBuffLength;

			for(int j = 0; j < MAX_TTL; j++)
				BufferSize += m_NodeList[i]->m_PacketListLength[j];
		}

		m_pCore->DebugLog( CommaIze(NumtoStr(BufferSize / 1024)) + "KB Packet Buffer," + CommaIze(NumtoStr(BackSize / 1024)) + "KB Back Buffer");

		m_Minute = 0; 
	}*/
}

void CGnuControl::HourlyTimer()
{
	// Web Cache check in
	if(!m_GnuClientMode == GNU_ULTRAPEER && !m_pNet->m_TcpFirewall)
		m_pCache->WebCacheUpdate();
}

void CGnuControl::ManageNodes()
{
	int Connecting     = 0;
	int NormalConnects = 0;
	int UltraConnects  = 0;
	int LeafConnects   = 0;
	int DnaConnects    = 0;

	m_NetSecBytesDown = 0;
	m_NetSecBytesUp	  = 0;


	// Add up bandwidth used by all nodes and take count
	for(int i = 0; i < m_NodeList.size(); i++)
	{
		if(SOCK_CONNECTED == m_NodeList[i]->m_Status)
		{
			m_NetSecBytesDown += m_NodeList[i]->m_AvgBytes[0].GetAverage();
			m_NetSecBytesUp   += m_NodeList[i]->m_AvgBytes[1].GetAverage();
			
				
			// Client in ultrapeer mode
			if(m_GnuClientMode == GNU_ULTRAPEER)
			{
				if(m_NodeList[i]->m_GnuNodeMode == GNU_LEAF)
					LeafConnects++;
				else
				{
					NormalConnects++;

					if(m_NodeList[i]->m_RemoteAgent.Find("GnucDNA") > 0)
						DnaConnects++;

					if(m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER)
						UltraConnects++;
				}
			}

			// Client in child mode
			else
			{
				NormalConnects++;

				if(m_NodeList[i]->m_RemoteAgent.Find("GnucDNA") > 0)
					DnaConnects++;
			}
		}
		else if(SOCK_CONNECTING == m_NodeList[i]->m_Status)
			Connecting++;

		m_NodeList[i]->Timer();
	}

	m_NormalConnectsApprox = NormalConnects;


	// Minute counter
	if(m_SecCount < 60)
		m_SecCount++;
	if(m_SecCount == 60)
	{
		m_UltraPongBytes = 0;
		m_SecCount = 0;
	}

	m_ExtPongBytes = 0;


	// Maybe go to ultrapeer after an amount of time?
	// No connects, reset leaf mode and check if we are supernode able
	/*if(NormalConnects == 0)
	{
		if(m_LeafModeActive)
			m_LeafModeActive = false;

		SuperNodeReady();
	}*/

	// Keep connecting less than 5

	if(m_GnuClientMode == GNU_LEAF)
	{
		if(Connecting < 5)
		{
			if(NormalConnects < m_pPrefs->m_LeafModeConnects)
				AddConnect();

			else if(NormalConnects && DnaConnects * 100 / NormalConnects < 50)
				AddConnect();
		}

		if(m_pPrefs->m_LeafModeConnects && NormalConnects > m_pPrefs->m_LeafModeConnects)
			DropNode();
	}
	
	if(m_GnuClientMode == GNU_ULTRAPEER)
	{
		if(m_pPrefs->m_MinConnects)
			if(NormalConnects < m_pPrefs->m_MinConnects)
				if(Connecting < 5)
				{
					AddConnect();
					return;
				}

		if(m_pPrefs->m_MaxConnects)
			if(NormalConnects > m_pPrefs->m_MaxConnects)
				DropNode();

		// Keep a 2/3rds connect to ultrapeers
		if(m_GnuClientMode == GNU_ULTRAPEER && NormalConnects)
			if((UltraConnects * 100 / NormalConnects < 66) ||
				(DnaConnects * 100   / NormalConnects < 50))
				if(Connecting < 5)
					AddConnect();

		while(LeafConnects > m_pPrefs->m_MaxLeaves)
		{
			DropLeaf();
			LeafConnects--;
		}
	}
}

void CGnuControl::AddConnect()
{
	if( !m_pNet->ConnectingSlotsOpen() )
		return;


	// If Real list has values
	if(m_pCache->m_GnuReal.size())
	{
		int randIndex = ( m_pCache->m_GnuReal.size() > 10) ? rand() % m_pCache->m_GnuReal.size() : rand() % 10;

		std::list<Node>::iterator itNode = m_pCache->m_GnuReal.begin();
		for(int i = 0; itNode != m_pCache->m_GnuReal.end(); itNode++, i++)
			if(i == randIndex)
			{
				AddNode( (*itNode).Host, (*itNode).Port);
				m_pCache->m_GnuReal.erase(itNode);
				return;
			}
	}


	// No nodes in cache, if not connected to anyone either, web cache update
	if(CountNormalConnects() == 0)
		m_pCache->WebCacheRequest(true);


	// If permanent list has values
	if(m_pCache->m_GnuPerm.size())
	{
		int randIndex = rand() % m_pCache->m_GnuPerm.size();

		std::list<Node>::iterator itNode = m_pCache->m_GnuPerm.begin();
		for(int i = 0; itNode != m_pCache->m_GnuPerm.end(); itNode++, i++)
			if(i == randIndex)
			{
				AddNode( (*itNode).Host, (*itNode).Port);
				return;
			}
	}	
}

void CGnuControl::DropNode()
{
	
	for(int mode = 0; mode < 2; mode++)
	{
		CGnuNode* DeadNode = NULL;
		CTime CurrentTime = CTime::GetCurrentTime();
		CTimeSpan LowestTime(0);

		// Drop Normal nodes first
		for(int i = 0; i < m_NodeList.size(); i++)
			if(SOCK_CONNECTED == m_NodeList[i]->m_Status)
			{
				if((mode == 0 && m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER && m_NodeList[i]->m_RemoteAgent.Find("GnucDNA") == -1) ||
				   (mode == 1 && m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER))

					if(LowestTime.GetTimeSpan() == 0 || CurrentTime - m_NodeList[i]->m_ConnectTime < LowestTime)
					{
						DeadNode	 = m_NodeList[i];
						LowestTime   = CurrentTime - m_NodeList[i]->m_ConnectTime;
					}
			}

		if(DeadNode)
		{
			DeadNode->CloseWithReason("Node Bumped");
			return;
		}
	}
}

void CGnuControl::DropLeaf()
{
	for(int i = 0; i < 2; i++)
	{
		CGnuNode* DeadNode = NULL;
		CTime CurrentTime = CTime::GetCurrentTime();
		CTimeSpan LowestTime(0);

		for(int j = 0; j < m_NodeList.size(); j++)
			if(SOCK_CONNECTED == m_NodeList[j]->m_Status && m_NodeList[j]->m_GnuNodeMode == GNU_LEAF) 
			{
				if((i == 0 && m_NodeList[j]->m_RemoteAgent.Find("GnucDNA") == -1) ||
				   (i == 1))
					
					if(LowestTime.GetTimeSpan() == 0 || CurrentTime - m_NodeList[j]->m_ConnectTime < LowestTime)
					{
						DeadNode	 = m_NodeList[j];
						LowestTime   = CurrentTime - m_NodeList[j]->m_ConnectTime;
					}
			}

		if(DeadNode)
		{
			DeadNode->CloseWithReason("Leaf Bumped");
			return;
		}
	}
}

void CGnuControl::ShareUpdate()
{
	m_NodeAccess.Lock();
		
	for(int i = 0; i < m_NodeList.size(); i++)	
		if(m_NodeList[i]->m_Status == SOCK_CONNECTED && m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER)
			m_NodeList[i]->m_SendDelayPatch = true;
	
	m_NodeAccess.Unlock();
}

void CGnuControl::CleanDeadSocks()
{
	// Node Sockets
	std::vector<CGnuNode*>::iterator itNode;

	itNode = m_NodeList.begin();
	while( itNode != m_NodeList.end())
		if((*itNode)->m_Status == SOCK_CLOSED && (*itNode)->m_CloseWait > 3)
		{
			m_NodeAccess.Lock();

			delete *itNode;

			itNode = m_NodeList.erase(itNode);
			
			m_NodeAccess.Unlock();
		}
		else
			itNode++;

	// Browse Sockets
	std::vector<CGnuNode*>::iterator itBrowse;

	itBrowse = m_NodesBrowsing.begin();
	while(itBrowse != m_NodesBrowsing.end())
		if((*itBrowse)->m_Status == SOCK_CLOSED)
		{
			delete *itBrowse;
			
			itBrowse = m_NodesBrowsing.erase(itBrowse);
		}
		else
			itBrowse++;
}

bool CGnuControl::UltrapeerAble()
{
	// Check prefs
	if(!m_pPrefs->m_SupernodeAble)
		return false;

	// Must be an NT based system
	if(!m_pCore->m_IsKernalNT)
		return false;
	
	// Cant be behind firewall
	if(m_pNet->m_TcpFirewall)
		return false;

	// Need an uptime of at least an 3 hours
	if( time(NULL) - m_ClientUptime.GetTime() < 3*60*60)
		return false;

	if(m_pNet->m_pG2)
		if(m_pNet->m_pG2->m_ClientMode == G2_HUB)
			return false;

	return true;
}


void CGnuControl::DowngradeClient()
{
	if(m_GnuClientMode == GNU_ULTRAPEER)
		for(int i = 0; i < m_NodeList.size(); i++)
		{
			CGnuNode *p = m_NodeList[i];

			p->CloseWithReason("Node Downgrading");
		}

	m_GnuClientMode   = GNU_LEAF;
	m_ForcedUltrapeer = false;
}

void CGnuControl::SwitchGnuClientMode(int GnuMode)
{
	// Requested mode already set
	if( m_GnuClientMode == GnuMode )
		return;

	// Remove all connections
	while( m_NodeList.size() )
	{
		delete m_NodeList.back();
		m_NodeList.pop_back();
	}

	// Change mode
	m_GnuClientMode = GnuMode;
}
