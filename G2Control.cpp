/********************************************************************************

	GnucDNA - A Gnutella Library
    Copyright (C) 2000-2004 John Marshall

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

#include "DnaCore.h"
#include "DnaNetwork.h"
#include "DnaSearch.h"
#include "DnaEvents.h"

#include "hash/md5.h"

#include "GnuCore.h"
#include "GnuNetworks.h"
#include "GnuCache.h"
#include "GnuPrefs.h"
#include "GnuShare.h"
#include "GnuMeta.h"
#include "GnuSchema.h"
#include "GnuWordHash.h"
#include "GnuSearch.h"
#include "GnuTransfers.h"
#include "GnuUploadQueue.h"
#include "GnuDownloadShell.h"
#include "GnuControl.h"
#include "GnuNode.h"
#include "GnuChat.h"

#include "G2Datagram.h"
#include "G2Protocol.h"
#include "G2Control.h"
#include "G2Node.h"


// CG2Control

CG2Control::CG2Control(CGnuNetworks* pNet)
{
	m_pNet   = pNet;
	m_pCache = pNet->m_pCache;
	m_pCore  = pNet->m_pCore;
	m_pPrefs = m_pCore->m_pPrefs;
	m_pShare = m_pCore->m_pShare;
	m_pTrans = m_pCore->m_pTrans;

	m_ClientMode   = G2_CHILD;
	m_ClientUptime = CTime::GetCurrentTime();
	
	srand((unsigned)time(NULL));
	m_ClientIdent  = rand() * rand();
	
	m_ForcedHub = false;

	m_GlobalUnique = rand() * rand();

	// Stagger cleanup every 5 mins
	m_CleanRoutesNext = time(NULL) + 2*60;
	m_CleanKeysNext   = time(NULL) + 1*60;
	m_CleanGlobalNext = time(NULL) + 3*60;

	// Hub Balancing
	m_HubBalanceCheck = 60;
	m_NextUpgrade     = time(NULL);

	// Bandwidth
	m_NetSecBytesDown	= 0;
	m_NetSecBytesUp		= 0;

	// Avg Query Packets
	m_PacketsAvgQKR[AVG_DNA].SetRange(30);
	m_PacketsAvgQKR[AVG_TOTAL].SetRange(30);
	m_PacketsAvgQ2[AVG_DNA].SetRange(30);
	m_PacketsAvgQ2[AVG_TOTAL].SetRange(30);
	
	m_PacketsQKR[0] = m_PacketsQKR[1] = 0;
	m_PacketsQ2[0]  = m_PacketsQ2[1]  = 0;


	m_pDispatch = new CG2Datagram(this);
	m_pProtocol = new CG2Protocol(this);
	m_pThreadProtocol = new CG2Protocol(this);
	
	// Signal downloads they can search over G2 now
	std::vector<CGnuDownloadShell*>::iterator itDownload;
	for(int i = 0; i < m_pTrans->m_DownloadList.size(); i++)
	{
		if(m_pTrans->m_DownloadList[i]->m_ShellStatus == CGnuDownloadShell::eActive)
			m_pTrans->m_DownloadList[i]->Start();

		if(m_pTrans->m_DownloadList[i]->m_ShellStatus == CGnuDownloadShell::eWaiting)
			m_pTrans->m_DownloadList[i]->m_ShellStatus = CGnuDownloadShell::ePending;
	}

	// Signal searches they can search over G2 now
	for(i = 0; i < m_pNet->m_SearchList.size(); i++)
	{
		G2_Search* pSearch = new G2_Search;

		pSearch->Query.SearchGuid      = m_pNet->m_SearchList[i]->m_QueryID;
		pSearch->Query.DescriptiveName = m_pNet->m_SearchList[i]->m_Search;
		pSearch->Query.Metadata        = m_pNet->m_SearchList[i]->m_MetaSearch;
		pSearch->Query.URNs.push_back(m_pNet->m_SearchList[i]->m_Hash);

		StartSearch(pSearch);
	}

	for(i = 0; i < 64; i++)
		RandKeyBlock[i] = rand() % 255;

	m_SearchReport = 0;

	m_SearchPacketsRecvd = 0;

	//	TestEncodeDecode();

}

CG2Control::~CG2Control()
{
	while( m_G2NodeList.size() )
	{
		delete m_G2NodeList.back();
		m_G2NodeList.pop_back();
	}

	while( m_G2Searches.size() )
	{
		delete m_G2Searches.back();
		m_G2Searches.pop_back();
	}

	delete m_pDispatch;
	m_pDispatch = NULL;

	delete m_pProtocol;
	m_pProtocol = NULL;

	delete m_pThreadProtocol;
	m_pThreadProtocol = NULL;
}


// CG2Control member functions

void CG2Control::Timer()
{
	CleanDeadSocks();

	ManageNodes();

	m_pDispatch->Timer();

	
	HubBalancing();

	// G2 Active Searches
	m_ActiveSearches = m_ActiveSearchCount;
	m_ActiveSearchCount = 0;

	// Avg Query Packets
	m_PacketsAvgQKR[AVG_DNA].Update(   m_PacketsQKR[AVG_DNA] );
	m_PacketsAvgQKR[AVG_TOTAL].Update( m_PacketsQKR[AVG_TOTAL] );
	m_PacketsAvgQ2[AVG_DNA].Update(    m_PacketsQ2[AVG_DNA] );
	m_PacketsAvgQ2[AVG_TOTAL].Update(  m_PacketsQ2[AVG_TOTAL]);
	
	m_PacketsQKR[0] = m_PacketsQKR[1] = 0;
	m_PacketsQ2[0]  = m_PacketsQ2[1]  = 0;


	//ASSERT( m_RouteMap.size()   < MAX_ROUTES);
	//ASSERT( m_QueryKeys.size()  < MAX_KEYS);
	//ASSERT( m_GlobalHubs.size() < MAX_GLOBAL);

	TrimMaps();

	// Clean Route Table
	if( time(NULL) > m_CleanRoutesNext )
	{
		std::map<uint32, G2_Route>::iterator itRoute = m_RouteMap.begin();

		while(itRoute != m_RouteMap.end())
			if( itRoute->second.ExpireTime && time(NULL) > itRoute->second.ExpireTime )
				itRoute = m_RouteMap.erase(itRoute);
			else
				itRoute++;

		m_CleanRoutesNext = time(NULL) + CLEAN_ROUTES;
	}

	// Clean Query Key Table
	if( time(NULL) > m_CleanKeysNext )
	{
		std::map<uint32, G2_QueryKey>::iterator itKey = m_KeyInfo.begin();

		while(itKey != m_KeyInfo.end())
			if( time(NULL) > itKey->second.RetryAfter )
			{
				// Erase key
				itKey = m_KeyInfo.erase(itKey);
			}
			else
				itKey++;
		
		m_CleanKeysNext = time(NULL) + CLEAN_KEYS;
	}

	// Clean Global Cache
	if( time(NULL) > m_CleanGlobalNext )
	{
		std::map<uint32, G2HubInfo>::iterator itHub = m_GlobalHubs.begin();

		while(itHub != m_GlobalHubs.end() && m_GlobalHubs.size() > 20)
		{
			// If connected to node dont remove
			std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(itHub->second.Address.Host.S_addr);
			if(itNode != m_G2NodeAddrMap.end())
			{
				itHub->second.ExpireTime = time(NULL) + GLOBAL_HUB_EXPIRE;

				itHub++;
				continue;
			}

			// If Expire time has passed
			if(itHub->second.ExpireTime < time(NULL) || itHub->second.TryCount > 10)
				itHub = m_GlobalHubs.erase(itHub);
			else
				itHub++;
		}

		m_CleanGlobalNext = time(NULL) + CLEAN_GLOBAL;
	}
		

	if( m_TriedConnects.size() > 100 )
		m_TriedConnects.clear();

#ifdef _DEBUG

	// Hub Report
	if(m_SearchReport == 60)
	{
		TRACE0("\nCluster Report:\n");

		for(int i = 0; i < m_G2NodeList.size(); i++)
			if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED && m_G2NodeList[i]->m_NodeMode == G2_HUB)
			{
				CG2Node* pNode = m_G2NodeList[i];

				//// IP: 234/300 leaves, 345mhz, 756mb ram, I/O 2343/2321
				CString Summary = IPtoStr( pNode->m_Address.Host ) + ": ";
				Summary += NumtoStr(pNode->m_NodeInfo.LeafCount) + "/" + NumtoStr(pNode->m_NodeInfo.LeafMax) + ", ";
				Summary += NumtoStr(pNode->m_NodeInfo.Cpu) + "mhz " + NumtoStr(pNode->m_NodeInfo.Mem) + "mb ram ";
				////Summary += "I/O " + NumtoStr(pNode->m_NodeInfo.BpsIn) + "/" + NumtoStr(pNode->m_NodeInfo.BpsOut);

				TRACE0(Summary + "\n");
			}
		
		TRACE0("\nChild Report:\n");

		for(int i = 0; i < m_G2NodeList.size(); i++)
			if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED && m_G2NodeList[i]->m_NodeMode == G2_CHILD)
			{
				CG2Node* pNode = m_G2NodeList[i];

				// IP: 200 leaf max, 345mhz, 756mb ram, I/O 2343/2321, hub able, firewall
				CString Summary = IPtoStr( pNode->m_Address.Host ) + ": ";
				Summary += NumtoStr(pNode->m_NodeInfo.LeafMax) + " leaf max, ";
				Summary += NumtoStr(pNode->m_NodeInfo.Cpu) + "mhz " + NumtoStr(pNode->m_NodeInfo.Mem) + "mb ram, ";
				//Summary += "I/O " + NumtoStr(pNode->m_NodeInfo.BpsIn) + "/" + NumtoStr(pNode->m_NodeInfo.BpsOut) + ", ";
				if(pNode->m_NodeInfo.HubAble)
					Summary += "hub able, ";
				if(pNode->m_NodeInfo.Firewall)
					Summary += "firewall";

				TRACE0(Summary + "\n");
			}

	
	}

	// Do search report
	if(m_SearchReport == 60)
	{
		
		int GlobalKeys = 0;
		std::map<uint32, G2HubInfo>::iterator itHub;
		for(itHub = m_GlobalHubs.begin(); itHub != m_GlobalHubs.end(); itHub++)
			if( itHub->second.QueryKey )
				GlobalKeys++;

		TRACE0("\nSearch Report:\n");
		TRACE0("Keys Aquired: " + NumtoStr(GlobalKeys) + "\n");

		std::list<G2_Search*>::iterator itSearch;
		for(itSearch = m_G2Searches.begin(); itSearch != m_G2Searches.end(); itSearch++)
		{
			// Query: blah, Hubs: 343, Children: 332, Dupes: 32
			CString Status = "Query: " + (*itSearch)->Query.DescriptiveName;
			Status += ", Hubs: " + NumtoStr((*itSearch)->SearchedHubs);
			Status += ", Children: " + NumtoStr((*itSearch)->SearchedChildren);
			Status += ", Dupes: " + NumtoStr((*itSearch)->SearchedDupes) + "\n";

			TRACE0(Status);
		}

		m_SearchReport = 0;
	}
	else
		m_SearchReport++;

#endif

}

void CG2Control::HourlyTimer()
{
	
}

void CG2Control::TrimMaps()
{
	// Basically a flooding failsafe, 
	// should be replaced with something that removes 50% oldest or something

	if( m_RouteMap.size() > MAX_ROUTES)
	{
		std::map<uint32, G2_Route>::iterator itRoute = m_RouteMap.begin();
		while(m_RouteMap.size() > MAX_ROUTES / 2)
			itRoute = m_RouteMap.erase(itRoute);
	}

	if( m_KeyInfo.size() > MAX_KEYS)
	{
		std::map<uint32, G2_QueryKey>::iterator itKey = m_KeyInfo.begin();
		while(m_KeyInfo.size() > MAX_KEYS / 2)
			itKey = m_KeyInfo.erase(itKey);
	}

	if( m_GlobalHubs.size() > MAX_GLOBAL)
	{
		std::map<uint32, G2HubInfo>::iterator itHub = m_GlobalHubs.begin();
		while(m_GlobalHubs.size() > MAX_GLOBAL / 2)
			itHub = m_GlobalHubs.erase(itHub);
	}
}

void CG2Control::ShareUpdate()
{
	m_G2NodeAccess.Lock();
		
	for(int i = 0; i < m_G2NodeList.size(); i++)	
		if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
		{
			if( m_G2NodeList[i]->m_NodeMode == G2_HUB )
				m_G2NodeList[i]->m_SendDelayQHT = true;

			m_G2NodeList[i]->m_SendDelayLNI  = true;
		}
	
	m_G2NodeAccess.Unlock();
}

void CG2Control::ManageNodes()
{
	m_NetSecBytesDown = 0;
	m_NetSecBytesUp	  = 0;

	int Connecting    = 0;
	int HubConnects   = 0;
	int ChildConnects = 0;

	// Add bandwidth and count connections
	for(int i = 0; i < m_G2NodeList.size(); i++)
	{
		m_G2NodeList[i]->Timer();

		if( m_G2NodeList[i]->m_Status == SOCK_CONNECTING )
			Connecting++;

		else if( m_G2NodeList[i]->m_Status == SOCK_CONNECTED )
		{
			m_NetSecBytesDown += m_G2NodeList[i]->m_AvgBytes[0].GetAverage();
			m_NetSecBytesUp   += m_G2NodeList[i]->m_AvgBytes[1].GetAverage();

			if( m_G2NodeList[i]->m_NodeMode == G2_HUB )
				HubConnects++;

			else if( m_G2NodeList[i]->m_NodeMode == G2_CHILD )
				ChildConnects++;
		}
	}

	// Add in udp bandwidth
	m_NetSecBytesDown += m_pDispatch->m_AvgUdpDown.GetAverage();
	m_NetSecBytesUp   += m_pDispatch->m_AvgUdpUp.GetAverage();

	// After 10 mins, if no connects, go into hub mode
	if( m_ClientMode == G2_CHILD )
		if(CTime::GetCurrentTime() - m_ClientUptime == CTimeSpan(0, 0, 10, 0) )
			if( CountHubConnects() == 0 )
				if( !m_pNet->m_TcpFirewall && m_pNet->m_UdpFirewall == UDP_FULL && !m_pNet->m_BehindRouter)
					SwitchG2ClientMode( G2_HUB, true);


	if(Connecting > 5)
		return;


	if( m_ClientMode == G2_CHILD )
	{
		// More hub connects only means people will find your files faster
		// Searching on G2 does not require a hub connection

		m_pPrefs->m_G2ChildConnects = (m_pNet->m_UdpFirewall != UDP_FULL) ? 3 : 1;

		if(HubConnects < m_pPrefs->m_G2ChildConnects)
			TryConnect();

		if(m_pPrefs->m_G2ChildConnects && HubConnects > m_pPrefs->m_G2ChildConnects)
			DropNode(G2_HUB);
	}

	else if( m_ClientMode == G2_HUB )
	{
		// Fixed, searching 1 node, searches 10
		// Sending queries to 10% of network should hit whole network

		if(m_pPrefs->m_G2MinConnects && HubConnects < m_pPrefs->m_G2MinConnects)
			TryConnect();

		if(m_pPrefs->m_G2MaxConnects && HubConnects > m_pPrefs->m_G2MaxConnects)
			DropNode(G2_HUB);


		while(ChildConnects > m_pPrefs->m_MaxLeaves)
		{
			DropNode(G2_CHILD);
			ChildConnects--;
		}
	}
}

void CG2Control::CleanDeadSocks()
{
	// Node Sockets
	std::vector<CG2Node*>::iterator itNode;

	itNode = m_G2NodeList.begin();
	while( itNode != m_G2NodeList.end())
		if((*itNode)->m_Status == SOCK_CLOSED && (*itNode)->m_CloseWait > 3)
		{
			m_G2NodeAccess.Lock();

			delete *itNode;

			itNode = m_G2NodeList.erase(itNode);
			
			m_G2NodeAccess.Unlock();
		}
		else
			itNode++;
}

void CG2Control::TryConnect()
{
	if( !m_pNet->ConnectingSlotsOpen() )
		return;

	int attempts = 10;

	while( attempts > 0 )
	{
		attempts--;

		// If Real list has values
		if(m_pCache->m_G2Real.size())
		{
			int randIndex = ( m_pCache->m_G2Real.size() > 10) ? rand() % m_pCache->m_G2Real.size() : rand() % 10;

			std::list<Node>::iterator itNode = m_pCache->m_G2Real.begin();
			for(int i = 0; itNode != m_pCache->m_G2Real.end(); itNode++, i++)
				if(i == randIndex)
				{
					std::map<uint32, bool>::iterator itAddr = m_TriedConnects.find( StrtoIP((*itNode).Host).S_addr );

					if(attempts && itAddr != m_TriedConnects.end())
						continue;

					m_TriedConnects[ StrtoIP((*itNode).Host).S_addr ] = true;


					CreateNode( *itNode );
					m_pCache->m_G2Real.erase(itNode);

					return;
				}
		}

		// If permanent list has values
		if(m_pCache->m_G2Perm.size())
		{
			int randIndex = rand() % m_pCache->m_G2Perm.size();

			std::list<Node>::iterator itNode = m_pCache->m_G2Perm.begin();
			for(int i = 0; itNode != m_pCache->m_G2Perm.end(); itNode++, i++)
				if(i == randIndex)
				{
					std::map<uint32, bool>::iterator itAddr = m_TriedConnects.find( StrtoIP((*itNode).Host).S_addr );

					if(attempts && itAddr != m_TriedConnects.end())
						continue;

					m_TriedConnects[ StrtoIP((*itNode).Host).S_addr ] = true;
					

					CreateNode( *itNode );

					return;
				}
		}

		// Nothing in G2 caches, try G1 nodes for entry to G2
		if(m_pCache->m_GnuReal.size())
		{
			int randIndex = ( m_pCache->m_GnuReal.size() > 10) ? rand() % m_pCache->m_GnuReal.size() : rand() % 10;

			std::list<Node>::iterator itNode = m_pCache->m_GnuReal.begin();
			for(int i = 0; itNode != m_pCache->m_GnuReal.end(); itNode++, i++)
				if(i == randIndex)
				{
					CreateNode( *itNode );
					return;
				}
		}
	}

	// Do web cache request only if not connected to g1 because g2 hosts can be found through there
	if( CountHubConnects() == 0 && m_pNet->m_pGnu == NULL)
		m_pCache->WebCacheGetRequest("gnutella2");

}

void CG2Control::CreateNode(Node HostInfo)
{
	// Check if already connected to node
	if( FindNode( HostInfo.Host, 0 ) != NULL)
		return;


	CG2Node* NewNode = new CG2Node(this, HostInfo.Host, HostInfo.Port);

	//Attempt to connect to node
	if( !NewNode->Create() )
	{
		m_pCore->LogError("Node Create Error: " + NumtoStr(NewNode->GetLastError()));
	
		delete NewNode;
		return;
	}
	
	if( !NewNode->Connect( HostInfo.Host, HostInfo.Port) )
		if (NewNode->GetLastError() != WSAEWOULDBLOCK)
		{
			m_pCore->LogError("Node Connect Error : " + NumtoStr(NewNode->GetLastError()));
				
			delete NewNode;
			return;
		}
	

	// Add node to list
	m_G2NodeAccess.Lock();
	m_G2NodeList.push_back(NewNode);
	m_G2NodeAccess.Unlock();
		

	G2NodeUpdate(NewNode);
}

void CG2Control::DropNode(int G2Mode)
{
	CG2Node* DeadNode = NULL;
	CTime CurrentTime = CTime::GetCurrentTime();
	CTimeSpan LowestTime(0);

	// Drop Normal nodes first
	for(int i = 0; i < m_G2NodeList.size(); i++)
		if(SOCK_CONNECTED == m_G2NodeList[i]->m_Status)
		{
			if(m_G2NodeList[i]->m_NodeMode == G2Mode) 
				if(LowestTime.GetTimeSpan() == 0 || CurrentTime - m_G2NodeList[i]->m_ConnectTime < LowestTime)
				{
					DeadNode	 = m_G2NodeList[i];
					LowestTime   = CurrentTime - m_G2NodeList[i]->m_ConnectTime;
				}
		}

	if(DeadNode)
	{
		DeadNode->CloseWithReason("Node Bumped");
		return;
	}
	
}

void CG2Control::RemoveNode(CG2Node* pNode)
{
	std::vector<CG2Node*>::iterator itNode;
	for(itNode = m_G2NodeList.begin(); itNode != m_G2NodeList.end(); itNode++)
		if(*itNode == pNode)
			pNode->CloseWithReason("Manually Removed");

	G2NodeUpdate(pNode);
}

CG2Node* CG2Control::FindNode(CString Host, UINT Port)
{
	std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find( StrtoIP(Host).S_addr);

	if(itNode != m_G2NodeAddrMap.end())
		return itNode->second;

	return NULL;
}

CG2Node* CG2Control::GetRandHub()
{
	int Hubs = 0;

	for(int i = 0; i < m_G2NodeList.size(); i++)
		if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
			if(m_G2NodeList[i]->m_NodeMode == G2_HUB)
				Hubs++;

	if(Hubs)
	{
		int hubReturn = rand() % Hubs;
		int hubCurrent = 0;

		for(int i = 0; i < m_G2NodeList.size(); i++)
		if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
			if(m_G2NodeList[i]->m_NodeMode == G2_HUB)
			{
				if( hubCurrent == hubReturn)
					return m_G2NodeList[i];
				else
					hubCurrent++;
			}
	}

	return NULL;
}

bool CG2Control::GetAltHubs(CString &HostList, CG2Node* NodeExclude)
{
	int Hosts = 0;

	for(int i = 0; i < m_G2NodeList.size() && Hosts < 10; i++)
		if(m_G2NodeList[i]->m_NodeMode == G2_HUB && 
		   m_G2NodeList[i]->m_Status == SOCK_CONNECTED && 
		   m_G2NodeList[i] != NodeExclude )
		{
			CG2Node* pNode = m_G2NodeList[i];
			HostList += IPtoStr(pNode->m_Address.Host) + ":" + NumtoStr(pNode->m_Address.Port) + " " + CTimeToStr( CTime::GetCurrentTime() ) + ",";	
			Hosts++;
		}

	std::list<Node>::iterator itNode;
	for( itNode = m_pCache->m_G2Real.begin(); itNode != m_pCache->m_G2Real.begin() && Hosts < 10; itNode++)
	{
		HostList += (*itNode).Host + ":" + NumtoStr((*itNode).Port) + " " + CTimeToStr( CTime((*itNode).LastSeen) ) + ",";	
		Hosts++;
	}


	// Delete Extra comma
	if(Hosts)
	{
		HostList = HostList.Left(HostList.ReverseFind(','));
		return true;
	}

	return false;
}

int CG2Control::CountHubConnects()
{
	int HubConnects = 0;

	for(int i = 0; i < m_G2NodeList.size(); i++)	
		if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
			if(m_G2NodeList[i]->m_NodeMode == G2_HUB)
				HubConnects++;

	return HubConnects;
}

int CG2Control::CountChildConnects()
{
	int ChildConnects = 0;

	for(int i = 0; i < m_G2NodeList.size(); i++)	
		if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
			if(m_G2NodeList[i]->m_NodeMode == G2_CHILD)
				ChildConnects++;

	return ChildConnects;
}

void CG2Control::HubBalancing()
{
	if(m_ClientMode != G2_HUB)
		return;

	// Only run his function once per minute
	if(m_HubBalanceCheck > 0)
	{
		m_HubBalanceCheck--;
		return;
	}
	else
		m_HubBalanceCheck = 60;

	// Get load of local hub cluster
	int LocalLoad = CountChildConnects() * 100 / m_pPrefs->m_MaxLeaves;
	
	std::vector<int> ClusterLoad;	
	for(int i = 0; i < m_G2NodeList.size(); i++)
		if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED && m_G2NodeList[i]->m_NodeMode == G2_HUB)
			if(m_G2NodeList[i]->m_NodeInfo.LeafMax)
				ClusterLoad.push_back(m_G2NodeList[i]->m_NodeInfo.LeafCount * 100 / m_G2NodeList[i]->m_NodeInfo.LeafMax);

	int AvgLoad = LocalLoad;
	for(i = 0; i < ClusterLoad.size(); i++)
		AvgLoad += ClusterLoad[i];

	AvgLoad /= 1 + ClusterLoad.size();
	TRACE("\nAverage Cluster Load " + NumtoStr(AvgLoad) + "%\n");

	// Wait an hour between upgrading nodes
	if( time(NULL) > m_NextUpgrade )
	{
		/* 
		bool DoUpgrade = true;

		if(LocalLoad > HIGH_HUB_CAPACITY)
			DoUpgrade = false;

		/*for(i = 0; i < ClusterLoad.size(); i++)
			if( ClusterLoad[i] < HIGH_HUB_CAPACITY )
			{
				DoUpgrade = false;
				break;
			}*/

		bool DoUpgrade = false;

		if(AvgLoad > HIGH_HUB_CAPACITY)
			DoUpgrade = true;

		if( DoUpgrade )
		{
			// Find child with highest score
			int HighScore = 0;
			CG2Node* UpgradeNode = NULL;

			for(i = 0; i < m_G2NodeList.size(); i++)	
				if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED && m_G2NodeList[i]->m_NodeMode == G2_CHILD)
				{
					int NodeScore = ScoreNode(m_G2NodeList[i]);

					if(NodeScore > HighScore)
					{
						HighScore = NodeScore;
						UpgradeNode = m_G2NodeList[i];
					}
				}

			// Send MCR to child node
			if(HighScore > 0)
			{
				Send_MCR(UpgradeNode);
				UpgradeNode->m_TriedUpgrade = true;
			}

			// When MCA received set timeout to an hour from now
		}

	}


	// Dont downgrade if this is the only hub
	if( ClusterLoad.size() && time(NULL) > m_ModeChangeTimeout )
	{
		/*bool DoDowngrade = true;

		if(LocalLoad > LOW_HUB_CAPACITY)
			DoDowngrade = false;

		for(i = 0; i < ClusterLoad.size(); i++)
			if( ClusterLoad[i] > LOW_HUB_CAPACITY )
			{
				DoDowngrade = false;
				break;
			}
		*/

		bool DoDowngrade = false;

		if(AvgLoad < LOW_HUB_CAPACITY)
			DoDowngrade = true;

		if( DoDowngrade )
		{
			// Create a node of local host to be scored
			CG2Node LocalNode(this, IPtoStr(m_pNet->m_CurrentIP), m_pNet->m_CurrentPort);
			LocalNode.m_NodeMode = G2_HUB;
			LocalNode.m_NodeInfo.HubAble   = m_pPrefs->m_SupernodeAble;
			LocalNode.m_NodeInfo.Firewall  = (m_pNet->m_TcpFirewall || m_pNet->m_UdpFirewall != UDP_FULL); 
			//LocalNode.m_NodeInfo.BpsIn   = m_pPrefs->m_SpeedDown * 1024;
			//LocalNode.m_NodeInfo.BpsOut  = m_pPrefs->m_SpeedUp * 1024;
			LocalNode.m_NodeInfo.LeafCount = CountChildConnects();
			LocalNode.m_NodeInfo.LeafMax   = m_pPrefs->m_MaxLeaves;
			LocalNode.m_NodeInfo.UpSince   = m_ClientUptime.GetTime();

			// Check if local node the lowest scored in cluster
			int LowScore = ScoreNode(&LocalNode); ASSERT(LowScore != 0);
			CG2Node* DowngradeNode = NULL;

			for(i = 0; i < m_G2NodeList.size(); i++)	
				if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED && m_G2NodeList[i]->m_NodeMode == G2_HUB)
				{
					int NodeScore = ScoreNode(m_G2NodeList[i]);

					if(NodeScore && NodeScore < LowScore)
					{
						LowScore = NodeScore;
						DowngradeNode = m_G2NodeList[i];
					}
				}
			
			// Downgrade self
			if(DowngradeNode == NULL && !m_ForcedHub)
				SwitchG2ClientMode(G2_CHILD);
		}
	}
}

int CG2Control::ScoreNode(CG2Node* pNode)
{
	int Score = 0;

	// Check for HubAble or Firewall
	if(pNode->m_NodeInfo.HubAble == false || 
	   pNode->m_NodeInfo.Firewall == true ||
	   pNode->m_TriedUpgrade == true)
		return 0;

	// Leaves connected may be a better indicator of hub performance than
	// bandwidth stats
	/* Bandwidth Down
	int bwdown = pNode->m_NodeInfo.BpsIn / 1024 * 8;   
	if( bwdown > OPT_BANDWIDTH_DOWN)
		bwdown = OPT_BANDWIDTH_DOWN;

	Score += bwdown * 100 / OPT_BANDWIDTH_DOWN;

	// Bandwidth Up
	int bwup = pNode->m_NodeInfo.BpsOut / 1024 * 8;
	if( bwup > OPT_BANDWIDTH_UP)
		bwup = OPT_BANDWIDTH_UP;

	Score += bwup * 100 / OPT_BANDWIDTH_UP;*/

	if(pNode->m_NodeMode == G2_HUB)
	{
		// Leaf Max
		int leafcount = pNode->m_NodeInfo.LeafCount;
		if( leafcount > OPT_LEAFMAX)
			leafcount = OPT_LEAFMAX;

		Score += leafcount * 100 / OPT_LEAFMAX;
	}

	if(pNode->m_NodeMode == G2_CHILD)
	{
		// Leaf Max
		int leafmax = pNode->m_NodeInfo.LeafMax;
		if( leafmax > OPT_LEAFMAX)
			leafmax = OPT_LEAFMAX;

		Score += leafmax * 100 / OPT_LEAFMAX;

		// Dont need uptime here, because first leaf selected has high uptime
		// Prevent high uptime nodes from always being elected, if election fails they
		// Go back to bottom of list

		// Upgrade nodes of equal or greater versions only, dont use for downgrade 
		if(pNode->m_RemoteAgent.Find("GnucDNA") > 0)
		{
			int dnapos = pNode->m_RemoteAgent.Find("GnucDNA");

			CString CurrentVersion = m_pCore->m_DnaVersion;
			CString RemoteVersion  = pNode->m_RemoteAgent.Mid(dnapos + 8, 7);

			CurrentVersion.Remove('.');
			RemoteVersion.Remove('.');

			if( atoi(RemoteVersion) < atoi(CurrentVersion) )
				return 0;
		}
	}

	/* Uptime
	uint64 Uptime = time(NULL) - pNode->m_NodeInfo.UpSince;
	if( Uptime  > OPT_UPTIME)
		Uptime  = OPT_UPTIME;

	Score += Uptime  * 100 / (OPT_UPTIME); // () because OPT_UPTIME is 3*60*60
	*/

	ASSERT(Score <= 400);

	return Score;
}

void CG2Control::UpdateGlobal(G2NodeInfo &HubNode)
{
	if(HubNode.Address.Host.S_addr == 0 || HubNode.Address.Port == 0)
		return;

	if(HubNode.Address.Host.S_addr == m_pNet->m_CurrentIP.S_addr)
		return;

	std::map<uint32,G2HubInfo>::iterator itHub = m_GlobalHubs.find(HubNode.Address.Host.S_addr ^ m_GlobalUnique);

	if(itHub == m_GlobalHubs.end())
	{
		m_GlobalHubs[HubNode.Address.Host.S_addr ^ m_GlobalUnique] = G2HubInfo(HubNode.Address);
		return;
	}

	itHub->second.ExpireTime = time(NULL) + GLOBAL_HUB_EXPIRE;
}

void CG2Control::G2NodeUpdate(CG2Node* updated)
{
	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->NetworkChange(updated->m_G2NodeID);
}

void CG2Control::SwitchG2ClientMode(int G2Mode, bool DownG1)
{
	// Downgrade G1 if requested
	if(DownG1 && m_pNet->m_pGnu)
		if(m_pNet->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER)
			m_pNet->m_pGnu->SwitchGnuClientMode(GNU_LEAF);

	// Requested mode already set
	if( m_ClientMode == G2Mode )
		return;

	// Remove all connections
	while( m_G2NodeList.size() )
	{
		delete m_G2NodeList.back();
		m_G2NodeList.pop_back();
	}

	// Change mode
	m_ClientMode = G2Mode;

	m_ModeChangeTimeout = time(NULL) + 60*60;
}

void CG2Control::TestEncodeDecode()
{
	// Test Encoding and decoding!!!
	G2_KHL KnownHubList;
	
	CTime refTime = CTime::GetCurrentTime();
	KnownHubList.RefTime = refTime.GetTime();

	for(int i = 0; i < 3; i++)
	{
		G2NodeInfo Neighbour;
		Neighbour.Address.Host = StrtoIP("127.0.0.1");
		Neighbour.Address.Port = 666;

		CoCreateGuid(&Neighbour.NodeID);

		strcpy(Neighbour.Vendor, "TEST");

		Neighbour.LibraryCount = 12345;
		Neighbour.LibrarySizeKB = 98000;

		Neighbour.LeafCount = rand() % 400 + 0;
		Neighbour.LeafMax = 400;

		CTime nowTime = CTime::GetCurrentTime();
		Neighbour.LastSeen = nowTime.GetTime();

		KnownHubList.Neighbours.push_back(Neighbour);
	}

	for(int i = 0; i < 2; i++)
	{
		G2NodeInfo Cached;
		Cached.Address.Host = StrtoIP("192.168.1.1");
		Cached.Address.Port = 777;

		CoCreateGuid(&Cached.NodeID);

		strcpy(Cached.Vendor, "TSET");

		Cached.LibraryCount = 54321;
		Cached.LibrarySizeKB = 12000;

		Cached.LeafCount = rand() % 200 + 0;
		Cached.LeafMax = 200;

		CTime nowTime = CTime::GetCurrentTime();
		Cached.LastSeen = nowTime.GetTime();

		KnownHubList.Cached.push_back(Cached);
	}


	m_pProtocol->Encode_KHL(KnownHubList);
/*
	// Decode
	G2_Header undefPacket;
	byte* pBuff   = m_pProtocol->m_FinalPacket;
	uint32 length = m_pProtocol->m_FinalSize;

	byte* readBuff = pBuff;
	m_pProtocol->ReadNextPacket(undefPacket, readBuff, length);

	G2_KHL decoded;
	m_pProtocol->Decode_KHL(undefPacket, decoded);

*/
	//////////////////////////////////////////////////////////////////////

	G2_LNI LocalInfo;

	LocalInfo.Node.Address.Host = m_pNet->m_CurrentIP;
	LocalInfo.Node.Address.Port = m_pNet->m_CurrentPort;

	LocalInfo.Node.NodeID = m_pPrefs->m_ClientID;

	memcpy( LocalInfo.Node.Vendor, (LPCSTR) m_pCore->m_ClientCode, 4);

	LocalInfo.Node.LibraryCount  = m_pShare->m_TotalLocalFiles;
	LocalInfo.Node.LibrarySizeKB = m_pShare->m_TotalLocalSize;

	LocalInfo.Node.LeafCount = CountChildConnects();
	LocalInfo.Node.LeafMax   = m_pPrefs->m_MaxLeaves;

	LocalInfo.Node.HubAble  = m_pPrefs->m_SupernodeAble;
	LocalInfo.Node.Firewall = (m_pNet->m_TcpFirewall || m_pNet->m_UdpFirewall != UDP_FULL);

	//LocalInfo.Node.BpsIn  = m_pPrefs->m_SpeedDown;
	//LocalInfo.Node.BpsOut = m_pPrefs->m_SpeedUp;

	LocalInfo.Node.Cpu = m_pCore->m_SysSpeed;
	LocalInfo.Node.Mem = m_pCore->m_SysMemory;

	LocalInfo.Node.UpSince = m_ClientUptime.GetTime();

	// Send packet
	m_pProtocol->Encode_LNI(LocalInfo);

	// Decode
	G2_Header undefPacket;
	byte* pBuff   = m_pProtocol->m_FinalPacket;
	uint32 length = m_pProtocol->m_FinalSize;

	CString hexdump;
	for(int i = 0; i < length; i++)
		hexdump += EncodeBase16(&pBuff[i], 1) + " ";


	byte* readBuff = pBuff;
	m_pProtocol->ReadNextPacket(undefPacket, readBuff, length);

	G2_LNI decoded;
	m_pProtocol->Decode_LNI(undefPacket, decoded);
}

void CG2Control::StartSearch(G2_Search *pSearch)
{
	ASSERT(pSearch);
	if(pSearch == NULL)
		return;

	std::list<G2_Search*>::iterator itSearch;
	for(itSearch = m_G2Searches.begin(); itSearch != m_G2Searches.end(); itSearch++)
		if((*itSearch)->Query.SearchGuid == pSearch->Query.SearchGuid)
		{
			ASSERT(0);
			return;
		}

	m_G2Searches.push_back(pSearch);


	// Send queries to connected hubs so they have GUID and can route packets for us
	for( int i = 0; i < m_G2NodeList.size(); i++ )
		if( m_G2NodeList[i]->m_Status == SOCK_CONNECTED) // Hubs get search GUID or or Q2 sent to chilren
		{
			Send_Q2( NULL, pSearch->Query, m_G2NodeList[i] );

			if(m_G2NodeList[i]->m_NodeMode == G2_HUB)
				pSearch->TriedHubs[m_G2NodeList[i]->m_Address.Host.S_addr] = true;
		}
}

void CG2Control::StepSearch(GUID SearchGuid)
{
	m_ActiveSearchCount++;
	
	G2_Search* pSearch = NULL;

	std::list<G2_Search*>::iterator itSearch;
	for(itSearch = m_G2Searches.begin(); itSearch != m_G2Searches.end(); itSearch++)
		if((*itSearch)->Query.SearchGuid == SearchGuid)
		{
			pSearch = *itSearch;
			break;
		}
	

	if(pSearch == NULL)
	{
		ASSERT(0);
		return;
	}

	if(m_GlobalHubs.size() == 0)
		return;

	int PacketsIn = m_SearchPacketsRecvd;
	m_SearchPacketsRecvd = 0;

	if(PacketsIn >= PACKETS_PER_SEC)
		return;

	// More searches, spread packets per sec among them
	int MaxPacketsOut  = PACKETS_PER_SEC - PacketsIn;
	if(m_ActiveSearches)
		MaxPacketsOut = (m_ActiveSearches > MaxPacketsOut) ? 1 : MaxPacketsOut / m_ActiveSearches;

	int PacketsSent = 0;

	int RetryTime = (m_GlobalHubs.size() - pSearch->TriedHubs.size()) / PACKETS_PER_SEC;
	if(RetryTime < GND_SEND_TIMEOUT * 2)
		RetryTime = GND_SEND_TIMEOUT * 2;

	// Try hubs that have been attempted the least first
	int MaxTryCount = 1;
	for(int iCount = 0; iCount < MaxTryCount; iCount++)
	{
		// Iterate through global cache
		int hubpos = 0;
		std::map<uint32, G2HubInfo>::iterator itHub;
		for(itHub = m_GlobalHubs.begin(); itHub != m_GlobalHubs.end(); itHub++, hubpos++)
		{
			if( PacketsSent >= MaxPacketsOut )
				return;

			G2HubInfo* pHub = &itHub->second;
			
			
			// Loop to host with max number of tries
			if(pHub->TryCount >= MaxTryCount)
				MaxTryCount = pHub->TryCount + 1;

			if(iCount != pHub->TryCount)
				continue;


			// Check if hub already tried
			std::map<uint32, bool>::iterator itTried = pSearch->TriedHubs.find( pHub->Address.Host.S_addr );
			if( itTried != pSearch->TriedHubs.end())
				continue;

			uint32 NodeAge = time(NULL) - (pHub->ExpireTime - (60*60));
			
			// If Query Key send query
			if( pHub->QueryKey)
			{
				if( time(NULL) > pHub->NextTry ) // do not combine with above
				{
					pSearch->Query.QueryKey = pHub->QueryKey;
					
					Send_Q2( pHub, pSearch->Query );
					PacketsSent++;

					pHub->TryCount++;
					pHub->NextTry = RetryTime * pHub->TryCount + time(NULL);	
				}
			}

			// Else send QKR
			else 
			{
				if( time(NULL) > pHub->NextTry)
				{
					Send_QKR( pHub->Address );
					PacketsSent++;

					pHub->TryCount++;
					pHub->NextTry = RetryTime * pHub->TryCount + time(NULL);	
				}
			}
			
			// end global cache loop
		}
		
		// end tries loop 
	} 
}
 
void CG2Control::EndSearch(GUID SearchGuid)
{
	std::list<G2_Search*>::iterator itSearch = m_G2Searches.begin();

	while(itSearch != m_G2Searches.end())
		if((*itSearch)->Query.SearchGuid == SearchGuid)
		{
			delete *itSearch;
			itSearch = m_G2Searches.erase(itSearch);
		}
		else
			itSearch++;
}

uint32 CG2Control::GenerateQueryKey(uint32 Address)
{
	ASSERT(Address != 0);
	
	MD5Context  MD5_Context;
	MD5Init(&MD5_Context);

	// Sha1 Address + RandomKeyBlock
	memcpy(RandKeyBlock, &Address, 4);
	MD5Update(&MD5_Context, RandKeyBlock,  64);

	byte MD5_Digest[16];
	MD5Final(&MD5_Context, MD5_Digest);

	// Take first four bytes of digest
	uint32 Key;
	memcpy(&Key, MD5_Digest, 4);
	return Key;
}

void CG2Control::ReceivePacket(G2_RecvdPacket &Packet)
{
	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketIncoming(NETWORK_G2, Packet.pTCP != 0, Packet.Source.Host.S_addr, Packet.Source.Port, Packet.Root.Packet, Packet.Root.PacketSize, false, ERROR_NONE);


	// Check if addressed packet
	GUID TargetID;
	if( m_pProtocol->Decode_TO(Packet.Root, TargetID) )
		if( TargetID != m_pPrefs->m_ClientID )
		{
			RoutePacket(Packet, TargetID);
			return;
		}

	// Ping
	if( strcmp(Packet.Root.Name, "/PI") == 0) 
		Receive_PI(Packet);

	// Pong
	else if( strcmp(Packet.Root.Name, "/PO") == 0) 
		Receive_PO(Packet);

	// Local Node Info
	else if( strcmp(Packet.Root.Name, "/LNI") == 0)
		Receive_LNI(Packet);

	// Known Hub List
	else if( strcmp(Packet.Root.Name, "/KHL") == 0)
		Receive_KHL(Packet);

	// Query Hash Table
	else if( strcmp(Packet.Root.Name, "/QHT") == 0)
		Receive_QHT(Packet);

	// Query Key Request
	else if( strcmp(Packet.Root.Name, "/QKR") == 0)
		Receive_QKR(Packet);

	// Query Key Answer
	else if( strcmp(Packet.Root.Name, "/QKA") == 0)
		Receive_QKA(Packet);

	// G1 Query
	else if( strcmp(Packet.Root.Name, "/Q1") == 0)
		Receive_Q1(Packet);

	// G2 Query
	else if( strcmp(Packet.Root.Name, "/Q2") == 0)
		Receive_Q2(Packet);

	// Query Acknowledgement
	else if( strcmp(Packet.Root.Name, "/QA") == 0)
		Receive_QA(Packet);

	// Query Hit
	else if( strcmp(Packet.Root.Name, "/QH2") == 0)
		Receive_QH2(Packet);

	// Push
	else if( strcmp(Packet.Root.Name, "/PUSH") == 0)
		Receive_PUSH(Packet);

	// Mode Change Request
	else if( strcmp(Packet.Root.Name, "/MCR") == 0)
		Receive_MCR(Packet);

	// Mode Change Ack
	else if( strcmp(Packet.Root.Name, "/MCA") == 0)
		Receive_MCA(Packet);

	// Private Message
	else if( strcmp(Packet.Root.Name, "/PM") == 0)
		Receive_PM(Packet);

	// Close
	else if( strcmp(Packet.Root.Name, "/CLOSE") == 0)
		Receive_CLOSE(Packet);

	// Crawl Request
	else if( strcmp(Packet.Root.Name, "/CRAWLR") == 0)
		Receive_CRAWLR(Packet);
}

void CG2Control::Receive_PI(G2_RecvdPacket &PacketPI)
{
	G2_PI Ping;
	m_pProtocol->Decode_PI(PacketPI.Root, Ping);

	if( PacketPI.pTCP )
	{
		if( Ping.UdpAddress.Host.S_addr )
		{
			Send_PO(Ping); // Send udp
			
			if( !Ping.Relay )
			{
				Ping.Relay = true;
				
				// forward via tcp
				if( m_ClientMode == G2_HUB )
					for(int i = 0; i < m_G2NodeList.size(); i++)
						if( m_G2NodeList[i] != PacketPI.pTCP && 
							m_G2NodeList[i]->m_NodeMode == G2_HUB && 
							m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
							Send_PI(m_G2NodeList[i]->m_Address, Ping, m_G2NodeList[i] ); 
			}	

			if( m_ClientMode == G2_CHILD )
			{
				if( Ping.TestFirewall )
					if(rand() % 25 == 0) // 25% chance of testing host for firewall
						CreateNode( Node(IPtoStr(Ping.UdpAddress.Host), Ping.UdpAddress.Port, NETWORK_G2) );
			}
		}

		else // keep alive ping
		{
			if(Ping.Ident)
			{		
				if(Ping.Ident == m_ClientIdent)
					PacketPI.pTCP->CloseWithReason("Loopback Connection");

				for(int i = 0; i < m_G2NodeList.size(); i++)
					if( m_G2NodeList[i]->m_RemoteIdent == Ping.Ident)
						PacketPI.pTCP->CloseWithReason("Duplicate Connection");

				if(PacketPI.pTCP->m_Status == SOCK_CLOSED)
				{
					m_pCache->RemoveIP( IPtoStr(PacketPI.pTCP->m_Address.Host), NETWORK_G2);
					m_pCache->RemoveIP( IPtoStr(PacketPI.pTCP->m_Address.Host), NETWORK_GNUTELLA);
				}

				PacketPI.pTCP->m_RemoteIdent = Ping.Ident;
			}
			
			Send_PO(Ping, PacketPI.pTCP ); // reply via tcp
		}
	}

	else 
	{
		Ping.UdpAddress = PacketPI.Source;
		Send_PO(Ping);
	}
		
}

void CG2Control::Receive_PO(G2_RecvdPacket &PacketPO)
{
	G2_PO Pong;
	m_pProtocol->Decode_PO(PacketPO.Root, Pong);

	bool Local = false;
	std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(PacketPO.Source.Host.S_addr);
		if(itNode != m_G2NodeAddrMap.end())
			Local = true;

	// If pong received udp, and not connected directly
	if(PacketPO.pTCP == NULL && !Local)
	{
		if(Pong.Relay)
			m_pNet->m_UdpFirewall = UDP_FULL;

		// If this turned on and NAT really doesnt work, net spam and dead acks ensue
		//else if(m_pNet->m_UdpFirewall != UDP_FULL)
		//	m_pNet->m_UdpFirewall = UDP_NAT;
	}
}

void CG2Control::Receive_LNI(G2_RecvdPacket &PacketLNI)
{
	if( PacketLNI.pTCP == NULL )
	{
		ASSERT(0);
		return;
	}

	G2_LNI LocalNodeInfo;
	m_pProtocol->Decode_LNI(PacketLNI.Root, LocalNodeInfo);

	PacketLNI.pTCP->m_NodeInfo = LocalNodeInfo.Node;

	// Add to cache
	Node LocalNode;
	LocalNode.Network  = NETWORK_G2;
	LocalNode.Host     = IPtoStr(PacketLNI.pTCP->m_NodeInfo.Address.Host);
	LocalNode.Port     = PacketLNI.pTCP->m_NodeInfo.Address.Port;
	LocalNode.LastSeen = time(NULL);

	m_pCache->AddWorking(LocalNode);

	// Add to global cache
	UpdateGlobal(PacketLNI.pTCP->m_NodeInfo);
	
	// Add to routing table, erase previous first
	GUID RemoteID = PacketLNI.pTCP->m_NodeInfo.NodeID;

	std::map<uint32, G2_Route>::iterator itRoute = m_RouteMap.find( HashGuid(RemoteID) );
	if(itRoute != m_RouteMap.end())
		m_RouteMap.erase(itRoute);

	G2_Route LocalRoute;
	LocalRoute.Address = PacketLNI.pTCP->m_Address;
	LocalRoute.RouteID = RemoteID;
	m_RouteMap[ HashGuid(RemoteID) ] = LocalRoute;
}

void CG2Control::Receive_KHL(G2_RecvdPacket &PacketKHL)
{
	if( PacketKHL.pTCP == NULL)
	{
		ASSERT(0);
		return;
	}

	G2_KHL KnownHubList;
	m_pProtocol->Decode_KHL(PacketKHL.Root, KnownHubList);

	PacketKHL.pTCP->m_HubNeighbors.clear();

	for(int i = 0; i < KnownHubList.Neighbours.size(); i++)
	{
		PacketKHL.pTCP->m_HubNeighbors.push_back( KnownHubList.Neighbours[i] );

		// Add to cache
		Node HubNode;
		HubNode.Network  = NETWORK_G2;
		HubNode.Host     = IPtoStr(KnownHubList.Neighbours[i].Address.Host);
		HubNode.Port     = KnownHubList.Neighbours[i].Address.Port;
		HubNode.LastSeen = time(NULL);

		m_pCache->AddWorking(HubNode);

		UpdateGlobal(KnownHubList.Neighbours[i]);
	}

	for(int i = 0; i < KnownHubList.Cached.size(); i++)
	{
		// Add to cache
		Node HubNode;
		HubNode.Network  = NETWORK_G2;
		HubNode.Host     = IPtoStr(KnownHubList.Cached[i].Address.Host);
		HubNode.Port     = KnownHubList.Cached[i].Address.Port;
		HubNode.LastSeen = KnownHubList.Cached[i].LastSeen;

		m_pCache->AddKnown(HubNode);

		UpdateGlobal(KnownHubList.Cached[i]);
	}
}

void CG2Control::Receive_QHT(G2_RecvdPacket &PacketQHT)
{
	if( PacketQHT.pTCP == NULL)
	{
		ASSERT(0);
		return;
	}

	ASSERT( m_ClientMode == G2_HUB );

	CG2Node* pNode = PacketQHT.pTCP;

	G2_QHT QueryHashTable;
	m_pProtocol->Decode_QHT(PacketQHT.Root, QueryHashTable);

	// Reset
	if( QueryHashTable.Reset )
	{
		if( QueryHashTable.TableSize == 0 || 
			QueryHashTable.TableSize % 8 != 0 ||
			QueryHashTable.TableSize > ( 1 << 23 ) || // Dont accept more than 1meg tables
			QueryHashTable.Infinity != 1)
		{
			pNode->CloseWithReason("QHT Reset Error");
			return;
		}

		pNode->m_PatchReady  = false;
		pNode->m_CurrentPart = 1;

		if( pNode->m_PatchBuffer )
			delete [] pNode->m_PatchBuffer;
		pNode->m_PatchBuffer = NULL;

		pNode->m_RemoteTableSize = QueryHashTable.TableSize / 8;

		memset( pNode->m_RemoteHitTable, 0xFF, G2_TABLE_SIZE );
	}

	// Patch
	else
	{
		if( QueryHashTable.PartNum == 0 || 
			QueryHashTable.PartNum > QueryHashTable.PartTotal ||
			(QueryHashTable.Bits != 1 && QueryHashTable.Bits != 4)) // Shareaza using 4 bit fields!
		{
			pNode->CloseWithReason("QHT Patch Error");
			return;
		}

		if(pNode->m_PatchReady)
		{
			//ASSERT(0);
			pNode->CloseWithReason("QHT Patch too Frequent");
			return;
		}

		if(QueryHashTable.PartNum != pNode->m_CurrentPart)
		{
			//ASSERT(0);
			pNode->CloseWithReason("QHT Order Error");
			return;
		}

		if( QueryHashTable.PartNum == 1)
		{	
			if( pNode->m_PatchBuffer )
				delete [] pNode->m_PatchBuffer;

			pNode->m_PatchSize   = pNode->m_RemoteTableSize * QueryHashTable.Bits;
			pNode->m_PatchBuffer = new byte[pNode->m_PatchSize];
			pNode->m_PatchBits   = QueryHashTable.Bits;
		
			pNode->m_PatchOffset  = 0;
		}

		pNode->m_CurrentPart++;

		// Make sure buffer created
		ASSERT(pNode->m_PatchBuffer);
		if(pNode->m_PatchBuffer == NULL)
		{
			pNode->CloseWithReason("QHT Patch Error");
			return;
		}

		// Copy part into buffer
		if( pNode->m_PatchOffset + QueryHashTable.PartSize <= pNode->m_PatchSize )
		{
			memcpy( pNode->m_PatchBuffer + pNode->m_PatchOffset, QueryHashTable.Part, QueryHashTable.PartSize);
			pNode->m_PatchOffset += QueryHashTable.PartSize;
		}
		else
		{
			pNode->CloseWithReason("QHT Patch Error");
			return;
		}

		// If all parts done process patch
		if( QueryHashTable.PartNum == QueryHashTable.PartTotal )
		{
			if(QueryHashTable.Compressed)
				pNode->m_PatchCompressed = true;

			pNode->m_PatchReady = true;

			return;
		}
	}
}

void CG2Control::ApplyPatchTable(CG2Node* pNode)
{
	byte* PatchTable = NULL;

	// Decompress table if needed
	if( pNode->m_PatchCompressed )
	{
		PatchTable = new byte[pNode->m_PatchSize];

		DWORD UncompressedSize = pNode->m_PatchSize;

		int zerror = uncompress( PatchTable, &UncompressedSize, pNode->m_PatchBuffer, pNode->m_PatchOffset);
		if( zerror != Z_OK )
		{
			pNode->CloseWithReason("QHT Decompress Error");
			delete [] PatchTable;
			return;
		}

		ASSERT( UncompressedSize == pNode->m_PatchSize );

		delete [] pNode->m_PatchBuffer;
		pNode->m_PatchBuffer = NULL;
	}
	else
	{
		PatchTable = pNode->m_PatchBuffer; // Make sure deleted in function!
		pNode->m_PatchBuffer = NULL; 
	}

	
	// Apply patch
	// Only can accurately convert smaller tables, a node's larger table is not kept around to patch against later

	int RemoteSize = pNode->m_PatchSize;
	if( pNode->m_PatchBits == 4 )
		RemoteSize /= 4;

	double Factor = (double) G2_TABLE_SIZE / (double) RemoteSize; // SMALLER means LARGER remote TABLE

	int remotePos = 0;
	int rByte = 0, rBit = 0;

	int i = 0, j = 0;
	for(i = 0; i < pNode->m_PatchSize; i++)
	{
		// 4 bit hit table (should not be in G2, but we handle it)
		if( pNode->m_PatchBits == 4 )
		{
			// high bit
			remotePos = i * 2;
			if( PatchTable[i] >> 4 )
			{
				if(Factor == 1)
					pNode->m_RemoteHitTable[remotePos >> 3] ^= 1 << (remotePos & 7);
				else
					SetQHTBit(remotePos, Factor, pNode);
			}
			
			// low bit
			remotePos++;
			if( PatchTable[i] & 0xF )
			{
				if(Factor == 1)
					pNode->m_RemoteHitTable[remotePos >> 3] ^= 1 << (remotePos & 7);
				else
					SetQHTBit(remotePos, Factor, pNode);
			}
		}

		// 1 bit standard hit table
		else if( pNode->m_PatchBits == 1 )
		{
			if(Factor == 1)
			{
				pNode->m_RemoteHitTable[i] ^= PatchTable[i];
			}
			else
			{
				for(j = 0; j < 8; j++)
				{
					remotePos = i * 8 + j;
					if( PatchTable[i] & (1 << j) )
						SetQHTBit(remotePos, Factor, pNode);
				}
			}
		}
	}


	delete [] PatchTable;
	PatchTable = NULL;

	// Patch table for node modified, if node a child update inter-hub QHT
	if( pNode->m_NodeMode == G2_CHILD )
		for( i = 0; i < m_G2NodeList.size(); i++)
			if(m_G2NodeList[i]->m_NodeMode == G2_HUB && m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
				m_G2NodeList[i]->m_SendDelayQHT = true;

}

// Only use when factor not 1
void CG2Control::SetQHTBit(int &remotePos, double &Factor, CG2Node* pNode)
{
	int localPos  = 0;
	int lByte = 0, lBit = 0;

	for(double Next = 0; Next < Factor; Next++)
	{
		localPos = remotePos * Factor + Next;

		lByte = ( localPos >> 3 ); 
		lBit  = ( localPos & 7 ); 

		// Switch byte
		pNode->m_RemoteHitTable[lByte] ^= 1 << lBit;
	}
}

void CG2Control::Receive_QKR(G2_RecvdPacket &PacketQKR)
{
	ASSERT( PacketQKR.pTCP == NULL ); // Should not be receiving TCP QKRs

	G2_QKR QueryKeyRequest;

	m_pProtocol->Decode_QKR(PacketQKR.Root, QueryKeyRequest);

	if(m_ClientMode == G2_HUB)
	{
		m_PacketsQKR[AVG_TOTAL]++;
		if(QueryKeyRequest.dna)
			m_PacketsQKR[AVG_DNA]++;

		if( QueryKeyRequest.RequestingAddress.Host.S_addr == 0)
			QueryKeyRequest.RequestingAddress = PacketQKR.Source;

		G2_QKA QueryKeyAnswer;
		QueryKeyAnswer.QueryKey = GenerateQueryKey(PacketQKR.Source.Host.S_addr);
		QueryKeyAnswer.SendingAddress = PacketQKR.Source;

		// Check if node banned
		std::map<uint32, G2_QueryKey>::iterator itInfo = m_KeyInfo.find(QueryKeyAnswer.QueryKey);
		if(itInfo != m_KeyInfo.end())
			if( itInfo->second.Banned || itInfo->second.RetryAfter > time(NULL))
				return;

		Send_QKA(QueryKeyRequest.RequestingAddress, QueryKeyAnswer);
	}
}

void CG2Control::Receive_QKA(G2_RecvdPacket &PacketQKA)
{
	G2_QKA QueryKeyAnswer;

	m_pProtocol->Decode_QKA(PacketQKA.Root, QueryKeyAnswer);

	IPv4 KeySource = PacketQKA.Source;

	// Forward QKA
	// Disabled checks because qka's come in for firewalled ips or zero still meant for us
	// Using a bad qk does hurt, on q2 if wrong another qk is issued
	//if(QueryKeyAnswer.SendingAddress.Host.S_addr != 0) // Why would QKA/SNA be 0?
	//	if(m_pNet->m_CurrentIP.S_addr != QueryKeyAnswer.SendingAddress.Host.S_addr)
	//	{
			if(m_ClientMode == G2_HUB)
			{
				std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(QueryKeyAnswer.SendingAddress.Host.S_addr);
				if(itNode != m_G2NodeAddrMap.end())
					if(itNode->second->m_NodeMode == G2_CHILD)
					{
						QueryKeyAnswer.QueriedAddress = KeySource;
						Send_QKA(QueryKeyAnswer.SendingAddress, QueryKeyAnswer, itNode->second);
						return;
					}
					//else
					//	ASSERT(0);
			}
			//else
			//	ASSERT(0);

			//return;
		//}

	// If this packet was forwarded from hub
	if( QueryKeyAnswer.QueriedAddress.Host.S_addr )
		KeySource = QueryKeyAnswer.QueriedAddress;

	std::map<uint32, G2HubInfo>::iterator itHub = m_GlobalHubs.find( KeySource.Host.S_addr ^ m_GlobalUnique);
	if( itHub != m_GlobalHubs.end())
	{
		m_SearchPacketsRecvd++;

		if( QueryKeyAnswer.QueryKey == 0) // Not authorized to use hub
		{
			m_GlobalHubs.erase(itHub);
			return;
		}

		itHub->second.ExpireTime = time(NULL) + GLOBAL_HUB_EXPIRE;
		itHub->second.TryCount  = 0;
		itHub->second.NextTry   = time(NULL); // Send query right away
		itHub->second.QueryKey  = QueryKeyAnswer.QueryKey;

		if( QueryKeyAnswer.QueriedAddress.Host.S_addr )
			itHub->second.Router = PacketQKA.Source;
	}
}

void CG2Control::Receive_Q1(G2_RecvdPacket &PacketQ1)
{
	if(PacketQ1.pTCP)
		PacketQ1.pTCP->CloseWithReason("G1 Queries not supported");
}

void CG2Control::Receive_Q2(G2_RecvdPacket &PacketQ2)
{
	G2_Q2 Query;

	m_pProtocol->Decode_Q2(PacketQ2.Root, Query);

	// Make sure not loopback
	if(PacketQ2.Source.Host.S_addr == m_pNet->m_CurrentIP.S_addr &&
		PacketQ2.Source.Port == m_pNet->m_CurrentPort)
	{
		ASSERT(0);
		return;
	}

	// Lookup SearchID in route table
	std::map<uint32, G2_Route>::iterator itRoute = m_RouteMap.find( HashGuid(Query.SearchGuid) );
	if(itRoute != m_RouteMap.end())
		return; // Already received query


	// If in hub mode and received udp
	if(m_ClientMode == G2_HUB && PacketQ2.pTCP == NULL)
	{
		m_PacketsQ2[AVG_TOTAL]++;
		if(Query.dna)
			m_PacketsQ2[AVG_DNA]++;

		IPv4 ResponseAddress = Query.ReturnAddress;
		if(ResponseAddress.Host.S_addr == 0)
			ResponseAddress = PacketQ2.Source;

		uint32 GenKey = GenerateQueryKey(PacketQ2.Source.Host.S_addr);
		if(Query.QueryKey != GenKey )
		{
			G2_QKA QueryKeyAnswer;
			QueryKeyAnswer.QueryKey = GenKey;
			QueryKeyAnswer.SendingAddress = PacketQ2.Source;

			Send_QKA(ResponseAddress, QueryKeyAnswer);

			return;
		}


		// Check if node banned
		std::map<uint32, G2_QueryKey>::iterator itInfo = m_KeyInfo.find(Query.QueryKey);
		if(itInfo != m_KeyInfo.end())
		{
			if( time(NULL) < itInfo->second.RetryAfter || itInfo->second.Banned)
			{
				 itInfo->second.Banned = true;
				 itInfo->second.RetryAfter = time(NULL) + QA_QUERY_RETRY * 2;
				 return;
			}
		}
		else
		{
			G2_QueryKey KeyInfo;
			KeyInfo.RetryAfter = time(NULL) + QA_QUERY_RETRY;
			
			m_KeyInfo[Query.QueryKey] = KeyInfo;
		}

		G2_QA QueryAck;
		QueryAck.SearchGuid = Query.SearchGuid;
		QueryAck.Timestamp  = time(NULL);
		QueryAck.RetryAfter = QA_QUERY_RETRY;
		

		// Add self as searched
		G2NodeInfo LocalInfo;
		LocalInfo.Address.Host = m_pNet->m_CurrentIP;
		LocalInfo.Address.Port = m_pNet->m_CurrentPort;
		LocalInfo.LeafCount    = CountChildConnects();

		QueryAck.DoneHubs.push_back(LocalInfo);

		int AltHosts = 0;
		
		// Add connected nodes as searched
		for(int i = 0; i < m_G2NodeList.size(); i++)
			if( m_G2NodeList[i]->m_NodeMode == G2_HUB && m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
			{
				QueryAck.DoneHubs.push_back(m_G2NodeList[i]->m_NodeInfo);

				// Add Alternate Host
				if( m_G2NodeList[i]->m_HubNeighbors.size() && AltHosts < ALT_HOST_MAX)
				{
					int rndIdx = rand() % m_G2NodeList[i]->m_HubNeighbors.size();
					QueryAck.AltHubs.push_back( m_G2NodeList[i]->m_HubNeighbors[ rndIdx ] );
				
					AltHosts++;
				}
			}

		Send_QA(ResponseAddress, QueryAck);
	}


	if(m_ClientMode == G2_CHILD && PacketQ2.pTCP == NULL)
	{
		//ASSERT(0); // Probably just downgraded
		return;
	}


	// Create query structure that share thread can use
	GnuQuery G2Query;
	G2Query.Network    = NETWORK_G2;
	G2Query.SearchGuid = Query.SearchGuid;

	if( PacketQ2.pTCP )
		G2Query.OriginID = PacketQ2.pTCP->m_G2NodeID;

	G2Query.DirectAddress = Query.ReturnAddress;

	if(m_ClientMode == G2_HUB)
	{
		// Set packet to be forwarded, program will know not to forward to hubs when originID is set
		G2Query.Forward = true;
		
		if(G2Query.DirectAddress.Host.S_addr == 0) // DA of zero means nat search, this prevents inter-hub QH2s
			G2Query.Forward = false;

		if(  PacketQ2.Root.PacketSize < MAX_QUERY_PACKET_SIZE )
		{
			memcpy(G2Query.Packet, PacketQ2.Root.Packet, PacketQ2.Root.PacketSize);
			G2Query.PacketSize = PacketQ2.Root.PacketSize;
		}
		else
		{
			//ASSERT(0); Large query, log
			return;
		}
	}

	// Add Search terms to query
	if( !Query.DescriptiveName.IsEmpty() )
		G2Query.Terms.push_back( Query.DescriptiveName );
	
	if( !Query.Metadata.IsEmpty() )
		G2Query.Terms.push_back( Query.Metadata );
	
	for( int i = 0; i < Query.URNs.size(); i++ )
		G2Query.Terms.push_back( Query.URNs[i] );

	G2Query.MinSize = Query.MinSize;
	G2Query.MaxSize = Query.MaxSize;

	// Put query on process queue and signal thread
	m_pShare->m_QueueAccess.Lock();
		m_pShare->m_PendingQueries.push_front(G2Query);	
	m_pShare->m_QueueAccess.Unlock();

	m_pShare->m_TriggerThread.SetEvent();

	// Insert into route table
	G2_Route Q2Route;
	Q2Route.Address = Query.ReturnAddress;
	Q2Route.RouteID = Query.SearchGuid;
	Q2Route.ExpireTime = time(NULL) + ROUTE_EXPIRE;

	if(Query.ReturnAddress.Host.S_addr == 0)
		Q2Route.Address = PacketQ2.Source;

	// If route goes to self, set it to the source of the Q2, node firewalled needs us to route QH2s
	if(Q2Route.Address.Host.S_addr == m_pNet->m_CurrentIP.S_addr && Q2Route.Address.Port == m_pNet->m_CurrentPort)
		Q2Route.Address = PacketQ2.Source;
	
	m_RouteMap[ HashGuid(Query.SearchGuid) ] = Q2Route;
}

void CG2Control::Receive_QA(G2_RecvdPacket &PacketQA)
{	
	G2_QA QueryAck;

	m_pProtocol->Decode_QA(PacketQA.Root, QueryAck);
		
	// Check if QA needs to be forwarded
	std::map<uint32, G2_Route>::iterator itRoute = m_RouteMap.find( HashGuid(QueryAck.SearchGuid) );
	if(itRoute != m_RouteMap.end())
		if(m_ClientMode == G2_HUB)
		{
			std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(itRoute->second.Address.Host.S_addr);
			if(itNode != m_G2NodeAddrMap.end()) 
				if(itNode->second->m_NodeMode == G2_CHILD)
				{
					QueryAck.FromAddress = PacketQA.Source;
					itRoute->second.ExpireTime = time(NULL) + ROUTE_EXPIRE;
					Send_QA(itNode->second->m_Address, QueryAck, itNode->second);
					return;
				}
		}

	// Get object that did the search
	G2_Search* pSearch = NULL;

	std::list<G2_Search*>::iterator itSearch;
	for(itSearch = m_G2Searches.begin(); itSearch != m_G2Searches.end(); itSearch++)
		if((*itSearch)->Query.SearchGuid == QueryAck.SearchGuid)
		{
			pSearch = *itSearch;
			break;
		}

	if(pSearch == NULL)
		return;

	m_SearchPacketsRecvd++;

	// Mark hubs as done
	for( int i = 0; i < QueryAck.DoneHubs.size(); i++)
	{
		if(QueryAck.DoneHubs[i].Address.Host.S_addr == 0)
			continue;

#ifdef _DEBUG
		std::map<uint32, bool>::iterator itTried = pSearch->TriedHubs.find( QueryAck.DoneHubs[i].Address.Host.S_addr );
		if( itTried != pSearch->TriedHubs.end())
			pSearch->SearchedDupes++;
#endif

		pSearch->TriedHubs[QueryAck.DoneHubs[i].Address.Host.S_addr] = true;

		pSearch->SearchedHubs++;
		pSearch->SearchedChildren += QueryAck.DoneHubs[i].LeafCount;

		// Add new hubs to cache
		UpdateGlobal( QueryAck.DoneHubs[i] );	
	}

	// Add alt hubs to cache
	for( i = 0; i < QueryAck.AltHubs.size(); i++)
	{
		if(QueryAck.AltHubs[i].Address.Host.S_addr == 0)
			continue;
		
		UpdateGlobal( QueryAck.AltHubs[i] );
	}

	IPv4 AckSource = PacketQA.Source;

	// If this packet was forwarded from hub
	if( QueryAck.FromAddress.Host.S_addr )
		AckSource = QueryAck.FromAddress;

	pSearch->TriedHubs[AckSource.Host.S_addr] = true;

	std::map<uint32, G2HubInfo>::iterator itHub = m_GlobalHubs.find(AckSource.Host.S_addr ^ m_GlobalUnique);
	if(itHub != m_GlobalHubs.end())
	{
		itHub->second.ExpireTime = time(NULL) + GLOBAL_HUB_EXPIRE;
		itHub->second.TryCount   = 0;
		itHub->second.NextTry    = QueryAck.RetryAfter + time(NULL);
	}

	// Progress made
	std::map<int, CGnuSearch*>::iterator itGnuSearch;
	for(itGnuSearch = m_pNet->m_SearchIDMap.begin(); itGnuSearch != m_pNet->m_SearchIDMap.end(); itGnuSearch++)
		if(itGnuSearch->second->m_QueryID == pSearch->Query.SearchGuid)
			if(m_pCore->m_dnaCore->m_dnaEvents)
				m_pCore->m_dnaCore->m_dnaEvents->SearchProgress(itGnuSearch->second->m_SearchID);
}

void CG2Control::Receive_QH2(G2_RecvdPacket &PacketQH2)
{	
	G2_QH2 QueryHit;
	m_pProtocol->Decode_QH2(PacketQH2.Root, QueryHit);

	// Check if QH2 needs to be forwarded
	std::map<uint32, G2_Route>::iterator itRoute = m_RouteMap.find( HashGuid(QueryHit.SearchGuid) );
	if(itRoute != m_RouteMap.end())
		if(m_ClientMode == G2_HUB)
		{
			QueryHit.HopCount++;
			if(QueryHit.HopCount > 5)
				return;

			itRoute->second.ExpireTime = time(NULL) + ROUTE_EXPIRE;
			Send_QH2(itRoute->second.Address, QueryHit);
		}

	// Add hubs to cache
	for( int i = 0; i < QueryHit.NeighbouringHubs.size(); i++)
	{
		if(QueryHit.NeighbouringHubs[i].Host.S_addr == 0)
			continue;

		G2NodeInfo Hub;
		Hub.Address.Host = QueryHit.NeighbouringHubs[i].Host;
		Hub.Address.Port = QueryHit.NeighbouringHubs[i].Port;
		UpdateGlobal( Hub );
	}

	// Find search
	CGnuSearch* pSearch = NULL;
	for(i = 0; i < m_pNet->m_SearchList.size(); i++)
		if(m_pNet->m_SearchList[i]->m_QueryID == QueryHit.SearchGuid)
		{
			pSearch = m_pNet->m_SearchList[i];
			break;
		}

	// Dont return here because there could be result of file downloading
	
	m_SearchPacketsRecvd++;

	// Convert G2 hit to common FileSource
	FileSource G2Source;
	G2Source.Address.Host = QueryHit.Address.Host;
	G2Source.Address.Port = QueryHit.Address.Port;

	G2Source.Network = NETWORK_G2;

	G2Source.PushID = QueryHit.NodeID;
	G2Source.Vendor = GetVendor( CString(QueryHit.Vendor, 4) );
	G2Source.Distance = QueryHit.HopCount;

	for(int i = 0; i < QueryHit.NeighbouringHubs.size(); i++)
		G2Source.DirectHubs.push_back( QueryHit.NeighbouringHubs[i] );

	
	std::map<int, int>     MetaIDMap;
	std::map<int, CString> MetaValueMap;
	m_pShare->m_pMeta->ParseMeta(QueryHit.UnifiedMetadata, MetaIDMap, MetaValueMap);


	for( i = 0; i < QueryHit.Hits.size(); i++)
	{
		G2Source.Name = QueryHit.Hits[i].DescriptiveName;
		G2Source.NameLower = QueryHit.Hits[i].DescriptiveName;
		G2Source.NameLower.MakeLower();

		G2Source.Size = QueryHit.Hits[i].ObjectSize;
		
		// Hash
		G2Source.Sha1Hash  = "";
		G2Source.TigerHash = "";

		for(int j = 0; j < QueryHit.Hits[i].URNs.size(); j++)
		{
			if( QueryHit.Hits[i].URNs[j].Left(13) == "urn:bitprint:" && QueryHit.Hits[i].URNs[j].GetLength() == 13 + 32 + 1 + 39)
			{
				G2Source.Sha1Hash  = QueryHit.Hits[i].URNs[j].Mid(13, 32);
				G2Source.TigerHash = QueryHit.Hits[i].URNs[j].Right(39);
			}

			else if( QueryHit.Hits[i].URNs[j].Left(9) == "urn:sha1:" && QueryHit.Hits[i].URNs[j].GetLength() == 9 + 32)
				G2Source.Sha1Hash  = QueryHit.Hits[i].URNs[j].Right(32);

			else if( QueryHit.Hits[i].URNs[j].Left(16) == "urn:tree:tiger/:" && QueryHit.Hits[i].URNs[j].GetLength() == 16 + 39)
				G2Source.TigerHash = QueryHit.Hits[i].URNs[j].Right(39);
		}

		// Meta
		G2Source.MetaID = 0;
		G2Source.AttributeMap.clear();
		G2Source.GnuExtraInfo.clear();

		std::map<int, int>::iterator itMetaID = MetaIDMap.find(QueryHit.Hits[i].Index);

		if(itMetaID != MetaIDMap.end())
			G2Source.MetaID = itMetaID->second;
		else if( !QueryHit.Hits[i].Metadata.IsEmpty() )
			G2Source.MetaID = m_pShare->m_pMeta->MetaIDfromXml( QueryHit.Hits[i].Metadata );
		
		if( G2Source.MetaID )
		{
			CString MetaXml;

			std::map<int, CString>::iterator itMetaValue  = MetaValueMap.find(QueryHit.Hits[i].Index);
			std::map<int, CGnuSchema*>::iterator itSchema = m_pShare->m_pMeta->m_MetaIDMap.find(G2Source.MetaID);

			if( itMetaValue != MetaValueMap.end() )
				MetaXml = itMetaValue->second;
			else if( !QueryHit.Hits[i].Metadata.IsEmpty() )
				MetaXml = QueryHit.Hits[i].Metadata;

			if( !MetaXml.IsEmpty() )
				if(itSchema != m_pShare->m_pMeta->m_MetaIDMap.end() )
				{
					itSchema->second->SetResultAttributes(G2Source.AttributeMap, MetaXml);

					MetaXml.Replace("&apos;", "'");
					G2Source.GnuExtraInfo.push_back(MetaXml);
				}
		}

	
		// Speed
		G2Source.Speed = 0;

		for(j = 0; j < QueryHit.HitGroups.size(); j++)
			if( QueryHit.HitGroups[j].GroupID == QueryHit.Hits[i].GroupID )
			{
				G2Source.Speed = QueryHit.HitGroups[j].Speed / 8;
			}


		// Send to searches
		if(pSearch)
			pSearch->IncomingSource( G2Source );


		// Send to downloads
		std::map<CString, CGnuDownloadShell*>::iterator itDown = m_pTrans->m_DownloadHashMap.find(G2Source.Sha1Hash);
		if(itDown != m_pTrans->m_DownloadHashMap.end())
		{
			CGnuDownloadShell* pShell = itDown->second;
			pShell->AddHost(G2Source);
		}
	}
}

void CG2Control::Receive_PUSH(G2_RecvdPacket &PacketPUSH)
{
	G2_PUSH Push;
	m_pProtocol->Decode_PUSH(PacketPUSH.Root, Push);

	GnuPush G2Push;
	G2Push.Network = NETWORK_G2;
	G2Push.Address = Push.BackAddress;

	m_pTrans->DoPush( G2Push );
}

void CG2Control::Receive_MCR(G2_RecvdPacket &PacketMCR)
{
	G2_MCR ModeChangeRequest;
	m_pProtocol->Decode_MCR(PacketMCR.Root, ModeChangeRequest);

	ASSERT(PacketMCR.pTCP);
	if( PacketMCR.pTCP == NULL)
		return;

	if(ModeChangeRequest.Hub)
	{
		if(time(NULL) < m_ModeChangeTimeout)
			return;

		if( PacketMCR.pTCP->m_NodeMode != G2_HUB ||
			m_ClientMode != G2_CHILD)
		{
			ASSERT(0);
			Send_MCA(PacketMCR.pTCP, false);
			return;
		}

		if(m_pPrefs->m_SupernodeAble == false ||
			m_pNet->m_TcpFirewall ||
			m_pNet->m_UdpFirewall != UDP_FULL ||
			!m_pNet->m_BehindRouter)
		{
			Send_MCA(PacketMCR.pTCP, false);
			return;
		}

		Send_MCA(PacketMCR.pTCP, true);

		SwitchG2ClientMode(G2_HUB, true);
	}
}

void CG2Control::Receive_MCA(G2_RecvdPacket &PacketMCA)
{
	G2_MCA ModeChangeAck;
	m_pProtocol->Decode_MCA(PacketMCA.Root, ModeChangeAck);

	CG2Node* UpgradedNode = NULL;

	std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(PacketMCA.Source.Host.S_addr);
	if(itNode != m_G2NodeAddrMap.end())
		UpgradedNode = itNode->second;

	if(UpgradedNode == NULL)
		return;

	if(UpgradedNode->m_TriedUpgrade && ModeChangeAck.Hub)
	{
		m_NextUpgrade = time(NULL) + 60*60; // Node upgrade do not upgrade another for a hour
		UpgradedNode->CloseWithReason("Child Upgraded");
	}
}

void CG2Control::Receive_PM(G2_RecvdPacket &PacketPM)
{
	G2_PM PrivateMessage;
	m_pProtocol->Decode_PM(PacketPM.Root, PrivateMessage);

	if(PrivateMessage.Destination.Host.S_addr == m_pNet->m_CurrentIP.S_addr)
	{
		m_pCore->m_pChat->RecvDirectMessage(PacketPM.Source, PrivateMessage);
		return;
	}

	if(PacketPM.pTCP || m_ClientMode != G2_HUB)
		return;
	
	std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(PrivateMessage.Destination.Host.S_addr);
	if(itNode != m_G2NodeAddrMap.end())
	{
		PrivateMessage.SendingAddress = PacketPM.Source;
		Send_PM(PrivateMessage.Destination, PrivateMessage, itNode->second);
	}
}

void CG2Control::Receive_CLOSE(G2_RecvdPacket &PacketCLOSE)
{
	G2_CLOSE Close;
	m_pProtocol->Decode_CLOSE(PacketCLOSE.Root, Close);

	if(PacketCLOSE.pTCP)
		PacketCLOSE.pTCP->Recv_Close(Close);
}

void CG2Control::Receive_CRAWLR(G2_RecvdPacket &PacketCRAWLR)
{
	G2_CRAWLR CrawlRequest;
	m_pProtocol->Decode_CRAWLR(PacketCRAWLR.Root, CrawlRequest);

	G2_CRAWLA CrawlAck;

	// Self Info
	GetLocalNodeInfo(CrawlAck.G2Self);
	CrawlAck.G2Self.Client = m_pCore->GetUserAgent();
	CrawlAck.G2Self.Mode   = m_ClientMode;

	// G2 Nodes
	for(int i = 0; i < m_G2NodeList.size(); i++)
		if( m_G2NodeList[i]->m_Status == SOCK_CONNECTED && m_G2NodeList[i]->m_NodeInfo.Address.Host.S_addr != 0)
			{
				G2NodeInfo G2Node  = m_G2NodeList[i]->m_NodeInfo;
				G2Node.Client      = m_G2NodeList[i]->m_RemoteAgent;
				G2Node.ConnectUptime = time(NULL) - m_G2NodeList[i]->m_ConnectTime.GetTime();

				if( m_G2NodeList[i]->m_NodeMode == G2_HUB ) 
					CrawlAck.G2Hubs.push_back( G2Node );

				if( CrawlRequest.ReqLeaves && m_G2NodeList[i]->m_NodeMode == G2_CHILD ) 
					CrawlAck.G2Leaves.push_back( G2Node );
			}

	// Gnu Info
	if( CrawlRequest.ReqG1 && m_pNet->m_pGnu)
	{
		CrawlAck.GnuSelf.Mode      = m_pNet->m_pGnu->m_GnuClientMode;
		CrawlAck.GnuSelf.LeafCount = m_pNet->m_pGnu->CountLeafConnects();
		CrawlAck.GnuSelf.LeafMax   = m_pPrefs->m_MaxLeaves;
		CrawlAck.GnuSelf.NetBpsIn  = m_pNet->m_pGnu->m_NetSecBytesDown;
		CrawlAck.GnuSelf.NetBpsOut = m_pNet->m_pGnu->m_NetSecBytesUp;
		CrawlAck.GnuSelf.UpSince   = m_pNet->m_pGnu->m_ClientUptime.GetTime();
		
		for(int i = 0; i < m_pNet->m_pGnu->m_NodeList.size(); i++)
			if(m_pNet->m_pGnu->m_NodeList[i]->m_Status == SOCK_CONNECTED)
			{
				GnuNodeInfo GnuNode;
				GnuNode.Address.Host = StrtoIP(m_pNet->m_pGnu->m_NodeList[i]->m_HostIP);
				GnuNode.Address.Port = m_pNet->m_pGnu->m_NodeList[i]->m_Port;
				GnuNode.Client       = m_pNet->m_pGnu->m_NodeList[i]->m_RemoteAgent;
				GnuNode.LeafMax      = m_pNet->m_pGnu->m_NodeList[i]->m_NodeLeafMax;
				GnuNode.LibraryCount = m_pNet->m_pGnu->m_NodeList[i]->m_NodeFileCount;
				GnuNode.UpSince      = m_pNet->m_pGnu->m_NodeList[i]->m_HostUpSince.GetTime();
				GnuNode.ConnectUptime = time(NULL) - m_pNet->m_pGnu->m_NodeList[i]->m_ConnectTime.GetTime();

				if(m_pNet->m_pGnu->m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER)
					CrawlAck.GnuUPs.push_back( GnuNode );

				if(CrawlRequest.ReqLeaves && m_pNet->m_pGnu->m_NodeList[i]->m_GnuNodeMode == GNU_LEAF)
					CrawlAck.GnuLeaves.push_back( GnuNode );
			}
	}

	CrawlAck.OrigRequest = CrawlRequest;

	Send_CRAWLA(PacketCRAWLR.Source, CrawlAck);
}

void CG2Control::GetLocalNodeInfo(G2NodeInfo &LocalNode)
{
	LocalNode.Address.Host = m_pNet->m_CurrentIP;
	LocalNode.Address.Port = m_pNet->m_CurrentPort;

	LocalNode.NodeID = m_pPrefs->m_ClientID;

	memcpy( LocalNode.Vendor, (LPCSTR) m_pCore->m_ClientCode, 4);

	LocalNode.LibraryCount  = m_pShare->m_TotalLocalFiles;
	LocalNode.LibrarySizeKB = m_pShare->m_TotalLocalSize;

	LocalNode.LeafCount = CountChildConnects();
	LocalNode.LeafMax   = m_pPrefs->m_MaxLeaves;

	LocalNode.HubAble  = m_pPrefs->m_SupernodeAble;
	LocalNode.Firewall = (m_pNet->m_TcpFirewall || m_pNet->m_UdpFirewall != UDP_FULL);
	LocalNode.Router   = m_pNet->m_BehindRouter;

	LocalNode.NetBpsIn  = m_NetSecBytesDown;
	LocalNode.NetBpsOut = m_NetSecBytesUp;
	LocalNode.UdpBpsIn  = m_pDispatch->m_AvgUdpDown.GetAverage();
	LocalNode.UdpBpsOut = m_pDispatch->m_AvgUdpUp.GetAverage();

	LocalNode.Cpu = m_pCore->m_SysSpeed;
	LocalNode.Mem = m_pCore->m_SysMemory;

	LocalNode.UpSince = m_ClientUptime.GetTime();
	
	LocalNode.Latitude  = m_pPrefs->m_GeoLatitude;
	LocalNode.Longitude = m_pPrefs->m_GeoLongitude;

	LocalNode.PacketsQKR[AVG_DNA]   = m_PacketsAvgQKR[AVG_DNA].GetAverage();
	LocalNode.PacketsQKR[AVG_TOTAL] = m_PacketsAvgQKR[AVG_TOTAL].GetAverage();
	LocalNode.PacketsQ2[AVG_DNA]   = m_PacketsAvgQ2[AVG_DNA].GetAverage();
	LocalNode.PacketsQ2[AVG_TOTAL] = m_PacketsAvgQ2[AVG_TOTAL].GetAverage();
}

void CG2Control::RoutePacket(G2_RecvdPacket &Packet, GUID &TargetID)
{
	std::map<uint32, G2_Route>::iterator itRoute = m_RouteMap.find( HashGuid(TargetID) );
	if(itRoute == m_RouteMap.end())
		return;


	G2_Route NodeRoute = itRoute->second;

	bool RouteTCP = false, RouteUDP = false;

	CG2Node* pDest = NULL;

	std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(NodeRoute.Address.Host.S_addr);
	if(itNode != m_G2NodeAddrMap.end()) 
		pDest = itNode->second;
		
	// If received via UDP, a packet may not be forwarded via UDP 
	if(Packet.pTCP == NULL && pDest)
		RouteTCP = true;

	else
	{
		// If received from a hub, a packet may only be forwarded to a leaf 
		if(Packet.pTCP->m_NodeMode == G2_HUB)
			if(pDest && pDest->m_NodeMode == G2_CHILD)
				RouteTCP = true;
		
		// If received from a leaf via TCP, a packet may be forwarded anywhere 
		if(Packet.pTCP->m_NodeMode == G2_CHILD)
		{
			if(pDest)
				RouteTCP = true;
			else 
				RouteUDP = true;
		}
	}

	if(RouteTCP)
		pDest->SendPacket( Packet.Root.Packet, Packet.Root.PacketSize );
	if(RouteUDP)
		m_pDispatch->SendPacket(NodeRoute.Address, Packet.Root.Packet, Packet.Root.PacketSize );

	// Keep route alive
	if( NodeRoute.ExpireTime )
		NodeRoute.ExpireTime = time(NULL) + ROUTE_EXPIRE;
}

void CG2Control::Send_PI(IPv4 Target, G2_PI &Ping, CG2Node* pTCP)
{
	m_pProtocol->Encode_PI(Ping);

	if(pTCP)
		pTCP->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
	else
		m_pDispatch->SendPacket(Target, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
}

void CG2Control::Send_PO(G2_PI &Ping, CG2Node* pTCP)
{
	G2_PO Pong;
	Pong.Relay = Ping.Relay;

	m_pProtocol->Encode_PO(Pong);

	if( Ping.UdpAddress.Host.S_addr )
		m_pDispatch->SendPacket(Ping.UdpAddress, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
	else if( pTCP)
	{
		if( !Ping.Relay )
			pTCP->SendPacket( m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
		else
			ASSERT(0);
	}
	else
		ASSERT(0);
}

void CG2Control::Send_LNI(CG2Node* pTCP)
{
	G2_LNI LocalInfo;

	GetLocalNodeInfo(LocalInfo.Node);

	// Send packet
	m_pProtocol->Encode_LNI(LocalInfo);

	pTCP->SendPacket( m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
}

void CG2Control::Send_KHL(CG2Node* pTCP)
{
	G2_KHL KnownHubList;

	KnownHubList.RefTime = time(NULL);

	for(int i = 0; i < m_G2NodeList.size(); i++)
		if( m_G2NodeList[i]->m_NodeMode == G2_HUB )
			if( m_G2NodeList[i]->m_NodeInfo.Address.Host.S_addr != 0)
				KnownHubList.Neighbours.push_back( m_G2NodeList[i]->m_NodeInfo );

	int AltHosts = 0;
	std::list<Node>::iterator itNode;
	for( itNode = m_pCache->m_G2Real.begin(); itNode != m_pCache->m_G2Real.begin() && AltHosts < ALT_HOST_MAX; itNode++)
	{
		G2NodeInfo CachedNode;
		CachedNode.Address.Host = StrtoIP((*itNode).Host);
		CachedNode.Address.Port = (*itNode).Port;
		CachedNode.LastSeen     = (*itNode).LastSeen.GetTime();

		KnownHubList.Cached.push_back( CachedNode );
		
		AltHosts++;
	}

	// Send packet
	m_pProtocol->Encode_KHL(KnownHubList);

	pTCP->SendPacket( m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
}

void CG2Control::Send_QHT(CG2Node* pTCP, bool Reset)
{
	if( pTCP->m_NodeMode == G2_CHILD)
	{
		ASSERT(0);
		return;
	}

	G2_QHT QueryHitTable;
	QueryHitTable.Reset     = Reset;
	QueryHitTable.TableSize = G2_TABLE_SIZE * 8;
	QueryHitTable.Infinity  = 1;
	QueryHitTable.Bits      = 1;

	// Reset
	if( Reset)
	{
		m_pProtocol->Encode_QHT(QueryHitTable);
		pTCP->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
		return;
	}

	// Else Patch
	byte PatchTable[G2_TABLE_SIZE];

	// Get local table
	memcpy(PatchTable, m_pShare->m_pWordTable->m_LocalHitTable, G2_TABLE_SIZE);
	

	if( m_ClientMode == G2_HUB )
	{
		// Build aggregate table of leaves
		for(int i = 0; i < m_G2NodeList.size(); i++)
			if(m_G2NodeList[i]->m_Status == SOCK_CONNECTED && m_G2NodeList[i]->m_NodeMode == G2_CHILD)
			{
				for(int k = 0; k < G2_TABLE_SIZE; k++)
					PatchTable[k] &= m_G2NodeList[i]->m_RemoteHitTable[k];
			}
	}

	// Create local table if not created yet (needed to save qht info if needed to send again)
	if( pTCP->m_LocalHitTable == NULL)
	{
		pTCP->m_LocalHitTable = new byte [G2_TABLE_SIZE];

		memset( pTCP->m_LocalHitTable,  0xFF, G2_TABLE_SIZE );
	}

	// xor current table, with what we already gave them to get patch table
	//int entries = 0;

	for(int i = 0; i < G2_TABLE_SIZE; i++)
	{
		byte temp = PatchTable[i];
		PatchTable[i] ^= pTCP->m_LocalHitTable[i];
		pTCP->m_LocalHitTable[i] = temp;

		
	
		/*for(byte mask = 1; mask != 0; mask *= 2)
			if( ~temp & mask )
			{
				byte cool = PatchTable[i];
				byte cool2 = temp;
			}*/
	}

	//CString TableFull = GetPercentage( G2_TABLE_SIZE * 8, entries );

	// Compress
	QueryHitTable.Compressed = true;

	const int CompAlloc = G2_TABLE_SIZE * 1.1 + 12;
	byte   CompressedPatch[CompAlloc];
	uint32 CompressedSize = CompAlloc;
	if( compress(CompressedPatch, (DWORD*) &CompressedSize, PatchTable, G2_TABLE_SIZE) != Z_OK )
	{
		ASSERT(0);
		return;
	}


	// Breakup into 2k chunks
	int Parts = 1;
	int MaxPartSize = 2048;

	while( CompressedSize > Parts * MaxPartSize  )
		Parts++;


	std::list<QueuedPacket*> QHTBundle;

	int Offset = 0;
	for(i = 1; i <= Parts; i++)
	{
		QueryHitTable.PartNum   = i;
		QueryHitTable.PartTotal = Parts;

		int PartSize = (CompressedSize > MaxPartSize) ? MaxPartSize : CompressedSize;

		byte* PartData = new byte[PartSize + 5]; // Make room for patch header
		memcpy(PartData + 5, CompressedPatch + Offset, PartSize);

		QueryHitTable.Part     = PartData;
		QueryHitTable.PartSize = PartSize;

		// Encode and send
		m_pProtocol->Encode_QHT(QueryHitTable);

		QueuedPacket* OutPacket = new QueuedPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
		QHTBundle.push_front( OutPacket );  // 4, 3, 2, 1 order

		delete [] PartData;

		CompressedSize -= PartSize;
		Offset += PartSize;
	}

	// Packets added manually to keep order of QHT in stream, not bundled together either because could cause a size max exception
	std::list<QueuedPacket*>::iterator itPacket;
	for( itPacket = QHTBundle.begin(); itPacket != QHTBundle.end(); itPacket++)
		pTCP->m_OutboundPackets.push_front(*itPacket); // 1, 2, 3, 4, x, x, x order

	pTCP->FlushSendQueue();
}

void CG2Control::Send_QKR(IPv4 Target)
{
	if( Target.Host.S_addr == 0 || Target.Port == 0 )
		return;


	G2_QKR QueryKeyRequest;

	if( m_pNet->m_UdpFirewall == UDP_FULL )
	{
		QueryKeyRequest.RequestingAddress.Host = m_pNet->m_CurrentIP;
		QueryKeyRequest.RequestingAddress.Port = m_pNet->m_CurrentPort;
	}
	//else if( m_pNet->m_UdpFirewall == UDP_NAT)
	//{
		// Send requesting address null, when behind nat return port can change
	//}
	else
	{
		CG2Node* pHub = GetRandHub();
		
		if( pHub )
			QueryKeyRequest.RequestingAddress = pHub->m_Address;
		else
			return;	
	}


	m_pProtocol->Encode_QKR(QueryKeyRequest);

	m_pDispatch->SendPacket(Target, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
}

void CG2Control::Send_QKA(IPv4 Target, G2_QKA &QueryKeyAnswer, CG2Node* pTCP)
{
	m_pProtocol->Encode_QKA(QueryKeyAnswer);

	// Forwarding QKA
	if(pTCP)
	{
		ASSERT( QueryKeyAnswer.QueriedAddress.Host.S_addr );

		pTCP->SendPacket( m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
	}

	// Response to QKR
	else
		// Send without requesting ack because by time ack come, qkr out of sendcache buffer
		m_pDispatch->SendPacket(Target, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize, false, false);
}

void CG2Control::Send_Q2(G2HubInfo* pHub, G2_Q2 &Query, CG2Node* pTCP)
{
	if( pTCP )
	{
		Query.ReturnAddress = IPv4();
	}
	else if( m_pNet->m_UdpFirewall == UDP_FULL)
	{
		Query.ReturnAddress.Host = m_pNet->m_CurrentIP;
		Query.ReturnAddress.Port = m_pNet->m_CurrentPort;
	}
	/*else if( m_pNet->m_UdpFirewall == UDP_NAT)
	{
		Query.ReturnAddress = IPv4();
	}*/
	else
	{
		CG2Node* pRandHub = GetRandHub();
		
		if( pRandHub )
			Query.ReturnAddress = pRandHub->m_Address;
		else
			return;
	}


	m_pProtocol->Encode_Q2(Query);

	if( pTCP )
		pTCP->SendPacket( m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
	else
	{
		ASSERT(pHub);
		m_pDispatch->SendPacket(pHub->Address, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
	}
}

// Called from share thread
void CG2Control::Send_Q2(GnuQuery &FileQuery, std::list<int> &MatchingNodes)
{
	ASSERT(FileQuery.Forward);

	m_G2NodeAccess.Lock();

		CG2Node* pOrigin = NULL;

		std::map<int, CG2Node*>::iterator itNode = m_G2NodeIDMap.find( FileQuery.OriginID );
		if( itNode != m_G2NodeIDMap.end() )
			pOrigin = itNode->second;

		// Forward to all matches except origin
		std::list<int>::iterator itMatch;

		for( itMatch = MatchingNodes.begin(); itMatch != MatchingNodes.end(); itMatch++)
		{	
			itNode = m_G2NodeIDMap.find( *itMatch );

			if( itNode != m_G2NodeIDMap.end() )
				if( (pOrigin == NULL ) || // Q2 received UDP, forward to all
					(pOrigin && pOrigin->m_NodeMode == G2_HUB && itNode->second->m_NodeMode == G2_CHILD) || // Q2 received tcp from hub, only forward to children
					(pOrigin && pOrigin->m_NodeMode == G2_CHILD && pOrigin != itNode->second) ) // Q2 received tcp from child, forward to everyone except back to same child
					itNode->second->SendPacket( FileQuery.Packet, FileQuery.PacketSize, true );
		}

	m_G2NodeAccess.Unlock();
}

void CG2Control::Send_QA(IPv4 Target, G2_QA &QueryAck, CG2Node* pTCP)
{
	m_pProtocol->Encode_QA(QueryAck);

	if( pTCP == NULL )
		// Send without requesting ack because by time ack come, q2 out of sendcache buffer
		m_pDispatch->SendPacket(Target, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize, false, false );
	else
		pTCP->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
}

// Called from share thread
void CG2Control::Send_QH2(GnuQuery &FileQuery, std::list<UINT> &MatchingIndexes)
{
	// Setup query hit
	G2_QH2 QueryHit;

	QueryHit.SearchGuid  = FileQuery.SearchGuid;
	QueryHit.NodeID      = m_pPrefs->m_ClientID;

	QueryHit.Address.Host = m_pNet->m_CurrentIP;
	QueryHit.Address.Port = m_pNet->m_CurrentPort;

	m_G2NodeAccess.Lock();

		for(int i = 0; i < m_G2NodeList.size(); i++)
			if( m_G2NodeList[i]->m_NodeMode == G2_HUB && m_G2NodeList[i]->m_Status == SOCK_CONNECTED )
				QueryHit.NeighbouringHubs.push_back( m_G2NodeList[i]->m_Address );

	m_G2NodeAccess.Unlock();

	memcpy( QueryHit.Vendor, (LPCSTR) m_pCore->m_ClientCode, 4);

	// Setup hit group
	G2_QH2_HG HitGroup;
	HitGroup.GroupID     = 0;
	HitGroup.QueueLength = m_pTrans->m_UploadQueue.m_Queue.size() + m_pTrans->m_UploadList.size();
	HitGroup.Capacity    = m_pPrefs->m_MaxUploads;
	HitGroup.Speed       = m_pNet->m_RealSpeedUp * 8 / 1024;

	QueryHit.HitGroups.push_back( HitGroup );


	// Add files to hit
	int HitsinPacket    = 0;
	int SentReplies		= 0;
	int	MaxReplies    	= m_pPrefs->m_MaxReplies;
	
	m_pShare->m_FilesAccess.Lock();

	std::list<UINT>::iterator itIndex;
	for( itIndex = MatchingIndexes.begin(); itIndex != MatchingIndexes.end(); itIndex++)
	{
		int pos = *itIndex;

		if(m_pShare->m_SharedFiles[pos].Name.size() == 0)
			continue;

		if( m_pShare->m_SharedFiles[pos].HashValues[HASH_SHA1].empty() )
			continue;

		if(MaxReplies && MaxReplies <= SentReplies)	
			break;


		HitsinPacket++;
		SentReplies++;
		m_pShare->m_SharedFiles[pos].Matches++;
		

		G2_QH2_H Hit;
		Hit.DescriptiveName = m_pShare->m_SharedFiles[pos].Name.c_str();
		Hit.ObjectSize		= m_pShare->m_SharedFiles[pos].Size;
		Hit.CachedSources	= m_pShare->m_SharedFiles[pos].AltHosts.size();
		Hit.URNs.push_back( CString("urn:sha1:") + m_pShare->m_SharedFiles[pos].HashValues[HASH_SHA1].c_str() );
		
		std::map<int, CGnuSchema*>::iterator itMeta = m_pCore->m_pMeta->m_MetaIDMap.find(m_pShare->m_SharedFiles[pos].MetaID);
		if(itMeta != m_pCore->m_pMeta->m_MetaIDMap.end())
			Hit.Metadata = itMeta->second->AttrMaptoNetXML(m_pShare->m_SharedFiles[pos].AttributeMap);

		QueryHit.Hits.push_back( Hit );

	
		// Send hits
		if( HitsinPacket >= 10 )
		{
			Send_QH2(FileQuery, QueryHit);
			
			QueryHit.Hits.clear();
			HitsinPacket = 0;
		}
	}


	// Send remaining hits
	if( HitsinPacket > 0 )
		Send_QH2(FileQuery, QueryHit);


	m_pShare->m_FilesAccess.Unlock();

	
}	

// Called from share thread
void CG2Control::Send_QH2(GnuQuery &FileQuery, G2_QH2 &QueryHit)
{
	m_pThreadProtocol->Encode_QH2(QueryHit);

	// If firewalled or no direct address send to origin hub
	if( FileQuery.DirectAddress.Host.S_addr == 0 /*|| m_pNet->m_UdpFirewall == UDP_BLOCK*/) // Save hub b/w for now // Send to hub because it can receive acks
	{
		std::map<int, CG2Node*>::iterator itNode = m_G2NodeIDMap.find( FileQuery.OriginID );

		if( itNode != m_G2NodeIDMap.end() )
			itNode->second->SendPacket( m_pThreadProtocol->m_FinalPacket, m_pThreadProtocol->m_FinalSize, true );
	}
	
	// Else send to direct address
	else if( FileQuery.DirectAddress.Host.S_addr )
	{
		m_pDispatch->SendPacket(FileQuery.DirectAddress, m_pThreadProtocol->m_FinalPacket, m_pThreadProtocol->m_FinalSize, true );
	}
}

void CG2Control::Send_QH2(IPv4 Target, G2_QH2 &QueryHit)
{
	CG2Node* pDest = NULL;
	std::map<uint32, CG2Node*>::iterator itNode = m_G2NodeAddrMap.find(Target.Host.S_addr);
	if(itNode != m_G2NodeAddrMap.end())
		pDest = itNode->second;
		

	m_pProtocol->Encode_QH2(QueryHit);

	if(pDest)
		pDest->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
	else
		m_pDispatch->SendPacket(Target, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
}

void CG2Control::Send_PUSH(FileSource* HostSource)
{
	if(HostSource->DirectHubs.size() == 0) 
		return; // Nowhere to send push to

	G2_PUSH Push;

	Push.Destination = HostSource->PushID;
	
	Push.BackAddress.Host = m_pNet->m_CurrentIP;
	Push.BackAddress.Port = m_pNet->m_CurrentPort;


	m_pProtocol->Encode_PUSH(Push);

	int randHub = rand() % HostSource->DirectHubs.size();
	m_pDispatch->SendPacket(HostSource->DirectHubs[randHub], m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
}

void CG2Control::Send_MCR(CG2Node* pTCP)
{
	G2_MCR ModeChangeRequest;
	ModeChangeRequest.Hub = true;

	m_pProtocol->Encode_MCR(ModeChangeRequest);

	pTCP->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
}

void CG2Control::Send_MCA(CG2Node* pTCP, bool Accept)
{
	G2_MCA ModeChangeAck;

	if(Accept)
		ModeChangeAck.Hub  = true;
	else
		ModeChangeAck.Deny = true;

	m_pProtocol->Encode_MCA(ModeChangeAck);

	pTCP->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize );
	m_pDispatch->SendPacket(pTCP->m_Address, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
}

void CG2Control::Send_PM(IPv4 Target, G2_PM &PrivateMessage, CG2Node* pTCP)
{
	m_pProtocol->Encode_PM(PrivateMessage);

	if(pTCP)
		pTCP->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
	else
		m_pDispatch->SendPacket(Target, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
}

void CG2Control::Send_CLOSE(CG2Node* pTCP, G2_CLOSE &Close)
{
	m_pProtocol->Encode_CLOSE(Close);

	pTCP->SendPacket(m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize);
}

void CG2Control::Send_CRAWLA(IPv4 Target, G2_CRAWLA &CrawlAck)
{
	m_pProtocol->Encode_CRAWLA(CrawlAck);

	m_pDispatch->SendPacket(Target, m_pProtocol->m_FinalPacket, m_pProtocol->m_FinalSize, false, true, true);
}