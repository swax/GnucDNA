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
#include "GnuDatagram.h"
#include "GnuProtocol.h"

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
//	m_GnuClientMode = GNU_LEAF;
//#else
	m_GnuClientMode = GNU_ULTRAPEER;
//#endif

	m_ForcedUltrapeer = false;
	
	m_NormalConnectsApprox = 0;
	

	// Bandwidth
	m_NetSecBytesDown	= 0;
	m_NetSecBytesUp		= 0;

	m_Minute = 0;

	m_UdpPort = 0;

	m_pDatagram = new CGnuDatagram(this);
	m_pProtocol = new CGnuProtocol(this);
}

CGnuControl::~CGnuControl()
{
	TRACE0("*** CGnuControl Deconstructing\n");

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

	std::map<uint32, DynQuery*>::iterator itDyn = m_DynamicQueries.begin();
	while(itDyn != m_DynamicQueries.end())
	{
		delete itDyn->second;
		itDyn = m_DynamicQueries.erase(itDyn);
	}

	delete m_pDatagram;
	m_pDatagram = NULL;

	delete m_pProtocol;
	m_pProtocol = NULL;
}

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

void CGnuControl::Timer()
{
	CleanDeadSocks();
	
	ManageNodes();

	DynQueryTimer();

	m_pDatagram->Timer();

	// Send random pong to random child
	if(m_GnuClientMode == GNU_ULTRAPEER)
	{
		CGnuNode* pUltra = GetRandNode(GNU_ULTRAPEER);
		CGnuNode* pLeaf  = GetRandNode(GNU_LEAF);

		if(pUltra && pLeaf)
		{
			// Build a pong
			packet_Pong Pong;
			GnuCreateGuid(&Pong.Header.Guid);
			Pong.Port			= pUltra->m_Address.Port;
			Pong.Host			= pUltra->m_Address.Host;
			Pong.FileCount		= 0;
			Pong.FileSize		= 1024;

			m_pProtocol->Send_Pong(pLeaf, Pong);
		}
	}

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

void CGnuControl::StopSearch(GUID SearchGuid)
{
	if(m_pNet->m_pGnu->m_GnuClientMode != GNU_LEAF)
		return;

	packet_VendMsg ReplyMsg;
	ReplyMsg.Header.Guid = SearchGuid;
	ReplyMsg.Ident = packet_VendIdent("BEAR", 12, 1);
	
	uint16 Hits = 0xFFFF;

	for(int i = 0; i < m_NodeList.size(); i++)	
		if(m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER && 
			m_NodeList[i]->m_Status == SOCK_CONNECTED &&
			m_NodeList[i]->m_SupportsVendorMsg)
			m_pProtocol->Send_VendMsg(m_NodeList[i], ReplyMsg, (byte*) &Hits, 2);
}

void CGnuControl::AddDynQuery(DynQuery* pQuery)
{
	packet_Query* pPacket = (packet_Query*) pQuery->Packet;

	std::map<uint32, DynQuery*>::iterator itDyn = m_DynamicQueries.find( HashGuid(pPacket->Header.Guid) );
	if( itDyn != m_DynamicQueries.end() )
	{
		delete pQuery;
		return;
	}
		
	// Search for other queries started by this node
	int InProgress = 0;
	int OldestAge  = 0;
	std::map<uint32, DynQuery*>::iterator itOldest;
	 
	for(itDyn = m_DynamicQueries.begin(); itDyn != m_DynamicQueries.end(); itDyn++)
		if(itDyn->second->NodeID == pQuery->NodeID)
		{
			InProgress++;

			if(itDyn->second->Secs > OldestAge)
			{
				OldestAge = itDyn->second->Secs;
				itOldest  = itDyn;
			}
		}

	// If too many in progress, end oldest search and start this new one
	if(InProgress > DQ_MAX_QUERIES)
	{
		delete itOldest->second;
		m_DynamicQueries.erase(itOldest);
	}

	// Dynamic Query ID:15 Created
	TRACE0("Dynamic Query ID:" + NumtoStr(pQuery->NodeID) + " Created\n");
	m_DynamicQueries[ HashGuid(pPacket->Header.Guid) ] = pQuery;
}

void CGnuControl::DynQueryTimer()
{
	std::map<uint32, DynQuery*>::iterator itDyn = m_DynamicQueries.begin();
	while(itDyn != m_DynamicQueries.end())
	{
		DynQuery* pQuery = itDyn->second;

		std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(pQuery->NodeID);
		
		if(itNode == m_NodeIDMap.end()   ||   // Leaf disconnected
		   pQuery->Hits > DQ_TARGET_HITS ||   // Target hits reached
		   pQuery->Secs > DQ_QUERY_TIMEOUT )  // Query's time over
		{
			delete pQuery;
			itDyn = m_DynamicQueries.erase(itDyn);
			continue;
		}

	
		pQuery->Secs++;	// Increase time query has been alive
		

		// Send query to node each interval
		if(pQuery->Secs % DQ_QUERY_INTERVAL == 0)
		{
			int Attempts = 0;
			while(Attempts < 10)
			{
				// Pick random ultrapeer to query next
				CGnuNode* pUltra = GetRandNode(GNU_ULTRAPEER);

				std::map<int, bool>::iterator itQueried = pQuery->NodesQueried.find(pUltra->m_NodeID);
				if(itQueried == pQuery->NodesQueried.end())
				{
					pUltra->SendPacket(pQuery->Packet, pQuery->PacketLength, PACKET_QUERY, MAX_TTL);

					pQuery->NodesQueried[pUltra->m_NodeID] = true;
					break;
				}

				Attempts++;
			}
		}
		

		// Send request to leaf to update hit count each interval
		if(pQuery->Secs % DQ_UPDATE_INTERVAL == 0 && itNode->second->m_SupportsLeafGuidance)
		{
			CGnuNode* pLeaf = itNode->second;

			packet_VendMsg StatusReq;
			memcpy(&StatusReq.Header.Guid, pQuery->Packet, 16);
			StatusReq.Ident = packet_VendIdent("BEAR", 11, 1);
			m_pProtocol->Send_VendMsg( pLeaf, StatusReq );
		}


		itDyn++;
	}
}


CGnuNode* CGnuControl::GetRandNode(int Type)
{
	int Nodes = 0;

	for(int i = 0; i < m_NodeList.size(); i++)
		if(m_NodeList[i]->m_Status == SOCK_CONNECTED)
			if(m_NodeList[i]->m_GnuNodeMode == Type)
				Nodes++;

	if(Nodes)
	{
		int upReturn = rand() % Nodes;
		int upCurrent = 0;

		for(int i = 0; i < m_NodeList.size(); i++)
			if(m_NodeList[i]->m_Status == SOCK_CONNECTED)
				if(m_NodeList[i]->m_GnuNodeMode == Type)
				{
					if( upCurrent == upReturn)
						return m_NodeList[i];
					else
						upCurrent++;
				}
	}

	return NULL;
}

DWORD CGnuControl::GetSpeed()
{	
	if(m_pNet->m_RealSpeedUp)
		return m_pNet->m_RealSpeedUp * 8 / 1024;
	else
	{
		if(m_pPrefs->m_SpeedStatic)
			return m_pPrefs->m_SpeedStatic;
		else
			return m_pNet->m_RealSpeedDown * 8 / 1024;
	}

	return 0;
}