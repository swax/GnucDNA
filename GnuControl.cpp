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
	m_LastSearchTime = time(NULL);

	m_LanSock = new CGnuLocal(this);
	m_LanSock->Init();

	m_NetworkName		= "GNUTELLA";

	m_LastConnect = 0;
	m_TryingConnect = false;

	// Ultrapeers
	m_GnuClientMode   = GNU_LEAF;
	m_ForcedUltrapeer = false;

	m_NextUpgrade	= time(NULL);
	m_ModeChangeTimeout = 0;
	m_AutoUpgrade   = time(NULL);
	
	m_MinsBelow10   = 0;
	m_MinsBelow70   = 0;
	m_NoConnections = 0;

	
	// Bandwidth
	m_NetSecBytesDown	= 0;
	m_NetSecBytesUp		= 0;
	m_TcpSecBytesDown	= 0;
	m_TcpSecBytesUp		= 0;

	m_Minute = 0;
	
	m_pProtocol = new CGnuProtocol(this);
	m_pDatagram = new CGnuDatagram(this);

	m_pProtocol->Init();
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

	m_OobHitsLock.Lock();
		std::map<uint32, OobHit*>::iterator itHit = m_OobHits.begin();
		while(itHit != m_OobHits.end())
		{
			delete itHit->second;
			itHit = m_OobHits.erase(itHit);
		}
	m_OobHitsLock.Unlock();


	delete m_pDatagram;
	m_pDatagram = NULL;

	delete m_pProtocol;
	m_pProtocol = NULL;
}

void CGnuControl::SendUdpConnectRequest(CString Host, UINT Port)
{
	if(FindNode(Host, Port, false) != NULL)
		return;

	IPv4 address;
	address.Host = StrtoIP(Host);
	address.Port = Port;

	m_pProtocol->Send_Ping(NULL, 1, true, NULL, address);
}

void CGnuControl::AddNode(CString Host, UINT Port)
{
	if(FindNode(Host, Port, false) != NULL)
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
			pNode->CloseWithReason("Manually Removed", BYE_MANUAL);

	NodeUpdate(pNode);
}

CGnuNode* CGnuControl::FindNode(CString Host, UINT Port, bool Connected)
{
	std::map<uint32, CGnuNode*>::iterator itNode = m_GnuNodeAddrMap.find( StrtoIP(Host).S_addr);

	if(itNode != m_GnuNodeAddrMap.end())
	{
		if(Connected && itNode->second->m_Status != SOCK_CONNECTED)
			return NULL;

		return itNode->second;
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
// TRANSFER CONTROL

int CGnuControl::CountUltraConnects()
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

int CGnuControl::CountConnecting()
{
	int Connecting = 0;

	for(int i = 0; i < m_NodeList.size(); i++)	
		if(m_NodeList[i]->m_Status == SOCK_CONNECTING)			
			Connecting++;

	return Connecting;
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
}

void CGnuControl::Timer()
{
	CleanDeadSocks();
	
	ManageNodes();

	DynQueryTimer();
	OobHitsTimer();

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


	m_Minute++;

	if(m_Minute == 60)
	{
		MinuteTimer();

		m_Minute = 0; 
	}

	if( m_TriedConnects.size() > 100 )
		m_TriedConnects.clear();
}

void CGnuControl::MinuteTimer()
{
	UltrapeerBalancing();


	// Purge old OOB Guids from conversion list
	std::map<uint32, GUID>::iterator itGuid = m_OobtoRealGuid.begin();

	while(itGuid != m_OobtoRealGuid.end() )
	{
		bool Found = false;

		// Find in dyn query list
		std::map<uint32, DynQuery*>::iterator itDyn;
		for(itDyn = m_DynamicQueries.begin(); itDyn != m_DynamicQueries.end(); itDyn++)
			if( memcmp( &itGuid->second, &itDyn->second->RealGuid, 16) == 0)
				Found = true;

		if(Found)
			itGuid++;
		else
			itGuid = m_OobtoRealGuid.erase(itGuid);
	}

	// Output buffer size load
	/*int BufferSize = 0;
	int BackSize   = 0;

	for(i = 0; i < m_NodeList.size(); i++)
	{
		BackSize += m_NodeList[i]->m_BackBuffLength;

		for(int j = 0; j < MAX_TTL; j++)
			BufferSize += m_NodeList[i]->m_PacketListLength[j];
	}

	m_pCore->DebugLog( CommaIze(NumtoStr(BufferSize / 1024)) + "KB Packet Buffer," + CommaIze(NumtoStr(BackSize / 1024)) + "KB Back Buffer");
	*/

	// Trace nodes, percent full of hash vs. bandwidth out
	/*TRACE0("\n");
	TRACE0("Remote Ultrapeer, % Full vs. Out Bandwidth\n");

	int TotalBW = 0;
	for(int i = 0; i < m_NodeList.size(); i++)
		if(m_NodeList[i]->m_Status == SOCK_CONNECTED)
		{
			int BitsNotSet = 0;

			for(int x = 0; x < GNU_TABLE_SIZE; x++)
				for(int y = 0; y < 8; y++)
					if( m_NodeList[i]->m_RemoteHitTable[x] & (1 << y) )
						BitsNotSet++;

			CString PrcFull = GetPercentage(GNU_TABLE_SIZE * 8, GNU_TABLE_SIZE * 8 - BitsNotSet);
			
			int bwout       = m_NodeList[i]->m_AvgBytes[1].GetAverage();
			TotalBW += bwout;

			CString mode = (m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER) ? "Ultrapeer" : "Leaf";

			TRACE0( mode + " " + IPtoStr(m_NodeList[i]->m_Address.Host) + ": " + PrcFull + " Full, out " + CommaIze(NumtoStr(bwout)) + " B/s\n");
		}
	
	TRACE0("\nTotal Bandwidth out " + CommaIze(NumtoStr(TotalBW)) + "B/s\n");

	TRACE0("\n");*/
}

void CGnuControl::HourlyTimer()
{
	// Web Cache check in
	if(m_GnuClientMode == GNU_ULTRAPEER)
		if(CountUltraConnects() == 0 || !m_pNet->m_TcpFirewall) // make sure if no connects to update, because firewall cant be tested without peers
			m_pCache->WebCacheUpdate("gnutella");

	m_TriedConnects.clear();


	// Helps promote new versions as ultrapeers
	
	if(m_GnuClientMode == GNU_LEAF && UltrapeerAble() &&		 // in child mode and ultrapeer able
	   time(NULL) - m_AutoUpgrade > 6 * 60 * 60 &&				 // up for at least 6 hours
	   m_pCore->m_SysSpeed > 1500 && m_pCore->m_SysMemory > 500) // processor higher than 2gz and mem higher than 512
	{
		// Only auto upgrade if connected to lesser versionsf
		bool DoUpgrade = true;

		for(int i = 0; i < m_NodeList.size(); i++)
			if( m_NodeList[i]->m_Status == SOCK_CONNECTED )
			{
				if(m_NodeList[i]->m_RemoteAgent.Find("GnucDNA ") == -1)
					continue;

				int dnapos = m_NodeList[i]->m_RemoteAgent.Find("GnucDNA ");

				// if remote version greater or equal to current, dont upgrade
				if( VersiontoInt(m_NodeList[i]->m_RemoteAgent.Mid(dnapos + 8, 7)) >= VersiontoInt(m_pCore->m_DnaVersion) )
					DoUpgrade = false;
			}

		// Upgrade to ultrapeer
		if( DoUpgrade )
		{
			SwitchGnuClientMode(GNU_ULTRAPEER);
			m_AutoUpgrade = time(NULL);
		}
	}
}

void CGnuControl::ManageNodes()
{
	m_NetSecBytesDown = 0;
	m_NetSecBytesUp	  = 0;

	int Connecting			= 0;
	int UltraConnects		= 0;
	int UltraDnaConnects	= 0;
	int LeafConnects		= 0;
	int LeafDnaConnects		= 0;

	// Add bandwidth and count connections
	for(int i = 0; i < m_NodeList.size(); i++)
	{
		m_NodeList[i]->Timer();

		if( m_NodeList[i]->m_Status == SOCK_CONNECTING )
			Connecting++;

		else if( m_NodeList[i]->m_Status == SOCK_CONNECTED )
		{
			m_NetSecBytesDown += m_NodeList[i]->m_AvgBytes[0].GetAverage();
			m_NetSecBytesUp   += m_NodeList[i]->m_AvgBytes[1].GetAverage();

			if( m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER )
			{
				UltraConnects++;

				if(m_NodeList[i]->m_RemoteAgent.Find("GnucDNA") > 0)
					UltraDnaConnects++;
			}

			else if( m_NodeList[i]->m_GnuNodeMode == GNU_LEAF )
			{
				LeafConnects++;

				if(m_NodeList[i]->m_RemoteAgent.Find("GnucDNA") > 0)
					LeafDnaConnects++;
			}
		}
	}

	// Add in udp bandwidth
	m_TcpSecBytesDown = m_NetSecBytesDown;
	m_TcpSecBytesUp   = m_NetSecBytesUp;

	m_NetSecBytesDown += m_pDatagram->m_AvgUdpDown.GetAverage();
	m_NetSecBytesUp   += m_pDatagram->m_AvgUdpUp.GetAverage();


	// After 10 mins, if no connects, go into ultrapeer mode
	if( m_GnuClientMode == GNU_LEAF)
	{
		if(CountUltraConnects() == 0)
		{
			m_NoConnections++;

			// After 10 minutes of no connections upgrade to hub
			if(m_NoConnections >= 10 * 60)
			{
				SwitchGnuClientMode( GNU_ULTRAPEER);
				m_NoConnections = 0;
			}
		}	
		else
			m_NoConnections = 0;
	}

	m_TryingConnect = false;

	int MaxHalfConnects = m_pNet->GetMaxHalfConnects();

	if( m_pNet->NetworkConnecting(NETWORK_G2) )
		MaxHalfConnects /= 2;

	if( m_pNet->TransfersConnecting() )
		MaxHalfConnects /= 2;

	if(Connecting >= MaxHalfConnects)
		return;

	NeedDnaUltras = false;
	
	int OpenSlots = MaxHalfConnects - Connecting;

	if(m_GnuClientMode == GNU_LEAF)
	{
		if(UltraConnects && UltraDnaConnects * 100 / UltraConnects < 50)
			NeedDnaUltras = true;
		
		// Reduce leaf connects by 1 each 6 mins of inactivity, leveling at 1 connect after a half hour
		int LeafConnects = 5 - ((time(NULL) - m_LastSearchTime) / 60 / 6);
		
		m_pPrefs->m_LeafModeConnects = (LeafConnects <= 0) ? 1 : LeafConnects;

		if(UltraConnects < m_pPrefs->m_LeafModeConnects)
		{
			for(int i = 0; i < OpenSlots; i++)
				AddConnect(NeedDnaUltras);
		}
		else if(UltraConnects && NeedDnaUltras)
		{
			for(int i = 0; i <= OpenSlots / 2; i++)
				AddConnect(NeedDnaUltras); // if all thats needed are more dna, dont tax half connects
		}
		if(m_pPrefs->m_LeafModeConnects && UltraConnects > m_pPrefs->m_LeafModeConnects)
			DropNode(GNU_ULTRAPEER, NeedDnaUltras); 
	}
	

	if(m_GnuClientMode == GNU_ULTRAPEER)
	{
		if(UltraConnects && UltraDnaConnects * 100 / UltraConnects < 25)
			NeedDnaUltras = true;

		if(m_pPrefs->m_MinConnects && UltraConnects < m_pPrefs->m_MinConnects)
		{
			for(int i = 0; i < OpenSlots; i++)
				AddConnect(NeedDnaUltras);
		}
		else if(UltraConnects && NeedDnaUltras)
		{
			for(int i = 0; i <= OpenSlots / 2; i++)
				AddConnect(NeedDnaUltras); // if all thats needed are more dna, dont tax half connects
		}
		if(m_pPrefs->m_MaxConnects && UltraConnects > m_pPrefs->m_MaxConnects)
			DropNode(GNU_ULTRAPEER, NeedDnaUltras);


		while(LeafConnects > MAX_LEAVES)
		{
			bool NeedDnaLeaves = false;
			if(LeafConnects && LeafDnaConnects * 100 / LeafConnects < 50)
				NeedDnaLeaves = true;

			DropNode(GNU_LEAF, NeedDnaLeaves);
			LeafConnects--;
		}
	}
}

void CGnuControl::AddConnect(bool PrefDna)
{
	m_TryingConnect = true;

	if( PrefDna && ConnectFromCache(m_pCache->m_GnuDna, false) )
		return;

	if( ConnectFromCache(m_pCache->m_GnuReal, false) )
		return;

	if( ConnectFromCache(m_pCache->m_GnuPerm, true) )
		return;

	// No nodes in cache, if not connected to anyone either, havent made a connection for a minute
	// do web cache update
	if(CountUltraConnects() == 0 && m_LastConnect < time(NULL) - 60)
		m_pCache->WebCacheGetRequest("gnutella");
}

bool CGnuControl::ConnectFromCache(std::list<Node> &Cache, bool Perm)
{
	if( Cache.size() )
	{
		int CachePos = (Perm) ? rand() % Cache.size() : 0;
	
		std::list<Node>::iterator itNode = Cache.begin();
		for(int i = 0; itNode != Cache.end(); itNode++, i++)
			if(i == CachePos)
			{
				Node TryNode = *itNode;

				if( !Perm )
					Cache.erase(itNode);
				
				std::map<uint32, bool>::iterator itAddr = m_TriedConnects.find( StrtoIP(TryNode.Host).S_addr );
				if(itAddr != m_TriedConnects.end())
					break;

				m_TriedConnects[ StrtoIP(TryNode.Host).S_addr ] = true;

				// send udp request (do if blocked anyways for quick detect)
				///SendUdpConnectRequest(TryNode.Host, TryNode.Port);

				// if blocked send tcp
				//if(m_pNet->m_UdpFirewall == UDP_BLOCK)
					AddNode( TryNode.Host, TryNode.Port);

				return true;
			}
	}
	return false;
}

void CGnuControl::DropNode(int GnuMode, bool NeedDna)
{
	
	CGnuNode* DeadNode = NULL;
	CTime CurrentTime = CTime::GetCurrentTime();
	CTimeSpan LowestTime(0);

	// Drop youngest
	for(int i = 0; i < m_NodeList.size(); i++)
		if(SOCK_CONNECTED == m_NodeList[i]->m_Status && m_NodeList[i]->m_GnuNodeMode == GnuMode)
		{
			if(NeedDna && m_NodeList[i]->m_RemoteAgent.Find("GnucDNA") != -1)
				continue;

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
	{
		CGnuNode* pNode = *itNode;

		if( pNode->m_Status == SOCK_CLOSED && pNode->m_CloseWait > 3)
		{
			if(pNode->m_LastState != NONEXISTENT && pNode->m_LastState != TIME_WAIT)
			{
				itNode++;
				continue;
			}

			m_NodeAccess.Lock();

				delete pNode;
				itNode = m_NodeList.erase(itNode);
			
			m_NodeAccess.Unlock();
		}
		else
			itNode++;
	}

	// Browse Sockets
	std::vector<CGnuNode*>::iterator itBrowse;

	itBrowse = m_NodesBrowsing.begin();
	while(itBrowse != m_NodesBrowsing.end())
	{
		CGnuNode* pNode = *itBrowse;

		if( pNode->m_Status == SOCK_CLOSED && pNode->m_CloseWait > 3)
		{
			if(pNode->m_LastState != NONEXISTENT && pNode->m_LastState != TIME_WAIT)
			{
				itNode++;
				continue;
			}

			delete pNode;
			itNode = m_NodeList.erase(itNode);
		}
		else
			itNode++;
	}
}

bool CGnuControl::UltrapeerAble()
{
	// Check prefs
	if( !m_pPrefs->m_SupernodeAble )
		return false;

	// Must be an NT based system
	if( !m_pCore->m_IsKernalNT )
		return false;
	
	// Cant be behind firewall
	if( m_pNet->m_TcpFirewall )
		return false;

	// Full udp support
	if( m_pNet->m_UdpFirewall != UDP_FULL )
		return false;

	// Must have sufficient bandwidth
	if( !m_pNet->m_HighBandwidth )
		return false;

	// Only upgrade if 40 minutes of last downgrade, start at 0
	if(time(NULL) < m_ModeChangeTimeout)
		return false;
	
	// Cant be in 2 supernode modes at once
	if(m_pNet->m_pG2 && m_pNet->m_pG2->m_ClientMode == G2_HUB)
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

void CGnuControl::UltrapeerBalancing()
{
	// Run once per minute

	if(m_GnuClientMode != GNU_ULTRAPEER)
		return;


	// Get local load
	int LocalLoad = CountLeafConnects() * 100 / MAX_LEAVES;
	


	// Upgrade node if running above 90%, every 40 mins
	if( time(NULL) > m_NextUpgrade && LocalLoad > 90)
	{
		// Find child with highest score
		int HighScore = 0;
		CGnuNode* UpgradeNode = NULL;

		for(int i = 0; i < m_NodeList.size(); i++)	
			if(m_NodeList[i]->m_Status == SOCK_CONNECTED && m_NodeList[i]->m_GnuNodeMode == GNU_LEAF)
			{
				if(!m_NodeList[i]->m_StatsRecvd || !m_NodeList[i]->m_SupportsModeChange)
					continue;

				int NodeScore = ScoreNode(m_NodeList[i]);

				if(NodeScore > HighScore)
				{
					HighScore = NodeScore;
					UpgradeNode = m_NodeList[i];
				}
			}

		if(HighScore > 0)
		{
			packet_VendMsg RequestMsg;
			GnuCreateGuid(&RequestMsg.Header.Guid);
			RequestMsg.Ident = packet_VendIdent("GNUC", 61, 1);
			
			byte ReqUpgrade = 0x01; // signal upgrade request

			m_pProtocol->Send_VendMsg(UpgradeNode, RequestMsg, &ReqUpgrade, 1);

			UpgradeNode->m_TriedUpgrade = true;
		}
	}


	// Downgrade if running below 70% capacity for 40 minutes
	if( CountUltraConnects() && LocalLoad < 70 )
	{
		m_MinsBelow70++;

		if(m_MinsBelow70 > 40 && !m_ForcedUltrapeer)
		{
			SwitchGnuClientMode(GNU_LEAF);
			m_MinsBelow70 = 0;
		}
	}
	else
		m_MinsBelow70 = 0;


	// Meant as emergency get dialup, or messed up node out of hub mode
	// Downgrade if less than 10 children for 10 minutes
	// New ultrapeers not filling up quick enough, wait the 40 minutes
	/*if( CountUltraConnects() && CountLeafConnects() < 10 )
	{
		m_MinsBelow10++;

		if(m_MinsBelow10 > 10 && !m_ForcedUltrapeer)
		{
			SwitchGnuClientMode(GNU_LEAF);
			m_MinsBelow10 = 0;
		}
	}
	else
		m_MinsBelow10 = 0;*/

}

int CGnuControl::ScoreNode(CGnuNode* pNode)
{
	// Check for HubAble or Firewall
	if( !pNode->m_UltraAble ||
		pNode->m_FirewallTcp ||
	    pNode->m_TriedUpgrade)
		return 0;

	
	ASSERT(pNode->m_GnuNodeMode == GNU_LEAF);
	
	int Score = 0;

	// Leaf Max
	int leafmax = pNode->m_LeafMax;
	if( leafmax > OPT_LEAFMAX)
		leafmax = OPT_LEAFMAX;

	Score += leafmax * 100 / OPT_LEAFMAX;

	// Upgrade nodes of equal or greater versions only, dont use for downgrade 
	if(pNode->m_RemoteAgent.Find("GnucDNA ") > 0)
	{
		int dnapos = pNode->m_RemoteAgent.Find("GnucDNA ");

		// if remote version less than current, dont upgrade
		if( VersiontoInt(pNode->m_RemoteAgent.Mid(dnapos + 8, 7)) < VersiontoInt(m_pCore->m_DnaVersion) )
			return 0;
	}
	

	// Nodes with high uptime and capacity will have a chance to be hubs

	// Uptime of connection to local node, so long lasting nodes are proven trustworthy
	uint64 Uptime = time(NULL) - pNode->m_ConnectTime.GetTime();
	if( Uptime  > OPT_UPTIME)
		Uptime  = OPT_UPTIME;

	Score += Uptime * 100 / (OPT_UPTIME); // () because OPT_UPTIME is 6*60*60
	

	ASSERT(Score <= 200);

	return Score;
}

void CGnuControl::SwitchGnuClientMode(int GnuMode)
{
	// Requested mode already set
	if( m_GnuClientMode == GnuMode )
		return;

	// Remove all connections
	CString Reason = (GnuMode == GNU_ULTRAPEER) ? "Upgrading to Ultrapeer" : "Downgrading to Leaf";
	
	for(int i = 0; i < m_NodeList.size(); i++)
		m_NodeList[i]->CloseWithReason(Reason);

	// Change mode
	m_GnuClientMode = GnuMode;

	m_ModeChangeTimeout = time(NULL) + 40*60;
}

void CGnuControl::StopSearch(GUID SearchGuid)
{
	if(m_pNet->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER)
	{
		std::map<uint32, DynQuery*>::iterator itDyn;

		for(itDyn = m_DynamicQueries.begin(); itDyn != m_DynamicQueries.end(); itDyn++)
			if( memcmp( itDyn->second->Packet, &SearchGuid, 16) == 0)
			{
				delete itDyn->second;
				m_DynamicQueries.erase(itDyn);
				break;
			}
	}

	if(m_pNet->m_pGnu->m_GnuClientMode == GNU_LEAF)
	{
		packet_VendMsg ReplyMsg;
		ReplyMsg.Header.Guid = SearchGuid;
		ReplyMsg.Ident = packet_VendIdent("BEAR", 12, 1);
		
		uint16 Hits = 0xFFFF;

		for(int i = 0; i < m_NodeList.size(); i++)	
			if(m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER && 
				m_NodeList[i]->m_Status == SOCK_CONNECTED &&
				m_NodeList[i]->m_SupportsVendorMsg)
				m_pProtocol->Send_VendMsg(m_NodeList[i], ReplyMsg, &Hits, 2);
	}
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

			if(itDyn->second->Secs >= OldestAge)
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
		
		if( (pQuery->NodeID && itNode == m_NodeIDMap.end())   ||   // Leaf disconnected, nodeID 0 if local
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
					packet_Query* pQueryPacket = (packet_Query*)pQuery->Packet;

					if(pQuery->NodeID)
						m_TableRouting.Insert(pQueryPacket->Header.Guid, pQuery->NodeID);
						
					// If ultrapeer able to receive udp
					if(m_pNet->m_UdpFirewall == UDP_FULL)
						// If query not using OOB or doesnt support OOB
						if( (pQueryPacket->Flags.Set && pQueryPacket->Flags.OobHits == false) ||
							pQueryPacket->Flags.Set == false)
						{
							memcpy((byte*)pQueryPacket, &m_pNet->m_CurrentIP.S_addr, 4);
							memcpy(((byte*)pQueryPacket) + 13, &m_pNet->m_CurrentPort, 2);
							
							pQueryPacket->Flags.Set     = true;
							pQueryPacket->Flags.OobHits = true;

							m_OobtoRealGuid[ HashGuid(pQueryPacket->Header.Guid) ] = pQuery->RealGuid;
						}

					pUltra->SendPacket(pQuery->Packet, pQuery->PacketLength, PACKET_QUERY, MAX_TTL);

					pQuery->NodesQueried[pUltra->m_NodeID] = true;
					break;
				}

				Attempts++;
			}
		}
		

		// Send request to leaf to update hit count each interval
		if(pQuery->Secs % DQ_UPDATE_INTERVAL == 0 && itNode != m_NodeIDMap.end() && itNode->second->m_SupportsLeafGuidance)
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

void CGnuControl::OobHitsTimer()
{
	m_OobHitsLock.Lock();

	std::map<uint32, OobHit*>::iterator itHit = m_OobHits.begin();
	while(itHit != m_OobHits.end())
	{
		OobHit* pHit = itHit->second;

		if(pHit->Secs > OOB_TIMEOUT )
		{
			// Timed out send hits over tcp
			std::map<int, CGnuNode*>::iterator itNode = m_NodeIDMap.find(pHit->OriginID);
			if(itNode != m_NodeIDMap.end())
				for(int i = 0; i < pHit->QueryHits.size(); i++)
					itNode->second->SendPacket(pHit->QueryHits[i], pHit->QueryHitLengths[i], PACKET_QUERYHIT, ((packet_QueryHit*) pHit->QueryHits[i])->Header.TTL - 1);

			delete pHit;
			itHit = m_OobHits.erase(itHit);
			continue;
		}

	
		pHit->Secs++;	// Increase time query has been alive
		

		if( !pHit->SentReplyNum && pHit->QueryHits.size())
		{
			packet_VendMsg ReplyNum;
			memcpy(&ReplyNum.Header.Guid, pHit->QueryHits[0], 16);
			ReplyNum.Ident = packet_VendIdent("LIME", 12, 1);

			byte Num = (pHit->TotalHits > 255) ? 255 : pHit->TotalHits;
			m_pProtocol->Send_VendMsg( NULL, ReplyNum, &Num, 1, pHit->Target );

			pHit->SentReplyNum = true;
		}

		itHit++;
	}

	m_OobHitsLock.Unlock();
}

CGnuNode* CGnuControl::GetRandNode(int Type, bool dnaOnly)
{
	int Nodes = 0;

	for(int i = 0; i < m_NodeList.size(); i++)
		if(m_NodeList[i]->m_Status == SOCK_CONNECTED)
			if(m_NodeList[i]->m_GnuNodeMode == Type)
			{
				if(dnaOnly && m_NodeList[i]->m_SupportsUdpCrawl)
					continue;

				Nodes++;
			}
				

	if(Nodes)
	{
		int upReturn = rand() % Nodes;
		int upCurrent = 0;

		for(int i = 0; i < m_NodeList.size(); i++)
			if(m_NodeList[i]->m_Status == SOCK_CONNECTED)
				if(m_NodeList[i]->m_GnuNodeMode == Type)
				{
					if(dnaOnly && m_NodeList[i]->m_SupportsUdpCrawl)
						continue;

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

CString CGnuControl::GetPushProxyHeader()
{
	if( m_GnuClientMode == GNU_ULTRAPEER)
		return "";

	CString ProxyList;

	for(int i = 0; i < m_NodeList.size(); i++)
		if(m_NodeList[i]->m_PushProxy.Host.S_addr)
			ProxyList += IPv4toStr(m_NodeList[i]->m_PushProxy) + ", ";

	if(ProxyList.IsEmpty())
		return "";

	ProxyList.Trim(", ");

	return "X-Push-Proxy: " + ProxyList + "\r\n";
}

void CGnuControl::GetLocalNodeInfo(GnuNodeInfo &LocalNode)
{
	LocalNode.Client		= m_pCore->GetUserAgent();	
	LocalNode.Mode			= m_GnuClientMode;
	LocalNode.Address.Host	= m_pNet->m_CurrentIP;
	LocalNode.Address.Port	= m_pNet->m_CurrentPort;
	LocalNode.LeafCount		= CountLeafConnects();
	LocalNode.LeafMax		= MAX_LEAVES;
	LocalNode.NetBpsIn		= m_TcpSecBytesDown;
	LocalNode.NetBpsOut		= m_TcpSecBytesUp;
	LocalNode.UdpBpsIn		= m_pDatagram->m_AvgUdpDown.GetAverage();
	LocalNode.UdpBpsOut		= m_pDatagram->m_AvgUdpUp.GetAverage();
	LocalNode.UpSince		= m_ClientUptime.GetTime();
	LocalNode.LibraryCount  = m_pShare->m_TotalLocalFiles;
	LocalNode.HubAble		= m_pPrefs->m_SupernodeAble;
	LocalNode.Firewall		= (m_pNet->m_TcpFirewall || m_pNet->m_UdpFirewall != UDP_FULL);
	LocalNode.Router		= m_pNet->m_BehindRouter;
	LocalNode.Cpu			= m_pCore->m_SysSpeed;
	LocalNode.Mem			= m_pCore->m_SysMemory;
	LocalNode.Latitude		= m_pPrefs->m_GeoLatitude;
	LocalNode.Longitude		= m_pPrefs->m_GeoLongitude;
}