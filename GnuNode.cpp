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
#include "GnuControl.h"
#include "G2Control.h"
#include "GnuWordHash.h"
#include "GnuMeta.h"
#include "GnuSchema.h"

#include "DnaCore.h"
#include "DnaNetwork.h"
#include "DnaEvents.h"

#include "GnuNode.h"


CGnuNode::CGnuNode(CGnuControl* pComm, CString Host, UINT Port)
{		
	m_pComm  = pComm;
	m_pNet   = pComm->m_pNet;
	m_pCore  = m_pNet->m_pCore;
	m_pTrans = m_pCore->m_pTrans;
	m_pPrefs = m_pCore->m_pPrefs;
	m_pCache = pComm->m_pCache;
	m_pShare = m_pCore->m_pShare;

	// Socket 
	m_NodeID = m_pNet->GetNextNodeID();
	m_pComm->m_NodeIDMap[m_NodeID] = this;

	m_pComm->m_GnuNodeAddrMap[StrtoIP(Host).S_addr] = this;

	m_Status = SOCK_CONNECTING;	
	m_StatusText = "Connecting";
	
	m_SecsTrying		= 0;
	m_SecsAlive			= 0;
	m_SecsDead			= 0;
	m_CloseWait			= 0;
	
	m_IntervalPing		= 0;
	m_NextRequeryWait   = 0;


	// Connection vars
	m_HostIP	= Host;
	m_HostName  = Host;

	m_NetworkName = m_pComm->m_NetworkName;
	
	m_GnuNodeMode		= 0;
	m_Port				= Port;
	m_Inbound			= false;
	m_ConnectTime		= CTime::GetCurrentTime();

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
	m_NodeFileCount		= 0;
	m_NodeLeafMax		= 0;
	
	m_DowngradeRequest  = false;
	

	m_UltraPongSent	= false;	
	

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

	
	// Bandwidth Control
	m_LeafBytesIn    = 0; 
	m_LeafBytesOut   = 0; 


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

	m_pComm->NodeUpdate(this);
}

CGnuNode::~CGnuNode()
{
	m_Status = SOCK_CLOSED;
	m_pComm->NodeUpdate(this);

	std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(m_NodeID);
	if(itNode != m_pComm->m_NodeIDMap.end())
		m_pComm->m_NodeIDMap.erase(itNode);
	
	std::map<uint32, CGnuNode*>::iterator itAddr = m_pComm->m_GnuNodeAddrMap.find( StrtoIP(m_HostIP).S_addr);
	if(itAddr != m_pComm->m_GnuNodeAddrMap.end())
		m_pComm->m_GnuNodeAddrMap.erase(itAddr);
	
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
	GetPeerName(m_HostIP, m_Port);


	// If node created to browse remote host
	if(m_BrowseID)
	{
		Handshake = "GET / HTTP/1.1\r\n";
		Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";
		Handshake += "Accept: text/html, application/x-gnutella-packets\r\n";
		Handshake += "Accept-Encoding: deflate\r\n";
		Handshake += "Connection: close\r\n";
		Handshake += "Host:" + m_HostIP + ":" + NumtoStr(m_Port) + "\r\n";

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
		Handshake += "Remote-IP: " + m_HostIP + "\r\n";

		Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";

		// LAN Header
		if(m_pPrefs->m_LanMode)
			Handshake += "LAN: " + m_pPrefs->m_LanName + "\r\n";
	

		// Ultrapeer Header
		if(m_pComm->m_GnuClientMode == GNU_LEAF)
			Handshake += "X-Ultrapeer: False\r\n";

		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			Handshake += "X-Ultrapeer: True\r\n";
			Handshake += "X-Leaf-Max: " + NumtoStr(m_pPrefs->m_MaxLeaves) + "\r\n";
		}

		// X-Degree
		Handshake += "X-Degree: " + NumtoStr(m_pPrefs->m_MaxConnects) + "\r\n";
		
		// Query Routing Header
		Handshake += "X-Query-Routing: 0.1\r\n";
		
		// X-Ultrapeer-Query-Routing
		Handshake += "X-Ultrapeer-Query-Routing: 0.1\r\n";
		
		// X-Max-TTL
		Handshake += "X-Max-TTL: " + NumtoStr(MAX_TTL) + "\r\n";

		// Accept-Encoding Header
		if(m_dnapressionOn)
			Handshake += "Accept-Encoding: deflate\r\n";

		// Uptime Header
		CTimeSpan Uptime(CTime::GetCurrentTime() - m_pComm->m_ClientUptime);
		Handshake += "Uptime: " + Uptime.Format("%DD %HH %MM") + "\r\n";

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

		// Parse User-Agent header
		m_RemoteAgent = FindHeader("User-Agent");
		if( !ValidAgent(m_RemoteAgent) )
		{
			CloseWithReason("Client Not Specified");
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

		// Parse Accept-Encoding
		CString EncodingHeader = FindHeader("Accept-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_DeflateSend = true;

		// Parse Uptime
		int days = 0, hours = 0, minutes = 0;
		CString UptimeHeader = FindHeader("Uptime");
		if(!UptimeHeader.IsEmpty())
		{
			sscanf(UptimeHeader, "%dD %dH %dM", &days, &hours, &minutes);
			m_HostUpSince = CTime::GetCurrentTime() - CTimeSpan(days, hours, minutes, 0);
		}


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


		// Parse leaf max header
		CString LeafMax = FindHeader("X-Leaf-Max");		
		if(!LeafMax.IsEmpty())
			m_NodeLeafMax = atoi(LeafMax);
		else
			m_NodeLeafMax = 75;

		if(m_NodeLeafMax > 1000)
			m_NodeLeafMax = 1000;


		if(m_lowHandshake.Find("bearshare 2.") != -1)
			m_GnuNodeMode = GNU_ULTRAPEER;


		// If in Ultrapeer Mode
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			// Ultrapeer Connecting
			if(m_GnuNodeMode == GNU_ULTRAPEER)
			{
				// If we have 66% free room of their max leaf count, request downgrade
				if(m_pShare->FreeCapacity(m_NodeLeafMax) > 66)
					m_DowngradeRequest = true;
				
				else if(m_pPrefs->m_MaxConnects != -1 && m_pComm->CountNormalConnects() >= m_pPrefs->m_MaxConnects)
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
				if(!QueryRouting)
				{
					Send_ConnectError("503 In Leaf Mode");
					return;
				}

				if(m_pShare->RunningCapacity(m_pPrefs->m_MaxLeaves) > 100)
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
				
				if(m_pComm->CountNormalConnects() >= m_pPrefs->m_LeafModeConnects || !QueryRouting)
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

		// Connecting Node a Normal Node
		/*else
		{
			if(m_pComm->m_GnuClientMode == GNU_LEAF)
			{
				Send_ConnectError("503 In Leaf Mode");
				return;
			}

			if(m_pPrefs->m_MaxConnects != -1 && m_pComm->CountNormalConnects() >= m_pPrefs->m_MaxConnects)
			{
				Send_ConnectError("503 Connects Maxed");
				return;
			}

		}*/
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
			

			if(m_DowngradeRequest)
			{
				if(UltraHeader == "false")
					m_GnuNodeMode = GNU_LEAF;
				else if(m_pPrefs->m_MaxConnects != -1 && m_pComm->CountNormalConnects() >= m_pPrefs->m_MaxConnects)
				{
					CloseWithReason("Connects Maxed");
					return;
				}
			}
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


	// Ok string, GNUTELLA/0.6 200 OK\r\n
	if(m_Handshake.Find(" 200 OK\r\n") != -1)
	{
		// Parse Remote-IP header
		CString RemoteIP = FindHeader("Remote-IP");
		if(!RemoteIP.IsEmpty())
			m_pNet->m_CurrentIP = StrtoIP(RemoteIP);
	

		// Parse User-Agent header
		m_RemoteAgent = FindHeader("User-Agent");
		if( !ValidAgent(m_RemoteAgent) )
		{
			CloseWithReason("Client Not Specified");
			return;
		}


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


		// Parse Accept-Encoding
		CString EncodingHeader = FindHeader("Accept-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_DeflateSend = true;

		// Parse Content-Encoding
		EncodingHeader = FindHeader("Content-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_InflateRecv = true;
		
		// Parse Uptime
		int days = 0, hours = 0, minutes = 0;
		CString UptimeHeader = FindHeader("Uptime");
		if(!UptimeHeader.IsEmpty())
		{
			sscanf(UptimeHeader, "%dD %dH %dM", &days, &hours, &minutes);
			m_HostUpSince = CTime::GetCurrentTime() - CTimeSpan(days, hours, minutes, 0);
		}

		// Parse Accept header
		CString AcceptHeader = FindHeader("Accept");
		if(!AcceptHeader.IsEmpty())
			if( AcceptHeader.Find("application/x-gnutella2") != -1 )
				m_pNet->m_pCache->AddKnown( Node(m_HostIP, m_Port, NETWORK_G2) );

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


		// Parse leaf max header
		CString LeafMax = FindHeader("X-Leaf-Max");		
		if(!LeafMax.IsEmpty())
			m_NodeLeafMax = atoi(LeafMax);
		else
			m_NodeLeafMax = 75;

		if(m_NodeLeafMax > 1500)
			m_NodeLeafMax = 1500;


		if(m_lowHandshake.Find("bearshare 2.") != -1)
			m_GnuNodeMode = GNU_ULTRAPEER;


		// In Ultrapeer Mode
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			// Connecting Ultrapeer
			if(m_GnuNodeMode == GNU_ULTRAPEER)
			{
				// Parse the Ultrapeers Needed header
				CString NeededHeader = FindHeader("X-Ultrapeer-Needed");
				NeededHeader.MakeLower();


				// This SuperNode wants more leaves
				if(NeededHeader == "false" && QueryRouting && !m_pComm->m_ForcedUltrapeer)
				{
					// Only downgrade if remote dna
					// If we are handling less than 33% of what they can handle, downgrade
					// If our uptime is longer do not become leaf unless real firewall is false
					if(m_RemoteAgent.Find("GnucDNA") > 0)
					{
						int dnapos = m_RemoteAgent.Find("GnucDNA");

						CString CurrentVersion = m_pCore->m_DnaVersion;
						CString RemoteVersion  = m_RemoteAgent.Mid(dnapos + 8, 7);

						CurrentVersion.Remove('.');
						RemoteVersion.Remove('.');

						if(atoi(RemoteVersion) >= atoi(CurrentVersion))
							if(m_HostUpSince.GetTime() < m_pComm->m_ClientUptime.GetTime())
								if(m_pShare->RunningCapacity(m_NodeLeafMax) < 33)
								{
									m_DowngradeRequest		  = true;

									for(int i = 0; i < m_pComm->m_NodeList.size(); i++)
									{
										CGnuNode *p = m_pComm->m_NodeList[i];

										if(p != this && p->m_Status == SOCK_CONNECTED)
											p->CloseWithReason("Node Downgrading");
									}

									m_pComm->m_GnuClientMode = GNU_LEAF;
								}
					}	
				}
				
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
		
		// Ultrapeer header not found, we are connecting to a normal node
		/*else
		{
			if(m_pComm->m_GnuClientMode == GNU_LEAF)
			{
				Send_ConnectError("503 In Leaf Mode");
				return;
			}

			Send_ConnectOK(true);
			SetConnected();
		}*/
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
		m_pCache->AddWorking( Node(m_HostIP, m_Port, NETWORK_GNUTELLA, CTime::GetCurrentTime() ) );


	TryHeader += ",";
	TryHeader.Replace(",,", ",");
	TryHeader.Remove(' ');
	
	int tryFront = 0, 
		tryMid = TryHeader.Find(":"), 
		tryBack = TryHeader.Find(",");
	
	while(tryBack != -1 && tryMid != -1)
	{
		Node tryNode;
		tryNode.Host = TryHeader.Mid(tryFront, tryMid - tryFront);
		tryNode.Port = atoi( TryHeader.Mid(tryMid + 1, tryBack - tryMid + 1));

		m_pCache->AddKnown( tryNode);


		tryFront  = tryBack + 1; 
		tryMid    = TryHeader.Find(":", tryFront);
		tryBack   = TryHeader.Find(",", tryFront);
	}
}

void CGnuNode::ParseHubsHeader(CString HubsHeader)
{
	if( m_pNet->m_pG2 == NULL )
		return;

	// 1.2.3.4:6346 2003-03-25T23:59Z,
	CString Address = ParseString(HubsHeader, ',');

	while( !Address.IsEmpty() )
	{
		Node tryNode;
		tryNode.Network = NETWORK_G2;
		tryNode.Host = ParseString(Address, ':');
		tryNode.Port = atoi(ParseString(Address, ' '));

		if(Address.IsEmpty()) // Is not sending timestamp, probably G1 node in G2 cache
			return;

		tryNode.LastSeen = StrToCTime(Address);

		m_pCache->AddKnown( tryNode);

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

		
		// We are converting from supernode to a leaf
		if(m_DowngradeRequest)
			Handshake += "X-Ultrapeer: False\r\n";

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
		UINT nTrash;
		GetPeerName(m_HostIP, nTrash);
		Handshake += "Remote-IP: " + m_HostIP + "\r\n";
		
		Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";


		// LAN header
		if(m_pPrefs->m_LanMode)
			Handshake += "LAN: " + m_pPrefs->m_LanName + "\r\n";

		// Ultrapeer header
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			Handshake += "X-Ultrapeer: True\r\n";
			Handshake += "X-Leaf-Max: " + NumtoStr(m_pPrefs->m_MaxLeaves) + "\r\n";

			if(m_DowngradeRequest)
				Handshake += "X-Ultrapeer-Needed: False\r\n";
		}
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

		// Compression headers
		Handshake += "Accept-Encoding: deflate\r\n";

		if(m_DeflateSend)
			Handshake += "Content-Encoding: deflate\r\n";

		// Uptime header
		CTimeSpan Uptime(CTime::GetCurrentTime() - m_pComm->m_ClientUptime);
		Handshake += "Uptime: " + Uptime.Format("%DD %HH %MM")+ "\r\n";

		// Bye Header
		Handshake += "Bye-Packet: 0.1\r\n";

		// Send authentication response
		if( !m_RemoteChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Response: " + m_RemoteChallengeAnswer + "\r\n";


		// Send authentication challenge
		if( !m_Challenge.IsEmpty() && !m_ChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Challenge: " + m_Challenge + "\r\n";


		// X-Try header
		CString HostsToTry;
		if(GetAlternateHostList(HostsToTry))
			Handshake += "X-Try: " + HostsToTry + "\r\n";


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
	UINT nTrash;
	GetPeerName(m_HostIP, nTrash);
	Handshake += "Remote-IP: " + m_HostIP + "\r\n";


	// X-Try header
	CString HostsToTry;
	if(GetAlternateHostList(HostsToTry))
		Handshake += "X-Try: " + HostsToTry + "\r\n";


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

	UINT nTrash;
	GetPeerName(m_HostIP, nTrash);
	Response += "Remote-IP: " + m_HostIP + "\r\n";

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
	m_pComm->Encode_QueryHit(BrowseQuery, FileIndexes, ReplyBuffer);


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
	// Get Remote host
	CString HostIP;
	UINT Port;
	GetPeerName(m_HostIP, Port);

	m_Status = SOCK_CONNECTED;
	m_StatusText = "Connected";
	m_pComm->NodeUpdate(this);


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
		Send_PatchReset(); // Send reset

		m_SendDelayPatch = true; // Set to send patch
	}


	Send_Ping(1);			
}

void CGnuNode::SendPacket(void* packet, int length, int type, int distance, bool thread)
{
	// Broadcast packets distance is hops, routed packets distance is ttl - 1

	if(length > PACKET_BUFF || length <= 0)
	{
		ASSERT(0);
		return;
	}

	// handled by inspect packet already, asserts on forwared queries to leaves
	// need to check cause packetlist array would go out of bounds
	if(distance >= MAX_TTL)
		distance = MAX_TTL - 1;

	if(distance < 0)
		distance = 0;

	ASSERT(packet);

	
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
					CloseWithReason("Send Buffer Error " + NumtoStr(lastError), true);
				
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

			m_pComm->PacketOutgoing(m_NodeID, FrontPacket->m_Packet, FrontPacket->m_Length, FrontPacket->m_Type == 0);

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
							CloseWithReason("Send Error " + NumtoStr(lastError), true);
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

void CGnuNode::CloseWithReason(CString Reason, bool RemoteClosed)
{
	if(m_Status == SOCK_CONNECTED && !RemoteClosed)
	{
		Send_Bye(Reason);
		FlushSendBuffer(true);
	}

	if(	RemoteClosed )
		m_StatusText = "Remote: " + Reason;
	else
		m_StatusText = "Local: " + Reason;


	if( m_SecsAlive > 30 && m_GnuNodeMode == GNU_ULTRAPEER)
	{
		//if( m_RemoteAgent.Find("GnucDNA") != -1)
		//{
		//	CTimeSpan Uptime = CTime::GetCurrentTime() - m_ConnectTime;
		//	m_pCore->DebugLog( m_RemoteAgent + " " + m_HostIP + ":" + NumtoStr(m_Port) + ", Uptime " + Uptime.Format("%DD %HH %MM") + " Closed: " + Reason);
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
				BuffLength = Receive(&m_pBuff[m_ExtraLength], PACKET_BUFF - m_ExtraLength);

				if(BuffLength > 0)
					SplitBundle(m_pBuff, BuffLength);

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


/////////////////////////////////////////////////////////////////////////////
// Packet handlers

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
	// First add previous buffer data
	length += m_ExtraLength;
	m_ExtraLength = 0;


	UINT Payload = 0;
	UINT nextPos = 0;

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

			if(packet->Payload < 16384)
			{
				if (nextPos + sizeof(packet_Header) + packet->Payload <= length)
				{
					HandlePacket(packet, 23 + packet->Payload);
					
					nextPos += 23 + packet->Payload;
					if (nextPos == length)
						theStatus = status_DONE;
				}
				else
					theStatus = status_INCOMPLETE_PACKET;
			}
			else
			{
				CloseWithReason("Packet Size Greater than 16k");
				return;

		        //if (nextPos < length - sizeof(packet_Header))
				//	nextPos++;
		        //else   
				//	theStatus = status_BAD_PACKET;
			}
		}
	} while(status_CONTINUE == theStatus);


	// Take extra length and save it for next receive
	m_ExtraLength = length - nextPos;

	if (0 != m_ExtraLength)
	{
		if(m_ExtraLength < PACKET_BUFF)
			memmove(m_pBuff, &m_pBuff[length - m_ExtraLength], m_ExtraLength);
		else
			m_ExtraLength = 0;
			//ASSERT(0); // Shouldnt happen
	}
}

void CGnuNode::HandlePacket(packet_Header* packet, DWORD length)
{
	m_dwSecPackets[0]++;

	switch(packet->Function)
	{
	case 0x00:
		Receive_Ping((packet_Ping*) packet, length);
		break;

	case 0x01:
		Receive_Pong((packet_Pong*) packet, length);
		break;

	case 0x30:
		if( ((packet_RouteTableReset*) packet)->PacketType == 0x0)
			Receive_RouteTableReset((packet_RouteTableReset*) packet, length);
		else if(((packet_RouteTableReset*) packet)->PacketType == 0x1)
			Receive_RouteTablePatch((packet_RouteTablePatch*) packet, length);

		break;
	case 0x40:
		Receive_Push((packet_Push*) packet, length);
		break;

	case 0x80:
		Receive_Query((packet_Query*) packet, length);
		break;

	case 0x81:
		Receive_QueryHit((packet_QueryHit*) packet, length);
		break;

	case 0x02:
		Receive_Bye((packet_Bye*) packet, length);
		break;

	default:
		// Disable unknowns
		// Receive_Unknown((byte *) packet, length);
		break;
	}
}

bool CGnuNode::InspectPacket(packet_Header* packet)
{
	if(packet->TTL == 0 || packet->Hops >= MAX_TTL)
		return false;


	packet->Hops++; 
	packet->TTL--;


	// Reset TTL if too high
	if(packet->TTL >= MAX_TTL)
		packet->TTL = MAX_TTL - packet->Hops;

	return true;
}


/////////////////////////////////////////////////////////////////////////////
// Receiving packets


void CGnuNode::Receive_Ping(packet_Ping* Ping, int nLength)
{
	// Packet stats
	int StatPos = UpdateStats(Ping->Header.Function);

	// Inspect
	if(!InspectPacket(&Ping->Header))
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Ping, nLength, ERROR_HOPS, false);
		return;
	}

	int RouteID	     = m_pComm->m_TableRouting.FindValue(Ping->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(Ping->Header.Guid);

	if(LocalRouteID != -1)
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Ping, nLength, ERROR_LOOPBACK, false);
		return;
	}

	// Fresh Ping?
	if(RouteID == -1)
	{
		m_pComm->m_TableRouting.Insert(Ping->Header.Guid, m_NodeID);
		
		// Ping from child node
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER && m_GnuNodeMode == GNU_LEAF)
		{
			if(Ping->Header.Hops == 1)
			{
				Send_Pong(Ping->Header.Guid, Ping->Header.Hops);
				Ping->Header.TTL = 0;
			}
			else
			{
				m_pComm->PacketIncoming(m_NodeID, (byte*) Ping, nLength, ERROR_ROUTING, false);
				return;
			}
		}

		// if network is private 
		else if(m_pPrefs->m_LanMode)
			Send_Pong(Ping->Header.Guid, Ping->Header.Hops);

		// else if connections below min or hops below 3
		else if(Ping->Header.Hops < 3 || (m_pPrefs->m_MinConnects != -1 && m_pComm->m_NormalConnectsApprox < m_pPrefs->m_MinConnects))
			Send_Pong(Ping->Header.Guid, Ping->Header.Hops);

		// Send 1kb of extended pongs a minute (advertise our ultrapeer)
		else if(m_pComm->m_ExtPongBytes < 22 && m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			Send_Pong(Ping->Header.Guid, Ping->Header.Hops);
			m_pComm->m_ExtPongBytes += 23;
		}

		// Broadcast if still alive
		if(Ping->Header.Hops < MAX_TTL && Ping->Header.TTL > 0)
			m_pComm->Broadcast_Ping(Ping, nLength, this);


		m_pComm->PacketIncoming(m_NodeID, (byte*) Ping, nLength, ERROR_NONE, false);
		
		AddGoodStat(Ping->Header.Function);	

		return;
	}
	else
	{
		if(RouteID == m_NodeID)
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) Ping, nLength, ERROR_DUPLICATE, false);
			return;
		}
		else
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) Ping, nLength, ERROR_ROUTING, false);
			return;
		}
	}
}


void CGnuNode::Receive_Pong(packet_Pong* Pong, int nLength)
{
	if(Pong->Header.Payload < 14)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Pong, Length " + NumtoStr(Pong->Header.Payload));
		return;
	}

	// Packet stats
	int StatPos = UpdateStats(Pong->Header.Function);

	// Inspect
	if(!InspectPacket(&Pong->Header))
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Pong, nLength, ERROR_HOPS, false);
		return;
	}

	// Detect if this pong is from an ultrapeer
	bool Ultranode = false;

	UINT Marker = 8;
	while(Marker <= Pong->FileSize && Marker)
	{
		if(Marker == Pong->FileSize)
		{
			Ultranode = true;
			break;
		}
		else
		{
			Marker *= 2;
		}
	}

	// Add to host cache
	if(Pong->Header.Hops)
	{
		m_pCache->AddKnown( Node(IPtoStr(Pong->Host), Pong->Port ) );
	}

	int RouteID		 = m_pComm->m_TableRouting.FindValue(Pong->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(Pong->Header.Guid);


	// Pong for us, or Pong coming in from same path we sent one out
	if(LocalRouteID == 0 || LocalRouteID == m_NodeID)
	{
		// If this pong is one we sent out
		if(LocalRouteID == m_NodeID)
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) Pong, nLength, ERROR_LOOPBACK, false);

			return;
		}
		else
		{
			// Nodes file count
			if(Pong->Header.Hops == 1)
				m_NodeFileCount = Pong->FileCount;


			// Mapping
			if(Pong->Header.Hops < 3)
				MapPong(Pong);
			

			// If node a leaf and we are ultrapeer
			//   Give this pong to other leaves so if we die they can find each other
			if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER && m_GnuNodeMode == GNU_LEAF)
				m_pComm->Route_UltraPong(Pong, nLength, LocalRouteID);
					

			AddGoodStat(Pong->Header.Function);
			m_pComm->PacketIncoming(m_NodeID, (byte*) Pong, nLength, ERROR_NONE, true);
			
			return;
		}
	}

	if(RouteID != -1)
	{	
		// Send it out	
		if(Pong->Header.Hops < MAX_TTL && Pong->Header.TTL > 0)
		{
			m_pComm->Route_Pong(Pong, nLength, RouteID);

			if(Ultranode && m_GnuNodeMode != GNU_LEAF)
				m_pComm->Route_UltraPong(Pong, nLength, RouteID);
		}

		AddGoodStat(Pong->Header.Function);
		m_pComm->PacketIncoming(m_NodeID, (byte*) Pong, nLength, ERROR_NONE, false);
		
		return;
	}
	else
	{
		// If pong advertising ultrapeer it is good
		if(m_pComm->m_GnuClientMode == GNU_LEAF && Ultranode)
		{
			AddGoodStat(Pong->Header.Function);
			m_pComm->PacketIncoming(m_NodeID, (byte*) Pong, nLength, ERROR_NONE, true);

			return;
		}

		m_pComm->PacketIncoming(m_NodeID, (byte*) Pong, nLength, ERROR_ROUTING, false);
		return;
	}  
}

void CGnuNode::Receive_Push(packet_Push* Push, int nLength)
{
	if(Push->Header.Payload < 26)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Push, Length " + NumtoStr(Push->Header.Payload));
		return;
	}


	// Packet stats
	int StatPos = UpdateStats(Push->Header.Function);
	
	// Host Cache
	m_pCache->AddKnown( Node(IPtoStr(Push->Host), Push->Port) );


	// Inspect
	if(!InspectPacket(&Push->Header))
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Push, nLength, ERROR_HOPS, false);
		return;
	}


	// Find packet in hash tables
	int RouteID		 = m_pComm->m_TableRouting.FindValue(Push->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(Push->Header.Guid);
	int PushRouteID  = m_pComm->m_TablePush.FindValue(Push->ServerID);

	if(LocalRouteID != -1)
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Push, nLength, ERROR_LOOPBACK, false);
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

		AddGoodStat(Push->Header.Function);
		m_pComm->PacketIncoming(m_NodeID, (byte*) Push, nLength, ERROR_NONE, true);
		
		return;
	}

	if(RouteID == -1)
	{
		m_pComm->m_TableRouting.Insert(Push->Header.Guid, m_NodeID);
	}
	else
	{
		if(RouteID == m_NodeID)
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) Push, nLength, ERROR_DUPLICATE, false);
			return;
		}
		else
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) Push, nLength, ERROR_ROUTING, false);
			return;
		}
	}

	if(PushRouteID != -1)
	{	
		if(Push->Header.Hops < MAX_TTL && Push->Header.TTL > 0)
			m_pComm->Route_Push(Push, nLength, PushRouteID);
		
		AddGoodStat(Push->Header.Function);
		m_pComm->PacketIncoming(m_NodeID, (byte*) Push, nLength, ERROR_NONE, false);
		
		return;	
	}
	else
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Push, nLength, ERROR_ROUTING, false);
		return;
	}
}

void CGnuNode::Receive_Query(packet_Query* Query, int nLength)
{
	if(Query->Header.Payload < 2)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Query, Length " + NumtoStr(Query->Header.Payload));
		return;
	}
	
	// Packet stats
	int StatPos = UpdateStats(Query->Header.Function);


	if(!InspectPacket(&Query->Header))
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Query, nLength, ERROR_HOPS, false);
		return;
	}

	// Inspect
	int QuerySize  = Query->Header.Payload - 2;
	int TextSize   = strlen((char*) Query + 25) + 1;

	// Bad packet, text bigger than payload
	if (TextSize > QuerySize)
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Query, nLength, ERROR_ROUTING, false);
		//TRACE0("Text Query too big " + CString((char*) Query + 25) + "\n");
		return;
	}

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

	
	int RouteID = m_pComm->m_TableRouting.FindValue(Query->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(Query->Header.Guid);

	if(LocalRouteID != -1)
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) Query, nLength, ERROR_LOOPBACK, false);
		return;
	}

	// Fresh Query?
	if(RouteID == -1)
	{
		m_pComm->m_TableRouting.Insert(Query->Header.Guid, m_NodeID);

		AddGoodStat(Query->Header.Function);

		if(*((char*) Query + 25) == '\\')
			return;

		// Client in Ultrapeer Mode
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			// Received from Leaf
			if( m_GnuNodeMode == GNU_LEAF )
			{
				Query->Header.Hops = 0; // Reset Hops
				Query->Header.TTL++;    // Increase TTL

				m_pComm->Broadcast_Query(Query, nLength, this);
			}

			// Received from Ultrapeer
			if( m_GnuNodeMode == GNU_ULTRAPEER )
			{
				if(Query->Header.Hops < MAX_TTL && Query->Header.TTL > 0)
					m_pComm->Broadcast_Query(Query, nLength, this);
			}
		}

		if(Query->Header.TTL == 0 && m_SupportInterQRP)
		{
			CString Text((char*) Query + 25);
			TRACE0("QUERY:" + Text + "\n");
		}
			
		// Queue to be compared with local files
		GnuQuery G1Query;
		G1Query.Network    = NETWORK_GNUTELLA;
		G1Query.OriginID   = m_NodeID;
		G1Query.SearchGuid = Query->Header.Guid;

		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		{
			G1Query.Forward = true;
			G1Query.Source  = this;

			if(Query->Header.TTL == 1)
				G1Query.UltraForward = true;

			memcpy(G1Query.Packet, (byte*) Query, nLength);
			G1Query.PacketSize = nLength;
		}

		G1Query.Terms.push_back( CString((char*) Query + 25, TextSize) );
		
		while(!ExtendedQuery.IsEmpty())
			G1Query.Terms.push_back( ParseString(ExtendedQuery, 0x1C) );


		m_pShare->m_QueueAccess.Lock();
			m_pShare->m_PendingQueries.push_front(G1Query);	
		m_pShare->m_QueueAccess.Unlock();


		m_pShare->m_TriggerThread.SetEvent();
		
		
		m_pComm->PacketIncoming(m_NodeID, (byte*) Query, nLength, ERROR_NONE, false);

		return;
	}
	else
	{
		if(RouteID == m_NodeID)
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) Query, nLength, ERROR_DUPLICATE, false);
			return;
		}
		else
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) Query, nLength, ERROR_ROUTING, false);
			return;
		}
	}
}


void CGnuNode::Receive_QueryHit(packet_QueryHit* QueryHit, DWORD nLength)
{
	if(QueryHit->Header.Payload < 27)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Query Hit, Length " + NumtoStr(QueryHit->Header.Payload));
		return;
	}

	// Packet stats
	int StatPos = UpdateStats(QueryHit->Header.Function);

	// Host Cache
	m_pCache->AddKnown( Node(IPtoStr(QueryHit->Host), QueryHit->Port) );
	
	// Inspect
	if(!InspectPacket(&QueryHit->Header))
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) QueryHit, nLength, ERROR_HOPS, false);
		return;
	}


	int RouteID		 = m_pComm->m_TableRouting.FindValue(QueryHit->Header.Guid);
	int LocalRouteID = m_pComm->m_TableLocal.FindValue(QueryHit->Header.Guid);

	if(m_BrowseID)
		LocalRouteID = 0;


	// Queryhit for us, or Queryhit coming in from same path we sent one out
	if(LocalRouteID == 0 || LocalRouteID == m_NodeID)
	{
		// Check for query hits we sent out
		if(LocalRouteID == m_NodeID)
		{
			m_pComm->PacketIncoming(m_NodeID, (byte*) QueryHit, nLength, ERROR_LOOPBACK, false);
			
			return;
		}

		else
		{	int i = 0;
			
			AddGoodStat(QueryHit->Header.Function);

			m_pComm->PacketIncoming(m_NodeID, (byte*) QueryHit, nLength, ERROR_NONE, true);

			CGnuSearch* pSearch = NULL;
			CGnuDownloadShell* pDownload = NULL;

			// Check for query hit meant for client
			for(i = 0; i < m_pNet->m_SearchList.size(); i++)
				if(QueryHit->Header.Guid == m_pNet->m_SearchList[i]->m_QueryID || m_BrowseID == m_pNet->m_SearchList[i]->m_SearchID)
				{
					pSearch = m_pNet->m_SearchList[i];
					break;
				}

			// Look for matches in current downloads
			for(i = 0; i < m_pTrans->m_DownloadList.size(); i++)
				if(QueryHit->Header.Guid == m_pTrans->m_DownloadList[i]->m_SearchGuid)
				{
					pDownload = m_pTrans->m_DownloadList[i];
					break;
				}	
					
			if( pSearch == NULL &&  pDownload == NULL )
				return;

			// Extract file sources from query hit and pass to search and download interfaces
			std::vector<FileSource> Sources;
			Decode_QueryHit(Sources, QueryHit, nLength);

			for(int i = 0; i < Sources.size(); i++)
			{
				if(pSearch)
					pSearch->IncomingSource(Sources[i]);
				if(pDownload)
					pDownload->AddHost(Sources[i]);
			}
		
			return;
		}
	}

	if(RouteID != -1)
	{	
		// Add ClientID of packet to push table
		if(m_pComm->m_TablePush.FindValue( *((GUID*) ((byte*)QueryHit + (nLength - 16)))) == -1)
			m_pComm->m_TablePush.Insert( *((GUID*) ((byte*)QueryHit + (nLength - 16))) , m_NodeID);
		
		// If received from child reset
		if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER && m_GnuNodeMode == GNU_LEAF)
		{
			QueryHit->Header.Hops = 0;
			QueryHit->Header.TTL  = MAX_TTL;
		}

		// Send it out
		if(QueryHit->Header.Hops < MAX_TTL && QueryHit->Header.TTL > 0)
			m_pComm->Route_QueryHit(QueryHit, nLength, RouteID);

		// Send if meant for child
		std::map<int, CGnuNode*>::iterator itNode = m_pComm->m_NodeIDMap.find(RouteID);
		if(itNode != m_pComm->m_NodeIDMap.end())
			if(itNode->second->m_Status == SOCK_CONNECTED && itNode->second->m_GnuNodeMode == GNU_LEAF)
			{	
				QueryHit->Header.TTL++;
				m_pComm->Route_QueryHit(QueryHit, nLength, RouteID);
			}

		AddGoodStat(QueryHit->Header.Function);

		m_pComm->PacketIncoming(m_NodeID, (byte*) QueryHit, nLength, ERROR_NONE, false);
		
		return;
	}
	else
	{
		m_pComm->PacketIncoming(m_NodeID, (byte*) QueryHit, nLength, ERROR_ROUTING, false);
		return;
	}  
}

void CGnuNode::Decode_QueryHit( std::vector<FileSource> &Sources, packet_QueryHit* QueryHit, uint32 length)
{
	byte* Packet   = (byte*) QueryHit;

	bool ExtendedPacket = false;
	bool Firewall		= false;
	bool Busy			= false;
	bool Stable			= false;
	bool ActualSpeed	= false;

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
		packet_QueryHitEx* QHD = (packet_QueryHitEx*) &Packet[i + 1];
	
		Vendor = CString((char*) QHD->VendorID, 4);
	
		ExtendedPacket = true;

		if(QHD->Length == 1)
			if(QHD->Push == 1)
				Firewall = true;

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
		}

		// Check for XML Metadata
		if(QHD->Length == 4&& QHD->MetaSize > 1)
			if(QHD->MetaSize < (ClientIDPos - 34))
			{
				CString MetaLoad = CString((char*) &Packet[ClientIDPos - QHD->MetaSize], QHD->MetaSize);

				// Decompress, returns pure xml response
				if(m_pShare->m_pMeta->DecompressMeta(MetaLoad, (byte*) &Packet[ClientIDPos - QHD->MetaSize], QHD->MetaSize))
					m_pShare->m_pMeta->ParseMeta(MetaLoad, MetaIDMap, MetaValueMap);
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

		Item.GnuRouteID = m_NodeID;
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

void CGnuNode::Receive_Bye(packet_Bye* Bye, int nLength)
{
	byte* ByeData = (byte*) Bye;

	CloseWithReason( CString( (char*) &ByeData[23]), true );
}

void CGnuNode::Receive_RouteTableReset(packet_RouteTableReset* TableReset, UINT Length)
{
	if(TableReset->Header.Payload < 6)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Table Reset, Length " + NumtoStr(TableReset->Header.Payload));
		return;
	}

	if(m_GnuNodeMode != GNU_ULTRAPEER)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Table Reset Received while in Leaf Mode");
		return;
	}

	if(TableReset->Header.Hops > 0)
	{
		m_pCore->DebugLog("Gnutella", "Table Reset Hops > 0");
		return;
	}

	m_pComm->PacketIncoming(m_NodeID, (byte*) TableReset, Length, ERROR_NONE, true);

	m_RemoteTableInfinity = TableReset->Infinity;
	m_RemoteTableSize     = TableReset->TableLength / 8;
	memset( m_RemoteHitTable, 0xFF, GNU_TABLE_SIZE );

	m_CurrentSeq = 1;
}

void CGnuNode::Receive_RouteTablePatch(packet_RouteTablePatch* TablePatch, UINT Length)
{
	if(TablePatch->Header.Payload < 5)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Bad Table Patch, Length " + NumtoStr(TablePatch->Header.Payload));
		return;
	}

	if(m_GnuNodeMode != GNU_ULTRAPEER)		   		 
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Received while in Leaf Mode");
		return;
	}

	if(TablePatch->Header.Hops > 0)
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Hops > 0");
		return;
	}

	if( TablePatch->SeqNum == 0 || TablePatch->SeqNum > TablePatch->SeqSize || m_CurrentSeq != TablePatch->SeqNum)
	{
		CloseWithReason("Table Patch Sequence Error");
		return;
	}

	if(TablePatch->EntryBits != 4 && TablePatch->EntryBits != 8)
	{
		CloseWithReason("Table Patch Invalid Entry Bits");
		return;
	}

	// Make sure table length and infinity have been set
	if(m_RemoteTableSize == 0 || m_RemoteTableInfinity == 0)
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Received Before Reset");
		return;
	}

	m_pComm->PacketIncoming(m_NodeID, (byte*) TablePatch, Length, ERROR_NONE, true);


	// If first patch in sequence, reset table
	if(TablePatch->SeqNum == 1)
	{
		if(m_PatchBuffer)
			delete [] m_PatchBuffer;

		m_PatchSize   = m_RemoteTableSize * TablePatch->EntryBits;
		m_PatchBuffer  = new byte[m_PatchSize];
		m_PatchOffset = 0;
	}
	
	// Check patch not received out of sync and buff not created
	if(m_PatchBuffer == NULL)
	{
		m_pCore->DebugLog("Gnutella", "Table Patch Received Out of Sync");
		return;
	}


	if(TablePatch->SeqNum <= TablePatch->SeqSize)
	{
		int PartSize = TablePatch->Header.Payload - 5;

		// As patches come in, build buffer of data
		if(m_PatchOffset + PartSize <= m_PatchSize)
		{
			memcpy(m_PatchBuffer + m_PatchOffset, (byte*) TablePatch + 28, PartSize);
			m_PatchOffset += PartSize;
		}
		else
		{
			CloseWithReason("Patch Exceeded Specified Size");
			m_pCore->DebugLog("Gnutella", "Table Patch Too Large");
		}
	}

	// Final patch received
	if(TablePatch->SeqNum == TablePatch->SeqSize)
	{
		if(TablePatch->Compression == 0x1)
			m_PatchCompressed = true;

		m_PatchBits = TablePatch->EntryBits;

		m_PatchReady = true;
	}
	else
		m_CurrentSeq++;
	
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
			if(m_pComm->m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER && m_pComm->m_NodeList[i]->m_Status == SOCK_CONNECTED)
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
void CGnuNode::Receive_Unknown(byte* UnkownPacket, DWORD dwLength)
{
	m_pComm->PacketIncoming(m_NodeID, (byte*) UnkownPacket, dwLength, ERROR_UNKNOWN, false);
}


/////////////////////////////////////////////////////////////////////////////
// Sending packets

void CGnuNode::Send_Ping(int TTL)
{
	GUID Guid = GUID_NULL;
	GnuCreateGuid(&Guid);
	if (Guid == GUID_NULL)
		return;

	packet_Ping Ping;
	
	Ping.Header.Guid	 = Guid;
	Ping.Header.Function = 0;
	Ping.Header.Hops	 = 0;
	Ping.Header.TTL		 = TTL;
	Ping.Header.Payload  = 0;

	
	m_pComm->m_TableLocal.Insert(Guid, 0);

	SendPacket(&Ping, 23, PACKET_PING, Ping.Header.Hops);
}

void CGnuNode::Send_Pong(GUID Guid, int nHops)
{
	// Build the packet
	packet_Pong Pong;

	Pong.Header.Guid		= Guid;
	Pong.Header.Function	= 0x01;
	Pong.Header.TTL			= nHops;
	Pong.Header.Hops		= 0;
	Pong.Header.Payload		= 14;
	Pong.Port				= (WORD) m_pNet->m_CurrentPort;
	Pong.Host				=  m_pNet->m_CurrentIP;
	Pong.FileCount			= m_pShare->m_TotalLocalFiles;

	if(m_pPrefs->m_ForcedHost.S_addr)
		Pong.Host = m_pPrefs->m_ForcedHost;


	// If we are an ultrapeer, the size field is used as a marker send that info
	if(m_pComm->m_GnuClientMode == GNU_ULTRAPEER)
		Pong.FileSize = m_pShare->m_UltrapeerSizeMarker;
	else
		Pong.FileSize = m_pShare->m_TotalLocalSize;

	m_pComm->m_TableLocal.Insert(Guid, m_NodeID);

	SendPacket(&Pong, 37, PACKET_PONG, Pong.Header.TTL - 1);
}

void CGnuNode::Send_QueryHit(GnuQuery &FileQuery, byte* pQueryHit, DWORD ReplyLength, byte ReplyCount, CString &MetaTail)
{
	packet_QueryHit*    QueryHit = (packet_QueryHit*)   pQueryHit;
	packet_QueryHitEx*  QHD      = (packet_QueryHitEx*) (pQueryHit + 34 + ReplyLength);


	// Build Query Packet
	int packetLength = 34 + ReplyLength;

	QueryHit->Header.Guid = FileQuery.SearchGuid;
	m_pComm->m_TableLocal.Insert(FileQuery.SearchGuid, m_NodeID);

	packet_Query* pQuery = (packet_Query*) FileQuery.Packet;

	QueryHit->Header.Function = 0x81;
	QueryHit->Header.TTL	  = pQuery->Header.Hops;
	QueryHit->Header.Hops	  = 0;

	QueryHit->TotalHits	= ReplyCount;
	QueryHit->Port		= (WORD) m_pNet->m_CurrentPort;
	QueryHit->Speed		= GetSpeed();
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

	
	// If we are browseing a remote host, send packet to custom buffer
	if(m_BrowseID)
	{
		byte* BrowsePacket = new byte[packetLength];
		memcpy(BrowsePacket, pQueryHit, packetLength);
		
		m_BrowseHits.push_back(BrowsePacket);
		m_BrowseHitSizes.push_back(packetLength);
	}
	else
		SendPacket(pQueryHit, packetLength, PACKET_QUERYHIT, QueryHit->Header.TTL - 1, true);

}

void CGnuNode::Send_ForwardQuery(GnuQuery &FileQuery)
{
	packet_Query* pQuery = (packet_Query*) FileQuery.Packet;
	SendPacket(pQuery, FileQuery.PacketSize, PACKET_QUERY, pQuery->Header.Hops, true);
}

void CGnuNode::Send_Bye(CString Reason)
{
	GUID Guid = GUID_NULL;
	GnuCreateGuid(&Guid);
	if (Guid == GUID_NULL)
		return;
	
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

	SendPacket(PacketData, PacketSize, PACKET_BYE, Bye->Header.Hops);

	delete [] PacketData;
}

void CGnuNode::Send_PatchReset()
{
	GUID Guid = GUID_NULL;
	GnuCreateGuid(&Guid);
	if (Guid == GUID_NULL)
		return;


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

	SendPacket(&Reset, 29, PACKET_QUERY, Reset.Header.Hops);
}

void CGnuNode::Send_PatchTable()
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
	if( m_LocalHitTable == NULL)
	{
		m_LocalHitTable = new byte [GNU_TABLE_SIZE];
		memset( m_LocalHitTable,  0xFF, GNU_TABLE_SIZE );
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
			pos++;

			// No change
			if( (PatchTable[i] & mask) == (m_LocalHitTable[i] & mask) )
			{
				
			}
			// Patch turning on ( set negetive value)
			else if( (PatchTable[i] & mask) == 0 && (m_LocalHitTable[i] & mask) > 0)
			{
				if(pos % 2 == 0) 
					FourBitPatch[pos / 2] = 15 << 4; // high -1
				else
					FourBitPatch[pos / 2] |= 15;  // low -1
			}
			// Patch turning off ( set positive value)
			else if( (PatchTable[i] & mask) > 0 && (m_LocalHitTable[i] & mask) == 0)
			{
				if(pos % 2 == 0)
					FourBitPatch[pos / 2] = 1 << 4;// high 1
				else
					FourBitPatch[pos / 2] |= 1;  // low 1
			}
		}

		m_LocalHitTable[i] = PatchTable[i];
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
		GUID Guid = GUID_NULL;
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
	SendPacket(PacketBuff, NextPos, PACKET_QUERY, PatchPacket->Header.Hops);

	delete [] PacketBuff;
	delete [] CompBuff;
	delete [] RawPacket;
}



bool CGnuNode::GetAlternateHostList(CString &HostList)
{
	// Give 5 hosts from real cache and 5 hosts from perm cache

	int Hosts = 0;
	int Count = 5;

	while(Count > 0 && m_pCache->m_GnuPerm.size())
	{
		int randIndex = rand() % m_pCache->m_GnuPerm.size();

		std::list<Node>::iterator itNode = m_pCache->m_GnuPerm.begin();
		for(int i = 0; itNode != m_pCache->m_GnuPerm.end(); itNode++, i++)
			if(i == randIndex)
			{
				HostList += (*itNode).Host + ":" + NumtoStr((*itNode).Port) + ",";	
				Count--;
				Hosts++;
			}
	}

	// Delete Extra comma
	if(Hosts)
	{
		HostList = HostList.Left(HostList.ReverseFind(','));
		return true;
	}

	return false;
}

bool CGnuNode::GetAlternateSuperList(CString &HostList)
{
	int Hosts = 0;

	for(int i = 0; i < m_pComm->m_NodeList.size() && Hosts < 10; i++)
		if(m_pComm->m_NodeList[i] != this && m_pComm->m_NodeList[i]->m_GnuNodeMode == GNU_ULTRAPEER)
		{
			HostList += m_pComm->m_NodeList[i]->m_HostIP + ":" + NumtoStr(m_pComm->m_NodeList[i]->m_Port) + ",";	
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

void CGnuNode::Refresh()
{
	m_pComm->NodeUpdate(this);

	Send_Ping(MAX_TTL);
}

DWORD CGnuNode::GetSpeed()
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

void CGnuNode::Timer()
{
	// Decrement time till next ReSearch is allowed on this socket
	if(m_NextRequeryWait > 0)
		m_NextRequeryWait--;

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
				Send_PatchTable();

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


		// Normal ping at 45 secs
		// Turned off to prevent avoidtriangles creating connection instability
		/*if(m_GnuNodeMode != GNU_LEAF)
			if(m_IntervalPing == 45)
			{
				AvoidTriangles();

				Send_Ping(2);

				m_IntervalPing = 0;
			}
			else
				m_IntervalPing++;
		*/

		// Drop if not socket gone mute 30 secs
		if(m_dwSecBytes[0] == 0)
		{
			m_SecsDead++;

			if(m_SecsDead == 30)
				Send_Ping(1);

			if(m_SecsDead > 60)
			{
				
				CloseWithReason("Minute Dead");
				return;
			}
		}
		else
			m_SecsDead = 0;


		// Re-Search on new connect after 30 seconds
		if(m_SecsAlive == 15 && m_GnuNodeMode == GNU_ULTRAPEER)
		{
			Send_Ping(MAX_TTL);

			for(int i = 0; i < m_pTrans->m_DownloadList.size(); i++)
				m_pTrans->m_DownloadList[i]->IncomingGnuNode(this);

			for(i = 0; i < m_pNet->m_SearchList.size(); i++)
				m_pNet->m_SearchList[i]->IncomingGnuNode(this);

			m_SecsAlive++;
		}
		else if(m_SecsAlive < 600)
			m_SecsAlive++;


		// Close if we're browsing host and all bytes received
		if(m_BrowseID)
			if(m_BrowseRecvBytes == m_BrowseSize)
				CloseWithReason("Browse Completed");


		// Reset trottle for host if it is a leaf
		m_LeafBytesIn    = 0; 
		m_LeafBytesOut   = 0; 
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
		//m_pCore->DebugLog( m_HostIP + " -> " + CommaIze( NumtoStr(DeflateStream.total_in) ) + " bytes sent compressed at " + CommaIze( NumtoStr(DeflateStream.total_out) ) + " bytes");

		m_ZipStat = 0;
	}
}

void CGnuNode::AvoidTriangles()
{
	int i, j, k;


	// Increment and erase old nodes from vector
	for(i = 0; i < 2; i++)
	{
		std::vector<MapNode>::iterator itItem = NearMap[i].begin();
		while( itItem != NearMap[i].end() )
			if( (*itItem).Age > 3)
			{
				itItem = NearMap[i].erase(itItem);	
			}
			else
			{	
				(*itItem).Age++;
				itItem++;
			}
	}

	
	// Check for collisions with nodes
	bool Active = false;

	for(i = 0; i < m_pComm->m_NodeList.size(); i++)
	{
		CGnuNode* pNode = m_pComm->m_NodeList[i];
	
		// Only check with nodes that are after this node's position in the list
		if(pNode == this)
			Active = true;

		else if(Active && pNode->m_GnuNodeMode != GNU_LEAF)
		{ 
			// Combine vectors and compare

			for(j = 0; j < NearMap[0].size(); j++)
				for(k = 0; k < pNode->NearMap[0].size(); k++)
					if(NearMap[0][j].Host.S_addr == pNode->NearMap[0][k].Host.S_addr && NearMap[0][j].Port == pNode->NearMap[0][k].Port)
					{
						m_pCache->RemoveIP(pNode->m_HostIP, NETWORK_GNUTELLA);
						pNode->CloseWithReason("Avoiding Triangles");
						return;
					}

			for(j = 0; j < NearMap[0].size(); j++)
				for(k = 0; k < pNode->NearMap[1].size(); k++)
					if(NearMap[0][j].Host.S_addr == pNode->NearMap[1][k].Host.S_addr && NearMap[0][j].Port == pNode->NearMap[1][k].Port)
					{
						m_pCache->RemoveIP(pNode->m_HostIP, NETWORK_GNUTELLA);
						pNode->CloseWithReason("Avoiding Triangles");
						return;
					}

			for(j = 0; j < NearMap[1].size(); j++)
				for(k = 0; k < pNode->NearMap[0].size(); k++)
					if(NearMap[1][j].Host.S_addr == pNode->NearMap[0][k].Host.S_addr && NearMap[1][j].Port == pNode->NearMap[0][k].Port)
					{
						m_pCache->RemoveIP(pNode->m_HostIP, NETWORK_GNUTELLA);
						pNode->CloseWithReason("Avoiding Triangles");
						return;
					}

			for(j = 0; j < NearMap[1].size(); j++)
				for(k = 0; k < pNode->NearMap[1].size(); k++)
					if(NearMap[1][j].Host.S_addr == pNode->NearMap[1][k].Host.S_addr && NearMap[1][j].Port == pNode->NearMap[1][k].Port)
					{
						m_pCache->RemoveIP(pNode->m_HostIP, NETWORK_GNUTELLA);
						pNode->CloseWithReason("Avoiding Triangles");
						return;
					}
		}
	}	
}

void CGnuNode::MapPong(packet_Pong* Pong)
{
	int ttl = Pong->Header.Hops - 1;

	for(int i = 0; i < NearMap[ttl].size(); i++)
		if(NearMap[ttl][i].Host.S_addr == Pong->Host.S_addr && NearMap[ttl][i].Port == Pong->Port)
		{
			NearMap[ttl][i].Age = 0;
			return;
		}
	
	MapNode Node;
	Node.Host      = Pong->Host;
	Node.Port	   = Pong->Port;
	Node.FileSize  = Pong->FileSize;
	Node.FileCount = Pong->FileCount;
	Node.Age = 0;

	// For pongs 2 hops away or less
	if(Pong->Header.Hops == 1)
		if(m_Port != Pong->Port)
		{
			m_Port = Pong->Port;
			m_pComm->NodeUpdate(this);
		}

	NearMap[ttl].push_back(Node);

	return;
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

	// Filter out hosts who try to use multiple vendors in user-agent header
	if(lowAgent.Find("gnucleus") != -1 && lowAgent.Find("morpheus") != -1)
		return false;

	return true;
}


