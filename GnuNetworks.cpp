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
#include "GnuControl.h"
#include "GnuDatagram.h"
#include "GnuCache.h"
#include "GnuNode.h"
#include "GnuSock.h"
#include "GnuSearch.h"
#include "G2Control.h"
#include "G2Node.h"
#include "G2Datagram.h"
#include "GnuPrefs.h"

#include "GnuTransfers.h"
#include "GnuDownloadShell.h"
#include "GnuDownload.h"
#include "GnuNetworks.h"


CGnuNetworks::CGnuNetworks(CGnuCore* pCore)
{
	m_pCore = pCore;

	m_pGnu = NULL;
	m_pG2  = NULL;

	m_NextNodeID   = 1;

	// Vars
	m_CurrentIP     = GuessLocalHost();
	m_CurrentPort   = rand() % 22500 + 2500;
	m_TcpFirewall	= true;
	m_UdpFirewall	= UDP_BLOCK;
	m_RealSpeedUp	= 0;
	m_RealSpeedDown	= 0;
	m_HaveUploaded	= false;
	m_HighBandwidth = false;

	// Searching
	m_NextSearchID = 1;
	
	// Cache
	m_pCache = new CGnuCache(this);
}

CGnuNetworks::~CGnuNetworks(void)
{
	// Socket cleanup
	if(m_SocketData.hSocket != INVALID_SOCKET)
		AsyncSelect(0);

	while( m_SockList.size() )
	{
		delete m_SockList.back();
		m_SockList.pop_back();
	}
	
	// Erase Searches
	while( m_SearchList.size() )
	{
		delete m_SearchList.back();
		m_SearchList.pop_back();
	}

	// Network cleanup
	if( m_pGnu )
	{
		delete m_pGnu;
		m_pGnu = NULL;
	}

	if( m_pG2 )
	{
		delete m_pG2;
		m_pG2 = NULL;
	}

	// Cache cleanup
	if( m_pCache )
	{
		m_pCache->endThreads();
		delete m_pCache;
		m_pCache = NULL;
	}
}

void CGnuNetworks::Connect_Gnu()
{
	if( m_pGnu == NULL )
	{
		m_pGnu = new CGnuControl(this);
		StartListening();
	}
}

void CGnuNetworks::Disconnect_Gnu()
{	
	if( m_pGnu )
	{
		delete m_pGnu;
		m_pGnu = NULL;
	}

	if( m_pGnu == NULL && m_pG2 == NULL)
		StopListening();
}

void CGnuNetworks::Connect_G2()
{
	if( m_pG2 == NULL )
	{
		m_pG2 = new CG2Control(this);
		StartListening();
	}
}

void CGnuNetworks::Disconnect_G2()
{
	if( m_pG2 )
	{
		delete m_pG2;
		m_pG2 = NULL;
	}

	if( m_pGnu == NULL && m_pG2 == NULL)
		StopListening();
}

int CGnuNetworks::GetNextNodeID()
{
	if(m_NextNodeID < 1)
		m_NextNodeID = 1;

	m_NextNodeID++;
	return m_NextNodeID;
}

void CGnuNetworks::Timer()
{
	// Node Timers
	if(m_pGnu)
		m_pGnu->Timer();

	if(m_pG2)
		m_pG2->Timer();
		

	// Search Timer
	for(int i = 0; i < m_SearchList.size(); i++)
		m_SearchList[i]->Timer();


	// Cache Timer
	m_pCache->Timer();


	// Sock Cleanup
	std::vector<CGnuSock*>::iterator itSock;

	itSock = m_SockList.begin();
	while( itSock != m_SockList.end())
	{
		CGnuSock* pSock = *itSock;

		if(pSock->m_SecsAlive > CONNECT_TIMEOUT || pSock->m_bDestroy)
		{
			delete *itSock;
			itSock = m_SockList.erase(itSock);
		}
		else
		{
			pSock->Timer();
			itSock++;
		}
	}

	// NAT detect clean
	if(m_UdpFirewall != UDP_FULL)
		while(m_NatDetectMap.size() > 5000) // can be large because firewalled node doesnt have much else to do
		{
			uint32 addy = m_NatDetectVect.front();

			std::map<uint32, bool>::iterator itHost = m_NatDetectMap.find(addy);
			if(itHost != m_NatDetectMap.end())
				m_NatDetectMap.erase(itHost);

			m_NatDetectVect.pop_front();
		}

}

void CGnuNetworks::HourlyTimer()
{
	if(m_pGnu)
		m_pGnu->HourlyTimer();

	if(m_pG2)
		m_pG2->HourlyTimer();
}

/*bool CGnuNetworks::ConnectingSlotsOpen()
{
	int OccupiedSlots = 0;

	// Gnu Sockets
	if( m_pGnu )
		for(int i = 0; i < m_pGnu->m_NodeList.size(); i++)
			if(m_pGnu->m_NodeList[i]->m_Status == SOCK_CONNECTING)
				OccupiedSlots++;

	// G2 Sockets
	if( m_pG2 )
		for(int i = 0; i < m_pG2->m_G2NodeList.size(); i++)
			if(m_pG2->m_G2NodeList[i]->m_Status == SOCK_CONNECTING)
				OccupiedSlots++;

	// Download Sockets
	for(int i = 0; i < m_pCore->m_pTrans->m_DownloadList.size(); i++)
		for(int j = 0; j < m_pCore->m_pTrans->m_DownloadList[i]->m_Sockets.size(); j++)
			if(m_pCore->m_pTrans->m_DownloadList[i]->m_Sockets[j]->m_Status == TRANSFER_CONNECTING)
				OccupiedSlots++;


	return OccupiedSlots < 20 ? true : false;
}*/

int CGnuNetworks::GetMaxHalfConnects()
{
	if(m_pCore->m_IsSp2 && !m_pCore->m_pPrefs->m_Sp2Override)
		return 6;

	return 18;
}

int CGnuNetworks::NetworkConnecting(int Network)
{
	int Connecting = 0;

	if(Network == NETWORK_GNUTELLA && m_pGnu)
	{
		for(int i = 0; i < m_pGnu->m_NodeList.size(); i++)
			if(m_pGnu->m_NodeList[i]->m_Status == SOCK_CONNECTING)
				Connecting++;
	}

	else if(Network == NETWORK_G2 && m_pG2)
	{
		for(int i = 0; i < m_pG2->m_G2NodeList.size(); i++)
			if(m_pG2->m_G2NodeList[i]->m_Status == SOCK_CONNECTING)
				Connecting++;
	}

	return Connecting;
}

int CGnuNetworks::TransfersConnecting()
{
	int Connecting = 0;

	for(int i = 0; i < m_pCore->m_pTrans->m_DownloadList.size(); i++)
		for(int j = 0; j < m_pCore->m_pTrans->m_DownloadList[i]->m_Sockets.size(); j++)
			if(m_pCore->m_pTrans->m_DownloadList[i]->m_Sockets[j]->m_Status == TRANSFER_CONNECTING)
				Connecting++;

	return Connecting;
}

IP CGnuNetworks::GuessLocalHost()
{
	m_BehindRouter = false;

	IP ReturnIP = StrtoIP("127.0.0.1");
	
	char ac[80];
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR)
		return ReturnIP;

	struct hostent* phe = gethostbyname(ac);
	if(phe == 0)
		return ReturnIP;

	for(int i = 0; phe->h_addr_list[i] != 0; i++) 
	{
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));

		IP Address = StrtoIP( inet_ntoa(addr) );

		if( IsPrivateIP(Address) )
		{
			if(ReturnIP.S_addr == StrtoIP("127.0.0.1").S_addr)
				ReturnIP = Address;

			m_BehindRouter = true;
		}
		else
			ReturnIP = Address;
	}
    
    return ReturnIP;
}
bool CGnuNetworks::StartListening()
{
	UINT AttemptPort = m_CurrentPort;

	if(m_pCore->m_pPrefs->m_ForcedPort)
		AttemptPort = m_pCore->m_pPrefs->m_ForcedPort;
	

	int	 Attempts = 0;
	bool Success  = false;
	int  Error    = 0;

	while(!Success && Attempts < 3)
	{
		StopListening();

		if( Create(AttemptPort) && Listen() )
		{	
			Success = true;
			m_CurrentPort = AttemptPort;

			// Set G2 to listen on same UDP port
			if( m_pG2 )
				m_pG2->m_pDispatch->Init();

			// Not same port, when protocols broken out tcp/udp will be same per protocol
			if( m_pGnu)
				m_pGnu->m_pDatagram->Init();
		
			return true;	
		}
		else
		{
			Error = GetLastError();
			ASSERT(0);
			//m_pCore->LogError("Create Error: " + NumtoStr(GetLastError()));
		}

		AttemptPort += rand() % 99 + 0;
		Attempts++;
	}

	return false;
}

void CGnuNetworks::StopListening()
{
	Close();
}


void CGnuNetworks::OnAccept(int nErrorCode) 
{
	if(nErrorCode)
		return;

	CGnuSock* Incoming = new CGnuSock(this);
	int Safe = Accept(*Incoming);

	if(Safe)
	{
		m_SockList.push_back(Incoming);
		Incoming->AsyncSelect(FD_WRITE|FD_READ|FD_CLOSE);
	}
	else
		delete Incoming;
}

void CGnuNetworks::EndSearch(int SearchID)
{
	std::vector<CGnuSearch*>::iterator itSearch;

	itSearch = m_SearchList.begin();
	while( itSearch != m_SearchList.end())
		if((*itSearch)->m_SearchID == SearchID)
		{
			delete *itSearch;
			itSearch = m_SearchList.erase(itSearch);
		}
		else
			itSearch++;
}

bool CGnuNetworks::NotLocal(Node TestNode)
{
	IP RemoteIP = StrtoIP(TestNode.Host);
	IP LocalIP = StrtoIP("127.0.0.1");
	
	UINT LocalPort  = m_CurrentPort;
	UINT RemotePort = TestNode.Port; 

	if(LocalPort == RemotePort)
	{
		if(RemoteIP.S_addr == LocalIP.S_addr)
			return false;

		LocalIP = m_CurrentIP;

		if(m_pCore->m_pPrefs->m_ForcedHost.S_addr)
			LocalIP = m_pCore->m_pPrefs->m_ForcedHost;

		if(RemoteIP.S_addr == LocalIP.S_addr)
			return false;
	}

	return true;
}

void CGnuNetworks::IncomingSource(GUID &SearchGuid, FileSource &Source)
{
	// Prevent user from getting unreachable sources
	if( m_TcpFirewall && Source.Firewall)
		return;

	// Find Requesting Searches
	CGnuSearch* pSearch = NULL;

	for(int i = 0; i < m_SearchList.size(); i++)
		if(SearchGuid == m_SearchList[i]->m_QueryID)// || (pNode && pNode->m_BrowseID == m_pNet->m_SearchList[i]->m_SearchID))
			m_SearchList[i]->IncomingHost(Source);
		

	// Find Matching Downloads
	if( !Source.Sha1Hash.IsEmpty() )
	{
		std::map<CString, CGnuDownloadShell*>::iterator itDown = m_pCore->m_pTrans->m_DownloadHashMap.find( Source.Sha1Hash );
		if(itDown != m_pCore->m_pTrans->m_DownloadHashMap.end())
			itDown->second->AddHost(Source);
	}
}

void CGnuNetworks::AddNatDetect(IP Host)
{
	if(m_UdpFirewall == UDP_FULL)
		return;

	// If node already in map its replaced, and dupe put on top of vector

	m_NatDetectMap[Host.S_addr] = true;
	m_NatDetectVect.push_back(Host.S_addr);
}