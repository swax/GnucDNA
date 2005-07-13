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
#include "GnuNetworks.h"
#include "UdpListener.h"
#include "ReliableSocket.h"


CReliableSocket::CReliableSocket(CSocketEvents* pEvents)
{
	m_pEventSink = pEvents;

	m_RudpMode			= false;
	m_RudpSendBlock	    = true;
	m_State				= RUDP_NONE;
	m_ConnectTimeout	= 0;
	m_LastError			= 0;

	m_SynAckReceieved = false;
	m_SynAckSent      = false;

	m_PeerID	   = 0;
	m_RemotePeerID = 0;
	
	m_CurrentSeq	  = 0;
	m_HighestSeqRecvd = 0;
	m_NextSeq		  = 0;
	m_RecvBuffLength  = 0;
	m_SendBuffLength  = 0;
	m_SendWindowSize  = 5;

	m_AvgLatency.SetSize(10);
	m_AvgLatency.Input(500);

	//m_RTT	  = 1000;
	//m_CalcRTT = false;

	m_InOrderAcks = 0;
	m_ReTransmits = 0;

	m_AvgBytesSent.SetSize(10);

	m_pNet = NULL;

	m_LastRecv = time(NULL);
	m_LastSend = time(NULL);
}

CReliableSocket::~CReliableSocket(void)
{
	std::map<uint16, RudpSendPacket* >::iterator itPacket;

	for(itPacket = m_SendPacketMap.begin(); itPacket != m_SendPacketMap.end(); itPacket++)
		delete itPacket->second;

	if( !m_RudpMode || m_pNet->m_pUdpSock == NULL)
		return;

	std::multimap<uint32, CReliableSocket*>::iterator itSocket = m_pNet->m_pUdpSock->m_TransferMap.find(m_Address.Host.S_addr);

	while(itSocket != m_pNet->m_pUdpSock->m_TransferMap.end() && itSocket->first == m_Address.Host.S_addr)
	{
		if(itSocket->second == this)
			itSocket = m_pNet->m_pUdpSock->m_TransferMap.erase(itSocket);
		else
			itSocket++;
	}
}

///////////////// TCP Functions /////////////////


BOOL CReliableSocket::Attach( SOCKET hSocket)
{
	return CAsyncSocketEx::Attach(hSocket);
}

BOOL CReliableSocket::Create()
{
	if(m_SocketData.hSocket != INVALID_SOCKET) // used by download create
		return true;

	return CAsyncSocketEx::Create();
}

BOOL CReliableSocket::Connect(LPCTSTR lpszHostAddress, UINT nHostPort)
{
	return CAsyncSocketEx::Connect(lpszHostAddress, nHostPort);
}

void CReliableSocket::OnAccept(int nErrorCode)
{
	m_pEventSink->OnAccept(nErrorCode);
}

void CReliableSocket::OnConnect(int nErrorCode)
{
	m_pEventSink->OnConnect(nErrorCode);
}

void CReliableSocket::OnReceive(int nErrorCode)
{
	m_pEventSink->OnReceive(nErrorCode);
}

void CReliableSocket::OnSend(int nErrorCode)
{
	m_pEventSink->OnSend(nErrorCode);
}

void CReliableSocket::OnClose(int nErrorCode)
{
	m_pEventSink->OnClose(nErrorCode);
}

///////////////// TCP / UDP Functions /////////////////


int CReliableSocket::Receive(void* lpBuf, int nBufLen)
{
	if( !m_RudpMode )
		return CAsyncSocketEx::Receive(lpBuf, nBufLen);


	if(nBufLen == 0)
		return 0;

	if(m_RecvBuffLength > nBufLen)
		return FinishReceive(lpBuf, nBufLen);
		
	// copy data from packets
	std::map<uint16, std::pair<byte*,int> >::iterator itPacket = m_RecvPacketMap.begin();

	// deal with reading in order at 0xFFFF to zero boundry
	if(m_NextSeq > 0xFFFF - 25) 
		while(itPacket != m_RecvPacketMap.end() && itPacket->first < 25)
			itPacket++; // set beginning in higher than 0xFFFF - 25 range

	// while next element of map equals next in sequence
	while(itPacket != m_RecvPacketMap.end() && itPacket->first == m_NextSeq )
	{
		RudpConnMsg* pConn = (RudpConnMsg*) itPacket->second.first;

		if(pConn->OpCode == OP_DATA)
		{
			RudpDataMsg*   pData   = (RudpDataMsg*) itPacket->second.first;
			packet_Header* pHeader = (packet_Header*) pConn;

			int DataSize = pData->DataSize + pHeader->Payload;

			if(DataSize > MAX_CHUNK_SIZE)
			{
				Log("Too Large Packet Received Size " + NumtoStr(pHeader->Payload) + ", Type 3");
				RudpClose(LARGE_PACKET);
				return SOCKET_ERROR;
			}
					
			// copy data from rudp header
			memcpy(m_RecvBuff + m_RecvBuffLength, pData->Data, pData->DataSize);
			m_RecvBuffLength += pData->DataSize;

			// copy data from gnutella payload
			memcpy(m_RecvBuff + m_RecvBuffLength, ((byte*) pHeader) + 23, pHeader->Payload);
			m_RecvBuffLength += pHeader->Payload;

			Log("Data Recv, Seq " + NumtoStr(pData->SeqNum) + ", ID " + NumtoStr(pData->PeerConnID));
			//Log("Data Recv Buff, Size " + NumtoStr(m_RecvBuffLength));
		}
		else
			break;

		m_HighestSeqRecvd = pConn->SeqNum;
	
		delete [] itPacket->second.first;
		itPacket = m_RecvPacketMap.erase(itPacket);
		m_NextSeq++;

		if(m_RecvBuffLength > nBufLen)
			return FinishReceive(lpBuf, nBufLen);
	}

	//Log("Reliable Receive " + NumtoStr(copysize) + ", " + NumtoStr(m_RecvBuffLength) + " left");

	return FinishReceive(lpBuf, nBufLen);
}

int CReliableSocket::FinishReceive(void* lpBuf, int nBufLen)
{		
	// copy extra data from recv buffer
	int copysize = (m_RecvBuffLength > nBufLen) ? nBufLen : m_RecvBuffLength;
	memcpy(lpBuf, m_RecvBuff, copysize);

	if(copysize != m_RecvBuffLength)
		memmove(m_RecvBuff, m_RecvBuff + copysize, m_RecvBuffLength - copysize);

	m_RecvBuffLength -= copysize;
	
	return copysize;
}

int CReliableSocket::Send(const void* lpBuf, int nBufLen)
{
	if(m_RudpMode)
	{
		if(m_pNet->m_pUdpSock == NULL)
			return SOCKET_ERROR;

		// multiplied by 2 so room to expand and basically 2 second buffer
		int MaxBufferSize = m_AvgBytesSent.GetAverage() * 2; 

		MaxBufferSize = MaxBufferSize < 4096 ? 4096 : MaxBufferSize;
		MaxBufferSize = MaxBufferSize > SEND_BUFFER_SIZE ? SEND_BUFFER_SIZE : MaxBufferSize;
		
		//int MaxBufferSize = SEND_BUFFER_SIZE;//m_SendWindowSize * CHUNK_SIZE;

		if(m_SendBuffLength >= MaxBufferSize)
		{
			m_RudpSendBlock = true;
			m_LastError     = WSAEWOULDBLOCK;
			return SOCKET_ERROR;
		}

		m_SendSection.Lock();

			int buffspace = MaxBufferSize - m_SendBuffLength;
			int copysize  = buffspace >= nBufLen ? nBufLen : buffspace;

			memcpy(m_SendBuff + m_SendBuffLength, lpBuf, copysize);
			m_SendBuffLength  += copysize;

		m_SendSection.Unlock();

		return copysize;
	}

	return CAsyncSocketEx::Send(lpBuf, nBufLen, 0);
}

void CReliableSocket::Close()
{
	if(m_RudpMode)
	{
		if(m_State == RUDP_CLOSED)
			return;

		SendFin(NORMAL_CLOSE);
		m_State = RUDP_CLOSED;

		return;
	}

	if(m_SocketData.hSocket != INVALID_SOCKET)
	{
		AsyncSelect(0);
		ShutDown(2);	

		CAsyncSocketEx::Close();
	}
}

BOOL CReliableSocket::GetPeerName(CString& rPeerAddress, UINT& rPeerPort)
{
	if(m_RudpMode)
	{
		rPeerAddress = IPtoStr(m_Address.Host);
		rPeerPort    = m_Address.Port;
		return true;
	}

	return CAsyncSocketEx::GetPeerName(rPeerAddress, rPeerPort);
}

int CReliableSocket::GetLastError()
{
	if(m_RudpMode)
		return m_LastError;

	return CAsyncSocketEx::GetLastError();
}

///////////////// UDP Functions /////////////////

void CReliableSocket::RudpConnect(IPv4 Address, CGnuNetworks* pNet, bool Listening)
{
	m_RudpMode = true;
	m_pNet     = pNet;
	
	m_Address   = Address;
	m_Listening = Listening;
	m_PeerID    = rand() % RAND_MAX + 1;

	m_State = RUDP_CONNECTING;
	m_ConnectTimeout = time(NULL) + 20;

	if(m_pNet->m_pUdpSock == NULL)
	{
		OnConnect(RUDP_CLOSED);
		return;
	}

	// tell udp socket to forward packets to us
	m_pNet->m_pUdpSock->m_TransferMap.insert( std::pair<uint32,CReliableSocket*> (Address.Host.S_addr, this));

	Log("Connecting");

	SendSyn();
}

void CReliableSocket::RudpReceive(Gnu_RecvdPacket &Packet)
{
	if( !m_RudpMode )
		return;

	RudpConnMsg* pConn = (RudpConnMsg*) Packet.Header;
	
	// check if packet meant for another socket
	if(pConn->PeerConnID != 0 && pConn->PeerConnID != m_PeerID)
		return;	

	pConn->SeqNum = ntohs(pConn->SeqNum); // switch to little endian for host

	//Log("Packet Recv ID " + NumtoStr(pConn->PeerConnID) + ", Type " + NumtoStr(pConn->OpCode) + ", Seq " + NumtoStr(pConn->SeqNum));

	// check for errors
	CString Error;

	if(pConn->SeqNum > m_HighestSeqRecvd && m_State == RUDP_CLOSED) // accept only packets that came before the fin
		Error = "Packet Received while in Close State ID " + NumtoStr(pConn->PeerConnID) + ", Type " + NumtoStr(pConn->OpCode);
		
	else if(pConn->PeerConnID == 0 && m_State != RUDP_CONNECTING)
		Error = "PeerID 0 Receive while not in connecting state";

	else if(Packet.Length > 4096)
	{
		Error = "Too Large Packet Received Size " + NumtoStr(Packet.Length) + ", Type " + NumtoStr(pConn->OpCode);
		RudpClose(LARGE_PACKET);
		return;
	}

	if( !Error.IsEmpty() )
	{
		Log(Error);
		return;
	}

	// try to clear up bufffer, helps if full, better than calling this on each return statement
	ManageRecvWindow();

	
	// if ACK or KEEPALIVE
	
	m_LastRecv = time(NULL);

	switch(pConn->OpCode)
	{
	case OP_ACK:
		ReceiveAck((RudpAckMsg*) pConn);
		return;
	case OP_KEEPALIVE:
		Log("Keep Alive Recv, Seq " + NumtoStr(pConn->SeqNum) + ", ID " + NumtoStr(pConn->PeerConnID));
		// m_LastRecv set, job done
		return;
	}


	// if SYN, DATA or FIN packet

	// stop acking so remote host catches up
	if(pConn->SeqNum > m_HighestSeqRecvd + 25 || m_RecvPacketMap.size() > MAX_WINDOW_SIZE)
	{
		Log("Error Packet Overflow");
		return;
	}

	// Send Ack - cant combine if statements doesnt work
	if(m_AckMap.find(pConn->SeqNum) != m_AckMap.end())
	{
		Log("Error Packet Seq " + NumtoStr(pConn->SeqNum) + " Already Received");
		SendAck(pConn);
		return;
	}
	
	// create new memory for packet
	byte* bPacket = new byte[Packet.Length];
	memcpy(bPacket, Packet.Header, Packet.Length);

	// insert into recv map
	m_RecvPacketMap[pConn->SeqNum] = std::pair<byte*,int> (bPacket, Packet.Length);

	ManageRecvWindow();

	// ack down here so highest received is iterated
	if(pConn->OpCode != OP_SYN)
		SendAck(pConn);
}

void CReliableSocket::ManageRecvWindow()
{
	std::map<uint16, std::pair<byte*,int> >::iterator itPacket = m_RecvPacketMap.begin();

	// deal with reading in order at 0xFFFF to zero boundry
	if(m_NextSeq > 0xFFFF - 25) 
		while(itPacket != m_RecvPacketMap.end() && itPacket->first < 25)
			itPacket++; // set beginning in higher than 0xFFFF - 25 range

	bool DataReceived = false;

	// while next element of map equals next in sequence
	while(itPacket != m_RecvPacketMap.end() && itPacket->first == m_NextSeq )
	{
		RudpConnMsg* pConn = (RudpConnMsg*) itPacket->second.first;

		if(pConn->OpCode == OP_SYN)
			ReceiveSyn((RudpSynMsg*) pConn);
		
		else if(pConn->OpCode == OP_DATA)
		{	
			/*if( !ReceiveData((RudpDataMsg*) pConn) )
				break;*/

			DataReceived = true;
			break;
		}

		else if(pConn->OpCode == OP_FIN)
			ReceiveFin((RudpFinMsg*) pConn);

		m_HighestSeqRecvd = pConn->SeqNum;
		
		delete [] itPacket->second.first;
		
		itPacket = m_RecvPacketMap.erase(itPacket);
		m_NextSeq++;
	}


	// if data waiting to be read
	if(m_RecvBuffLength || DataReceived)
		OnReceive(0);
}

void CReliableSocket::ManageSendWindow()
{
	int Outstanding = 0;
	
	std::vector<RudpSendPacket*> ReTransmit;

	int RTT = m_AvgLatency.GetAverage();


	// go through packet list
	std::map<uint16, RudpSendPacket* >::iterator itPacket;
	for(itPacket = m_SendPacketMap.begin(); itPacket != m_SendPacketMap.end(); itPacket++)
	{
		// acked only set if out of order, otherwise should already be removed
		if(itPacket->second->Acked)
			continue;

		// connecting so must be a syn packet
		if(m_State == RUDP_CONNECTING)
		{
			if(GetTickCount() - itPacket->second->TimeSent > 1000)
				ReTransmit.push_back(itPacket->second);
			
			continue;
		}

		// connected sending data packets

		// check if any re-sends needed
		else if(GetTickCount() > itPacket->second->TimeSent + RTT * 2)
		{	
			/*if(itPacket->second->Retries >= 5)
			{
				SendFin(TOO_MANY_RESENDS);
				m_State = RUDP_CLOSED;
				OnClose(WSAEHOSTDOWN);
				return;
			}*/

			//if(m_SendWindowSize > 1)
			//	m_SendWindowSize--;
				
			ReTransmit.push_back(itPacket->second);
		}

		// else mark as outstanding
		else
			Outstanding++;
	}

	// re-send packets
	for(int i = 0; i < ReTransmit.size() && Outstanding < m_SendWindowSize; i++)
	{
		RudpConnMsg* pConn = (RudpConnMsg*) ReTransmit[i]->Packet;
		Log("Re-Send ID " + NumtoStr(pConn->PeerConnID) + ", Type " + NumtoStr(pConn->OpCode) + ", Seq " + NumtoStr(ntohs(pConn->SeqNum)) + ", Retries " + NumtoStr(ReTransmit[i]->Retries) + ", Passed " + NumtoStr((uint32)(GetTickCount() - ReTransmit[i]->TimeSent)) + " ms");

		ReTransmit[i]->Retries++;
		ReTransmit[i]->TimeSent = GetTickCount();
		
		m_ReTransmits++;
		RudpSend(ReTransmit[i]->Packet, ReTransmit[i]->Size);
		Outstanding++;
	}

	m_SendSection.Lock();

		// send number of packets so that outstanding equals window size
		while(Outstanding < m_SendWindowSize && m_SendBuffLength > 0 && m_SendPacketMap.size() < MAX_WINDOW_SIZE)
		{
			int nBufLen = (m_SendBuffLength > CHUNK_SIZE) ? CHUNK_SIZE : m_SendBuffLength;

			int packetSize = 23;
			
			// 12 bytes go inside header so subtract from payload
			if(nBufLen > 12)
				packetSize += nBufLen - 12;

			RudpSendPacket* pSend = new RudpSendPacket(packetSize);

			packet_Header* pHeader  = (packet_Header*) pSend->Packet;
			RudpDataMsg*   pData    = (RudpDataMsg*)   pSend->Packet;

			InitHeader(pHeader);

			pHeader->Payload  = packetSize - 23;

			pData->PeerConnID = m_RemotePeerID;
			pData->DataSize   = 0;
			pData->OpCode     = OP_DATA;
			pData->SeqNum	  = m_CurrentSeq++;

			int dataSize = (nBufLen > 12) ? 12 : nBufLen;
			pData->DataSize   = dataSize;
			memcpy(pData->Data, m_SendBuff, dataSize);

			if(nBufLen > 12) 
				memcpy(pSend->Packet + 23, ((byte*)m_SendBuff) + dataSize, nBufLen - 12);

			// move next data on deck for next send
			if(m_SendBuffLength > nBufLen)
				memmove(m_SendBuff, m_SendBuff + nBufLen, m_SendBuffLength - nBufLen);
			m_SendBuffLength -= nBufLen;

			m_SendPacketMap[pData->SeqNum] = pSend; // add to map to get acked
			
			Log("Data Sent, Seq " + NumtoStr(pData->SeqNum) + ", ID " + NumtoStr(pData->PeerConnID) + ", Size " + NumtoStr(nBufLen));

			pData->SeqNum   = htons(pData->SeqNum);   // switch to big endian for network
			pSend->TimeSent = GetTickCount();
			RudpSend(pSend->Packet, pSend->Size);   // send

			Outstanding++;
		}

	m_SendSection.Unlock();

	// if we can take more data call onsend
	if(m_SendBuffLength == 0 && m_RudpSendBlock)
	{
		m_RudpSendBlock = false;
		OnSend(0);
	}
}

void CReliableSocket::RudpSend(byte* packet, int length)
{
	if(m_pNet->m_pUdpSock == NULL)
		return;

	m_LastSend = time(NULL);

	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_port   = htons(m_Address.Port);
	sa.sin_addr.S_un.S_addr = m_Address.Host.S_addr;

	m_pNet->m_pUdpSock->SendTo( packet, length, (SOCKADDR*) &sa, sizeof(SOCKADDR) );
}

void CReliableSocket::RudpClose(CloseReason code)
{
	SendFin(code);
	m_State = RUDP_CLOSED;
	OnClose(0);
}

void CReliableSocket::SendSyn()
{
	RudpSendPacket* pSend   = new RudpSendPacket( sizeof(packet_Header) );
	packet_Header*  pHeader = (packet_Header*) pSend->Packet;
	RudpSynMsg*     pSyn    = (RudpSynMsg*)    pSend->Packet;

	InitHeader(pHeader);

	pSyn->PeerConnID = 0;
	pSyn->DataSize   = 0;
	pSyn->OpCode     = OP_SYN;
	pSyn->SeqNum	 = m_CurrentSeq++;

	pSyn->ConnID     = m_PeerID;
	pSyn->Version	 = 0;

	m_SendPacketMap[pSyn->SeqNum] = pSend;

	Log("Syn Sent, Seq " + NumtoStr(pSyn->SeqNum) + ", ID " + NumtoStr(pSyn->ConnID));

	// switch to big endian for network
	pSyn->Version = htons(pSyn->Version);
	pSyn->Version = htons(pSyn->Version); 

	pSend->TimeSent = GetTickCount();
	RudpSend(pSend->Packet, pSend->Size);
}

void CReliableSocket::ReceiveSyn(RudpSynMsg* pSyn)
{
	pSyn->Version = ntohs(pSyn->Version); // switch to little endian for host

	if(m_RemotePeerID == 0)
		m_RemotePeerID = pSyn->ConnID;
	
	Log("Syn Recv, Seq " + NumtoStr(pSyn->SeqNum) + ", ID " + NumtoStr(pSyn->ConnID));

	SendAck((RudpConnMsg*) pSyn); // send ack here also because peerID now set

	m_SynAckSent = true;

	if(m_SynAckSent && m_SynAckReceieved)
	{
		Log("Connected (recv syn)");
		m_State = RUDP_CONNECTED;
		SetConnected(0);
	}
}

void CReliableSocket::SendAck(RudpConnMsg* pConn)
{
	packet_Header Header;
	RudpAckMsg* pAck = (RudpAckMsg*) &Header;

	InitHeader(&Header);

	pAck->PeerConnID = m_RemotePeerID;
	pAck->DataSize   = 0;
	pAck->OpCode     = OP_ACK;
	pAck->SeqNum	 = pConn->SeqNum;

	pAck->WindowStart = m_HighestSeqRecvd;
	pAck->WindowSpace = 0;//MAX_WINDOW_SIZE - m_RecvPacketMap.size();

	Log("Ack Sent, Seq " + NumtoStr(pAck->SeqNum) + ", ID " + NumtoStr(pAck->PeerConnID) + ", highest " + NumtoStr(m_HighestSeqRecvd));

	if(m_AckMap.find(pAck->SeqNum) == m_AckMap.end())
	{	
		m_AckMap[pAck->SeqNum] = true;
		m_AckOrder.push_back(pAck->SeqNum);
	}

	while(m_AckMap.size() > MAX_WINDOW_SIZE * 2)
	{
		m_AckMap.erase( m_AckMap.find( m_AckOrder.front() ) );
		m_AckOrder.pop_front();
	}
	
	// switch to big endian for network
	pAck->SeqNum      = htons(pAck->SeqNum);
	pAck->WindowStart = htons(pAck->WindowStart); 
	pAck->WindowSpace = htons(pAck->WindowSpace); 

	RudpSend((byte*) &Header, sizeof(packet_Header));
}

void CReliableSocket::ReceiveAck(RudpAckMsg* pAck)
{
	// switch to little endian for host
	pAck->WindowStart = ntohs(pAck->WindowStart);
	pAck->WindowSpace = ntohs(pAck->WindowSpace); 

	int latency = 0;
	int retries = -1;
	
	std::map<uint16, RudpSendPacket* >::iterator itPacket = m_SendPacketMap.find(pAck->SeqNum);
	if(itPacket != m_SendPacketMap.end())
	{
		RudpConnMsg* pConn = (RudpConnMsg*) itPacket->second->Packet;

		
		// connect handshake
		if(m_State == RUDP_CONNECTING && pConn->OpCode == OP_SYN)
		{
			m_SynAckReceieved = true;
			
			if(m_SynAckSent && m_SynAckReceieved)
			{
				Log("Connected (recv ack)");
				m_State = RUDP_CONNECTED;
				SetConnected(0);
			}
		}

		if( !itPacket->second->Acked )
		{
			m_InOrderAcks++;

			if(itPacket->second->Retries == 0)
			{
				latency = (uint32)GetTickCount() - itPacket->second->TimeSent;
				latency = latency < 5 ? 5 : latency;
				m_AvgLatency.Input(latency);
				m_AvgLatency.Next();
			}
		}

		retries = itPacket->second->Retries;

		itPacket->second->Acked = true;
	}

	Log("Ack Recv, Seq " + NumtoStr(pAck->SeqNum) + ", ID " + NumtoStr(pAck->PeerConnID) + ", highest " + NumtoStr(pAck->WindowStart) + ", retries " + NumtoStr(retries) + ", latency " + NumtoStr(latency));
	
	// ack possibly un-acked packets
	for(itPacket = m_SendPacketMap.begin(); itPacket != m_SendPacketMap.end(); itPacket++)
	{
		RudpConnMsg* pConn = (RudpConnMsg*) itPacket->second->Packet;

		if(ntohs(pConn->SeqNum) > pAck->WindowStart)
			break;

		itPacket->second->Acked = true;
	}

	// remove acked
	bool PacketsRemoved = false;

	for(itPacket = m_SendPacketMap.begin(); itPacket != m_SendPacketMap.end(); itPacket++)
		if(itPacket->second->Acked)
		{
			// calculate receive speed of remote host by rate they ack
			packet_Header* pHeader = (packet_Header*) itPacket->second->Packet;
			
			m_AvgBytesSent.Input( pHeader->Payload + 12 );

			PacketsRemoved = true;

			delete itPacket->second;		
			itPacket = m_SendPacketMap.erase(itPacket);
		}
		else
			break; // only remove packets from front of map


	// increase window if packets removed from beginning of buffer
	//if(PacketsRemoved && m_SendWindowSize < 25)
	// 	m_SendWindowSize++;


	ManageSendWindow();
}

void CReliableSocket::SendKeepAlive()
{
	packet_Header* pHeader = new packet_Header;
	RudpAckMsg*    pAck    = (RudpAckMsg*) pHeader;

	InitHeader(pHeader);

	pAck->PeerConnID  = m_RemotePeerID;
	pAck->DataSize    = 0;
	pAck->OpCode      = OP_KEEPALIVE;
	pAck->SeqNum	  = 0;
	pAck->WindowStart = m_HighestSeqRecvd;
	pAck->WindowSpace = 0;//MAX_WINDOW_SIZE - m_RecvPacketMap.size();


	Log("Keep Alive Sent, Seq " + NumtoStr(pAck->SeqNum) + ", ID " + NumtoStr(pAck->PeerConnID));
	
	// switch to big endian for network
	pAck->SeqNum = htons(pAck->SeqNum);
	pAck->WindowStart = htons(pAck->WindowStart); 
	pAck->WindowSpace = htons(pAck->WindowSpace); 

	RudpSend((byte*) pHeader, sizeof(packet_Header));
}


bool CReliableSocket::ReceiveData(RudpDataMsg* pData)
{
	packet_Header* pHeader = (packet_Header*) pData;

	int DataSize = pData->DataSize + pHeader->Payload;

	if(DataSize > MAX_CHUNK_SIZE)
	{
		Log("Too Large Packet Received Size " + NumtoStr(pHeader->Payload) + ", Type 3");
		RudpClose(LARGE_PACKET);
		return false;
	}

	// if not enough space in receive buffer
	if(RECEIVE_BUFFER_SIZE - m_RecvBuffLength < DataSize)
		return false;
			
	// copy data from rudp header
	memcpy(m_RecvBuff + m_RecvBuffLength, pData->Data, pData->DataSize);
	m_RecvBuffLength += pData->DataSize;

	// copy data from gnutella payload
	memcpy(m_RecvBuff + m_RecvBuffLength, ((byte*) pHeader) + 23, pHeader->Payload);
	m_RecvBuffLength += pHeader->Payload;

	Log("Data Recv, Seq " + NumtoStr(pData->SeqNum) + ", ID " + NumtoStr(pData->PeerConnID));
	//Log("Data Recv Buff, Size " + NumtoStr(m_RecvBuffLength));

	return true;
}

void CReliableSocket::SendFin(CloseReason code)
{
	RudpSendPacket* pSend   = new RudpSendPacket( sizeof(packet_Header) );
	packet_Header*  pHeader = (packet_Header*) pSend->Packet;
	RudpFinMsg*     pFin    = (RudpFinMsg*) pHeader;

	InitHeader(pHeader);

	pFin->PeerConnID = m_RemotePeerID;
	pFin->DataSize   = 0;
	pFin->OpCode     = OP_FIN;
	pFin->SeqNum	 = m_CurrentSeq++;

	pFin->ReasonCode = code;

	m_SendPacketMap[pFin->SeqNum] = pSend;

	Log("Fin Sent, Seq " + NumtoStr(pFin->SeqNum) + ", ID " + NumtoStr(pFin->PeerConnID) + ", Reason " + NumtoStr(pFin->ReasonCode));
	
	// switch to big endian for network
	pFin->SeqNum = htons(pFin->SeqNum);

	pSend->TimeSent = GetTickCount();
	RudpSend(pSend->Packet, pSend->Size);
}

void CReliableSocket::ReceiveFin(RudpFinMsg* pFin)
{
	Log("Fin Recv, Seq " + NumtoStr(pFin->SeqNum) + ", ID " + NumtoStr(pFin->PeerConnID) + ", Reason " + NumtoStr(pFin->ReasonCode));
	
	if(m_State == RUDP_CLOSED)
		return;

	RudpClose(YOU_CLOSED);
}

void CReliableSocket::OnSecond()
{
	if(m_pNet == NULL)
		return;

	if(m_pNet->m_pUdpSock == NULL && m_State != RUDP_CLOSED)
	{
		OnClose(WSAENETDOWN);
		m_State = RUDP_CLOSED;
		return;
	}

	// connecting timeout
	if(m_State == RUDP_CONNECTING && time(NULL) > m_ConnectTimeout)
	{
		SendFin(TIMEOUT);
		m_State = RUDP_CLOSED;
		SetConnected(WSAETIMEDOUT);
	}
	
	int PacketLoss = 0;

	if(m_State == RUDP_CONNECTED)
	{
		// manage send window
		PacketLoss = 0;
		
		if(m_InOrderAcks)
			PacketLoss = m_ReTransmits * 100 / m_InOrderAcks;

		m_ReTransmits = 0;
		m_InOrderAcks = 0;

		if(PacketLoss < 10 && m_SendWindowSize < 25)
			m_SendWindowSize++;
		if(PacketLoss > 20 && m_SendWindowSize > 1)
			m_SendWindowSize /= 2;
		

		// if data waiting to be read
		if(m_RecvBuffLength > 0)
			OnReceive(0);

		// if nothing received for 20 seconds disconnect
		if( time(NULL) > m_LastRecv + 20 )
			RudpClose(TIMEOUT);

		// if 10 seconds without receiving or sending
		if( time(NULL)  > m_LastRecv + 3 ||
			time(NULL)  > m_LastSend + 3)
			SendKeepAlive();
	}

	// re-send packets in out buffer
	if(m_State == RUDP_CONNECTING || m_State == RUDP_CONNECTED)
	{	
		ManageSendWindow();
	}

	// update bandwidth rate used for determining internal send buffer
	m_AvgBytesSent.Next();

	//Log("UDP Transfer Report: ");
	//Log("   Sending: Packet Loss " + NumtoStr(PacketLoss) + "%, Speed " + NumtoStr(m_AvgBytesSent.GetAverage()) + " B/s, BufferSize " + NumtoStr(m_SendBuffLength) + ", Packet Map Size " + NumtoStr(m_SendPacketMap.size()) + ", Window Size " + NumtoStr(m_SendWindowSize) + ", Latency " + NumtoStr(m_AvgLatency.GetAverage()) + " ms");
	//Log("   Receiving: BufferSize " + NumtoStr(m_RecvBuffLength) + ", Packet Map Size " + NumtoStr(m_RecvPacketMap.size()));

}

void CReliableSocket::InitHeader(packet_Header* pHeader)
{
	pHeader->Guid		= GUID_NULL;
	pHeader->Function	= 0x41;
	pHeader->TTL		= 1;
	pHeader->Hops		= 0;
	pHeader->Payload	= 0;
}

void CReliableSocket::SetConnected(int nErrorCode)
{
	if(m_Listening)
		m_pEventSink->OnAccept(nErrorCode);
	else
		m_pEventSink->OnConnect(nErrorCode);
}

void CReliableSocket::Log(CString Entry)
{
	TRACE0("RUDP: " + IPv4toStr(m_Address) + ":" + NumtoStr(time(NULL) % 60) + "." + NumtoStr((uint32)GetTickCount() % 1000) + " " + Entry + "\n");
}
