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
#include "GnuTransfers.h"
#include "GnuUpload.h"
#include "GnuUploadShell.h"
#include "GnuDownloadShell.h"

#include "GnuPrefs.h"
#include "GnuShare.h"
#include "GnuSearch.h"
#include "GnuCache.h"
#include "GnuRouting.h"
#include "G2Control.h"
#include "GnuProtocol.h"

#include "DnaCore.h"
#include "DnaNetwork.h"
#include "DnaEvents.h"

#include "GnuNode.h"


CGnuNode::CGnuNode(CGnuControl* pComm, CString Host, UINT Port)
{		
	m_pComm     = pComm;
	m_pNet      = pComm->m_pNet;
	m_pCore     = m_pNet->m_pCore;
	m_pTrans    = m_pCore->m_pTrans;
	m_pPrefs    = m_pCore->m_pPrefs;
	m_pCache    = pComm->m_pCache;
	m_pShare    = m_pCore->m_pShare;
	m_pProtocol = pComm->m_pProtocol;

	// Socket 
	m_NodeID = m_pNet->GetNextNodeID();
	m_pComm->m_NodeIDMap[m_NodeID] = this;

	// Duplicate can arise when doing tcp connect back test, make sure entry in map isnt replaced
	std::map<uint32, CGnuNode*>::iterator itAddr = m_pComm->m_GnuNodeAddrMap.find( StrtoIP(Host).S_addr);
	if(itAddr == m_pComm->m_GnuNodeAddrMap.end())
		m_pComm->m_GnuNodeAddrMap[StrtoIP(Host).S_addr] = this;

	m_Status = SOCK_CONNECTING;	
	m_StatusText = "Connecting";
	
	m_SecsTrying		= 0;
	m_SecsAlive			= 0;
	m_SecsDead			= 0;
	m_CloseWait			= 0;
	
	m_NextStatUpdate    = time(NULL) + 30*60;

	// Connection vars
	m_Address.Host = StrtoIP(Host);
	m_Address.Port = Port;

	m_pNet->AddNatDetect(m_Address.Host);

	m_NetworkName = m_pComm->m_NetworkName;
	
	m_GnuNodeMode		= 0;
	m_Inbound			= false;
	m_ConnectBack		= false;
	m_ConnectTime		= CTime::GetCurrentTime();

	m_SupportsVendorMsg    = false;
	m_SupportsLeafGuidance = false;
	m_SupportsDynQuerying  = false;
	m_SupportsStats		   = false;
	m_SupportsModeChange   = false;

	m_RemoteMaxTTL = 0;

	// Compression
	m_dnapressionOn = true;
	m_InflateRecv   = false;
	m_DeflateSend   = false;

	m_DeflateStreamSize = 0;
	m_ZipStat = 0;

	InflateStream.zalloc = Z_NULL;
	InflateStream.zfree  = Z_NULL;
	InflateStream.opaque = Z_NULL;
	InflateStream.state  = Z_NULL;

	DeflateStream.zalloc   = Z_NULL;
	DeflateStream.zfree    = Z_NULL;
	DeflateStream.opaque   = Z_NULL;
	DeflateStream.state    = Z_NULL;


	// Ultrapeers
	m_NodeFileCount	= 0;
	m_TriedUpgrade  = false;

	m_StatsRecvd	= false;
	m_LeafMax		= 0;
	m_LeafCount		= 0;
	m_UpSince		= 0;
	m_Cpu			= 0;
	m_Mem			= 0;
	m_UltraAble		= false;
	m_Router		= false;
	m_FirewallTcp   = false;
	m_FirewallUdp   = false;


	// QRP
	m_CurrentSeq = 1;

	m_PatchBuffer = NULL;
	m_PatchOffset = 0;
	m_PatchSize   = 0;
	m_PatchCompressed = false;
	m_PatchBits   = 0;

	m_PatchReady   = false;
	m_PatchTimeout = 60;

	m_RemoteTableInfinity = 0;
	m_RemoteTableSize     = 0;
	

	m_SendDelayPatch    = false;
	m_PatchWait         = 60;
	m_SupportInterQRP   = false;
	m_LocalHitTable     = NULL;

	// Host Browsing
	m_BrowseID	 = 0;
	m_BrowseSize = 0;
	m_BrowseBuffer	  = NULL;
	m_BrowseBuffSize  = 0;
	m_BrowseSentBytes = 0;
	m_BrowseRecvBytes = 0;
	m_BrowseCompressed = false;


	// Receiving
	m_ExtraLength  = 0;

	// Sending
	for(int i = 0; i < MAX_TTL; i++)
		m_PacketListLength[i] = 0;
	m_BackBuffLength = 0;


	// Packet Stats
	for(int i = 0; i < PACKETCACHE_SIZE; i++)
		for(int j = 0; j < 2; j++)
			m_StatPackets[i][j] = 0;

	m_StatPos		= 0;
	m_StatElements	= 0;
	m_Efficeincy    = 0;

	for(i = 0; i < 2; i++)
	{
		m_StatPings[i]		= 0;    
		m_StatPongs[i]		= 0; 
		m_StatQueries[i]	= 0; 
		m_StatQueryHits[i]	= 0; 
		m_StatPushes[i]		= 0; 
	}


	// Bandwidth
	for(i = 0; i < 3; i++)
	{
		m_AvgPackets[i].SetRange(30);
		m_AvgBytes[i].SetRange(30);

		m_dwSecPackets[i] = 0;
		m_dwSecBytes[i]   = 0;
	}

	m_QuerySendThrottle = 0;

	m_pComm->NodeUpdate(this);
}

CGnuNode::~CGnuNode()
{
	m_Status = SOCK_CLOSED;
	m_pComm->NodeUpdate(this);

	std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(m_NodeID);
	if(itNode != m_pComm->m_NodeIDMap.end())
		m_pComm->m_NodeIDMap.erase(itNode);
	
	std::map<uint32, CGnuNode*>::iterator itAddr = m_pComm->m_GnuNodeAddrMap.find( m_Address.Host.S_addr);
	if(itAddr != m_pComm->m_GnuNodeAddrMap.end())
		m_pComm->m_GnuNodeAddrMap.erase(itAddr);
	
	std::map<int, GUID>::iterator itProxy = m_pComm->m_PushProxyHosts.find( m_NodeID);
	if(itProxy != m_pComm->m_PushProxyHosts.end())
		m_pComm->m_PushProxyHosts.erase(itProxy);
	
	
	// Clear browse buffer
	for(int i = 0; i < m_BrowseHits.size(); i++)
		delete [] m_BrowseHits[i];
	
	m_BrowseHits.clear();

	if(m_BrowseBuffer)
	{
		delete [] m_BrowseBuffer;
		m_BrowseBuffer = NULL;
	}


	// Close socket
	//CloseWithReason("Deconstructor");

	// Clean packet stuff
	for(i = 0; i < MAX_TTL; i++)
		m_PacketListLength[i] = 0;

	m_BackBuffLength = 0;

	for(i = 0; i < MAX_TTL; i++)
		while(!m_PacketList[i].empty())
		{
			delete m_PacketList[i].back();
			m_PacketList[i].pop_back();
		}

	m_TransferPacketAccess.Lock();
		
		while( m_TransferPackets.size())
		{
			delete m_TransferPackets.back();
			m_TransferPackets.pop_back();
		}

	m_TransferPacketAccess.Unlock();

	// Delete patch table
	if(m_PatchBuffer)		
	{
		delete [] m_PatchBuffer;
		m_PatchBuffer = NULL;
	}

	if(m_LocalHitTable)
	{
		delete [] m_LocalHitTable;
		m_LocalHitTable = NULL;
	}

	// Clean up compression
	inflateEnd(&InflateStream);
	deflateEnd(&DeflateStream);
}


// Do not edit the following lines, which are needed by ClassWizard.
#if 0
BEGIN_MESSAGE_MAP(CGnuNode, CAsyncSocket)
	//{{AFX_MSG_MAP(CGnuNode)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
#endif	// 0


void CGnuNode::OnConnect(int nErrorCode) 
{
	if(nErrorCode)
	{
		m_pCore->LogError("GnuNode OnConnect Error " + NumtoStr(nErrorCode));
		return;
	}
	
	CString Handshake;


	// Get Remote host
	CString HostIP;
	GetPeerName(HostIP, m_Address.Port);
	m_Address.Host = StrtoIP(HostIP);


	// Connected node requested we connect back to test their firewall
	if(m_ConnectBack)
	{
		Send("\n\n", 2);
		CloseWithReason("TCP Connect Back");
		m_WholeHandshake += "\r\n\r\n";
		return;
	}


	// If node created to browse remote host
	if(m_BrowseID)
	{
		Handshake = "GET / HTTP/1.1\r\n";
		Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";
		Handshake += "Accept: text/html, application/x-gnutella-packets\r\n";
		Handshake += "Accept-Encoding: deflate\r\n";
		Handshake += "Connection: close\r\n";
		Handshake += "Host:" + IPv4toStr(m_Address) + "\r\n";

		// Authentication
		if(m_pCore->m_dnaCore->m_dnaEvents)
			m_pCore->m_dnaCore->m_dnaEvents->NetworkAuthenticate(m_NodeID);
		
		if( !m_Challenge.IsEmpty() && !m_ChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Challenge: " + m_Challenge + "\r\n";

		Handshake += "\r\n";
	}

	// Else if we're making a normal gnutella 0.6 connection
	else
	{
		Handshake =  m_pComm->m_NetworkName + " CONNECT/0.6\r\n";
		
		// Listen-IP header
		Handshake += "Listen-IP: " + IPtoStr(m_pNet->m_CurrentIP) + ":" + NumtoStr(m_pNet->m_CurrentPort) + "\r\n";

		// Remote-IP header
		Handshake += "Remote-IP: " + IPtoStr(m_Address.Host) + "\r\n";

		Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";

		// LAN Header
		if(m_pPrefs->m_LanMode)
			Handshake += "LAN: " + m_pPrefs->m_LanName + "\r\n";
	

		// Ultrapeer Header
		if(m_pComm->m_GnuClientMode == GNU_LEAF)
			Handshake += "X-Ultrapeer: False\r\n";

		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
			Handshake += "X-Ultrapeer: True\r\n";

		// X-Degree
		Handshake += "X-Degree: " + NumtoStr(m_pPrefs->m_MaxConnects) + "\r\n";
		
		// Query Routing Header
		Handshake += "X-Query-Routing: 0.1\r\n";
		
		// X-Ultrapeer-Query-Routing
		Handshake += "X-Ultrapeer-Query-Routing: 0.1\r\n";
		
		// X-Max-TTL
		Handshake += "X-Max-TTL: " + NumtoStr(MAX_TTL) + "\r\n";

		// X-Dynamic-Querying
		Handshake += "X-Dynamic-Querying: 0.1\r\n";

		// Vendor-Message
		Handshake += "Vendor-Message: 0.1\r\n";

		// GGEP
		Handshake += "GGEP: 0.5\r\n";

		// Accept-Encoding Header
		if(m_dnapressionOn)
			Handshake += "Accept-Encoding: deflate\r\n";

		// Bye Header
		Handshake += "Bye-Packet: 0.1\r\n";

		// Authentication
		if(m_pCore->m_dnaCore->m_dnaEvents)
			m_pCore->m_dnaCore->m_dnaEvents->NetworkAuthenticate(m_NodeID);
		
		if( !m_Challenge.IsEmpty() && !m_ChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Challenge: " + m_Challenge + "\r\n";

		Handshake += "\r\n";
	}

	Send(Handshake, Handshake.GetLength());


	// Add to log
	Handshake.Replace("\n\n", "\r\n\r\n");
	m_WholeHandshake += Handshake;

	CAsyncSocket::OnConnect(nErrorCode);
}

void CGnuNode::OnReceive(int nErrorCode) 
{
	if(nErrorCode)
	{
		CloseWithReason("GnuNode OnReceive Error " + NumtoStr(nErrorCode));
		return;
	}

	// Incoming bandwidth throttling
	// if average bw in over 30 secs is greater than 1 kb/s, limit traffic to 1 kb/s
	if( m_dwSecBytes[0] > 1024 && m_AvgBytes[0].GetAverage() > 1024)
		return;


	int BuffLength = 0;
	
	
	if(m_Status != SOCK_CONNECTED || !m_InflateRecv)
		BuffLength = Receive(&m_pBuff[m_ExtraLength], PACKET_BUFF - m_ExtraLength);
	else	
		BuffLength = Receive(InflateBuff, ZSTREAM_BUFF);
	

	// Handle Errors
	if(BuffLength <= 0)
	{
		if(BuffLength == SOCKET_ERROR)
		{
			int lastError = GetLastError();
			if(lastError != WSAEWOULDBLOCK)
			{				
				CloseWithReason("Receive Error " + NumtoStr(lastError));
				return;
			}
		}

		return;
	}


	// Bandwidth stats
	m_dwSecBytes[0] += BuffLength;


	// Connected to node, sending and receiving packets
	if(m_Status == SOCK_CONNECTED)
	{
		if(m_BrowseID)
			m_BrowseRecvBytes += BuffLength;


		FinishReceive(BuffLength);
	}

	// Still in handshake mode
	else
	{
		CString MoreData((char*) m_pBuff, BuffLength);

		m_InitData += MoreData;

		// Gnutella 0.6 Handshake
		if( m_InitData.Find("\r\n\r\n") != -1)
		{
			if(m_BrowseID)
				ParseBrowseHandshakeResponse(m_InitData, m_pBuff, BuffLength);

			else if(m_Inbound)
				ParseIncomingHandshake06(m_InitData, m_pBuff, BuffLength);

			else
				ParseOutboundHandshake06(m_InitData, m_pBuff, BuffLength);
		}
		

		if(m_InitData.GetLength() > 4096)
		{
			m_WholeHandshake += m_InitData;
			CloseWithReason("Handshake Overflow");
		}		
	}


	CAsyncSocket::OnReceive(nErrorCode);
}


/////////////////////////////////////////////////////////////////////////////
// New connections

void CGnuNode::ParseIncomingHandshake06(CString Data, byte* Stream, int StreamLength)
{
	m_Handshake = Data.Mid(0, Data.Find("\r\n\r\n") + 4);
	m_WholeHandshake += m_Handshake;

	m_lowHandshake = m_Handshake;
	m_lowHandshake.MakeLower();


	// Make sure agent valid before adding hosts from it
	if( m_RemoteAgent.IsEmpty() )
		m_RemoteAgent = FindHeader("User-Agent");
		
	if( !ValidAgent(m_RemoteAgent) || !FindHeader("OPnext-uid").IsEmpty() ) // dont connect to openext client
	{
		CloseWithReason("Client Not Valid");
		return;
	}

	// Parse X-Try header
	CString TryHeader = FindHeader("X-Try");
	if(!TryHeader.IsEmpty())
		ParseTryHeader( TryHeader );

	// Parse X-Try-Ultrapeers header
	CString UltraTryHeader = FindHeader("X-Try-Ultrapeers");
	if(!UltraTryHeader.IsEmpty())
		ParseTryHeader( UltraTryHeader );

	// Parse X-Try-Hubs header
	CString HubsToTry = FindHeader("X-Try-Hubs");
	if( !HubsToTry.IsEmpty() )
		ParseHubsHeader( HubsToTry );


	// Connect string, GNUTELLA CONNECT/0.6\r\n
	if(m_Handshake.Find(m_NetworkName + " CONNECT/") != -1)
	{

		// Parse LAN header
		if(m_pPrefs->m_LanMode)
			if(FindHeader("LAN") != m_pPrefs->m_LanName)
			{
				CloseWithReason("LAN Name Mismatch");
				return;
			}
		
		// Parse X-Query-Routing
		bool QueryRouting = false;
		CString RoutingHeader = FindHeader("X-Query-Routing");
		if(!RoutingHeader.IsEmpty() && RoutingHeader == "0.1")
			QueryRouting = true;

		// Parse X-Ultrapeer-Query-Routing header
		RoutingHeader = FindHeader("X-Ultrapeer-Query-Routing");
		if(!RoutingHeader.IsEmpty() && RoutingHeader == "0.1")
			m_SupportInterQRP = true;

		// Parse X-Max-TTL header
		CString MaxTTL = FindHeader("X-Max-TTL");
		if( !MaxTTL.IsEmpty() )
			m_RemoteMaxTTL = atoi(MaxTTL);

		// Parse X-Dynamic-Querying header
		if( FindHeader("X-Dynamic-Querying") == "0.1")
			m_SupportsDynQuerying = true;

		// Parse Vendor-Message header
		if( FindHeader("Vendor-Message") == "0.1")
			m_SupportsVendorMsg = true;
		
		// Parse Accept-Encoding
		CString EncodingHeader = FindHeader("Accept-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_DeflateSend = true;


		// Parse Authentication Challenge
		CString ChallengeHeader = FindHeader("X-Auth-Challenge");
		if( !ChallengeHeader.IsEmpty() )
		{
			m_RemoteChallenge = ChallengeHeader;
			if(m_pCore->m_dnaCore->m_dnaEvents)
				m_pCore->m_dnaCore->m_dnaEvents->NetworkChallenge(m_NodeID, m_RemoteChallenge);
		}
	

		// Authenticate connection
		if(m_pCore->m_dnaCore->m_dnaEvents)
			m_pCore->m_dnaCore->m_dnaEvents->NetworkAuthenticate(m_NodeID);


		//Parse Ultrapeer header
		CString UltraHeader = FindHeader("X-Ultrapeer");
		if(!UltraHeader.IsEmpty())
		{
			UltraHeader.MakeLower();

			// Connecting client an ultrapeer
			if(UltraHeader == "true")
				m_GnuNodeMode = GNU_ULTRAPEER;
			else
				m_GnuNodeMode = GNU_LEAF;
		}


		if(m_lowHandshake.Find("bearshare 2.") != -1)
			m_GnuNodeMode = GNU_ULTRAPEER;

		if(!QueryRouting)
		{
			Send_ConnectError("503 No QRP");
			return;
		}

		// If in Ultrapeer Mode
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			// Ultrapeer Connecting
			if(m_GnuNodeMode == GNU_ULTRAPEER)
			{
				if(m_pPrefs->m_MaxConnects != -1 && m_pComm->CountUltraConnects() >= m_pPrefs->m_MaxConnects && !LetConnect())
				{
					Send_ConnectError("503 Connects Maxed");
					return;
				}	

				Send_ConnectOK(false);
				return;
			}

			// Connecting Leaf
			if(m_GnuNodeMode == GNU_LEAF)
			{
				

				if(m_pComm->CountLeafConnects() > m_pPrefs->m_MaxLeaves && !LetConnect())
				{
					Send_ConnectError("503 Leaf Capacity Maxed");
					return;
				}

				// Compressing for each leaf takes way too much memory
				//m_DeflateSend = false;

				Send_ConnectOK(false);
				return;
			}
		}

		// If in Leaf Mode
		else if(m_pComm->m_GnuClientMode == GNU_LEAF)
		{
			// Ultrapeer connecting
			if(m_GnuNodeMode == GNU_ULTRAPEER)
			{
				if(m_pComm->CountUltraConnects() >= m_pPrefs->m_LeafModeConnects && !LetConnect())
				{
					Send_ConnectError("503 Ultrapeer Connects Maxed");
					return;
				}

				Send_ConnectOK(false);
				return;
			}

			// Leaf connecting
			if(m_GnuNodeMode == GNU_LEAF)
			{
				Send_ConnectError("503 In Leaf Mode");
				return;
			}
		}

		// Not ultra or leaf, not supported
		else
		{
			Send_ConnectError("503 Not Supported");
			return;
		}

	}


	// Ok string, GNUTELLA/0.6 200 OK\r\n
	else if(m_Handshake.Find(" 200 OK\r\n") != -1)
	{

		// Parse Content-Encoding Response
		CString EncodingHeader = FindHeader("Content-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_InflateRecv = true;
		

		// Parse Authentication Response
		if( !m_Challenge.IsEmpty() )
			if(m_ChallengeAnswer.Compare( FindHeader("X-Auth-Response") ) != 0 )
			{
				CloseWithReason("Authentication Failed");
				return;
			}


		// Supernode to Supernode connect finishing
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER && m_GnuNodeMode == GNU_ULTRAPEER)
		{
			//Parse Ultrapeer header
			CString UltraHeader = FindHeader("X-Ultrapeer");
			if(!UltraHeader.IsEmpty())
				UltraHeader.MakeLower();
		}


		SetConnected();


		// Stream begins
		for(int i = 0; i < StreamLength - 4; i++)
			if(strncmp((char*) &Stream[i], "\r\n\r\n", 4) == 0)
			{
				int BuffLength = StreamLength - (i + 4);

				if(m_InflateRecv)
					memcpy(InflateBuff, &Stream[i + 4], BuffLength);
				else	
					memmove(m_pBuff, &Stream[i + 4], BuffLength);

				FinishReceive(BuffLength);

				break;
			}
	}

	// Error string
	else
	{
		CString StatusLine = m_Handshake.Left( m_Handshake.Find("\r\n") );
		StatusLine.Replace( m_NetworkName + "/0.6 ", "");

		
		CloseWithReason(StatusLine, true);
		return;
	}
}

void CGnuNode::ParseOutboundHandshake06(CString Data, byte* Stream, int StreamLength)
{
	m_Handshake = Data.Mid(0, Data.Find("\r\n\r\n") + 4);
	m_WholeHandshake += m_Handshake;

	m_lowHandshake = m_Handshake;
	m_lowHandshake.MakeLower();


	// Make sure agent valid before adding hosts from it
	if( m_RemoteAgent.IsEmpty() )
		m_RemoteAgent = FindHeader("User-Agent");
		
	if( !ValidAgent(m_RemoteAgent) || !FindHeader("OPnext-uid").IsEmpty() ) // dont connect to openext client)
	{
		CloseWithReason("Client Not Valid");
		return;
	}

	// Parse X-Try header
	CString TryHeader = FindHeader("X-Try");
	if(!TryHeader.IsEmpty())
		ParseTryHeader( TryHeader );

	// Parse X-Try-Ultrapeers header
	CString UltraTryHeader = FindHeader("X-Try-Ultrapeers");
	if(!UltraTryHeader.IsEmpty())
		ParseTryHeader( UltraTryHeader );

	// Parse X-Try-Hubs header
	CString HubsToTry = FindHeader("X-Try-Hubs");
	if( !HubsToTry.IsEmpty() )
		ParseHubsHeader( HubsToTry );


	// Ok string, GNUTELLA/0.6 200 OK\r\n
	if(m_Handshake.Find(" 200 OK\r\n") != -1)
	{
		// Parse Remote-IP header
		CString RemoteIP = FindHeader("Remote-IP");
		if(!RemoteIP.IsEmpty())
			m_pNet->m_CurrentIP = StrtoIP(RemoteIP);
	

		// Parse LAN header
		if(m_pPrefs->m_LanMode)
			if(FindHeader("LAN") != m_pPrefs->m_LanName)
			{
				CloseWithReason("LAN Name Mismatch");
				return;
			}

		// Parse X-Query-Routing header
		bool QueryRouting = false;
		CString RoutingHeader = FindHeader("X-Query-Routing");
		if(!RoutingHeader.IsEmpty() && RoutingHeader == "0.1")
			QueryRouting = true;

		// Parse X-Ultrapeer-Query-Routing header
		RoutingHeader = FindHeader("X-Ultrapeer-Query-Routing");
		if(!RoutingHeader.IsEmpty() && RoutingHeader == "0.1")
			m_SupportInterQRP = true;

		// Parse X-Max-TTL header
		CString MaxTTL = FindHeader("X-Max-TTL");
		if( !MaxTTL.IsEmpty() )
			m_RemoteMaxTTL = atoi(MaxTTL);

		// Parse Vendor-Message header
		if( FindHeader("Vendor-Message") == "0.1")
			m_SupportsVendorMsg = true;

		// Parse X-Dynamic-Querying header
		if( FindHeader("X-Dynamic-Querying") == "0.1")
			m_SupportsDynQuerying = true;

		// Parse Accept-Encoding
		CString EncodingHeader = FindHeader("Accept-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_DeflateSend = true;

		// Parse Content-Encoding
		EncodingHeader = FindHeader("Content-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_InflateRecv = true;
		
		// Parse Accept header
		CString AcceptHeader = FindHeader("Accept");
		if(!AcceptHeader.IsEmpty())
			if( AcceptHeader.Find("application/x-gnutella2") != -1 )
				m_pNet->m_pCache->AddKnown( Node(IPtoStr(m_Address.Host), m_Address.Port, NETWORK_G2) );


		// Parse Authentication Response
		if( !m_Challenge.IsEmpty() )
			if(m_ChallengeAnswer.Compare( FindHeader("X-Auth-Response") ) != 0 )
			{
				CloseWithReason("Authentication Failed");
				return;
			}

		// Parse Authentication Challenge
		CString ChallengeHeader = FindHeader("X-Auth-Challenge");
		if( !ChallengeHeader.IsEmpty() )
		{
			m_RemoteChallenge = ChallengeHeader;
			if(m_pCore->m_dnaCore->m_dnaEvents)
				m_pCore->m_dnaCore->m_dnaEvents->NetworkChallenge(m_NodeID, m_RemoteChallenge);
		}


		// Parse Ultrapeer header
		CString UltraHeader = FindHeader("X-Ultrapeer");		
		if(!UltraHeader.IsEmpty())
		{
			UltraHeader.MakeLower();

			if(UltraHeader == "true")
				m_GnuNodeMode = GNU_ULTRAPEER;
			else
				m_GnuNodeMode = GNU_LEAF;
		}


		if(m_lowHandshake.Find("bearshare 2.") != -1)
			m_GnuNodeMode = GNU_ULTRAPEER;


		// In Ultrapeer Mode
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			// Connecting Ultrapeer
			if(m_GnuNodeMode == GNU_ULTRAPEER)
			{		
				Send_ConnectOK(true);
				SetConnected();
				return;
			}

			// Connecting Leaf
			if(m_GnuNodeMode == GNU_LEAF)
			{
				if( !QueryRouting)
				{
					Send_ConnectError("503 QRP Unsupported");
					return;
				}

				// Compressing for each leaf takes way too much memory
				//m_DeflateSend = false;

				Send_ConnectOK(true);
				SetConnected();
				return;
			}
		}

		// In Leaf Mode
		else if(m_pComm->m_GnuClientMode == GNU_LEAF)
		{
			// Ultrapeer connecting
			if( m_GnuNodeMode == GNU_ULTRAPEER)
			{
				if(!QueryRouting)
				{
					CloseWithReason("QRP Unsupported");
					return;
				}


				CString NeededHeader = FindHeader("X-Ultrapeer-Needed");
				NeededHeader.MakeLower();


				// This SuperNode wants more leaves
				if(NeededHeader == "true" && m_pComm->UltrapeerAble())
				{
					// Become a supernode (only upgrade if dna)
					if(m_RemoteAgent.Find("GnucDNA") > 0)
					{ 
						for(int i = 0; i < m_pComm->m_NodeList.size(); i++)
						{
							CGnuNode *p = m_pComm->m_NodeList[i];

							if(p != this && p->m_Status != SOCK_CLOSED)
								p->CloseWithReason("Node Upgrading");
						}

						m_pComm->m_GnuClientMode = GNU_ULTRAPEER;
					}
				}

				Send_ConnectOK(true);
				SetConnected();
				return;
			}

			// Leaf Connecting
			if( m_GnuNodeMode == GNU_LEAF)
			{
				Send_ConnectError("503 In Leaf Mode");
				return;
			}
		}

		// Not ultra or leaf, not supported
		else
		{
			Send_ConnectError("503 Not Supported");
			return;
		}
	
	}


	// Connect failed, 200 OK not received
	else
	{ 	
		CString StatusLine = m_Handshake.Left( m_Handshake.Find("\r\n") );
		StatusLine.Replace( m_NetworkName + "/0.6 ", "");

		
		CloseWithReason(StatusLine, true);
		return;
	}
}

void CGnuNode::ParseTryHeader(CString TryHeader)
{
	// This host responds with more hosts, put on Perm list
	if( !m_Inbound)
		m_pCache->AddWorking( Node( IPtoStr(m_Address.Host), m_Address.Port, NETWORK_GNUTELLA, CTime::GetCurrentTime() ) );


	TryHeader += ",";
	TryHeader.Replace(",,", ",");
	TryHeader.Remove(' ');
	
	int tryFront = 0, 
		tryMid = TryHeader.Find(":"), 
		tryBack = TryHeader.Find(",");

	int Added = 0;
	
	while(tryBack != -1 && tryMid != -1 && Added < 5)
	{
		Node tryNode;
		tryNode.Host = TryHeader.Mid(tryFront, tryMid - tryFront);
		tryNode.Port = atoi( TryHeader.Mid(tryMid + 1, tryBack - tryMid + 1));

		m_pCache->AddKnown( tryNode);
		Added++;


		tryFront  = tryBack + 1; 
		tryMid    = TryHeader.Find(":", tryFront);
		tryBack   = TryHeader.Find(",", tryFront);
	}
}

void CGnuNode::ParseHubsHeader(CString HubsHeader)
{
	if( m_pNet->m_pG2 == NULL )
		return;

	int Added = 0;

	// 1.2.3.4:6346 2003-03-25T23:59Z,
	CString Address = ParseString(HubsHeader, ',');

	while( !Address.IsEmpty() && Added < 5)
	{
		Node tryNode;
		tryNode.Network = NETWORK_G2;
		tryNode.Host = ParseString(Address, ':');
		tryNode.Port = atoi(ParseString(Address, ' '));

		if(Address.IsEmpty()) // Is not sending timestamp, probably G1 node in G2 cache
			return;

		tryNode.LastSeen = StrToCTime(Address);

		m_pCache->AddKnown( tryNode);
		Added++;

		Address = ParseString(HubsHeader, ',');
	}
}

CString CGnuNode::FindHeader(CString Name)
{	
	CString Data;
	
	Name += ":";
	Name.MakeLower();
	
	int keyPos = m_lowHandshake.Find(Name);

	if (keyPos != -1)
	{
		keyPos += Name.GetLength();

		Data = m_Handshake.Mid(keyPos, m_Handshake.Find("\r\n", keyPos) - keyPos);
		Data.TrimLeft();
	}
	
	return Data;
}

void CGnuNode::Send_ConnectOK(bool Reply)
{
	CString Handshake;

	// Reply to CONNECT OK
	if(Reply)
	{
		Handshake = m_NetworkName + "/0.6 200 OK\r\n";

		// If remote host accepts deflate encoding
		if(m_DeflateSend)
		{
			Handshake += "Accept-Encoding: deflate\r\n";
			Handshake += "Content-Encoding: deflate\r\n";
		}

		// Send Answer to Challenge
		if( !m_RemoteChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Response: " + m_RemoteChallengeAnswer + "\r\n";


		Handshake += "\r\n";
	}

	// Sending initial CONNECT OK
	else
	{
		Handshake =  m_NetworkName + "/0.6 200 OK\r\n";

		// Listen-IP header
		Handshake += "Listen-IP: " + IPtoStr(m_pNet->m_CurrentIP) + ":" + NumtoStr(m_pNet->m_CurrentPort) + "\r\n";

		// Remote IP header
		CString HostIP;
		UINT nTrash;
		GetPeerName(HostIP, nTrash);
		m_Address.Host = StrtoIP(HostIP);
		Handshake += "Remote-IP: " + HostIP + "\r\n";
		
		Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";


		// LAN header
		if(m_pPrefs->m_LanMode)
			Handshake += "LAN: " + m_pPrefs->m_LanName + "\r\n";

		// Ultrapeer header
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
			Handshake += "X-Ultrapeer: True\r\n";
		else
			Handshake += "X-Ultrapeer: False\r\n";
		
		// X-Degree
		Handshake += "X-Degree: " + NumtoStr(m_pPrefs->m_MaxConnects) + "\r\n";

		// Query Routing Header
		Handshake += "X-Query-Routing: 0.1\r\n";

		// X-Ultrapeer-Query-Routing
		Handshake += "X-Ultrapeer-Query-Routing: 0.1\r\n";

		// X-Max-TTL
		Handshake += "X-Max-TTL: " + NumtoStr(MAX_TTL) + "\r\n";

		// X-Dynamic-Querying
		Handshake += "X-Dynamic-Querying: 0.1\r\n";

		// Vendor-Message
		Handshake += "Vendor-Message: 0.1\r\n";

		// GGEP
		Handshake += "GGEP: 0.5\r\n";

		// Compression headers
		Handshake += "Accept-Encoding: deflate\r\n";

		if(m_DeflateSend)
			Handshake += "Content-Encoding: deflate\r\n";

		// Bye Header
		Handshake += "Bye-Packet: 0.1\r\n";

		// Send authentication response
		if( !m_RemoteChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Response: " + m_RemoteChallengeAnswer + "\r\n";


		// Send authentication challenge
		if( !m_Challenge.IsEmpty() && !m_ChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Challenge: " + m_Challenge + "\r\n";



		// X-Try-Ultrapeers header
		CString SuperHostsToTry;
		if(GetAlternateSuperList(SuperHostsToTry))
			Handshake += "X-Try-Ultrapeers: " + SuperHostsToTry + "\r\n";	

		// X-Try-Hubs header
		if( m_pNet->m_pG2 )
		{
			CString HubsToTry;
			if( m_pNet->m_pG2->GetAltHubs(HubsToTry) )
				Handshake += "X-Try-Hubs: " + HubsToTry + "\r\n";	
		}

		Handshake += "\r\n";
	}


	Send(Handshake, Handshake.GetLength());
	
	Handshake.Replace("\n\n", "\r\n");
	m_WholeHandshake += Handshake;
}

void CGnuNode::Send_ConnectError(CString Reason)
{
	CString Handshake;

	Handshake =  m_NetworkName + "/0.6 " + Reason + "\r\n";
	Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";


	// LAN header
	if(m_pPrefs->m_LanMode)
		Handshake += "LAN: " + m_pPrefs->m_LanName + "\r\n";


	// Remote-IP header
	CString HostIP;
	UINT nTrash;
	GetPeerName(HostIP, nTrash);
	m_Address.Host = StrtoIP(HostIP);
	Handshake += "Remote-IP: " + HostIP + "\r\n";


	// X-Try-Ultrapeers header
	CString SuperHostsToTry;
	if(GetAlternateSuperList(SuperHostsToTry))
		Handshake += "X-Try-Ultrapeers: " + SuperHostsToTry + "\r\n";

	// X-Try-Hubs header
	if( m_pNet->m_pG2 )
	{
		CString HubsToTry;
		if( m_pNet->m_pG2->GetAltHubs(HubsToTry) )
			Handshake += "X-Try-Hubs: " + HubsToTry + "\r\n";	
	}

	Handshake += "\r\n";


	Send(Handshake, Handshake.GetLength());
	
	Handshake.Replace("\n\n", "\r\n");
	m_WholeHandshake += Handshake;

	CloseWithReason(Reason);
}

void CGnuNode::ParseBrowseHandshakeRequest(CString Data)
{
	m_Handshake = Data.Mid(0, Data.Find("\r\n\r\n") + 4);
	m_WholeHandshake += m_Handshake;

	m_lowHandshake = m_Handshake;
	m_lowHandshake.MakeLower();

	m_BrowseID = 1;

	//GET / HTTP/1.1
	//Accept: text/html, application/x-gnutella-packets
	//Accept-Encoding: deflate
	//Connection: close
	//Host: 127.0.0.1:17053


	// Parse Content-Encoding header
	CString Encoding = FindHeader("Accept-Encoding");
	if(Encoding == "deflate")
		m_BrowseCompressed = true;
	
	CString Response;
	Response += "HTTP/1.1 200 OK\r\n";

	Response += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";

	CString HostIP;
	UINT nTrash;
	GetPeerName(HostIP, nTrash);
	m_Address.Host = StrtoIP(HostIP);
	Response += "Remote-IP: " + HostIP + "\r\n";

	Response += "Accept-Ranges: bytes\r\n";
	Response += "Connection: Close\r\n";
	Response += "Content-Type: application/x-gnutella-packets\r\n";

	if(m_BrowseCompressed)
		Response += "Content-Encoding: deflate\r\n";
	

	// Create query for share files
	GnuQuery BrowseQuery;
	BrowseQuery.OriginID  = m_NodeID;
	GnuCreateGuid(&BrowseQuery.SearchGuid);

	std::list<UINT> FileIndexes;

	m_pShare->m_FilesAccess.Lock(); 

	int i = 0;
	std::vector<SharedFile>::iterator itFile;
	for(itFile = m_pShare->m_SharedFiles.begin(); itFile != m_pShare->m_SharedFiles.end(); itFile++, i++)
		FileIndexes.push_back( i );
	
	m_pShare->m_FilesAccess.Unlock();

	byte ReplyBuffer[4096];
	m_pProtocol->Encode_QueryHit(BrowseQuery, FileIndexes, ReplyBuffer);


	// Get total size of what we're sending
	int PacketsSize = 0;

	for(i = 0; i < m_BrowseHitSizes.size(); i++)
		PacketsSize += m_BrowseHitSizes[i];

	if(PacketsSize)
	{
		// Copy all packets into one buffer, make room for compression
		int   PacketBuffSize = PacketsSize * 1.1;
		byte* m_TempBuffer  = new byte[PacketBuffSize];
		
		int   BufferPos = 0;

		for(i = 0; i < m_BrowseHits.size(); i++)
		{
			memcpy(m_TempBuffer + BufferPos, m_BrowseHits[i], m_BrowseHitSizes[i]);
			BufferPos += m_BrowseHitSizes[i];
		}


		m_BrowseBuffer = new byte[PacketBuffSize];
		
		// Compress buffer, Stream defalte into buffer
		if(m_BrowseCompressed)
		{
			DeflateStream.next_in  = m_TempBuffer;
			DeflateStream.avail_in = BufferPos;
			BufferPos = 0; // reset so if this fails we report zero bytes available

			DeflateStream.next_out  = m_BrowseBuffer;
			DeflateStream.avail_out = PacketBuffSize;

			int status = deflate(&DeflateStream, Z_FINISH);

			if(status == Z_STREAM_END)
				if (DeflateStream.avail_out < PacketBuffSize) 
					BufferPos =  PacketBuffSize - DeflateStream.avail_out;
		}
		else
			memcpy( m_BrowseBuffer, m_TempBuffer, BufferPos);

		delete [] m_TempBuffer;

		m_BrowseBuffSize = BufferPos;
	}


	// Send handshake
	Response += "Content-Length: " + NumtoStr(m_BrowseBuffSize) + "\r\n";
	Response += "\r\n";
	Send(Response, Response.GetLength());


	// Send buffer
	if(m_BrowseBuffSize)
		m_BrowseSentBytes = Send(m_BrowseBuffer, m_BrowseBuffSize);

	if(m_BrowseSentBytes == m_BrowseBuffSize)
	{
		CloseWithReason("Browse Completed");
		return;
	}
}

void CGnuNode::ParseBrowseHandshakeResponse(CString Data, byte* Stream, int StreamLength)
{
	m_Handshake = Data.Mid(0, Data.Find("\r\n\r\n") + 4);
	m_WholeHandshake += m_Handshake;

	m_lowHandshake = m_Handshake;
	m_lowHandshake.MakeLower();
	

	//HTTP/1.1 200 OK
	//Remote-IP: 24.147.54.36
	//Accept-Ranges: bytes
	//Connection: Close
	//Content-Type: application/x-gnutella-packets
	//Content-Encoding: deflate
	//Content-Length: 87378


	// Check HTTP header of OK
	if(m_Handshake.Find("200 OK\r\n") == -1)
	{
		CString StatusLine = m_Handshake.Left( m_Handshake.Find("\r\n") );
		StatusLine.Mid( StatusLine.Find(' ') );

		CloseWithReason(StatusLine);
		return;
	}

	// Parse Remote-IP header
	CString RemoteIP = FindHeader("Remote-IP");
	if(!RemoteIP.IsEmpty())
		m_pNet->m_CurrentIP = StrtoIP(RemoteIP);

	// Parse Content-Type header
	CString ContentType = FindHeader("Content-Type");
	if(ContentType.Find("x-gnutella-packets") == -1)
	{
		CloseWithReason("Browse in Wrong Format");
		return;
	}

	// Parse Content-Encoding header
	CString Encoding = FindHeader("Content-Encoding");
	if(Encoding == "deflate")
		m_InflateRecv = true;

	// Parse Content-Length header
	CString ContentLength = FindHeader("Content-Length");
	if(!ContentLength.IsEmpty())
		m_BrowseSize = atoi(ContentLength);
	
	// If host has no files make it seem like 100% received still
	if(m_BrowseSize == 0)
	{
		m_BrowseRecvBytes = 1;
		m_BrowseSize = 1;

		CloseWithReason("Browse Completed");
		return;
	}

	m_Status = SOCK_CONNECTED;
	
	// Send end bytes of handshake to buffer
	for(int i = 0; i < StreamLength - 4; i++)
		if(strncmp((char*) &Stream[i], "\r\n\r\n", 4) == 0)
		{
			int BuffLength = StreamLength - (i + 4);

			if(m_InflateRecv)
				memcpy(InflateBuff, &Stream[i + 4], BuffLength);
			else	
				memmove(m_pBuff, &Stream[i + 4], BuffLength);

			
			m_BrowseRecvBytes += BuffLength;
			FinishReceive(BuffLength);

			break;
		}

	m_pComm->NodeUpdate(this);
}

void CGnuNode::SetConnected()
{
	m_Status = SOCK_CONNECTED;
	m_StatusText = "Connected";
	m_pComm->NodeUpdate(this);

	
	// For testing protcol compatibility
	/*if( m_RemoteAgent.Find("1.1.0.7") == -1 )
		//if( m_RemoteAgent.Find("Lime") == -1 && m_RemoteAgent.Find("Bear") == -1 )
		{
			CloseWithReason("Not 1.1", false, false);
			return;
		}*/

	// Setup inflate if remote host supports it
	if(m_InflateRecv)
	{
		if(inflateInit(&InflateStream) != Z_OK)
		{
			CloseWithReason("InflateInit Failed");
			return;
		}
	
		InflateStream.avail_in  = 0;
		InflateStream.next_out  = m_pBuff;
		InflateStream.avail_out = PACKET_BUFF;
	}

	//Setup deflate if remote host supports it
	if(m_DeflateSend)
	{
		if(deflateInit(&DeflateStream, Z_DEFAULT_COMPRESSION) != Z_OK)
		{
			CloseWithReason("DeflateInit Failed");
			return;
		}
		
		DeflateStream.avail_in  = 0;
		DeflateStream.next_out  = m_BackBuff;
		DeflateStream.avail_out = m_BackBuffLength;
	}

	// Init Remote QRP Table
	memset( m_RemoteHitTable, 0xFF, GNU_TABLE_SIZE );

	// We are a leaf send index to supernode
	if( (m_pComm->m_GnuClientMode == GNU_LEAF && m_GnuNodeMode == GNU_ULTRAPEER) ||
		(m_GnuNodeMode == GNU_ULTRAPEER && m_SupportInterQRP))
	{
		m_pProtocol->Send_PatchReset(this); // Send reset

		m_SendDelayPatch = true; // Set to send patch
	}


	m_pProtocol->Send_Ping(this, 1);	


	if(m_SupportsVendorMsg)
	{
		// Put together vector of support message types
		std::vector<packet_VendIdent> SupportedMessages;
		SupportedMessages.push_back( packet_VendIdent("BEAR", 7, 1) );		// tcp connect back
		SupportedMessages.push_back( packet_VendIdent("GNUC", 7, 1) );		// udp connect back
		SupportedMessages.push_back( packet_VendIdent("BEAR", 11, 1) );		// leaf guidance
		//SupportedMessages.push_back( packet_VendIdent("BEAR", 12, 1) );	// leaf guidance
		SupportedMessages.push_back( packet_VendIdent("GNUC", 60, 1) );		// mode change
		SupportedMessages.push_back( packet_VendIdent("GNUC", 61, 1) );		// mode change
		SupportedMessages.push_back( packet_VendIdent("GTKG", 7, 2) );		// udp connect back
		//SupportedMessages.push_back( packet_VendIdent("LIME", 11, 2) );	// oob query
		//SupportedMessages.push_back( packet_VendIdent("LIME", 12, 1) );	// oob query
		SupportedMessages.push_back( packet_VendIdent("LIME", 21, 1) );	    // push proxy

		uint16 VectorSize = SupportedMessages.size();

		int   length  = 2 + VectorSize * 8;
		byte* payload = new byte[length];

		memcpy(payload, &VectorSize, 2);

		for(int i = 0; i < VectorSize; i++)
			memcpy(payload + 2 + (i * 8), &SupportedMessages[i], 8);


		packet_VendMsg SupportedMsg;
		GnuCreateGuid(&SupportedMsg.Header.Guid);
		SupportedMsg.Ident = packet_VendIdent("\0\0\0\0", 0, 0);
		m_pProtocol->Send_VendMsg( this, SupportedMsg, payload, length );

		delete [] payload;
	}
}

void CGnuNode::SendPacket(void* packet, int length, int type, int distance, bool thread)
{
	// Broadcast packets distance is hops, routed packets distance is ttl - 1

	if(length > (PACKET_BUFF/2) || length <= 0) // legacy clients 16kb max
	{
		//m_pCore->DebugLog("Gnutella", "Bad Send Packet Size " + NumtoStr(length));

		return;
	}

	// handled by inspect packet already, asserts on forwared queries to leaves
	// need to check cause packetlist array would go out of bounds
	if(distance >= MAX_TTL)
		distance = MAX_TTL - 1;

	if(distance < 0)
		distance = 0;

	ASSERT(packet);


	// Throttle outbound query rate at 1 KB/s
	if(type == PACKET_QUERY)
	{
		m_QuerySendThrottle += length;
		if( m_QuerySendThrottle > 1024 )
			return;
	}
	
	// Build a priority packet
	PriorityPacket* OutPacket = new PriorityPacket((byte*) packet, length, type, distance);	
	m_PacketListLength[distance] += OutPacket->m_Length;
	
	if( thread )
	{
		m_TransferPacketAccess.Lock();
			m_TransferPackets.push_back( OutPacket );
		m_TransferPacketAccess.Unlock();
	}
	else
		SendPacket( OutPacket );

}

void CGnuNode::SendPacket(PriorityPacket* OutPacket)
{
	int distance = OutPacket->m_Hops;

	// Remove old packets if back list too large (does not apply to local packets)
	while(m_PacketList[distance].size() > 100)
	{
		PriorityPacket* lastPacket = m_PacketList[distance].back();

		m_PacketListLength[distance] -= lastPacket->m_Length;
		delete lastPacket;
		
		m_PacketList[distance].pop_back();
	}

	// Insert packet into list from least to greatest hops value
	std::list<PriorityPacket*>::iterator itPacket;
	for(itPacket = m_PacketList[distance].begin(); itPacket != m_PacketList[distance].end(); itPacket++) 
		if( OutPacket->m_Type <= (*itPacket)->m_Type )
		{
			m_PacketList[distance].insert(itPacket, OutPacket);
			break;
		}

	if(itPacket == m_PacketList[distance].end())
		m_PacketList[distance].push_front(OutPacket);
	
	FlushSendBuffer();
}

void CGnuNode::FlushSendBuffer(bool FullFlush)
{
// DO NOT CALL CloseWithReason() from here, causes infinite loop

	if(m_Status != SOCK_CONNECTED)
		return;

	
	// If full flush, try to get out of internal gzip buffer anything left
	if(FullFlush && m_DeflateSend)
	{
		int BuffSize = PACKET_BUFF - m_BackBuffLength;

		DeflateStream.next_out  = m_BackBuff + m_BackBuffLength;
		DeflateStream.avail_out = BuffSize;

		int stat = deflate(&DeflateStream, Z_SYNC_FLUSH);
		if( stat < 0)
		{
			// No progress possible keep going
			if(stat != Z_BUF_ERROR)
				m_pCore->LogError("Deflate Error " + NumtoStr(stat));
		}
	

		if(DeflateStream.avail_out < BuffSize) 
			m_BackBuffLength += BuffSize - DeflateStream.avail_out;
	}

	ASSERT(m_BackBuffLength >= 0);

	// Send back buffer
	while(m_BackBuffLength > 0)
	{
		int BytesSent = Send(m_BackBuff, m_BackBuffLength);

		if(BytesSent < m_BackBuffLength)
		{
			if(BytesSent == SOCKET_ERROR)
			{
				int lastError = GetLastError();
				if(lastError != WSAEWOULDBLOCK)
					CloseWithReason("Send Buffer Error " + NumtoStr(lastError), true, false);
				
				return;
			}

			memmove(m_BackBuff, m_BackBuff + BytesSent, m_BackBuffLength - BytesSent);
			m_BackBuffLength -= BytesSent;

			return;
		}
		else
			m_BackBuffLength = 0;
	}


	// Each Packet type
	for(int i = 0; i < MAX_TTL; i++)
		// while list not empty
		while(!m_PacketList[i].empty())
		{
			PriorityPacket* FrontPacket = m_PacketList[i].front();

			if(m_pCore->m_dnaCore->m_dnaEvents)
				m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketOutgoing(NETWORK_GNUTELLA, true, m_Address.Host.S_addr, m_Address.Port, FrontPacket->m_Packet, FrontPacket->m_Length, false );

			// Compress Packet
			if( m_DeflateSend )
			{
				DeflateStream.next_in  = FrontPacket->m_Packet;
				DeflateStream.avail_in = FrontPacket->m_Length;

				DeflateStream.next_out  = m_BackBuff;
				DeflateStream.avail_out = PACKET_BUFF;
				
				int flushMode = Z_NO_FLUSH;
				m_DeflateStreamSize += FrontPacket->m_Length;
				
				if(m_DeflateStreamSize > 4096)
				{
					flushMode = Z_SYNC_FLUSH;
					m_DeflateStreamSize = 0;
				}

				while(DeflateStream.avail_in != 0) 
				{
					int stat = deflate(&DeflateStream, flushMode);
					if( stat < 0)
					{
						m_pCore->LogError("Deflate Error " + NumtoStr(stat));
						break;
					}
				}

				if(DeflateStream.avail_out < PACKET_BUFF) 
					m_BackBuffLength = PACKET_BUFF - DeflateStream.avail_out;
			}
			else
			{
				memcpy(m_BackBuff, FrontPacket->m_Packet, FrontPacket->m_Length);
				m_BackBuffLength = FrontPacket->m_Length;
			}


			// Send packet
			bool SendFinished = false;

			if(m_BackBuffLength)
			{
				int BytesSent = Send(m_BackBuff, m_BackBuffLength);
				
				// If send fails, copy to back buffer so it is the first to be sent
				if(BytesSent < m_BackBuffLength)
				{
					if(BytesSent == SOCKET_ERROR)
					{
						int lastError = GetLastError();
						if(lastError != WSAEWOULDBLOCK)
							CloseWithReason("Send Error " + NumtoStr(lastError), true, false);
					}
					else
					{
						m_dwSecPackets[2]++;
						m_dwSecBytes[2] += m_BackBuffLength - BytesSent;

						memmove(m_BackBuff, m_BackBuff + BytesSent, m_BackBuffLength - BytesSent);
						m_BackBuffLength -= BytesSent;
					}

					SendFinished = true;
				}
				else
					m_BackBuffLength = 0;
			}
			

			// Delete packet once sent
			m_PacketListLength[i] -= FrontPacket->m_Length;
			delete FrontPacket;
			FrontPacket = NULL;

			m_PacketList[i].pop_front();

			m_dwSecPackets[1]++;

			if(SendFinished)
				return;
		}	
}

int CGnuNode::Send(const void* lpBuf, int nBuffLen, int nFlags) 
{
	int BytesSent = 0;

	// Throttle leaf bandwidth
	BytesSent = CAsyncSocket::Send(lpBuf, nBuffLen, nFlags);
					
	if(BytesSent > 0)
		m_dwSecBytes[1] += BytesSent;

	return BytesSent;
}

void CGnuNode::OnSend(int nErrorCode) 
{
	FlushSendBuffer();


	// If we are sending custom browse buffer
	if(m_BrowseID && m_BrowseSentBytes < m_BrowseBuffSize)
		m_BrowseSentBytes += Send(m_BrowseBuffer + m_BrowseSentBytes, m_BrowseBuffSize - m_BrowseSentBytes);
	

	CAsyncSocket::OnSend(nErrorCode);
}

void CGnuNode::OnClose(int nErrorCode) 
{
	m_Status = SOCK_CLOSED;

	CString Reason = "Closed";
	if(nErrorCode)
		Reason += " #" + NumtoStr(nErrorCode);

	CloseWithReason(Reason, true);

	CAsyncSocket::OnClose(nErrorCode);
}

void CGnuNode::CloseWithReason(CString Reason, bool RemoteClosed, bool SendBye)
{
	if(m_Status == SOCK_CONNECTED && SendBye && !RemoteClosed)
	{
		m_pProtocol->Send_Bye(this, Reason);
		FlushSendBuffer(true);
	}

	if(	RemoteClosed )
		m_StatusText = "Gnu Remote: " + Reason;
	else
		m_StatusText = "Gnu Local: " + Reason;


	if( m_SecsAlive > 30 && m_GnuNodeMode == GNU_ULTRAPEER)
	{
		//if( m_RemoteAgent.Find("GnucDNA") != -1)
		//{
		//	CTimeSpan Uptime = CTime::GetCurrentTime() - m_ConnectTime;
		//	m_pCore->DebugLog( m_RemoteAgent + " " + IPv4toStr(m_Address) + ", Uptime " + Uptime.Format("%DD %HH %MM") + " Closed: " + Reason);
		//}
	}

	Close();
}

void CGnuNode::Close() 
{
	if(m_hSocket != INVALID_SOCKET)
	{
		// Clear receive buffer
		int BuffLength = 0;

		
		if(m_Status == SOCK_CONNECTED)
			do {
				if( !m_InflateRecv )
					BuffLength = Receive(&m_pBuff[m_ExtraLength], PACKET_BUFF - m_ExtraLength);
				else	
					BuffLength = Receive(InflateBuff, ZSTREAM_BUFF);

				if(BuffLength > 0)
					FinishReceive(BuffLength);

			} while(BuffLength > 0);


		// Close socket
		if(m_hSocket != INVALID_SOCKET)
		{
			AsyncSelect(0);
			ShutDown(2);
		}

		CAsyncSocket::Close();
	}

	m_Status = SOCK_CLOSED;
	m_pComm->NodeUpdate(this);
}

void CGnuNode::FinishReceive(int BuffLength)
{
	if(m_InflateRecv)
	{
		InflateStream.next_in  = InflateBuff;
		InflateStream.avail_in = BuffLength;

		while (InflateStream.avail_in != 0) 
		{
			BuffLength   = 0;
			int BuffAvail = PACKET_BUFF - m_ExtraLength;

			InflateStream.next_out  = &m_pBuff[m_ExtraLength];
			InflateStream.avail_out = BuffAvail;
			
			if( inflate(&InflateStream, Z_NO_FLUSH) < 0)
			{
				m_pCore->LogError("Inflate Error");
				return;
			}

			if(InflateStream.avail_out < BuffAvail) 
			{ 
				BuffLength = BuffAvail - InflateStream.avail_out;
				SplitBundle(m_pBuff, BuffLength);
			}
		}
	}
	else
		SplitBundle(m_pBuff, BuffLength);
}

void CGnuNode::SplitBundle(byte* bundle, DWORD length)
{
	int extra = m_ExtraLength;

	// First add previous buffer data
	length += m_ExtraLength;
	m_ExtraLength = 0;


	int  nextPos = 0;

	packet_Header* packet;

	enum status { status_DONE,       status_CONTINUE, 
				  status_BAD_PACKET, status_INCOMPLETE_PACKET };

	status theStatus = status_CONTINUE;

	do
	{
		if (nextPos + sizeof(packet_Header) > length)
			theStatus = status_INCOMPLETE_PACKET;
		else
		{
			packet = (packet_Header*) (bundle + nextPos);

			if(packet->Payload < PACKET_BUFF - 23)
			{
				if (nextPos + sizeof(packet_Header) + packet->Payload <= length)
				{
					m_dwSecPackets[0]++;

					Gnu_RecvdPacket Packet( m_Address, packet, 23 + packet->Payload, this);
					m_pProtocol->ReceivePacket( Packet );
				
					nextPos += 23 + packet->Payload;
					if (nextPos == length)
						theStatus = status_DONE;
				}
				else
					theStatus = status_INCOMPLETE_PACKET;
			}
			else
			{

				//////////////////////////////////////////////////////////
				// debug bad packets
				/*
				if( nextPos != 0 )
				{
					m_pCore->DebugLog("Gnutella", "Packet Size Greater than 16k: " + m_RemoteAgent);
					m_pCore->DebugLog("Gnutella", "Payload " + NumtoStr(packet->Payload) + ", NextPos " + NumtoStr(nextPos) + ", Length " + NumtoStr(length) + ", Extra " + NumtoStr(extra));

					// dump all packets
					nextPos   = 0;
					theStatus = status_CONTINUE;

					do
					{
						if (nextPos + sizeof(packet_Header) > length)
							theStatus = status_INCOMPLETE_PACKET;

						else
						{
							packet = (packet_Header*) (bundle + nextPos);

							if(packet->Payload < 16384)
							{
								if (nextPos + sizeof(packet_Header) + packet->Payload <= length)
								{
									m_pCore->DebugLog("Gnutella", HexDump((byte*) packet, 23 + packet->Payload));
													
									nextPos += 23 + packet->Payload;

									if (nextPos == length)
										theStatus = status_DONE;
								}
								else
									theStatus = status_INCOMPLETE_PACKET;
							}
							else
							{
								m_pCore->DebugLog("Gnutella", "Bad: " + HexDump((byte*) packet, length - nextPos));
								break;
							}			
						}

					} while(status_CONTINUE == theStatus);		
				}
				*/
				//////////////////////////////////////////////////////////

				CloseWithReason("Packet Size Greater than 32k");
				return;
			}
		}
	} while(status_CONTINUE == theStatus);


	// Take extra length and save it for next receive
	m_ExtraLength = length - nextPos;

	if (0 < m_ExtraLength && m_ExtraLength < PACKET_BUFF)
		memmove(m_pBuff, &m_pBuff[length - m_ExtraLength], m_ExtraLength);
	else if(m_ExtraLength != 0)
	{
		// client, connecttime, variables log, disconnect
		m_pCore->DebugLog("Gnu Network", "Extra Length Error - " + m_RemoteAgent + ", Uptime " + NumtoStr(time(NULL) - m_ConnectTime.GetTime()) + ", " + ", Length " + NumtoStr(length) + ", NextPos " + NumtoStr(nextPos) + ", ExtraLength " + NumtoStr(m_ExtraLength));
		
		CloseWithReason("Packet received too large"); // without this crashes in inflate
		//ASSERT(0); // Shouldnt happen

	}
}

void CGnuNode::ApplyPatchTable()
{
	byte* PatchTable = NULL;

	// Decompress table if needed
	if( m_PatchCompressed )
	{
		PatchTable = new byte[m_PatchSize];

		DWORD UncompressedSize = m_PatchSize;

		int zerror = uncompress( PatchTable, &UncompressedSize, m_PatchBuffer, m_PatchOffset);
		if( zerror != Z_OK )
		{
			CloseWithReason("Patch Table Decompress Error");
			delete [] PatchTable;
			return;
		}

		ASSERT( UncompressedSize == m_PatchSize );

		delete [] m_PatchBuffer;
		m_PatchBuffer = NULL;
	}
	else
	{
		PatchTable = m_PatchBuffer; // Make sure deleted in function!
		m_PatchBuffer = NULL; 
	}

	
	// Apply patch
	// Only can accurately convert smaller tables, a node's larger table is not kept around to patch against later

	int RemoteSize = m_PatchSize;
	RemoteSize /= m_PatchBits;

	double Factor = (double) GNU_TABLE_SIZE / (double) RemoteSize; // SMALLER means LARGER remote TABLE

	int remotePos = 0;

	int i = 0, j = 0;
	for(i = 0; i < m_PatchSize; i++)
	{
		if(m_PatchBits == 4)
		{
			

			// high bit
			remotePos = i * 2;
			SetPatchBit(remotePos, Factor, PatchTable[i] >> 4);
			
			// low bit
			remotePos++;
			SetPatchBit(remotePos, Factor, PatchTable[i] & 0xF);
		}
		else if(m_PatchBits == 8)
		{
			remotePos = i;
			SetPatchBit(remotePos, Factor, PatchTable[i]);
		}
	}

	delete [] PatchTable;
	PatchTable = NULL;

	// Patch table for node modified, if node a child update inter-hub QHT
	if( m_GnuNodeMode == GNU_LEAF )
		for( i = 0; i < m_pComm->m_NodeList.size(); i++)
			if(m_pComm->m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER && 
				m_pComm->m_NodeList[i]->m_SupportInterQRP &&
				m_pComm->m_NodeList[i]->m_Status == SOCK_CONNECTED)
				m_pComm->m_NodeList[i]->m_SendDelayPatch = true;
}

void CGnuNode::SetPatchBit(int &remotePos, double &Factor, byte value)
{
	int localPos  = 0;
	int lByte = 0, lBit = 0;

	for(double Next = 0; Next < Factor; Next++)
	{
		localPos = remotePos * Factor + Next;

		lByte = ( localPos >> 3 ); 
		lBit  = ( localPos & 7 ); 

		// Switch byte
		if(value > 7)  // turn on (byte negetive)
			m_RemoteHitTable[lByte] &= ~(1 << lBit);
		
		else if(value > 0) // turn off (byte positive)
			m_RemoteHitTable[lByte] |= 1 << lBit;

		// zero no change
	}
}

bool CGnuNode::GetAlternateSuperList(CString &HostList)
{
	int Hosts = 0;

	bool PrefDna = false;
	if(m_RemoteAgent.Find("GnucDNA") != -1 && m_GnuNodeMode == GNU_LEAF)
		PrefDna = true;

	for(int i = 0; i < m_pComm->m_NodeList.size() && Hosts < 10; i++)
		if(m_pComm->m_NodeList[i] != this && m_pComm->m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER)
		{
			if(PrefDna && m_pComm->m_NodeList[i]->m_RemoteAgent.Find("GnucDNA") == -1)
				continue;

			HostList += IPv4toStr(m_pComm->m_NodeList[i]->m_Address) + ",";	
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


/////////////////////////////////////////////////////////////////////////////
// Misc functions

void CGnuNode::Timer()
{
	NodeManagement();
	
	CompressionStats();

	
	// Statistics
	// When i = 0 its receive stats
    //      i = 1 its send stats
	//		i = 2 its dropped stats
	for(int i = 0; i < 3; i++)
	{
		m_AvgPackets[i].Update( m_dwSecPackets[i] );
		m_AvgBytes[i].Update( m_dwSecBytes[i] );

		m_dwSecPackets[i] = 0;
		m_dwSecBytes[i]   = 0;	
	}
	
	m_QuerySendThrottle = 0;

	// Efficiency calculation
	UINT dPart  = m_StatPings[1] + m_StatPongs[1] + m_StatQueries[1] + m_StatQueryHits[1] + m_StatPushes[1]; 
	UINT dWhole = m_StatPings[0] + m_StatPongs[0] + m_StatQueries[0] + m_StatQueryHits[0] + m_StatPushes[0];
	
	if(dWhole)
		m_Efficeincy = dPart * 100 / dWhole;
	else
		m_Efficeincy = 0;

	
}

void CGnuNode::NodeManagement()
{
	if(SOCK_CONNECTING == m_Status)
	{
		m_SecsTrying++;
		
		if(m_SecsTrying > CONNECT_TIMEOUT)
		{
			CloseWithReason("Timed Out");
			return;
		}
	}

	else if(SOCK_CONNECTED == m_Status)
	{
		// QRP - Recv
		if(m_PatchTimeout > 0)
			m_PatchTimeout--;

		if(m_PatchReady)
			if(m_PatchTimeout == 0 || m_GnuNodeMode == GNU_ULTRAPEER) // Hub always able to update, prevent child from taking hub cpu
			{
				ApplyPatchTable();

				m_PatchReady   = false;
				m_PatchTimeout = 60;
				m_CurrentSeq  = 1;
			}

		// QRP - Send
		if( m_PatchWait > 0)
			m_PatchWait--;

		if(m_SendDelayPatch && m_PatchWait == 0)
		{
			if( (m_pComm->m_GnuClientMode == GNU_LEAF && m_GnuNodeMode == GNU_ULTRAPEER) ||
				(m_GnuNodeMode == GNU_ULTRAPEER && m_SupportInterQRP))
				m_pProtocol->Send_PatchTable(this);

			m_SendDelayPatch  = false;
			m_PatchWait       = PATCH_TIMEOUT;
		}

		// Transfer packets from share thread to main thread
		m_TransferPacketAccess.Lock();
		
			for( int i = 0; i < m_TransferPackets.size(); i++ )
				SendPacket( m_TransferPackets[i] );
			
			m_TransferPackets.clear();

		m_TransferPacketAccess.Unlock();

		// Keep flushing send buffers
		FlushSendBuffer(true);

		// Drop if not socket gone mute 30 secs
		if(m_dwSecBytes[0] == 0)
		{
			m_SecsDead++;

			if(m_SecsDead == 30)
				m_pProtocol->Send_Ping(this, 1);

			if(m_SecsDead > 60)
			{		
				CloseWithReason("Minute Dead");
				return;
			}
		}
		else
			m_SecsDead = 0;


		if(m_SecsAlive < 60 * 10)
			m_SecsAlive++;

		// Stable Connection
		if(m_SecsAlive == 30)
		{
			packet_VendMsg FirewallTest;
			CoCreateGuid(&FirewallTest.Header.Guid);

			// Send TCP test if needed
			if(m_SupportsVendorMsg && m_pNet->m_TcpFirewall)
			{
				FirewallTest.Ident = packet_VendIdent("BEAR", 7, 1);
				m_pProtocol->Send_VendMsg( this, FirewallTest, &m_pNet->m_CurrentPort, 2);
			}

			// Send UDP test if needed
			if(m_SupportsVendorMsg && m_pNet->m_UdpFirewall != UDP_FULL && m_GnuNodeMode == GNU_ULTRAPEER)
			{
				FirewallTest.Ident = packet_VendIdent("GNUC", 7, 1);
				IPv4 SendBack;
				SendBack.Host = m_pNet->m_CurrentIP;
				SendBack.Port = m_pComm->m_UdpPort;
				m_pProtocol->Send_VendMsg( this, FirewallTest, &SendBack, 6);
			}

			// Update searches with a new host
			if(m_GnuNodeMode == GNU_ULTRAPEER)
				for(i = 0; i < m_pNet->m_SearchList.size(); i++)
					m_pNet->m_SearchList[i]->IncomingGnuNode(this);
		}

		// Allow 20 secs for tcp and udp tests to come back
		if(m_SecsAlive == 50)
		{
			// Send node our stats
			if(m_SupportsStats)
				m_pProtocol->Send_StatsMsg(this);

			// Send PushProxy Request
			if(m_pComm->m_GnuClientMode == GNU_LEAF && m_pNet->m_TcpFirewall && m_SupportsVendorMsg)
			{
				packet_VendMsg PushProxyRequest;
				PushProxyRequest.Header.Guid = m_pPrefs->m_ClientID;
				PushProxyRequest.Ident = packet_VendIdent("LIME", 21, 1);
				m_pProtocol->Send_VendMsg( this, PushProxyRequest);
			}
		}
		
		// Send stat update at 30 min interval
		if( time(NULL) > m_NextStatUpdate && m_SupportsStats)
		{
			m_pProtocol->Send_StatsMsg(this);
			m_NextStatUpdate = time(NULL) + 30*60;
		}

		// Close if we're browsing host and all bytes received
		if(m_BrowseID)
			if(m_BrowseRecvBytes == m_BrowseSize)
				CloseWithReason("Browse Completed");
	}

	else if(SOCK_CLOSED == m_Status)
	{
		m_CloseWait++;
	}
}

void CGnuNode::CompressionStats()
{
	if( !m_DeflateSend )
		return;

	m_ZipStat++;

	if(m_ZipStat == 60)
	{
		// 2.3.5.4 -> 2,000 bytes sent compressed at 1,000 bytes
		//m_pCore->DebugLog( IPtoStr(m_Address.Host) + " -> " + CommaIze( NumtoStr(DeflateStream.total_in) ) + " bytes sent compressed at " + CommaIze( NumtoStr(DeflateStream.total_out) ) + " bytes");

		m_ZipStat = 0;
	}
}

int CGnuNode::UpdateStats(int type)
{
	bool Clean = true;
	if(m_StatElements < PACKETCACHE_SIZE)
	{
		Clean = false;
		m_StatElements++;
	}

	if(m_StatPos == PACKETCACHE_SIZE)
		m_StatPos = 0;

	int StatPos = m_StatPos;

	if(Clean)
		RemovePacket(m_StatPos);
	
	m_StatPackets[m_StatPos][0] = type;

	m_StatPos++;
	
	switch(type)
	{
	case 0x00:
		m_StatPings[0]++;
		break;
	case 0x01:
		m_StatPongs[0]++;
		break;
	case 0x80:
		m_StatQueries[0]++;
		break;
	case 0x81:
		m_StatQueryHits[0]++;
		break;
	case 0x40:
		m_StatPushes[0]++;
		break;
	}


	return StatPos;
}

void CGnuNode::AddGoodStat(int type)
{
	m_StatPackets[m_StatPos][1] = 1;
	
	switch(type)
	{
	case 0x00:
		m_StatPings[1]++;
		break;
	case 0x01:
		m_StatPongs[1]++;
		break;
	case 0x80:
		m_StatQueries[1]++;
		break;
	case 0x81:
		m_StatQueryHits[1]++;
		break;
	case 0x40:
		m_StatPushes[1]++;
		break;
	}
}

void CGnuNode::RemovePacket(int pos)
{
	switch(m_StatPackets[pos][0])
	{
	case 0x00:
		m_StatPings[0]--;

		if(m_StatPackets[pos][1])
			m_StatPings[1]--;

		break;
	case 0x01:
		m_StatPongs[0]--;

		if(m_StatPackets[pos][1])
			m_StatPongs[1]--;

		break;
	case 0x80:
		m_StatQueries[0]--;

		if(m_StatPackets[pos][1])
			m_StatQueries[1]--;

		break;
	case 0x81:
		m_StatQueryHits[0]--;

		if(m_StatPackets[pos][1])
			m_StatQueryHits[1]--;

		break;
	case 0x40:
		m_StatPushes[0]--;

		if(m_StatPackets[pos][1])
			m_StatPushes[1]--;

		break;
	}

	m_StatPackets[pos][1] = 0;
}

bool CGnuNode::ValidAgent(CString Agent)
{
	CString lowAgent = Agent;
	lowAgent.MakeLower();

	//if(lowAgent.Find("xxx") != -1)
	//	return false;

	return true;
}

bool CGnuNode::LetConnect()
{
	// Let GnucDNA's connect if local client full

	// Preferencing says if the local client already has 50% dna, this new connection will be dropped after setconnected

	if( m_RemoteAgent.Find("GnucDNA") != -1 )
		return true;

	return false;
}