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
#include "G2Control.h"
#include "G2Node.h"
#include "G2Protocol.h"
#include "UdpListener.h"

#include "G2Datagram.h"


// CG2Datagram

CG2Datagram::CG2Datagram(CG2Control* pG2Comm)
{
	m_pG2Comm = pG2Comm;
	m_pProtocol = pG2Comm->m_pProtocol;

	m_NextSequence = 1;

	m_SendBytesAvail = 0;

	m_AvgUdpDown.SetRange(30);
	m_AvgUdpUp.SetRange(30);

	m_UdpSecBytesDown = 0;
	m_UdpSecBytesUp   = 0;

	m_FlushCounter = 0;
}

CG2Datagram::~CG2Datagram()
{
	while( m_RecvCache.size() )
	{
		delete m_RecvCache.front();
		m_RecvCache.pop_front();
	}

	while( m_SendCache.size() )
	{
		delete m_SendCache.front();
		m_SendCache.pop_front();
	}

	m_AckCache.clear();

	m_TransferPacketAccess.Lock();
		
		while( m_TransferPackets.size())
		{
			delete m_TransferPackets.back();
			m_TransferPackets.pop_back();
		}

	m_TransferPacketAccess.Unlock();
}

void CG2Datagram::Timer()
{
	m_AvgUdpDown.Update(m_UdpSecBytesDown);
	m_AvgUdpUp.Update(m_UdpSecBytesUp);

	m_UdpSecBytesDown = 0;
	m_UdpSecBytesUp   = 0;

	// Timeout recv buff
	std::list<GND_Packet*>::iterator itPacket = m_RecvCache.begin();
	while( itPacket != m_RecvCache.end() )
		if( (*itPacket)->Timeout == GND_RECV_TIMEOUT)
		{
			delete *itPacket;
			itPacket = m_RecvCache.erase(itPacket);
		}
		else
		{
			(*itPacket)->Timeout++;
			itPacket++;
		}

	// Timeout send buff
	itPacket = m_SendCache.begin();
	while( itPacket != m_SendCache.end() )
		if( (*itPacket)->Timeout == GND_SEND_TIMEOUT)
		{
			std::map<uint16, GND_Packet*>::iterator itSequence = m_SequenceMap.find((*itPacket)->Sequence);
			if(itSequence != m_SequenceMap.end())
				m_SequenceMap.erase(itSequence);

			delete *itPacket;
			itPacket = m_SendCache.erase(itPacket);
		}
		else
		{
			(*itPacket)->Timeout++;
			itPacket++;
		}

	// Reset allocated send bytes
	if(m_pG2Comm->m_ClientMode == G2_HUB)
		m_SendBytesAvail = UDP_HUB_LIMIT;
	else
		m_SendBytesAvail = UDP_CHILD_LIMIT;

	// Transfer packets from thread to main thread
	m_TransferPacketAccess.Lock();
	
		for( int i = 0; i < m_TransferPackets.size(); i++ )
			SendPacket( m_TransferPackets[i] );
		
		m_TransferPackets.clear();

	m_TransferPacketAccess.Unlock();


	FlushSendBuffer();
}	

void CG2Datagram::OnReceive(IPv4 Address, byte* pRecvBuff, int RecvLength)
{
	Decode_GND(Address, (GND_Header*) pRecvBuff, RecvLength );
}

void CG2Datagram::Decode_GND(IPv4 Address, GND_Header* RecvPacket, int length)
{
	if(length < 8)
	{
		// Short UDP Recvd
		m_pG2Comm->m_pCore->DebugLog("G2 Network", "Short UDP Recvd: " + HexDump((byte*) RecvPacket, length));
		return;
	}

	if( memcmp(RecvPacket->Tag, "GND", 3) != 0 )
	{
		//ASSERT(0); // Not a valid tag
		return;
	}

	// Record bandwidth
	std::map<uint32, CG2Node*>::iterator itNode = m_pG2Comm->m_G2NodeAddrMap.find(Address.Host.S_addr);
	if(itNode != m_pG2Comm->m_G2NodeAddrMap.end())
		itNode->second->m_dwSecBytes[0] += length;
	else
		m_UdpSecBytesDown += length;

	byte debugFlags = RecvPacket->Flags;

	bool deflate = (RecvPacket->Flags & 0x01) ? true : false; 
	bool sendack = (RecvPacket->Flags & 0x02) ? true : false; 

	if( (RecvPacket->Flags & 0x04) || (RecvPacket->Flags & 0x08) )
	{
		ASSERT(0); // Critical flags not known
		return;
	}

	// If this is an ACK packet
	if( RecvPacket->Count == 0 )
	{
		ProcessACK(RecvPacket);
		return;
	}

	// Send ACK if needed
	if( sendack )
		DispatchACK(Address, RecvPacket);
	
	// Lookup packet in RecvPacketList
	GND_Packet* inPacket = NULL;

	std::list<GND_Packet*>::iterator itPacket;
	for(itPacket = m_RecvCache.begin(); itPacket != m_RecvCache.end(); itPacket++)
		if( (*itPacket)->Address.Host.S_addr == Address.Host.S_addr && (*itPacket)->Sequence == RecvPacket->Sequence)
		{
			inPacket = *itPacket;
			break;
		}
	
	// If not in cache create
	if( inPacket == NULL )
	{
		inPacket = new GND_Packet(Address, RecvPacket->Sequence, RecvPacket->Count, deflate);
		m_RecvCache.push_back( inPacket );

		while( m_RecvCache.size() > 100)
		{
			delete m_RecvCache.front();
			m_RecvCache.pop_front();
		}
	}

	if( !inPacket->Processed )
	{
		// Look up fragment
		bool FragmentFound = false;
		std::list<GND_Fragment*>::iterator itFrag;
		for(itFrag = inPacket->Fragments.begin(); itFrag != inPacket->Fragments.end(); itFrag++)
			if( (*itFrag)->Part == RecvPacket->Part )
			{
				FragmentFound = true;
				break;
			}

		// Add new fragment
		if( !FragmentFound )
		{
			ASSERT( inPacket->Fragments.size() < inPacket->FragemntCount);

			GND_Fragment* pFrag = new GND_Fragment(RecvPacket->Part, (byte*) RecvPacket + 8, length - 8);
			inPacket->Fragments.push_back(pFrag);
		}

		// Check if all fragments received
		if( inPacket->Fragments.size() == inPacket->FragemntCount)
		{
			inPacket->Processed = true;

			// Get total size of packet
			int FinSize = 0;
			for(itFrag = inPacket->Fragments.begin(); itFrag != inPacket->Fragments.end(); itFrag++)
				FinSize += (*itFrag)->Length;

			// Copy packet from fragments into one buffer
			byte* FinBuffer = new byte[FinSize];
			int   FinOffset = 0;
			
			for(int i = 1; i <= inPacket->FragemntCount; i++)
				for(itFrag = inPacket->Fragments.begin(); itFrag != inPacket->Fragments.end(); itFrag++)
					if( (*itFrag)->Part == i )
					{
						memcpy(FinBuffer + FinOffset, (*itFrag)->Data, (*itFrag)->Length);
						FinOffset += (*itFrag)->Length;
					}

			// Deflate packet if it needs to be
			uint32 PacketSize = GND_PACKET_BUFF;
		
			// Decompress packet if needed
			if( inPacket->Compressed )
			{
				if( uncompress( m_pRecvPacketBuff, (DWORD*) &PacketSize, FinBuffer, FinSize) != Z_OK )
				{
					//ASSERT(0);
					delete [] FinBuffer;
					return;
				}
			}
			else
			{
				PacketSize = FinSize;
				memcpy(m_pRecvPacketBuff, FinBuffer, PacketSize);
			}

			// Read Packet
			G2_Header undefPacket;
			byte* PacketPos = m_pRecvPacketBuff;
			G2ReadResult streamStatus = m_pProtocol->ReadNextPacket( undefPacket, PacketPos, PacketSize );

			if( streamStatus == PACKET_GOOD )
			{
				ASSERT(PacketSize == 0);
				if(PacketSize != 0)
					m_pG2Comm->m_pCore->DebugLog("G2 Network", "UDP Recvd Error: " + HexDump(FinBuffer, FinSize));

				G2_RecvdPacket Packet(Address, undefPacket);
				m_pG2Comm->ReceivePacket( Packet );
			}
			else
			{
				// UDP Recvd Error
				m_pG2Comm->m_pCore->DebugLog("G2 Network", "UDP Recvd Error: " + HexDump(PacketPos, PacketSize));
			}

			delete [] FinBuffer;
		}
	}
}


void CG2Datagram::DispatchACK(IPv4 Address, GND_Header* Packet)
{
	GND_Ack Ack(Address);

	memcpy(&Ack.Packet, Packet, ACK_LENGTH);
	Ack.Packet.Count = 0;

	m_AckCache.push_back(Ack);

	while( m_AckCache.size() > 1000)
		m_AckCache.pop_front();


	// Too frequent
	//FlushSendBuffer();
}

void CG2Datagram::ProcessACK(GND_Header* Packet)
{
	std::map<uint16, GND_Packet*>::iterator itSequence = m_SequenceMap.find(Packet->Sequence);
	if(itSequence != m_SequenceMap.end())
	{
		std::list<GND_Fragment*>::iterator itFrag;
		for(itFrag = (itSequence->second)->Fragments.begin(); itFrag != (itSequence->second)->Fragments.end(); itFrag++)
			if( (*itFrag)->Part == Packet->Part )
			{
				(*itFrag)->Acked = true;

				return;
			}
	}
}

void CG2Datagram::SendPacket(IPv4 Address, byte* packet, uint32 length, bool Thread, bool ReqAck, bool Priority)
{
	//ASSERT( !(Address.Host.S_addr == m_pG2Comm->m_pNet->m_CurrentIP.S_addr && Address.Port == m_pG2Comm->m_pNet->m_CurrentPort) );

	if(Address.Host.S_addr == m_pG2Comm->m_pNet->m_CurrentIP.S_addr && Address.Port == m_pG2Comm->m_pNet->m_CurrentPort)
		return;

	ASSERT(packet && length);
	ASSERT(length < 65536);

	if(length == 0 || length > 65536 || packet == NULL)
		return;
	
	// Force ack off if cant receive them
	if(m_pG2Comm->m_pNet->m_UdpFirewall == UDP_BLOCK)
		ReqAck = false;

	if(!Thread && m_pG2Comm->m_pCore->m_dnaCore->m_dnaEvents)
		m_pG2Comm->m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketOutgoing(NETWORK_G2, false , Address.Host.S_addr, Address.Port, packet, length, false);
	

	bool PacketCompressed = false;
	if(length > 64)
		PacketCompressed = true;

	// Compress packet
	uint32 PacketSize = length;

	if(PacketCompressed)
	{
		PacketSize = GND_PACKET_BUFF;

		if( compress(m_pSendPacketBuff, (DWORD*) &PacketSize, packet, length) != Z_OK )
		{
			ASSERT(0);
			return;
		}
	}
	else
		memcpy(m_pSendPacketBuff, packet, length);

	// Determine how many fragments
	int MaxSendSize = GND_MTU - 8; // Take into account udp header

	int Frags = 1;
	while( PacketSize > Frags * MaxSendSize  )
		Frags++;

	// Create packet
	m_NextSequence++;
	GND_Packet* outPacket = new GND_Packet(Address, m_NextSequence, Frags, true, Priority);
	
	// Create udp header
	GND_Header UdpHeader;
	memcpy(UdpHeader.Tag, "GND", 3); 
	UdpHeader.Sequence = m_NextSequence; 
	UdpHeader.Count    = Frags;

	UdpHeader.Flags      = 0;

	if(PacketCompressed)
		UdpHeader.Flags     |= 0x01;

	if( ReqAck )
	{
		UdpHeader.Flags |= 0x02;
		outPacket->AckRequired = true;
	}

	// Add fragments
	int Offset = 0;
	for(int i = 1; i <= Frags; i++)
	{
		int FragSize = (PacketSize > MaxSendSize) ? MaxSendSize : PacketSize;

		UdpHeader.Part = i;

		GND_Fragment* pFrag = new GND_Fragment(i);
		pFrag->Data = new byte[8 + FragSize];
		memcpy(pFrag->Data, (byte*) &UdpHeader, 8);
		memcpy(pFrag->Data + 8, m_pSendPacketBuff + Offset, FragSize);
		pFrag->Length = 8 + FragSize;

		outPacket->Fragments.push_back(pFrag);

		PacketSize -= FragSize;
		Offset += FragSize;
	}

	if( Thread )
	{
		m_TransferPacketAccess.Lock();
			m_TransferPackets.push_back( outPacket );
		m_TransferPacketAccess.Unlock();
	}
	else
		SendPacket( outPacket );
	
}

void CG2Datagram::SendPacket(GND_Packet* OutPacket)
{
	int SendCacheMax = 200;

	while( m_SendCache.size() >= SendCacheMax)
	{
		std::map<uint16, GND_Packet*>::iterator itSequence = m_SequenceMap.find(m_SendCache.back()->Sequence);
		if(itSequence != m_SequenceMap.end())
			m_SequenceMap.erase(itSequence);

		delete m_SendCache.back();
		m_SendCache.pop_back();
	}

	// Packets with priority always first to go out
	if(OutPacket->Priority || m_SendCache.size() == 0)
		m_SendCache.push_front( OutPacket );
	else
	{
		std::list<GND_Packet*>::iterator itPacket;
		for( itPacket = m_SendCache.begin(); itPacket != m_SendCache.end(); itPacket++)
			if( !(*itPacket)->Priority )
			{
				m_SendCache.insert(itPacket, OutPacket);
				break;
			}

		if(itPacket == m_SendCache.end())
			m_SendCache.insert(m_SendCache.end(), OutPacket);
	}

	m_SequenceMap[OutPacket->Sequence] = OutPacket;

	ASSERT(m_SequenceMap.size() <= SendCacheMax);


	if(m_FlushCounter == FLUSH_PERIOD)
	{
		FlushSendBuffer();
		m_FlushCounter = 0;
	}
	else
		m_FlushCounter++;
}

void CG2Datagram::FlushSendBuffer()
{

	// Send Acks first
	std::list<GND_Ack>::iterator itAck = m_AckCache.begin();
	while( itAck != m_AckCache.end() )
	{
		if( m_SendBytesAvail <= 0)
			return;

		m_pG2Comm->m_pNet->AddNatDetect((*itAck).Address.Host);

		SOCKADDR_IN sa;
		sa.sin_family = AF_INET;
		sa.sin_port   = htons((*itAck).Address.Port);
		sa.sin_addr.S_un.S_addr = (*itAck).Address.Host.S_addr;

		int UdpSent = 0;
		
		if(m_pG2Comm->m_pNet->m_pUdpSock)
			UdpSent = m_pG2Comm->m_pNet->m_pUdpSock->SendTo( &(*itAck).Packet, ACK_LENGTH, (SOCKADDR*) &sa, sizeof(SOCKADDR) );
		//SendTo( &(*itAck).Packe, ACK_LENGTH, (*itAck)->Address.Port, IPtoStr((*itAck)->Address.Host));

		if(UdpSent < ACK_LENGTH)
		{
			if(UdpSent < 0)
			{
				int ErrorCode = GetLastError();
				ASSERT(0);
			}

			return;
		}

		m_SendBytesAvail -= ACK_LENGTH;

		// Record bandwidth
		std::map<uint32, CG2Node*>::iterator itNode = m_pG2Comm->m_G2NodeAddrMap.find((*itAck).Address.Host.S_addr);
		if(itNode != m_pG2Comm->m_G2NodeAddrMap.end())
			itNode->second->m_dwSecBytes[1] += ACK_LENGTH;
		else
			m_UdpSecBytesUp += ACK_LENGTH;

		itAck = m_AckCache.erase(itAck);
	}


	// Loop sending single fragment from each pending packet
	GND_Packet*   pPacket = NULL;
	GND_Fragment* pFrag   = NULL;
	
	while( m_SendCache.size() )
	{
		int BytesSent = 0;

		std::list<GND_Packet*>::iterator itPacket = m_SendCache.begin();
		while( itPacket != m_SendCache.end())
		{
			pPacket = *itPacket;

			int FragsDone = 0;

			std::list<GND_Fragment*>::iterator itFrag;
			for(itFrag = pPacket->Fragments.begin(); itFrag != pPacket->Fragments.end(); itFrag++)
			{
				pFrag = *itFrag;

				if( ( !pPacket->AckRequired && pFrag->Sent) || (pPacket->AckRequired && pFrag->Acked) )
				{
					FragsDone++;
					continue;
				}

				if( !pFrag->Sent || (pPacket->AckRequired && pPacket->Timeout >= pFrag->Wait ) )
				{
					m_pG2Comm->m_pNet->AddNatDetect(pPacket->Address.Host);

					SOCKADDR_IN sa;
					sa.sin_family = AF_INET;
					sa.sin_port   = htons(pPacket->Address.Port);
					sa.sin_addr.S_un.S_addr = pPacket->Address.Host.S_addr;

					int UdpSent = 0;
					
					if(m_pG2Comm->m_pNet->m_pUdpSock)
						UdpSent = m_pG2Comm->m_pNet->m_pUdpSock->SendTo( pFrag->Data, pFrag->Length, (SOCKADDR*) &sa, sizeof(SOCKADDR) );
					//SendTo( pFrag->Data, pFrag->Length, pPacket->Address.Port, IPtoStr(pPacket->Address.Host));

					if(UdpSent < pFrag->Length)
					{
						//crit broadcast address WSAEACCES causes never ending loop, packet timeout?
						if(UdpSent < 0)
						{
							int ErrorCode = GetLastError();
							//ASSERT(0);
						}

						return;
					}

					// Record bandwidth
					std::map<uint32, CG2Node*>::iterator itNode = m_pG2Comm->m_G2NodeAddrMap.find(pPacket->Address.Host.S_addr);
					if(itNode != m_pG2Comm->m_G2NodeAddrMap.end())
						itNode->second->m_dwSecBytes[1] += pFrag->Length;
					else
						m_UdpSecBytesUp += pFrag->Length;

					pFrag->Sent = true;
					pFrag->Wait = pPacket->Timeout + GND_SEND_RETRY;

					m_SendBytesAvail -= pFrag->Length;
					BytesSent += pFrag->Length;

					if( !pPacket->AckRequired )
						FragsDone++;

					break;
				}
				else
					continue;
			}

			// If packet done erase
			if( FragsDone == pPacket->FragemntCount )
			{
				std::map<uint16, GND_Packet*>::iterator itSequence = m_SequenceMap.find((*itPacket)->Sequence);
				if(itSequence != m_SequenceMap.end())
					m_SequenceMap.erase(itSequence);

				delete *itPacket;
				itPacket = m_SendCache.erase(itPacket);
			}
			else
				itPacket++;

			// No bytes available to send with
			if( m_SendBytesAvail <= 0)
				return;
		}
			
		// No change packet cache stagnant (waiting for acks or something)
		if( BytesSent == 0 )
			return;
	}

}
