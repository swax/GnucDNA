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

#include "DnaNetwork.h"
#include "DnaCore.h"
#include "DnaEvents.h"

#include "GnuCore.h"
#include "GnuNetworks.h"
#include "GnuCache.h"
#include "GnuPrefs.h"
#include "G2Control.h"
#include "G2Datagram.h"
#include "G2Protocol.h"
#include "G2Node.h"


// CG2Node

CG2Node::CG2Node(CG2Control* pG2Comm, CString Host, uint32 Port)
{
	m_pG2Comm = pG2Comm;
	m_pDispatch = pG2Comm->m_pDispatch;
	m_pProtocol = pG2Comm->m_pProtocol;

	m_pNet    = pG2Comm->m_pNet;
	m_pCore	  = m_pNet->m_pCore;
	m_pCache  = m_pNet->m_pCache;
	m_pPrefs  = pG2Comm->m_pPrefs;

	// Socket
	m_G2NodeID = m_pNet->GetNextNodeID();
	m_pG2Comm->m_G2NodeIDMap[m_G2NodeID] = this;

	m_Address.Host = StrtoIP(Host);
	m_Address.Port = Port;

	m_pG2Comm->m_G2NodeAddrMap[m_Address.Host.S_addr] = this;

	m_Inbound      = false;
	m_TriedUpgrade = false;

	m_ConnectTime = CTime::GetCurrentTime();
	m_RemoteIdent = 0;

	m_Status     = SOCK_CONNECTING;	
	m_StatusText = "Connecting";
	m_NodeMode   = G2_UNKNOWN;

	m_SecsTrying = 0;
	m_SecsDead   = 0;
	m_CloseWait  = 0;

	
	m_SendDelayQHT = false;
	m_SendDelayLNI = false;
	
	m_QHTwait = 60; // Connect for at least 60 before sending qht
	m_LNIwait = 0;
	m_KHLwait = KHL_TIMEOUT_HUB; // Send KHL after 60 secs, allow time for LNI to come in


	// QHT
	m_CurrentPart	   = 1;
	m_PatchReady       = false;
	m_PatchTimeout     = 60;

	m_PatchCompressed  = false;
	m_PatchBits        = 1;
	
	m_PatchBuffer      = NULL;
	m_PatchOffset      = 0;
	m_PatchSize	       = 0;
	m_RemoteTableSize  = 0;

	m_LocalHitTable = NULL;

	// Receiving
	m_ExtraLength  = 0;

	// Sending 
	m_SendBuffLength = 0;

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

	
	// Bandwidth
	for(int i = 0; i < 3; i++)
	{
		m_AvgBytes[i].SetRange(30);

		m_dwSecBytes[i] = 0;
	}



	m_pG2Comm->G2NodeUpdate(this);
}

CG2Node::~CG2Node()
{
	m_Status = SOCK_CLOSED;
	m_pG2Comm->G2NodeUpdate(this);

	
	std::map<int, CG2Node*>::iterator itNode = m_pG2Comm->m_G2NodeIDMap.find(m_G2NodeID);
	if(itNode != m_pG2Comm->m_G2NodeIDMap.end())
		m_pG2Comm->m_G2NodeIDMap.erase(itNode);

	std::map<uint32, CG2Node*>::iterator itAddr = m_pG2Comm->m_G2NodeAddrMap.find( m_Address.Host.S_addr);
	if(itAddr != m_pG2Comm->m_G2NodeAddrMap.end())
		m_pG2Comm->m_G2NodeAddrMap.erase(itAddr);


	std::map<uint32, G2_Route>::iterator itRoute = m_pG2Comm->m_RouteMap.find( HashGuid(m_NodeInfo.NodeID) );
	if(itRoute != m_pG2Comm->m_RouteMap.end())
		itRoute->second.ExpireTime = ROUTE_EXPIRE;

	if( m_PatchBuffer )
		delete [] m_PatchBuffer;

	while( !m_OutboundPackets.empty() )
	{
		delete m_OutboundPackets.back();
		m_OutboundPackets.pop_back();
	}

	m_TransferPacketAccess.Lock();
		
		while( m_TransferPackets.size())
		{
			delete m_TransferPackets.back();
			m_TransferPackets.pop_back();
		}

	m_TransferPacketAccess.Unlock();

	if(m_LocalHitTable)
		delete [] m_LocalHitTable;

	// Clean up compression
	inflateEnd(&InflateStream);
	deflateEnd(&DeflateStream);
}


// CG2Node member functions
void CG2Node::Timer()
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
		// Delayed QHT
		if( m_QHTwait > 0)
			m_QHTwait--;

		if(m_SendDelayQHT && m_QHTwait == 0)
		{
			m_pG2Comm->Send_QHT(this);

			m_SendDelayQHT  = false;
			m_QHTwait       = QHT_TIMEOUT;
		}

		// Delayed LNI
		if( m_LNIwait > 0)
			m_LNIwait--;

		if(m_SendDelayLNI && m_LNIwait == 0)
		{
			m_pG2Comm->Send_LNI(this);

			m_SendDelayLNI  = false;
			m_LNIwait       = LNI_TIMEOUT;
		}

		// Delayed Apply Patch
		if(m_PatchTimeout > 0)
			m_PatchTimeout--;

		if(m_PatchReady)
			if(m_PatchTimeout == 0 || m_NodeMode == G2_HUB) // Hub always able to update, prevent child from taking hub cpu
			{
				m_pG2Comm->ApplyPatchTable(this);

				m_PatchReady   = false;
				m_PatchTimeout = 60;
				m_CurrentPart  = 1;
			}

		if( m_KHLwait == 0 )
		{
			m_pG2Comm->Send_KHL(this);

			if( m_pG2Comm->m_ClientMode == G2_HUB )
				m_KHLwait = KHL_TIMEOUT_HUB;

			if( m_pG2Comm->m_ClientMode == G2_CHILD )
				m_KHLwait = KHL_TIMEOUT_CHILD;
		}
		else if( m_KHLwait > 0)
			m_KHLwait--;


		// Drop if socket stops responding for a minute
		if(m_dwSecBytes[0] == 0)
		{
			m_SecsDead++;

			if(m_SecsDead == 30)
			{
				G2_PI Ping;
				m_pG2Comm->Send_PI(this->m_Address, Ping, this );
				//m_pG2Comm->Send_PI(this->m_Address, Ping); 
			}

			if(m_SecsDead > 60)
			{	
				CloseWithReason("Minute Dead");
				return;
			}
		}


		// Transfer packets from thread to main thread
		m_TransferPacketAccess.Lock();
		
			for( int i = 0; i < m_TransferPackets.size(); i++ )
				SendPacket( m_TransferPackets[i] );
			
			m_TransferPackets.clear();

		m_TransferPacketAccess.Unlock();


		// Flush compressed send buffer
		FlushSendQueue(true);
	}

	else if(m_Status == SOCK_CLOSED)
	{
		m_CloseWait++;
	}

	
	// Bandwidth
	// When i = 0 its receive stats
    //      i = 1 its send stats
	//		i = 2 its dropped stats
	for(int i = 0; i < 3; i++)
	{
		m_AvgBytes[i].Update( m_dwSecBytes[i] );

		m_dwSecBytes[i]   = 0;	
	}
}

void CG2Node::OnConnect(int nErrorCode)
{
	if(nErrorCode)
	{
		m_pCore->LogError("G2Node OnConnect Error " + NumtoStr(nErrorCode));
		return;
	}

	CString Handshake;

	Handshake =  "GNUTELLA CONNECT/0.6\r\n";
		
	// Listen-IP header
	Handshake += "Listen-IP: " + IPtoStr(m_pNet->m_CurrentIP) + ":" + NumtoStr(m_pNet->m_CurrentPort) + "\r\n";

	// Remote-IP header
	Handshake += "Remote-IP: " + IPtoStr(m_Address.Host) + "\r\n";

	// User-Agent header
	Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";

	// Accept header
	Handshake += "Accept: application/x-gnutella2\r\n";

	// Ultrapeer header
	if(m_pG2Comm->m_ClientMode == G2_CHILD)
		Handshake += "X-Ultrapeer: False\r\n";

	else if(m_pG2Comm->m_ClientMode == G2_HUB)
		Handshake += "X-Ultrapeer: True\r\n";

	// Accept-Encoding Header
	if(m_dnapressionOn)
		Handshake += "Accept-Encoding: deflate\r\n";

	// Authentication
	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->NetworkAuthenticate(m_G2NodeID);
	
	if( !m_Challenge.IsEmpty() && !m_ChallengeAnswer.IsEmpty() )
		Handshake += "X-Auth-Challenge: " + m_Challenge + "\r\n";

	Handshake += "\r\n";

	Send(Handshake, Handshake.GetLength());


	// Add to log
	Handshake.Replace("\n\n", "\r\n\r\n");
	m_WholeHandshake += Handshake;

	CAsyncSocket::OnConnect(nErrorCode);
}

void CG2Node::OnReceive(int nErrorCode) 
{
	if(nErrorCode)
	{
		CloseWithReason("G2Node OnReceive Error " + NumtoStr(nErrorCode));
		return;
	}

	int RecvLength = 0;
	
	if(m_Status != SOCK_CONNECTED || !m_InflateRecv)
		RecvLength = Receive(&m_pRecvBuff[m_ExtraLength], G2_PACKET_BUFF - m_ExtraLength);
	else	
		RecvLength = Receive(InflateBuff, G2_ZSTREAM_BUFF);
	

	// Handle Errors
	if(RecvLength <= 0)
	{
		if(RecvLength == SOCKET_ERROR)
		{
			int lastError = GetLastError();
			if(lastError != WSAEWOULDBLOCK)
			{				
				CloseWithReason("G2Node Receive Error " + NumtoStr(lastError));
				return;
			}
		}

		return;
	}


	// Bandwidth stats
	m_dwSecBytes[0] += RecvLength;

	m_SecsDead = 0;


	// Connected to node, sending and receiving packets
	if(m_Status == SOCK_CONNECTED)
	{
		FinishReceive(RecvLength);
	}

	// Still in handshake mode
	else
	{
		CString MoreData((char*) m_pRecvBuff, RecvLength);

		m_InitData += MoreData;


		// Gnutella 0.6 Handshake
		if( m_InitData.Find("\r\n\r\n") != -1)
		{
			if(m_Inbound)
				ParseIncomingHandshake(m_InitData, m_pRecvBuff, RecvLength);
			else
				ParseOutboundHandshake(m_InitData, m_pRecvBuff, RecvLength);
		}
		

		if(m_InitData.GetLength() > 4096)
		{
			m_WholeHandshake += m_InitData;
			CloseWithReason("Handshake Overflow");
		}		
	}


	CAsyncSocket::OnReceive(nErrorCode);

}

void CG2Node::Recv_Close(G2_CLOSE &Close)
{
	for(int i = 0; i < Close.CachedHubs.size(); i++)
	{
		Node tryNode;
		tryNode.Network = NETWORK_G2;
		tryNode.Host = IPtoStr(Close.CachedHubs[i].Host);
		tryNode.Port = Close.CachedHubs[i].Port;
		m_pCache->AddKnown(tryNode);
	}

	CloseWithReason(Close.Reason, true);
}

void CG2Node::Send_Close(CString Reason)
{
	G2_CLOSE Close;
	Close.Reason = Reason;

	// Add cached hubs
	CString TryHeader;
	m_pG2Comm->GetAltHubs(TryHeader, this);
	
	CString Address = ParseString(TryHeader, ',');
	while( !Address.IsEmpty() )
	{
		IPv4 tryNode;
		tryNode.Host = StrtoIP(ParseString(Address, ':'));
		tryNode.Port = atoi(ParseString(Address, ' '));

		Close.CachedHubs.push_back(tryNode);

		Address = ParseString(TryHeader, ',');
	}

	m_pG2Comm->Send_CLOSE(this, Close);
}

void CG2Node::OnClose(int nErrorCode) 
{
	m_Status = SOCK_CLOSED;

	CString Reason = "Closed";
	if(nErrorCode)
		Reason += " #" + NumtoStr(nErrorCode);

	CloseWithReason(Reason, true);

	CAsyncSocket::OnClose(nErrorCode);
}

void CG2Node::CloseWithReason(CString Reason, bool RemoteClosed)
{
	if(m_Status == SOCK_CONNECTED && !RemoteClosed)
	{
		Send_Close(Reason);
		FlushSendQueue(true);
	}

	if(	RemoteClosed )
		m_StatusText = "Remote: " + Reason;
	else
		m_StatusText = "Local: " + Reason;

	Close();
}

void CG2Node::Close() 
{
	if(m_hSocket != INVALID_SOCKET)
	{
		// Clear receive buffer
		int RecvLength = 0;

		do 
		{
			RecvLength = Receive(m_pRecvBuff, G2_PACKET_BUFF); //Receive(&m_pRecvBuff[m_ExtraLength], G2_PACKET_BUFF - m_ExtraLength);

			//if(RecvLength > 0)
			//	SplitBundle(m_pRecvBuff, RecvLength);

		} while(RecvLength > 0);


		// Close socket
		if(m_hSocket != INVALID_SOCKET)
		{
			AsyncSelect(0);
			ShutDown(2);
		}

		CAsyncSocket::Close();
	}

	m_Status = SOCK_CLOSED;
	m_pG2Comm->G2NodeUpdate(this);
}

void CG2Node::ParseOutboundHandshake(CString Data, byte* Stream, int StreamLength)
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

		// Parse Accept header
		CString AcceptHeader = FindHeader("Accept");
		if( AcceptHeader.IsEmpty() || AcceptHeader.Find("application/x-gnutella2") == -1)
		{
			ParseG1TryHeader( FindHeader("X-Try-Ultrapeers") );
			Send_ConnectError("503 Required network not accepted");
			return;
		}

		// Parse Content-Type header
		CString ContentHeader = FindHeader("Content-Type");
		if( ContentHeader.IsEmpty() || ContentHeader.Find("application/x-gnutella2") == -1)
		{
			ParseG1TryHeader( FindHeader("X-Try-Ultrapeers") );
			Send_ConnectError("503 Required network not provided");
			return;
		}

		// Parse Accept-Encoding
		CString EncodingHeader = FindHeader("Accept-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_DeflateSend = true;

		// Parse Content-Encoding
		EncodingHeader = FindHeader("Content-Encoding");
		if(m_dnapressionOn && EncodingHeader == "deflate")
			m_InflateRecv = true;

		// Parse X-Try-Ultrapeers header
		CString UltraTryHeader = FindHeader("X-Try-Ultrapeers");
		if(!UltraTryHeader.IsEmpty())
			ParseTryHeader( UltraTryHeader );

		// Parse X-Try-Hubs header
		CString HubsToTry = FindHeader("X-Try-Hubs");
		if( !HubsToTry.IsEmpty() )
			ParseTryHeader( HubsToTry );

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
				m_pCore->m_dnaCore->m_dnaEvents->NetworkChallenge(m_G2NodeID, m_RemoteChallenge);
		}

		// Parse Ultrapeer header
		CString UltraHeader = FindHeader("X-Ultrapeer");		
		if(!UltraHeader.IsEmpty())
		{
			UltraHeader.MakeLower();

			if(UltraHeader == "true")
				m_NodeMode = G2_HUB;
			else
				m_NodeMode = G2_CHILD;
		}

		
		// Child connecting to hub
		if( m_pG2Comm->m_ClientMode == G2_CHILD && m_NodeMode == G2_HUB)
		{
			if( m_pG2Comm->CountHubConnects() < m_pPrefs->m_G2ChildConnects )
			{
				Send_ConnectOK(true);
				SetConnected();
				return;
			}
			else
			{
				Send_ConnectError("503 Maximum hub connections reached");
				return;
			}
		}

		// Child connecting to child
		else if(m_pG2Comm->m_ClientMode == G2_CHILD && m_NodeMode == G2_CHILD)
		{	
			Send_ConnectError("503 I am in child mode");
			return;
		}
	
		// Hub connecting to hub
		else if(m_pG2Comm->m_ClientMode == G2_HUB && m_NodeMode == G2_HUB)
		{
			if( m_pPrefs->m_G2MaxConnects && m_pG2Comm->CountHubConnects() < m_pPrefs->m_G2MaxConnects )
			{
				Send_ConnectOK(true);
				SetConnected();
				return;
			}
			else
			{
				Send_ConnectError("503 Maximum hub connections reached");
				return;
			}
		}

		// Hub connecting to child
		else if(m_pG2Comm->m_ClientMode == G2_HUB && m_NodeMode == G2_CHILD)
		{
			if( m_pG2Comm->CountChildConnects() < m_pPrefs->m_MaxLeaves)
			{
				m_DeflateSend = false;

				Send_ConnectOK(true);
				SetConnected();
				return;
			}
			else
			{
				Send_ConnectError("503 Maximum child connections reached");
				return;
			}
		}
		
	}

	// Connect failed, 200 OK not received
	else
	{
		// Parse X-Try-Ultrapeers header
		CString UltraTryHeader = FindHeader("X-Try-Ultrapeers");
		if(!UltraTryHeader.IsEmpty())
			ParseTryHeader( UltraTryHeader );

		// Parse X-Try-Hubs header
		CString HubsToTry = FindHeader("X-Try-Hubs");
		if( !HubsToTry.IsEmpty() )
			ParseTryHeader( HubsToTry );

		ParseG1TryHeader( FindHeader("X-Try-Ultrapeers") );


		CString StatusLine = m_Handshake.Left( m_Handshake.Find("\r\n") );
		StatusLine.Replace( "GNUTELLA/0.6 ", "");

		CloseWithReason(StatusLine, true);
		return;
	}
}

void CG2Node::ParseIncomingHandshake(CString Data, byte* Stream, int StreamLength)
{
	m_Handshake = Data.Mid(0, Data.Find("\r\n\r\n") + 4);
	m_WholeHandshake += m_Handshake;

	m_lowHandshake = m_Handshake;
	m_lowHandshake.MakeLower();


	// Connect string, GNUTELLA CONNECT/0.6\r\n
	if(m_Handshake.Find("GNUTELLA CONNECT/") != -1)
	{
		// Parse Remote-IP header
		CString RemoteIP = FindHeader("Remote-IP");
		if(!RemoteIP.IsEmpty())
			m_pNet->m_CurrentIP = StrtoIP(RemoteIP);

		// Parse Listen-IP header
		CString ListenIP = FindHeader("Listen-IP");
		if(!ListenIP.IsEmpty())
		{
			Node G2Host = ListenIP;
			G2Host.Network = NETWORK_G2;
			m_pNet->m_pCache->AddKnown( G2Host );

			m_Address.Port = G2Host.Port;
		}

		// Parse User-Agent header
		m_RemoteAgent = FindHeader("User-Agent");


		// Parse Accept header
		CString AcceptHeader = FindHeader("Accept");
		if( AcceptHeader.IsEmpty() || AcceptHeader.Find("application/x-gnutella2") == -1)
		{
			ParseG1TryHeader( FindHeader("X-Try-Ultrapeers") );
			Send_ConnectError("503 Required network not accepted");
			return;
		}

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
				m_pCore->m_dnaCore->m_dnaEvents->NetworkChallenge(m_G2NodeID, m_RemoteChallenge);
		}
	

		// Authenticate connection
		if(m_pCore->m_dnaCore->m_dnaEvents)
			m_pCore->m_dnaCore->m_dnaEvents->NetworkAuthenticate(m_G2NodeID);



		// Parse X-Try-Ultrapeers header
		CString UltraTryHeader = FindHeader("X-Try-Ultrapeers");
		if(!UltraTryHeader.IsEmpty())
			ParseTryHeader( UltraTryHeader );

		// Parse X-Try-Hubs header
		CString HubsToTry = FindHeader("X-Try-Hubs");
		if( !HubsToTry.IsEmpty() )
			ParseTryHeader( HubsToTry );


		//Parse Ultrapeer header
		CString UltraHeader = FindHeader("X-Ultrapeer");
		if(!UltraHeader.IsEmpty())
		{
			UltraHeader.MakeLower();

			// Connecting client an ultrapeer
			if(UltraHeader == "true")
				m_NodeMode = G2_HUB;
			else
				m_NodeMode = G2_CHILD;
		}


		// Remote Hub connecting to Child
		if( m_pG2Comm->m_ClientMode == G2_CHILD && m_NodeMode == G2_HUB)
		{
			if( m_pG2Comm->CountHubConnects() < m_pPrefs->m_G2ChildConnects)
			{
				Send_ConnectOK(false);
				return;
			}
			else
			{
				Send_ConnectError("503 Maximum hub connections reached");
				return;
			}
		}

		// Remote Child connecting to Child
		else if(m_pG2Comm->m_ClientMode == G2_CHILD && m_NodeMode == G2_CHILD)
		{	
			Send_ConnectError("503 I am in child mode");
			return;
		}
	
		// Remote Hub connecting to Hub
		else if(m_pG2Comm->m_ClientMode == G2_HUB && m_NodeMode == G2_HUB)
		{
			if( m_pPrefs->m_G2MaxConnects && m_pG2Comm->CountHubConnects() < m_pPrefs->m_G2MaxConnects )
			{
				Send_ConnectOK(false);
				return;
			}
			else
			{
				Send_ConnectError("503 Maximum hub connections reached");
				return;
			}
		}

		// Remote Child connecting to Hub
		else if(m_pG2Comm->m_ClientMode == G2_HUB && m_NodeMode == G2_CHILD)
		{
			if( m_pG2Comm->CountChildConnects() < m_pPrefs->m_MaxLeaves)
			{
				m_DeflateSend = false;

				Send_ConnectOK(false);
				return;
			}
			else
			{
				Send_ConnectError("503 Maximum child connections reached");
				return;
			}
		}	

		// No NodeMode Specified
		else
		{
			Send_ConnectError("503 No Hub Mode Specified");
			return;
		}
	}

	else if(m_Handshake.Find(" 200 OK\r\n") != -1)
	{
		// Parse Content-Type header
		CString ContentHeader = FindHeader("Content-Type");
		if( ContentHeader.IsEmpty() || ContentHeader.Find("application/x-gnutella2") == -1)
		{
			ParseG1TryHeader( FindHeader("X-Try-Ultrapeers") );
			Send_ConnectError("503 Required network not provided");
			return;
		}

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

		SetConnected();


		// Stream begins
		for(int i = 0; i < StreamLength - 4; i++)
			if(strncmp((char*) &Stream[i], "\r\n\r\n", 4) == 0)
			{
				int BuffLength = StreamLength - (i + 4);

				if(m_InflateRecv)
					memcpy(InflateBuff, &Stream[i + 4], BuffLength);
				else	
					memmove(m_pRecvBuff, &Stream[i + 4], BuffLength);

				FinishReceive(BuffLength);

				break;
			}

		return;
	}

	// Error string
	else
	{
		// Parse X-Try-Ultrapeers header
		CString UltraTryHeader = FindHeader("X-Try-Ultrapeers");
		if(!UltraTryHeader.IsEmpty())
			ParseTryHeader( UltraTryHeader );

		// Parse X-Try-Hubs header
		CString HubsToTry = FindHeader("X-Try-Hubs");
		if( !HubsToTry.IsEmpty() )
			ParseTryHeader( HubsToTry );

		ParseG1TryHeader( FindHeader("X-Try-Ultrapeers") );


		CString StatusLine = m_Handshake.Left( m_Handshake.Find("\r\n") );
		StatusLine.Replace( "GNUTELLA/0.6 ", "");

		
		CloseWithReason(StatusLine, true);
		return;
	}

}

void CG2Node::Send_ConnectOK(bool Reply)
{
	CString Handshake;

	// Reply to CONNECT OK
	if(Reply)
	{
		Handshake = "GNUTELLA/0.6 200 OK\r\n";

		// Ultrapeer header
		if(m_pG2Comm->m_ClientMode == G2_HUB)
			Handshake += "X-Ultrapeer: True\r\n";

		else if(m_pG2Comm->m_ClientMode == G2_CHILD)
			Handshake += "X-Ultrapeer: False\r\n";

		// Content-Type header
		//Handshake += "Accept: application/x-gnutella2\r\n"; // Some shareaza bug
		Handshake += "Content-Type: application/x-gnutella2\r\n";

		// If remote host accepts deflate encoding
		if(m_DeflateSend)
		{
			//Handshake += "Accept-Encoding: deflate\r\n"; // Some shareaza bug
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
		Handshake =  "GNUTELLA/0.6 200 OK\r\n";
		
		// Listen-IP header
		Handshake += "Listen-IP: " + IPtoStr(m_pNet->m_CurrentIP) + ":" + NumtoStr(m_pNet->m_CurrentPort) + "\r\n";

		// Remote IP header
		Handshake += "Remote-IP: " + IPtoStr(m_Address.Host) + "\r\n";

		// User-Agent header
		Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";
		
		// Content-Type header
		Handshake += "Content-Type: application/x-gnutella2\r\n";
		
		// Accept header
		Handshake += "Accept: application/x-gnutella2\r\n";

		// Ultrapeer header
		if(m_pG2Comm->m_ClientMode == G2_HUB)
			Handshake += "X-Ultrapeer: True\r\n";

		else if(m_pG2Comm->m_ClientMode == G2_CHILD)
			Handshake += "X-Ultrapeer: False\r\n";
		
		// Compression headers
		Handshake += "Accept-Encoding: deflate\r\n";
		if(m_DeflateSend)
			Handshake += "Content-Encoding: deflate\r\n";

		// Send authentication response
		if( !m_RemoteChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Response: " + m_RemoteChallengeAnswer + "\r\n";

		// Send authentication challenge
		if( !m_Challenge.IsEmpty() && !m_ChallengeAnswer.IsEmpty() )
			Handshake += "X-Auth-Challenge: " + m_Challenge + "\r\n";


		// X-Try-Ultrapeers header
		CString HubsToTry;
		if(m_pG2Comm->GetAltHubs(HubsToTry, this))
			Handshake += "X-Try-Ultrapeers: " + HubsToTry + "\r\n";	

		Handshake += "\r\n";
	}


	Send(Handshake, Handshake.GetLength());
	
	Handshake.Replace("\n\n", "\r\n");
	m_WholeHandshake += Handshake;
}

void CG2Node::Send_ConnectError(CString Reason)
{
	CString Handshake;

	Handshake =  "GNUTELLA/0.6 " + Reason + "\r\n";
	
	// Remote-IP header
	Handshake += "Remote-IP: " + IPtoStr(m_Address.Host) + "\r\n";

	Handshake += "User-Agent: " + m_pCore->GetUserAgent() + "\r\n";

	// X-Try-Ultrapeers header
	CString HubsToTry;
	if(m_pG2Comm->GetAltHubs(HubsToTry, this))
		Handshake += "X-Try-Ultrapeers: " + HubsToTry + "\r\n";

	Handshake += "\r\n";

	Send(Handshake, Handshake.GetLength());
	
	Handshake.Replace("\n\n", "\r\n");
	m_WholeHandshake += Handshake;

	CloseWithReason(Reason);
}

CString CG2Node::FindHeader(CString Name)
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

void CG2Node::ParseTryHeader(CString TryHeader)
{
	// 1.2.3.4:6346 2003-03-25T23:59Z,
	CString Address = ParseString(TryHeader, ',');

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

		// Add hubs to global cache
		G2NodeInfo Hub;
		Hub.Address.Host = StrtoIP(tryNode.Host);
		Hub.Address.Port = tryNode.Port;
		m_pG2Comm->UpdateGlobal( Hub );
		
		/*if(m_pNet->m_UdpFirewall == UDP_BLOCK)
		{
			G2_PI Ping;
			m_pG2Comm->Send_PI(Hub.Address, Ping); // Fix this
		}*/

		Address = ParseString(TryHeader, ',');
	}

	// This host responds with more hosts, put on Perm list
	if( !m_Inbound)
		m_pCache->AddWorking( Node( IPtoStr(m_Address.Host), m_Address.Port, NETWORK_G2, CTime::GetCurrentTime() ) );

}

void CG2Node::ParseG1TryHeader(CString TryHeader)
{
	// 1.2.3.4:6346,
	CString Address = ParseString(TryHeader, ',');

	while( !Address.IsEmpty() )
	{
		Node tryNode;
		tryNode.Host    = ParseString(Address, ':');
		tryNode.Port    = atoi(ParseString(Address, ' '));

		m_pCache->AddKnown(tryNode);

		Address = ParseString(TryHeader, ',');
	}
}

void CG2Node::SetConnected()
{
	m_Status = SOCK_CONNECTED;
	m_StatusText = "Connected";
	m_pG2Comm->G2NodeUpdate(this);

	// Setup inflate if remote host supports it
	if(m_InflateRecv)
	{
		if(inflateInit(&InflateStream) != Z_OK)
		{
			CloseWithReason("InflateInit Failed");
			return;
		}
	
		InflateStream.avail_in  = 0;
		InflateStream.next_out  = m_pRecvBuff;
		InflateStream.avail_out = G2_PACKET_BUFF;
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
		DeflateStream.next_out  = m_SendBuff;
		DeflateStream.avail_out = G2_PACKET_BUFF;
	}

	// Send TCP and UDP ping
	G2_PI Ping;
	Ping.Ident = m_pG2Comm->m_ClientIdent;
	m_pG2Comm->Send_PI(this->m_Address, Ping, this); 
	//m_pG2Comm->Send_PI(this->m_Address, Ping); 
	
	// Tests ability to receive udp
	if(m_pNet->m_UdpFirewall != UDP_FULL)
	{
		Ping.UdpAddress.Host = m_pNet->m_CurrentIP;
		Ping.UdpAddress.Port = m_pNet->m_CurrentPort;

		if(m_pNet->m_TcpFirewall)
			Ping.TestFirewall = true;

		m_pG2Comm->Send_PI(this->m_Address, Ping, this);
	}

	m_SendDelayLNI = true;

	if(m_NodeMode == G2_HUB)
	{
		G2NodeInfo Hub;
		Hub.Address = m_Address;

		m_pG2Comm->UpdateGlobal( Hub );

		// Send current queries out
		std::list<G2_Search*>::iterator itSearch;
		for(itSearch = m_pG2Comm->m_G2Searches.begin(); itSearch != m_pG2Comm->m_G2Searches.end(); itSearch++)
		{
			m_pG2Comm->Send_Q2(NULL, (*itSearch)->Query, this);
			(*itSearch)->TriedHubs[m_Address.Host.S_addr] = true;
		}
	}

	memset( m_RemoteHitTable, 0xFF, G2_TABLE_SIZE );
	

	if( m_NodeMode == G2_HUB )
	{
		m_pG2Comm->Send_QHT(this, true); // Send reset

		m_SendDelayQHT  = true; // Set to send patch
	}

	// Update child counts with all connections
	if( m_pG2Comm->m_ClientMode == G2_HUB && m_NodeMode == G2_CHILD )
		for( int i = 0; i < m_pG2Comm->m_G2NodeList.size(); i++)
			m_pG2Comm->m_G2NodeList[i]->m_SendDelayLNI = true;
}

void CG2Node::FinishReceive(int RecvLength)
{
	if(m_InflateRecv)
	{
		InflateStream.next_in  = InflateBuff;
		InflateStream.avail_in = RecvLength;

		while (InflateStream.avail_in != 0) 
		{
			RecvLength = 0;
			int BuffAvail = G2_PACKET_BUFF - m_ExtraLength;

			InflateStream.next_out  = &m_pRecvBuff[m_ExtraLength];
			InflateStream.avail_out = BuffAvail;
			
			if( inflate(&InflateStream, Z_NO_FLUSH) < 0)
			{
				//ASSERT(0);
				CloseWithReason("Inflate Error");
				return;
			}

			if(InflateStream.avail_out < BuffAvail) 
			{ 
				uint32 BuffLength = (BuffAvail - InflateStream.avail_out) + m_ExtraLength;
				SplitPackets(m_pRecvBuff, BuffLength);
			}
		}
	}
	else
	{
		uint32 BuffLength = RecvLength + m_ExtraLength;
		SplitPackets(m_pRecvBuff, BuffLength);
	}
}

void CG2Node::SplitPackets(byte* buffer, uint32 length)
{
	m_ExtraLength = 0;

	G2ReadResult streamStatus = PACKET_GOOD;

	while( streamStatus == PACKET_GOOD && m_Status == SOCK_CONNECTED)
	{
		G2_Header undefPacket;
		streamStatus = m_pProtocol->ReadNextPacket( undefPacket, buffer, length );

		if( streamStatus != PACKET_GOOD )
			break;
		
		G2_RecvdPacket Packet(m_Address, undefPacket, this);
		m_pG2Comm->ReceivePacket( Packet );
	}

	if( streamStatus == PACKET_INCOMPLETE && length > 0)
	{
		memmove(m_pRecvBuff, buffer, length);
		m_ExtraLength = length;
	}

	if( streamStatus == PACKET_ERROR )
	{
		CloseWithReason("Packet Error");

		// TCP Packet Error
		m_pG2Comm->m_pCore->DebugLog("G2 Network", "TCP Packet Error: " + HexDump(buffer, length));
		return;

		return;
	}

	if ( streamStatus == STREAM_END ) 
	{
		CloseWithReason("Stream Ended", true);
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////


void CG2Node::SendPacket(byte* packet, uint32 length, bool thread)
{
	ASSERT(packet && length);
	ASSERT(length < 65536);

	if(length == 0 || length > 65536 || packet == NULL)
		return;

	// packet is in CG2Protocol m_FinalPacket so it needs to be copied
	QueuedPacket* OutPacket = new QueuedPacket(packet, length);

	if( thread )
	{
		m_TransferPacketAccess.Lock();
			m_TransferPackets.push_back( OutPacket );
		m_TransferPacketAccess.Unlock();
	}
	else
		SendPacket( OutPacket );
}
void CG2Node::SendPacket(QueuedPacket* OutPacket)
{
	while(m_OutboundPackets.size() > 100)
	{
		delete m_OutboundPackets.back();
		m_OutboundPackets.pop_back();
	}

	m_OutboundPackets.push_front( OutPacket );

	FlushSendQueue();
}

void CG2Node::FlushSendQueue(bool FullFlush)
{
	if(m_Status != SOCK_CONNECTED)
		return;

	
	// If full flush, try to get out of internal gzip buffer anything left
	if(FullFlush && m_DeflateSend)
	{
		int BuffSize = G2_PACKET_BUFF - m_SendBuffLength;

		DeflateStream.next_out  = m_SendBuff + m_SendBuffLength;
		DeflateStream.avail_out = BuffSize;

		int stat = deflate(&DeflateStream, Z_SYNC_FLUSH);
		if( stat < 0)
		{
			// No progress possible keep going
			if(stat != Z_BUF_ERROR)
			{
				ASSERT(0);
				CloseWithReason("Deflate Error " + NumtoStr(stat));
				return;
			}
		}
	

		if(DeflateStream.avail_out < BuffSize) 
			m_SendBuffLength += BuffSize - DeflateStream.avail_out;
	}



	// Send back buffer
	while(m_SendBuffLength > 0)
	{
		int BytesSent = Send(m_SendBuff, m_SendBuffLength);

		if(BytesSent < m_SendBuffLength)
		{
			if(BytesSent == SOCKET_ERROR)
			{
				int lastError = GetLastError();
				if(lastError != WSAEWOULDBLOCK)
					CloseWithReason("Send Buffer Error " + NumtoStr(lastError), true);

				return;
			}

			memmove(m_SendBuff, m_SendBuff + BytesSent, m_SendBuffLength - BytesSent);
			m_SendBuffLength -= BytesSent;

			return;
		}
		else
			m_SendBuffLength = 0;
	}

	// while list not empty
	while( !m_OutboundPackets.empty() )
	{
		QueuedPacket* FrontPacket = m_OutboundPackets.front();

		if(m_pCore->m_dnaCore->m_dnaEvents)
			m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketOutgoing(NETWORK_G2, true , m_Address.Host.S_addr, m_Address.Port, FrontPacket->m_Packet, FrontPacket->m_Length, false);

		// Compress Packet
		if( m_DeflateSend )
		{
			DeflateStream.next_in  = FrontPacket->m_Packet;
			DeflateStream.avail_in = FrontPacket->m_Length;

			DeflateStream.next_out  = m_SendBuff;
			DeflateStream.avail_out = G2_PACKET_BUFF;
			
			int flushMode = Z_NO_FLUSH;
			m_DeflateStreamSize += FrontPacket->m_Length;
			
			if(m_DeflateStreamSize > 4096) // Flush buffer every 4kb
			{
				flushMode = Z_SYNC_FLUSH;
				m_DeflateStreamSize = 0;
			}

			while(DeflateStream.avail_in != 0) 
			{
				int stat = deflate(&DeflateStream, flushMode);
				if( stat < 0)
				{
					ASSERT(0);
					CloseWithReason("Deflate Error " + NumtoStr(stat));
					return;
				}
			}

			if(DeflateStream.avail_out < G2_PACKET_BUFF) 
				m_SendBuffLength = G2_PACKET_BUFF - DeflateStream.avail_out;
		}
		else
		{
			memcpy(m_SendBuff, FrontPacket->m_Packet, FrontPacket->m_Length);
			m_SendBuffLength = FrontPacket->m_Length;
		}


		// Send packet
		bool SendFinished = false;

		ASSERT(m_SendBuffLength >= 0);
		if(m_SendBuffLength > 0)
		{
			int BytesSent = Send(m_SendBuff, m_SendBuffLength);
			
			// If send fails, copy to back buffer so it is the first to be sent
			if(BytesSent < m_SendBuffLength)
			{
				if(BytesSent == SOCKET_ERROR)
				{
					int lastError = GetLastError();
					if(lastError != WSAEWOULDBLOCK)
						CloseWithReason("Send Error " + NumtoStr(lastError), true);
				}
				else
				{
					m_dwSecBytes[2] += m_SendBuffLength - BytesSent;

					memmove(m_SendBuff, m_SendBuff + BytesSent, m_SendBuffLength - BytesSent);
					m_SendBuffLength -= BytesSent;
				}

				SendFinished = true;
			}
			else
				m_SendBuffLength = 0;
		}
		

		// Delete packet once sent
		delete FrontPacket;
		FrontPacket = NULL;

		m_OutboundPackets.pop_front();

		if(SendFinished)
			return;
	}	
}

void CG2Node::OnSend(int nErrorCode)
{
	FlushSendQueue();

	CAsyncSocket::OnSend(nErrorCode);
}


