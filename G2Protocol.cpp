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
#include "math.h"
#include "GnuWordHash.h"
#include "GnuCore.h"
#include "G2Control.h"
#include "GnuControl.h"
#include "G2Node.h"
#include "G2Protocol.h"

CG2Protocol::CG2Protocol(CG2Control* pG2Comm)
{
	m_pG2Comm = pG2Comm;

	m_FrameCount = 0;
	m_FrameSize  = sizeof(G2_Frame);

	m_WriteOffset = 0;

	m_FinalSize = 0;
}	

CG2Protocol::~CG2Protocol(void)
{

}

G2ReadResult CG2Protocol::ReadNextPacket( G2_Header &Packet, byte* &stream, uint32 &length )
{
	if( length == 0 )
		return PACKET_INCOMPLETE;

	byte*  beginStream = stream;
	uint32 beginLength = length;

	Packet.Packet = stream;

	// Read Control Byte
	byte control = stream[0];

	stream += 1;
	length -= 1;

	if ( control == 0 ) 
		return STREAM_END;

	byte lenLen  = ( control & 0xC0 ) >> 6; 
	byte nameLen = ( control & 0x38 ) >> 3;
	byte flags   = ( control & 0x07 ); 

	bool bigEndian  = ( flags & 0x02 ) ? true : false; 
	bool isCompound = ( flags & 0x04 ) ? true : false; 

	if( bigEndian )
		return PACKET_ERROR;

	Packet.HasChildren = isCompound;
	
	// Read Packet Length
	Packet.InternalSize = 0;
	if( lenLen )
	{	
		if(length < lenLen)
		{
			stream = beginStream;
			length = beginLength;
			return PACKET_INCOMPLETE;
		}
		
		memcpy( &Packet.InternalSize, stream, lenLen );

		ASSERT(MAX_FINAL_SIZE < G2_PACKET_BUFF);
		if(Packet.InternalSize >= MAX_FINAL_SIZE)
		{
			//ASSERT(0);
			return PACKET_ERROR;
		}

		stream += lenLen;
		length -= lenLen;
	}

	// Read Packet Name (length is always one greater)
	nameLen += 1;

	if(length < nameLen)
	{
		stream = beginStream;
		length = beginLength;
		return PACKET_INCOMPLETE;
	}

	if(Packet.NameLen + 1 + nameLen > MAX_NAME_SIZE - 1)
	{
		memset(Packet.Name, 'X', MAX_NAME_SIZE);
		ASSERT(0);
	}
	else
	{
		Packet.Name[Packet.NameLen] = '/';
		Packet.NameLen += 1;

		memcpy(Packet.Name + Packet.NameLen, stream, nameLen);
		Packet.NameLen += nameLen;

		Packet.Name[Packet.NameLen] = NULL; // Dont increase len because it can be appended to

		//Packet.Name += "/" + CString((char*)stream, nameLen);
	}

	stream += nameLen;
	length -= nameLen;

	// Check if full packet length available in stream
	if(length < Packet.InternalSize)
	{
		stream = beginStream;
		length = beginLength;
		return PACKET_INCOMPLETE;
	}

	Packet.Internal = (Packet.InternalSize > 0) ? stream : NULL;
	
	Packet.NextBytePos   = Packet.Internal;
	Packet.NextBytesLeft = Packet.InternalSize;

	stream += Packet.InternalSize;
	length -= Packet.InternalSize;

	Packet.PacketSize = 1 + lenLen + nameLen + Packet.InternalSize;

	return PACKET_GOOD;
}

G2ReadResult CG2Protocol::ReadNextChild( G2_Header &Root, G2_Header &Child)
{
	if( !Root.HasChildren )
		return STREAM_END;

	memcpy(Child.Name, Root.Name, Root.NameLen + 1); // Get null too
	Child.NameLen = Root.NameLen;

	return ReadNextPacket(Child, Root.NextBytePos, Root.NextBytesLeft);
}

bool CG2Protocol::ReadPayload(G2_Header &Packet)
{
	ResetPacket(Packet);

	G2_Header Child;

	G2ReadResult streamStatus = PACKET_GOOD;
	while( streamStatus == PACKET_GOOD )
		streamStatus = ReadNextChild(Packet, Child);

	if(streamStatus == STREAM_END)
	{
		if( Packet.NextBytesLeft > 0)
		{
			Packet.Payload     = Packet.NextBytePos;
			Packet.PayloadSize = Packet.NextBytesLeft;

			return true;
		}
	}
	else if( Packet.NextBytesLeft > 0)
	{
		// Payload Read Error
		m_pG2Comm->m_pCore->DebugLog("G2 Network", "Payload Read Error: " + HexDump(Packet.Packet, Packet.PacketSize));
	}

	return false;
}

void CG2Protocol::ResetPacket(G2_Header &Packet)
{
	Packet.NextBytePos   = Packet.Internal;
	Packet.NextBytesLeft = Packet.InternalSize;

	Packet.Payload = NULL;
	Packet.PayloadSize = 0;
}

void CG2Protocol::ReadNodeInfo(G2_Header Packet, G2NodeInfo &ReadNode)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( Packet, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		for(int slashpos = childPacket.NameLen; slashpos > 0; slashpos--)
			if( childPacket.Name[slashpos] == '/')
				break;

		//  Node Address 
		if( strcmp(childPacket.Name + slashpos, "/NA") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize == 6)
			{
				memcpy( &ReadNode.Address.Host, childPacket.Payload, 4);
				memcpy( &ReadNode.Address.Port, childPacket.Payload + 4, 2);
			}
		}

		 //  GUID 
		else if( strcmp(childPacket.Name + slashpos, "/GU") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize == 16)
				memcpy( &ReadNode.NodeID, childPacket.Payload, 16);
		}

		//  Vendor Code 
		else if( strcmp(childPacket.Name + slashpos, "/V") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize == 4)
				memcpy( ReadNode.Vendor, childPacket.Payload, 4);
		}

		//  Library Statistics  
		else if( strcmp(childPacket.Name + slashpos, "/LS") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 8)
			{
				memcpy( &ReadNode.LibraryCount, childPacket.Payload, 4);
				memcpy( &ReadNode.LibrarySizeKB, childPacket.Payload + 4, 4);
			}
		}

		//  Hub Status 
		else if( strcmp(childPacket.Name + slashpos, "/HS") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
			{
				memcpy( &ReadNode.LeafCount, childPacket.Payload, 2);
				memcpy( &ReadNode.LeafMax, childPacket.Payload + 2, 2);
			}
		}

		// Hub Able
		else if( strcmp(childPacket.Name + slashpos, "/HA") == 0 )
		{
			ReadNode.HubAble = true;
		}

		// Firewall
		else if( strcmp(childPacket.Name + slashpos, "/FW") == 0 )
		{
			ReadNode.Firewall = true;
		}

		// Router
		else if( strcmp(childPacket.Name + slashpos, "/RTR") == 0 )
		{
			ReadNode.Router = true;
		}

		// Bandwidth
		else if( strcmp(childPacket.Name + slashpos, "/NBW") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 8)
			{
				memcpy( &ReadNode.NetBpsIn, childPacket.Payload, 4);
				memcpy( &ReadNode.NetBpsOut, childPacket.Payload + 4, 4);
			}
		}

		// Cpu/Mem
		else if( strcmp(childPacket.Name + slashpos, "/CM") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
			{
				memcpy( &ReadNode.Cpu, childPacket.Payload, 2);
				memcpy( &ReadNode.Mem, childPacket.Payload + 2, 2);
			}
		}

		// Uptime
		else if( strcmp(childPacket.Name + slashpos, "/UP") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
			{
				uint32 Uptime;
				memcpy( &Uptime, childPacket.Payload, 4);
				ReadNode.UpSince = time(NULL) - Uptime;
			}

		}

		// Location
		else if( strcmp(childPacket.Name + slashpos, "/GPS") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
			{
				memcpy( &ReadNode.Latitude, childPacket.Payload, 2);
				memcpy( &ReadNode.Longitude, childPacket.Payload + 2, 2);
			}
		}
	}
}

G2_Frame* CG2Protocol::WritePacket(G2_Frame* Root, char* Name, void* payload, uint32 length)
{
	// If new packet
	if(Root == NULL)
	{
		if(m_FrameCount || m_WriteOffset)
		{
			ASSERT(0); // Careful, can be caused by previous assert further down call stack

			m_FrameCount  = 0;
			m_WriteOffset = 0;
		}

		m_FinalSize = 0;
	}

	byte nameLen = strlen(Name);
	if(nameLen > 8)
	{
		ASSERT(0);
		return NULL;
	}

	if(m_FrameCount * m_FrameSize + m_FrameSize > FRAME_BUFF_SIZE)
	{
		ASSERT(0);
		return NULL;
	}

	// Create new frame
	G2_Frame* pPacket = (G2_Frame*) (m_Frames + (m_FrameCount * m_FrameSize)); //new G2_Frame();
	memset(pPacket, 0, m_FrameSize);
	m_FrameCount++;

	pPacket->Parent = Root;
	memcpy(pPacket->Name, Name, nameLen);
	pPacket->Name[nameLen] = NULL;
	pPacket->NameLen = nameLen;
	
	if(length)
	{
		if(m_WriteOffset + length > MAX_WRITE_SIZE)
		{
			ASSERT(0);
			return NULL;
		}

		pPacket->PayloadLength = length;
		pPacket->Payload = m_WriteData + m_WriteOffset;
		memcpy(m_WriteData + m_WriteOffset, payload, length);
		m_WriteOffset += length;
	}

	return pPacket;
}

void CG2Protocol::WriteFinish()
{
	// Reverse iterate through packet structure, set lengths
	for(int i = m_FrameCount - 1; i >= 0; i--)
	{		
		G2_Frame* pPacket = (G2_Frame*) (m_Frames + (i * m_FrameSize));

		if(pPacket->InternalLength > 0 && pPacket->PayloadLength > 0)
			pPacket->InternalLength += 1; // For endstream byte

		pPacket->PayloadOffset = pPacket->InternalLength;
		pPacket->InternalLength += pPacket->PayloadLength;

		pPacket->LenLen = 0;
		while(pPacket->InternalLength >= pow(256, pPacket->LenLen) )
			pPacket->LenLen++;

		ASSERT(pPacket->LenLen < 4);

		pPacket->HeaderLength = 1 + pPacket->LenLen + pPacket->NameLen;

		if( pPacket->Parent != NULL)
		{
			pPacket->Parent->InternalLength += pPacket->HeaderLength + pPacket->InternalLength;	
			pPacket->Parent->Compound = 1;
		}
	}

	// Iterate through packet stucture, build packet
	for(i = 0; i < m_FrameCount; i++)
	{
		G2_Frame* pPacket = (G2_Frame*) (m_Frames + (i * m_FrameSize));

		byte* NextByte = NULL;
		
		if( pPacket->Parent )
		{
			ASSERT(pPacket->Parent->NextChild);
			NextByte = pPacket->Parent->NextChild;
			pPacket->Parent->NextChild += pPacket->HeaderLength + pPacket->InternalLength;
		}
		else // beginning of packet
		{
			m_FinalSize = pPacket->HeaderLength + pPacket->InternalLength;
			NextByte    = m_FinalPacket;
		}

		int control = 0;
		control |= (pPacket->LenLen << 6);
		control |= ((pPacket->NameLen - 1) << 3);
		control |= (pPacket->Compound << 2);
		
		*NextByte = control;
		NextByte += 1;

		 // DNA should not pass packets greater than 4096, though pass through packets could be bigger
		if(pPacket->HeaderLength + pPacket->InternalLength > MAX_WRITE_SIZE)
		{
			ASSERT(0);

			m_FrameCount  = 0;
			m_WriteOffset = 0;
			m_FinalSize   = 0;

			return;
		}

		memcpy(NextByte, &pPacket->InternalLength, pPacket->LenLen);
		NextByte += pPacket->LenLen;

		memcpy(NextByte, pPacket->Name, pPacket->NameLen);
		NextByte += pPacket->NameLen;

		if(pPacket->Compound)
			pPacket->NextChild = NextByte;

		if( pPacket->Payload )
		{
			byte* PayloadPos = NextByte + pPacket->PayloadOffset;
			memcpy(PayloadPos, pPacket->Payload, pPacket->PayloadLength);
			
			if(pPacket->Compound) // Set stream end
			{
				PayloadPos -= 1;
				*PayloadPos = 0;
			}
		}
	}

	ASSERT(m_FinalSize);

	m_FrameCount  = 0;
	m_WriteOffset = 0;
}

bool CG2Protocol::Decode_TO(G2_Header PacketTO, GUID &TargetID)
{
	G2_Header childPacket;
	G2ReadResult childStatus = ReadNextChild( PacketTO, childPacket );

	if( childStatus == PACKET_GOOD )
		if( strcmp( childPacket.Name + PacketTO.NameLen, "/TO") == 0)
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 16)
			{
				memcpy( &TargetID, childPacket.Payload, 16 );
				
				ResetPacket(PacketTO);

				return true;
			}


	ResetPacket(PacketTO);

	return false;
}

void CG2Protocol::Decode_PI(G2_Header PacketPI, G2_PI &Ping)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketPI, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		//  Relay
		if( strcmp(childPacket.Name, "/PI/RELAY") == 0 )
			Ping.Relay = true;

		//  Udp 
		else if( strcmp(childPacket.Name, "/PI/UDP") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 6)
			{
				memcpy( &Ping.UdpAddress.Host, childPacket.Payload, 4);
				memcpy( &Ping.UdpAddress.Port, childPacket.Payload + 4, 2);
			}
		}

		// Ident
		else if( strcmp(childPacket.Name, "/PI/IDENT") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
				memcpy( &Ping.Ident, childPacket.Payload, 4);
		}

		// Test Firewall
		else if( strcmp(childPacket.Name, "/PI/TFW") == 0 )
		{
			Ping.TestFirewall = true;
		}

		// connect request
		else if( strcmp(childPacket.Name, "/PI/CR") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 1)
			{
				Ping.ConnectRequest = true;
				Ping.HubMode = (*childPacket.Payload | 1);
			}
		}
	}
}

void CG2Protocol::Decode_PO(G2_Header PacketPO, G2_PO &Pong)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketPO, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;


		//  Relay
		if( strcmp(childPacket.Name, "/PO/RELAY") == 0 )
			Pong.Relay = true;

		// connect ack
		else if( strcmp(childPacket.Name, "/PO/CA") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 1)
			{
				Pong.ConnectAck = true;
				Pong.SpaceAvailable = (*childPacket.Payload | 1);
			}
		}

		// cached hubs
		else if( strcmp(childPacket.Name, "/PO/CH") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize % 6 == 0)
			{
				for(int i = 0; i < childPacket.PayloadSize; i += 6)
				{
					IPv4 address;
					memcpy(&address.Host, childPacket.Payload, 4);
					memcpy(&address.Port, childPacket.Payload + 4, 2);
					Pong.Cached.push_back(address);
				}
			}
		}
	}
}

void CG2Protocol::Decode_LNI(G2_Header PacketLNI, G2_LNI &LocalNodeInfo)
{	
	ReadNodeInfo(PacketLNI, LocalNodeInfo.Node);

	LocalNodeInfo.Node.LastSeen = time(NULL);
}

void CG2Protocol::Decode_KHL(G2_Header PacketKHL, G2_KHL &KnownHubList)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketKHL, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		//  Timestamp
		if( strcmp(childPacket.Name, "/KHL/TS") == 0 )
		{
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
				memcpy( &KnownHubList.RefTime, childPacket.Payload, 4);
		}	

		// Neighbouring Hub
		else if( strcmp(childPacket.Name, "/KHL/NH") == 0 )
		{
			G2NodeInfo Neighbour;

			ReadNodeInfo(childPacket, Neighbour);

			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 6)
			{
				memcpy( &Neighbour.Address.Host, childPacket.Payload, 4);
				memcpy( &Neighbour.Address.Port, childPacket.Payload + 4, 2);
			}

			KnownHubList.Neighbours.push_back(Neighbour);
		}

		// Cached Hub
		else if( strcmp(childPacket.Name, "/KHL/CH") == 0 )
		{
			G2NodeInfo Cached;

			ReadNodeInfo(childPacket, Cached);

			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 10)
			{
				memcpy( &Cached.Address.Host, childPacket.Payload, 4);
				memcpy( &Cached.Address.Port, childPacket.Payload + 4, 2);
				memcpy( &Cached.LastSeen, childPacket.Payload + 6, 4);
			}

			KnownHubList.Cached.push_back(Cached);
		}
	}
}

void CG2Protocol::Decode_QHT(G2_Header PacketQHT, G2_QHT &QueryHashTable)
{
	if( ReadPayload(PacketQHT) )
	{
		// Reset
		if( PacketQHT.Payload[0] == 0 && PacketQHT.PayloadSize >= 6)
		{
			G2_QHT_Reset* Reset = (G2_QHT_Reset*) PacketQHT.Payload;

			QueryHashTable.Reset     = true;
			QueryHashTable.TableSize = Reset->table_size;
			QueryHashTable.Infinity  = Reset->infinity;
		}

		// Patch
		else if( PacketQHT.Payload[0] == 1 && PacketQHT.PayloadSize >= 5)
		{
			G2_QHT_Patch* Patch = (G2_QHT_Patch*) PacketQHT.Payload;

			QueryHashTable.PartNum    = Patch->fragment_no;
			QueryHashTable.PartTotal  = Patch->fragment_count;
			QueryHashTable.Compressed = Patch->compression;
			QueryHashTable.Bits       = Patch->bits;

			QueryHashTable.Part     = PacketQHT.Payload + 5;
			QueryHashTable.PartSize = PacketQHT.PayloadSize - 5;
		}
	}
}

void CG2Protocol::Decode_QKR(G2_Header PacketQKR, G2_QKR &QueryKeyRequest)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketQKR, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		//  Requesting Node Address
		if( strcmp(childPacket.Name, "/QKR/RNA") == 0 )
			if( ReadPayload(childPacket) )
			{
				if( childPacket.PayloadSize >= 4)
					memcpy( &QueryKeyRequest.RequestingAddress.Host, childPacket.Payload, 4);
				if(childPacket.PayloadSize >= 6)
					memcpy( &QueryKeyRequest.RequestingAddress.Port, childPacket.Payload + 4, 2);
			}
		
		if( strcmp(childPacket.Name, "/QKR/dna") == 0 )
			QueryKeyRequest.dna = true;

	}
}

void CG2Protocol::Decode_QKA(G2_Header PacketQKA, G2_QKA &QueryKeyAnswer)
{	
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketQKA, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		//  Query Key
		if( strcmp(childPacket.Name, "/QKA/QK") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
				memcpy( &QueryKeyAnswer.QueryKey, childPacket.Payload, 4);
				
		//  Sending Node Address
		if( strcmp(childPacket.Name, "/QKA/SNA") == 0 )
			if( ReadPayload(childPacket) )
			{
				if( childPacket.PayloadSize >= 4)
					memcpy( &QueryKeyAnswer.SendingAddress.Host, childPacket.Payload, 4);
				if(childPacket.PayloadSize >= 6)
					memcpy( &QueryKeyAnswer.SendingAddress.Port, childPacket.Payload + 4, 2);
			}

		//  Queried Node Address
		if( strcmp(childPacket.Name, "/QKA/QNA") == 0 )
			if( ReadPayload(childPacket) )
			{
				if( childPacket.PayloadSize >= 4)
					memcpy( &QueryKeyAnswer.QueriedAddress.Host, childPacket.Payload, 4);
				if(childPacket.PayloadSize >= 6)
					memcpy( &QueryKeyAnswer.QueriedAddress.Port, childPacket.Payload + 4, 2);
			}
	}
}

void CG2Protocol::Decode_Q2(G2_Header PacketQ2, G2_Q2 &Query)
{
	if( ReadPayload(PacketQ2) && PacketQ2.PayloadSize >= 16)
		memcpy( &Query.SearchGuid, PacketQ2.Payload, 16);

	ResetPacket(PacketQ2);


	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketQ2, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		// UDP
		if( strcmp(childPacket.Name, "/Q2/UDP") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 10)
			{
				memcpy( &Query.ReturnAddress, childPacket.Payload, 6);
				memcpy( &Query.QueryKey, childPacket.Payload + 6, 4);
			}
	
		// URN
		if( strcmp(childPacket.Name, "/Q2/URN") == 0 )
			if( ReadPayload(childPacket))
				Decode_URN(Query.URNs, childPacket.Payload, childPacket.PayloadSize);

		// Descriptive Name
		if( strcmp(childPacket.Name, "/Q2/DN") == 0 )
			if( ReadPayload(childPacket))
				Query.DescriptiveName = CString( (char*) childPacket.Payload, childPacket.PayloadSize);

		// Metadata 
		if( strcmp(childPacket.Name, "/Q2/MD") == 0 )
			if( ReadPayload(childPacket))
				Query.Metadata = CString( (char*) childPacket.Payload, childPacket.PayloadSize);

		// Size Restriction 
		if( strcmp(childPacket.Name, "/Q2/SZR") == 0 )
			if( ReadPayload(childPacket) )
			{
				if(childPacket.PayloadSize == 8)
				{
					memcpy( &Query.MinSize, childPacket.Payload, 4);
					memcpy( &Query.MaxSize, childPacket.Payload + 4, 4);
				}

				if(childPacket.PayloadSize >= 16)
				{
					memcpy( &Query.MinSize, childPacket.Payload, 8);
					memcpy( &Query.MaxSize, childPacket.Payload + 8, 8);
				}
			}

		// Interests
		if( strcmp(childPacket.Name, "/Q2/I") == 0 )
			if( ReadPayload(childPacket) )
			{
				int iPos = 0;

				while( iPos < childPacket.PayloadSize)
				{
					int iNull = memfind(childPacket.Payload + iPos, childPacket.PayloadSize - iPos, 0);

					if( iNull != -1)
					{
						CString intrest = CString( (char*) childPacket.Payload + iPos );
						Query.Interests.push_back( intrest );
					}
					else
						break;

					iPos += iNull + 1;
				}
			}

		if( strcmp(childPacket.Name, "/Q2/NAT") == 0 )
			Query.NAT = true;

		if( strcmp(childPacket.Name, "/Q2/dna") == 0 )
			Query.dna = true;
	}
}

void CG2Protocol::Decode_QA(G2_Header PacketQA, G2_QA &QueryAcknowledgement)
{
	if( ReadPayload(PacketQA) && PacketQA.PayloadSize >= 16)
		memcpy( &QueryAcknowledgement.SearchGuid, PacketQA.Payload, 16);

	ResetPacket(PacketQA);


	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketQA, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		//  Timestamp
		if( strcmp(childPacket.Name, "/QA/TS") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
				memcpy( &QueryAcknowledgement.Timestamp, childPacket.Payload, 4);
				
		//  Done Hub
		if( strcmp(childPacket.Name, "/QA/D") == 0 )
			if( ReadPayload(childPacket) )
			{
				if( childPacket.PayloadSize == 8)
				{
					G2NodeInfo DoneHub;
					memcpy( &DoneHub.LeafCount, childPacket.Payload + (childPacket.PayloadSize - 2), 2);
					memcpy( &DoneHub.Address, childPacket.Payload, (childPacket.PayloadSize - 2));

					QueryAcknowledgement.DoneHubs.push_back( DoneHub );
				}
			}

		//  Search Hub
		if( strcmp(childPacket.Name, "/QA/S") == 0 )
			if( ReadPayload(childPacket) )
			{
				if( childPacket.PayloadSize >= 6)
				{
					G2NodeInfo AltHub;
					memcpy( &AltHub.Address, childPacket.Payload, 6);
				
					if( childPacket.PayloadSize >= 10 )
						memcpy( &AltHub.LastSeen, childPacket.Payload + 6, 4);

					//ASSERT( AltHub.Address.Host.S_addr && AltHub.Address.Port );
					if(AltHub.Address.Host.S_addr && AltHub.Address.Port)
						QueryAcknowledgement.AltHubs.push_back( AltHub );
				}
			}
	
		// Retry After
		if( strcmp(childPacket.Name, "/QA/RA") == 0 )
			if( ReadPayload(childPacket) && (childPacket.PayloadSize == 2 || childPacket.PayloadSize == 4))
				memcpy( &QueryAcknowledgement.RetryAfter, childPacket.Payload, childPacket.PayloadSize);

		//  From Address
		if( strcmp(childPacket.Name, "/QA/FA") == 0 )
			if( ReadPayload(childPacket) )
			{
				if( childPacket.PayloadSize >= 4)
					memcpy( &QueryAcknowledgement.FromAddress.Host, childPacket.Payload, 4);
				if(childPacket.PayloadSize >= 6)
					memcpy( &QueryAcknowledgement.FromAddress.Port, childPacket.Payload + 4, 2);
			}
	}
}

void CG2Protocol::Decode_QH2(G2_Header PacketQH2, G2_QH2 &QueryHit)
{
	if( ReadPayload(PacketQH2) && PacketQH2.PayloadSize >= 17)
	{
		memcpy( &QueryHit.HopCount, PacketQH2.Payload, 1);
		memcpy( &QueryHit.SearchGuid, PacketQH2.Payload + 1, 16);
	}

	ResetPacket(PacketQH2);

	uint32 NextIndex = 0;

	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketQH2, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		// Node GUID
		if( strcmp(childPacket.Name, "/QH2/GU") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 16)
				memcpy( &QueryHit.NodeID, childPacket.Payload, childPacket.PayloadSize);
	
		//  Node Address
		if( strcmp(childPacket.Name, "/QH2/NA") == 0 )
			if( ReadPayload(childPacket)  && childPacket.PayloadSize >= 6)
				memcpy( &QueryHit.Address, childPacket.Payload, 6);
		
		// Neighbouring Hub
		if( strcmp(childPacket.Name, "/QH2/NH") == 0 )
			if( ReadPayload(childPacket)  && childPacket.PayloadSize >= 6)
			{
				IPv4 Address;
				memcpy( &Address, childPacket.Payload, 6);
				QueryHit.NeighbouringHubs.push_back(Address);
			}
		
		// Vendor Code
		if( strcmp(childPacket.Name, "/QH2/V") == 0 )
			if( ReadPayload(childPacket)  && childPacket.PayloadSize == 4)
				memcpy( &QueryHit.Vendor, childPacket.Payload, 4);
			
		// Firewall Tag
		if( strcmp(childPacket.Name, "/QH2/FW") == 0 )
			QueryHit.Firewalled = true;

		// Browse User Profile Tag
		if( strcmp(childPacket.Name, "/QH2/BUP") == 0 )
			QueryHit.Profile = true;

		// Peer Chat Tag
		if( strcmp(childPacket.Name, "/QH2/PCH") == 0 )
			QueryHit.Chat = true;

		
		// Hit Group Descriptor
		if( strcmp(childPacket.Name, "/QH2/HG") == 0 )
		{
			G2_QH2_HG HitGroup;

			G2_Header HGPacket;
			G2ReadResult HGStatus = ReadNextChild( childPacket, HGPacket );

			if( HGStatus == PACKET_GOOD )
				if( strcmp(HGPacket.Name, "/QH2/HG/SS") == 0 )
					if( ReadPayload(HGPacket)  && HGPacket.PayloadSize >= 7)
					{
						memcpy( &HitGroup.QueueLength, HGPacket.Payload, 2);
						memcpy( &HitGroup.Capacity, HGPacket.Payload + 2, 1);
						memcpy( &HitGroup.Speed, HGPacket.Payload + 3, 4);		
					}
			
			if(ReadPayload(childPacket) && childPacket.PayloadSize >= 1)
				memcpy( &HitGroup.GroupID, childPacket.Payload, 1);

			QueryHit.HitGroups.push_back( HitGroup );
		}


		// Hit Descriptor
		if( strcmp(childPacket.Name, "/QH2/H") == 0 )	
		{
			G2_QH2_H Hit;

			Hit.Index = NextIndex;
			NextIndex++;

			G2_Header HPacket;
			G2ReadResult HStatus = PACKET_GOOD;
			
			while( HStatus == PACKET_GOOD )
			{
				HStatus = ReadNextChild( childPacket, HPacket );

				if( HStatus != PACKET_GOOD )
					continue;

				// Universal Resource Name
				if( strcmp(HPacket.Name, "/QH2/H/URN") == 0 )
					if( ReadPayload(HPacket) )
						Decode_URN(Hit.URNs, HPacket.Payload, HPacket.PayloadSize);

				// Universal Resource Location
				if( strcmp(HPacket.Name, "/QH2/H/URL") == 0 )
					if( ReadPayload(HPacket) )
						Hit.URL = CString((char*) HPacket.Payload, HPacket.PayloadSize);

				// Descriptive Name (Generic) Criteria
				if( strcmp(HPacket.Name, "/QH2/H/DN") == 0 )
					if( ReadPayload(HPacket) )
					{
						memcpy(&Hit.ObjectSize, HPacket.Payload, 4);
						Hit.DescriptiveName = CString((char*) HPacket.Payload + 4, HPacket.PayloadSize - 4);
					}

				// Metadata
				if( strcmp(HPacket.Name, "/QH2/H/MD") == 0 )
					if( ReadPayload(HPacket) )
						Hit.Metadata = CString((char*) HPacket.Payload, HPacket.PayloadSize);

				// Object Size
				if( strcmp(HPacket.Name, "/QH2/H/SZ") == 0 )
					if( ReadPayload(HPacket) && (HPacket.PayloadSize == 4 || HPacket.PayloadSize == 8))
						memcpy( &Hit.ObjectSize, HPacket.Payload, HPacket.PayloadSize);

				// Group Identifier
				if( strcmp(HPacket.Name, "/QH2/H/G") == 0 )
					if( ReadPayload(HPacket) && HPacket.PayloadSize >= 1)
						memcpy( &Hit.GroupID, HPacket.Payload, 1);

				// Object Identifier
				if( strcmp(HPacket.Name, "/QH2/H/ID") == 0 )
					if( ReadPayload(HPacket) && HPacket.PayloadSize >= 4)
						memcpy( &Hit.ObjectID, HPacket.Payload, 4);

				// Cached Source Count
				if( strcmp(HPacket.Name, "/QH2/H/CSC") == 0 )
					if( ReadPayload(HPacket) && HPacket.PayloadSize >= 2)
						memcpy( &Hit.CachedSources, HPacket.Payload, 2);

				// Partial Content Tag
				if( strcmp(HPacket.Name, "/QH2/H/PART") == 0 )
					if( ReadPayload(HPacket) && HPacket.PayloadSize >= 4)
					{
						Hit.Partial = true;
						memcpy( &Hit.PartialSize, HPacket.Payload, 4);
					}

				// Comment
				if( strcmp(HPacket.Name, "/QH2/H/COM") == 0 )
					if( ReadPayload(HPacket) )
						Hit.Comment = CString((char*) HPacket.Payload, HPacket.PayloadSize);

				// Preview URL
				if( strcmp(HPacket.Name, "/QH2/H/PVU") == 0 )
				{
					Hit.Preview = true;
				
					if( ReadPayload(HPacket) )
						Hit.PreviewURL = CString((char*) HPacket.Payload, HPacket.PayloadSize);
				}
			}

			QueryHit.Hits.push_back( Hit );
		}


		// Unified Metadata Block
		if( strcmp(childPacket.Name, "/QH2/MD") == 0 )
			if( ReadPayload(childPacket) )
				QueryHit.UnifiedMetadata = CString((char*) childPacket.Payload, childPacket.PayloadSize);


		// User Profile
		if( strcmp(childPacket.Name, "/QH2/UPRO") == 0 )
		{
			G2_Header UProPacket;
			G2ReadResult UProStatus = ReadNextChild( childPacket, UProPacket );

			if( UProStatus == PACKET_GOOD )
				if( strcmp(UProPacket.Name, "/QH2/UPRO/NICK") == 0 )
					if( ReadPayload(UProPacket) )
						QueryHit.Nickname = CString((char*) UProPacket.Payload, UProPacket.PayloadSize);
		}
	}
}

void CG2Protocol::Decode_PUSH(G2_Header PacketPUSH, G2_PUSH &Push)
{
	if( ReadPayload(PacketPUSH) && PacketPUSH.PayloadSize >= 6)
		memcpy( &Push.BackAddress, PacketPUSH.Payload, 6);
}

void CG2Protocol::Decode_MCR(G2_Header PacketMCR, G2_MCR &ModeChangeRequest)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketMCR, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		// Hub Upgrade
		if( strcmp(childPacket.Name, "/MCR/HUB") == 0 )
			ModeChangeRequest.Hub = true;
	}
}

void CG2Protocol::Decode_MCA(G2_Header PacketMCA, G2_MCA &ModeChangeAck)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketMCA, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		// Hub
		if( strcmp(childPacket.Name, "/MCA/HUB") == 0 )
			ModeChangeAck.Hub = true;

		// Leaf
		if( strcmp(childPacket.Name, "/MCA/LEAF") == 0 )
			ModeChangeAck.Leaf = true;

		// Deny
		if( strcmp(childPacket.Name, "/MCA/DENY") == 0 )
			ModeChangeAck.Deny = true;
	}
}

void CG2Protocol::Decode_PM(G2_Header PacketPM, G2_PM &PrivateMessage)
{
	if( ReadPayload(PacketPM) && PacketPM.PayloadSize)
		PrivateMessage.Message = CString((char*) PacketPM.Payload, PacketPM.PayloadSize);


	ResetPacket(PacketPM);

	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketPM, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		// Unique ID
		if( strcmp(childPacket.Name, "/PM/ID") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
				memcpy( &PrivateMessage.UniqueID, childPacket.Payload, 4);

		// Destination Address
		if( strcmp(childPacket.Name, "/PM/DA") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 6)
				memcpy( &PrivateMessage.Destination, childPacket.Payload, 6);

		// Sending Node Address
		if( strcmp(childPacket.Name, "/PM/SNA") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 6)
				memcpy( &PrivateMessage.SendingAddress, childPacket.Payload, 6);

		// Neighboring hubs
		if( strcmp(childPacket.Name, "/PM/NH") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 6)
			{
				IPv4 Neighbor;
				memcpy( &Neighbor, childPacket.Payload, 6);
				PrivateMessage.Neighbours.push_back(Neighbor);
			}

		// Firewall
		if( strcmp(childPacket.Name, "/PM/FW") == 0 )
			PrivateMessage.Firewall = true;
	}
	
}

void CG2Protocol::Decode_CLOSE(G2_Header PacketCLOSE, G2_CLOSE &Close)
{
	// Reason
	if( ReadPayload(PacketCLOSE) && PacketCLOSE.PayloadSize)
		Close.Reason = CString((char*) PacketCLOSE.Payload, PacketCLOSE.PayloadSize);


	ResetPacket(PacketCLOSE);

	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketCLOSE, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		// Cached Hubs
		if( strcmp(childPacket.Name, "/CLOSE/CH") == 0 )
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 6)
			{
				IPv4 CachedHub;
				memcpy( &CachedHub, childPacket.Payload, 6);
				Close.CachedHubs.push_back(CachedHub);
			}
	}
}

void CG2Protocol::Decode_CRAWLR(G2_Header PacketCRAWLR, G2_CRAWLR &CrawlRequest)
{
	G2_Header childPacket;
	G2ReadResult childStatus = PACKET_GOOD;

	while( childStatus == PACKET_GOOD )
	{
		childStatus = ReadNextChild( PacketCRAWLR, childPacket );

		if( childStatus != PACKET_GOOD )
			continue;

		// Request Leaves
		if( strcmp(childPacket.Name, "/CRAWLR/RLEAF") == 0 )
			CrawlRequest.ReqLeaves = true;

		// Request Names
		if( strcmp(childPacket.Name, "/CRAWLR/RNAME") == 0 )
			CrawlRequest.ReqNames = true;

		// Request GPS
		if( strcmp(childPacket.Name, "/CRAWLR/RGPS") == 0 )
			CrawlRequest.ReqGPS = true;


		// Request G1
		if( strcmp(childPacket.Name, "/CRAWLR/RG1") == 0 )
			CrawlRequest.ReqG1 = true;

		// Request Extended
		if( strcmp(childPacket.Name, "/CRAWLR/REXT") == 0 )
			CrawlRequest.ReqExt = true;

		// Request ID
		if( strcmp(childPacket.Name, "/CRAWLR/RID") == 0)
			if( ReadPayload(childPacket) && childPacket.PayloadSize >= 4)
				memcpy( &CrawlRequest.ReqID, childPacket.Payload, 4);
	}
}

void CG2Protocol::Decode_URN(std::vector<CString> &URNs, byte* urn, int length)
{
	if( memcmp(urn, "bitprint\0", 9) == 0 && length == 9 + 20 + 24)
		URNs.push_back( "urn:bitprint:" + EncodeBase32(urn + 9, 20) + "." + EncodeBase32(urn + 29, 24) );

	else if( memcmp(urn, "bp\0", 3) == 0 && length == 3 + 20 + 24 && length == 3 + 20 + 24)
		URNs.push_back( "urn:bitprint:" + EncodeBase32(urn + 3, 20) + "." + EncodeBase32(urn + 23, 24) );

	else if( memcmp(urn, "sha1\0", 5) == 0 && length == 5 + 20 )
		URNs.push_back( "urn:sha1:" + EncodeBase32(urn + 5, 20) );

	else if( memcmp(urn, "tree:tiger/\0", 12) == 0 && length == 12 + 24 )
		URNs.push_back( "urn:tree:tiger/:" + EncodeBase32(urn + 12, 24) );

	else if( memcmp(urn, "tth\0", 4) == 0 && length == 4 + 24 )
		URNs.push_back( "urn:tree:tiger/:" + EncodeBase32(urn + 4, 24) );

	else if( memcmp(urn, "md5\0", 4) == 0 && length == 4 + 16 )
		URNs.push_back( "urn:md5:" + EncodeBase16(urn + 4, 16) );

	else if( memcmp(urn, "ed2k\0", 5) == 0 && length == 5 + 16 )
		URNs.push_back( "urn:ed2k:" + EncodeBase16(urn + 5, 16) );

	else
	{
		CString unknownUrn;
		int nullpos = memfind( urn, length, 0);

		if(nullpos > 0) // not -1
			unknownUrn = "urn:" + CString( (char*) urn ) + ":" + EncodeBase16( &urn[nullpos + 1], length - nullpos - 1);
		else
			unknownUrn = "urn:" + EncodeBase16( urn, length);

		URNs.push_back(unknownUrn);
	}

}
void CG2Protocol::Encode_PI(G2_PI &Ping)
{
	G2_Frame* pPI = WritePacket(NULL, "PI");

	if(Ping.Relay)
		WritePacket(pPI, "RELAY");

	if( Ping.UdpAddress.Host.S_addr )
	{
		byte assm[6];
		memcpy(assm, &Ping.UdpAddress.Host, 4);
		memcpy(assm + 4, &Ping.UdpAddress.Port, 2);
		WritePacket(pPI, "UDP", assm, 6);
	}

	if(Ping.Ident)
		WritePacket(pPI, "IDENT", &Ping.Ident, 4);
	
	if(Ping.TestFirewall)
		WritePacket(pPI, "TFW");
	
	if(Ping.ConnectRequest)
	{
		byte hubMode = (Ping.HubMode) ? 1 : 0;
		WritePacket(pPI, "CR", &hubMode, 1);
	}

	WriteFinish();
}

void CG2Protocol::Encode_PO(G2_PO &Pong)
{
	G2_Frame* pPO = WritePacket(NULL, "PO");

	if(Pong.Relay)
		WritePacket(pPO, "RELAY");

	if(Pong.ConnectAck)
	{
		byte space = (Pong.SpaceAvailable) ? 1 : 0;
		WritePacket(pPO, "CA", &space, 1);
	}

	
	if(Pong.Cached.size())
	{
		byte assm[6 * 5];
		int offset = 0;
		for(int i = 0; i < Pong.Cached.size() && i < 5; i++)
		{
			memcpy(assm + offset, &Pong.Cached[i].Host, 4);
			memcpy(assm + offset + 4, &Pong.Cached[i].Port, 2);
			offset += 6;
		}

		WritePacket(pPO, "CH", assm, offset);
	}

	WriteFinish();
}

void CG2Protocol::Encode_LNI(G2_LNI &LocalInfo)
{
	byte assm[32];

	G2_Frame* pLNI = WritePacket(NULL, "LNI");

	// Network Address
	WritePacket(pLNI, "NA", &LocalInfo.Node.Address, 6);

	// GUID
	WritePacket(pLNI, "GU", &LocalInfo.Node.NodeID, 16);

	// Vendor
	WritePacket(pLNI, "V", LocalInfo.Node.Vendor, 4);

	// Library Statistics
	memcpy(assm, &LocalInfo.Node.LibraryCount, 4);
	memcpy(assm + 4, &LocalInfo.Node.LibrarySizeKB, 4);

	WritePacket(pLNI, "LS", assm, 8);

	//Hub Status
	memcpy(assm, &LocalInfo.Node.LeafCount, 2);
	memcpy(assm + 2, &LocalInfo.Node.LeafMax, 2);

	WritePacket(pLNI, "HS", assm, 4);

	// Hub Able
	if(LocalInfo.Node.HubAble)
		WritePacket(pLNI, "HA");

	// Firewall
	if(LocalInfo.Node.Firewall)
		WritePacket(pLNI, "FW");

	// Router
	if(LocalInfo.Node.Router)
		WritePacket(pLNI, "RTR");

	// Bandwidth
	memcpy(assm, &LocalInfo.Node.NetBpsIn, 4);
	memcpy(assm + 4, &LocalInfo.Node.NetBpsOut, 4);
	WritePacket(pLNI, "NBW", assm, 8);

	// Cpu/Mem
	memcpy(assm, &LocalInfo.Node.Cpu, 2);
	memcpy(assm + 2, &LocalInfo.Node.Mem, 2);
	WritePacket(pLNI, "CM", assm, 4);

	// Uptime
	uint32 Uptime = time(NULL) - LocalInfo.Node.UpSince;
	WritePacket(pLNI, "UP", &Uptime, 4);

	// Location
	memcpy(assm, &LocalInfo.Node.Latitude, 2);
	memcpy(assm + 2, &LocalInfo.Node.Longitude, 2);
	WritePacket(pLNI, "GPS", assm, 4);

	WriteFinish();
}

void CG2Protocol::Encode_KHL(G2_KHL &KnownHubList)
{
	G2_Frame* pKHL = WritePacket(NULL, "KHL");

	if(KnownHubList.RefTime)
		WritePacket(pKHL, "TS", &KnownHubList.RefTime, 4);

	byte assm[32];

	for(int i = 0; i < KnownHubList.Neighbours.size(); i++)
	{
		G2NodeInfo* pNode = &KnownHubList.Neighbours[i];

		G2_Frame* pNH = WritePacket(pKHL, "NH", &pNode->Address, 6);

		WritePacket(pNH, "GU", &pNode->NodeID, 16);
		WritePacket(pNH, "V",  pNode->Vendor, 4);

		memcpy(assm, &pNode->LibraryCount, 4);
		memcpy(assm + 4, &pNode->LibrarySizeKB, 4);
		WritePacket(pNH, "LS",  assm, 8);

		memcpy(assm, &pNode->LeafCount, 2);
		memcpy(assm + 2, &pNode->LeafMax, 2);
		WritePacket(pNH, "HS",  assm, 4);
	}

	for(int i = 0; i < KnownHubList.Cached.size(); i++)
	{
		G2NodeInfo* pNode = &KnownHubList.Cached[i];

		memcpy(assm, &pNode->Address, 6);
		memcpy(assm + 6, &pNode->LastSeen, 4);
		G2_Frame* pCH = WritePacket(pKHL, "CH", assm, 10);

		WritePacket(pCH, "GU", &pNode->NodeID, 16);
		WritePacket(pCH, "V",  pNode->Vendor, 4);

		memcpy(assm, &pNode->LibraryCount, 4);
		memcpy(assm + 4, &pNode->LibrarySizeKB, 4);
		WritePacket(pCH, "LS",  assm, 8);

		memcpy(assm, &pNode->LeafCount, 2);
		memcpy(assm + 2, &pNode->LeafMax, 2);
		WritePacket(pCH, "HS",  assm, 4);
	}

	WriteFinish();
}

void CG2Protocol::Encode_QHT(G2_QHT &QueryHitTable)
{
	G2_Frame* pQHT = NULL;
	
	// Reset
	if( QueryHitTable.Reset )
	{
		G2_QHT_Reset Payload;
		Payload.command    = 0;
		Payload.table_size = QueryHitTable.TableSize;
		Payload.infinity   = QueryHitTable.Infinity;

		pQHT = WritePacket(NULL, "QHT", &Payload, 6);
	}
	
	// Patch
	else
	{
		ASSERT( QueryHitTable.Part );

		G2_QHT_Patch Payload;
		Payload.command		   = 1;
		Payload.fragment_no    = QueryHitTable.PartNum;
		Payload.fragment_count = QueryHitTable.PartTotal; 
		Payload.compression    = QueryHitTable.Compressed; 
		Payload.bits		   = QueryHitTable.Bits;  

		memcpy(QueryHitTable.Part, &Payload, 5);

		pQHT = WritePacket(NULL, "QHT", QueryHitTable.Part, QueryHitTable.PartSize + 5);
	}

	WriteFinish();
}

void CG2Protocol::Encode_QKR(G2_QKR &QueryKeyRequest)
{
	G2_Frame* pQKR = WritePacket(NULL, "QKR");

	// Requesting Node Address
	if( QueryKeyRequest.RequestingAddress.Host.S_addr )
	{
		byte assm[6];
		memcpy(assm, &QueryKeyRequest.RequestingAddress.Host, 4);
		memcpy(assm + 4, &QueryKeyRequest.RequestingAddress.Port, 2);
		WritePacket(pQKR, "RNA", assm, 6);
	}

	WritePacket(pQKR, "dna");

	WriteFinish();

	int i = 0;
	i++;
}

void CG2Protocol::Encode_QKA(G2_QKA &QueryKeyAnswer)
{
	G2_Frame* pQKA = WritePacket(NULL, "QKA");

	byte assm[6];
	
	// Query Key
	WritePacket(pQKA, "QK", &QueryKeyAnswer.QueryKey, 4);

	// Sending Node Address
	if( QueryKeyAnswer.SendingAddress.Host.S_addr )
	{
		memcpy(assm, &QueryKeyAnswer.SendingAddress.Host, 4);
		memcpy(assm + 4, &QueryKeyAnswer.SendingAddress.Port, 2);
		WritePacket(pQKA, "SNA", assm, 6);
	}
	else 
		ASSERT(0);

	// Queried Node Address
	if( QueryKeyAnswer.QueriedAddress.Host.S_addr )
	{
		memcpy(assm, &QueryKeyAnswer.QueriedAddress.Host, 4);
		memcpy(assm + 4, &QueryKeyAnswer.QueriedAddress.Port, 2);
		WritePacket(pQKA, "QNA", assm, 6);
	}
	
	WriteFinish();
}

void CG2Protocol::Encode_Q2(G2_Q2 &Query)
{
	G2_Frame* pQ2 = WritePacket(NULL, "Q2", &Query.SearchGuid, 16);

	byte assm[255];

	// UDP
	if( Query.ReturnAddress.Host.S_addr )
	{
		memcpy(assm, &Query.ReturnAddress, 6);
		memcpy(assm + 6, &Query.QueryKey, 4);
		WritePacket(pQ2, "UDP", assm, 10);
	}

	// URN
	for( int i = 0; i < Query.URNs.size(); i++)
		if( Query.URNs[i].GetLength() )
		{
			int length = 255;
			Encode_URN(Query.URNs[i], assm, length);

			WritePacket(pQ2, "URN", assm, length);
		}

	// Descriptive Name
	if( Query.DescriptiveName.GetLength() )
		WritePacket(pQ2, "DN", (byte*) ((LPCSTR) Query.DescriptiveName), Query.DescriptiveName.GetLength());

	// Metadata
	if( Query.Metadata.GetLength() )
		WritePacket(pQ2, "MD", (byte*) ((LPCSTR) Query.Metadata), Query.Metadata.GetLength());

	// Size Criteria
	if( Query.MinSize || Query.MaxSize )
	{
		memcpy(assm, &Query.MinSize, 8);
		memcpy(assm + 8, &Query.MaxSize, 8);
		WritePacket(pQ2, "SZR", assm, 16);
	}

	// Interests
	int ilength = 0;
	for( int i = 0; i < Query.Interests.size(); i++)
	{
		memcpy(assm + ilength, Query.Interests[i], Query.Interests[i].GetLength());
		memcpy(assm + ilength + 1, '\0', 1);
		
		ilength += Query.Interests[i].GetLength() + 1;
	}

	if( ilength)
		WritePacket(pQ2, "I", assm, ilength);

	if(Query.NAT)
		WritePacket(pQ2, "NAT");

	WritePacket(pQ2, "dna");
 

	WriteFinish();
}

void CG2Protocol::Encode_QA(G2_QA &QueryAck)
{
	G2_Frame* pQA = WritePacket(NULL, "QA", &QueryAck.SearchGuid, 16);

	byte assm[32];

	// Timestamp
	if( QueryAck.Timestamp )
		WritePacket(pQA, "TS", &QueryAck.Timestamp, 4);
	
	// Done hubs
	for(int i = 0; i < QueryAck.DoneHubs.size(); i++)
	{
		memcpy(assm, &QueryAck.DoneHubs[i].Address, 6);
		memcpy(assm + 6, &QueryAck.DoneHubs[i].LeafCount, 2);
		WritePacket(pQA, "D", assm, 8);
	}

	// Search hubs
	for(int i = 0; i < QueryAck.AltHubs.size(); i++)
	{
		memcpy(assm, &QueryAck.AltHubs[i].Address, 6);
		WritePacket(pQA, "S", assm, 6);
	}

	// Retry After
	if( QueryAck.RetryAfter )
		WritePacket(pQA, "RA", &QueryAck.RetryAfter, 4);

	// From Address
	if( QueryAck.FromAddress.Host.S_addr )
		WritePacket(pQA, "FR", &QueryAck.FromAddress, 6);

	WriteFinish();
}

void CG2Protocol::Encode_QH2(G2_QH2 &QueryHit)
{
	byte assm[255];

	memcpy(assm, &QueryHit.HopCount, 1);
	memcpy(assm + 1, &QueryHit.SearchGuid, 16);
	
	G2_Frame* pQH2 = WritePacket(NULL, "QH2", assm, 17);

	// Node GUID
	WritePacket(pQH2, "GU", &QueryHit.NodeID, 16);

	// Node Address
	WritePacket(pQH2, "NA", &QueryHit.Address, 6);

	// Neighbouring Hubs
	for(int i = 0; i < QueryHit.NeighbouringHubs.size(); i++)
		WritePacket(pQH2, "NH", &QueryHit.NeighbouringHubs[i], 6);

	// Vendor Code
	WritePacket(pQH2, "V", &QueryHit.Vendor, 4);

	// Firewall Tag
	if(QueryHit.Firewalled)
		WritePacket(pQH2, "FW");

	// Hit Group Descriptors
	for( i = 0; i < QueryHit.HitGroups.size(); i++)
	{
		G2_Frame* pHG = WritePacket(pQH2, "HG", &QueryHit.HitGroups[i].GroupID, 1);

		// Server State
		memcpy(assm,     &QueryHit.HitGroups[i].QueueLength, 2);
		memcpy(assm + 2, &QueryHit.HitGroups[i].Capacity,    1);
		memcpy(assm + 3, &QueryHit.HitGroups[i].Speed,       4);

		WritePacket(pHG, "SS", assm, 7);
	}

	// Hit Descriptors
	for( i = 0; i < QueryHit.Hits.size(); i++)
	{
		G2_Frame* pH = WritePacket(pQH2, "H");

		// URN
		for( int j = 0; j < QueryHit.Hits[i].URNs.size(); j++)
		{
			int length = 255;
			Encode_URN(QueryHit.Hits[i].URNs[j], assm, length);

			WritePacket(pH, "URN", assm, length);
		}

		// Universal Resource Location
		if( !QueryHit.Hits[i].URL.IsEmpty() )
			WritePacket(pH, "URL", (byte*) ((LPCSTR) QueryHit.Hits[i].URL), QueryHit.Hits[i].URL.GetLength());
		else
			WritePacket(pH, "URL");

		// Descriptive Name 
		if( !QueryHit.Hits[i].DescriptiveName.IsEmpty() )
			if( QueryHit.Hits[i].DescriptiveName.GetLength() + 4 < 255 )
			{
				memset(assm, 0, 4);
				memcpy(assm + 4, (LPCSTR) QueryHit.Hits[i].DescriptiveName, QueryHit.Hits[i].DescriptiveName.GetLength());
			
				WritePacket(pH, "DN", assm, 4 + QueryHit.Hits[i].DescriptiveName.GetLength() );
			}

		// Metadata
		if( !QueryHit.Hits[i].Metadata.IsEmpty() )
			WritePacket(pH, "MD", (byte*) ((LPCSTR) QueryHit.Hits[i].Metadata), QueryHit.Hits[i].Metadata.GetLength());

		// Object Size
		WritePacket(pH, "SZ", &QueryHit.Hits[i].ObjectSize, 8);

		// Group Identifier
		if( QueryHit.Hits[i].GroupID )
			WritePacket(pH, "G", &QueryHit.Hits[i].GroupID, 1);

		// Object Identifier
		if( QueryHit.Hits[i].ObjectID )
			WritePacket(pH, "ID", &QueryHit.Hits[i].ObjectID, 4);

		// Cached Source Count
		if( QueryHit.Hits[i].CachedSources )
			WritePacket(pH, "CSC", &QueryHit.Hits[i].CachedSources, 2);
	}

	WriteFinish();

	/*CString Hex;
	for(int i = 0; i < m_FinalSize; i++)
		Hex += EncodeBase16(m_FinalPacket + i, 1) + " ";

	CString Final = Hex;*/

}

void CG2Protocol::Encode_PUSH(G2_PUSH &Push)
{
	G2_Frame* pPUSH = WritePacket(NULL, "PUSH", &Push.BackAddress, 6);

	// TO Packet
	WritePacket(pPUSH, "TO", &Push.Destination, 16);

	WriteFinish();
}

void CG2Protocol::Encode_MCR(G2_MCR &ModeChangeRequest)
{
	G2_Frame* pMCR = WritePacket(NULL, "MCR");

	if(ModeChangeRequest.Hub)
		WritePacket(pMCR, "HUB");

	WriteFinish();
}	

void CG2Protocol::Encode_MCA(G2_MCA &ModeChangeAck)
{
	G2_Frame* pMCA = WritePacket(NULL, "MCA");

	if(ModeChangeAck.Hub)
		WritePacket(pMCA, "HUB");
	if(ModeChangeAck.Leaf)
		WritePacket(pMCA, "LEAF");
	if(ModeChangeAck.Deny)
		WritePacket(pMCA, "DENY");

	WriteFinish();
}

void CG2Protocol::Encode_PM(G2_PM &PrivateMessage)
{
	G2_Frame* pPM = WritePacket(NULL, "PM", (byte*) ((LPCSTR) PrivateMessage.Message), PrivateMessage.Message.GetLength());

	WritePacket(pPM, "ID", &PrivateMessage.UniqueID, 4);

	WritePacket(pPM, "DA", &PrivateMessage.Destination, 6);
	
	if(PrivateMessage.SendingAddress.Host.S_addr)
		WritePacket(pPM, "SNA", &PrivateMessage.SendingAddress, 6);
	
	for(int i = 0; i < PrivateMessage.Neighbours.size(); i++)
		WritePacket(pPM, "NH", &PrivateMessage.Neighbours[i], 6);

	if(PrivateMessage.Firewall)
		WritePacket(pPM, "FW");

	WriteFinish();
}

void CG2Protocol::Encode_CLOSE(G2_CLOSE &Close)
{
	G2_Frame* pCLOSE = WritePacket(NULL, "CLOSE", (byte*) ((LPCSTR) Close.Reason), Close.Reason.GetLength());

	for(int i = 0; i < Close.CachedHubs.size(); i++)
		WritePacket(pCLOSE, "CH", &Close.CachedHubs[i], 6);

	WriteFinish(); 
}

void CG2Protocol::Encode_CRAWLA(G2_CRAWLA &CrawlAck)
{
	G2_Frame* pCRAWLA = WritePacket(NULL, "CRAWLA");

	int  i = 0;

	if(CrawlAck.Network == NETWORK_G2)
	{
		// G2 Self
		G2_Frame* pG2SELF = WritePacket(pCRAWLA, "SELF");

		if(CrawlAck.G2Self.Mode == G2_HUB)
			WritePacket(pG2SELF, "G2HUB");
		if(CrawlAck.G2Self.Mode == G2_CHILD)
			WritePacket(pG2SELF, "G2LEAF");

		Encode_G2CrawlInfo(pG2SELF, CrawlAck.G2Self, CrawlAck);

		// G2 Hubs
		for(i = 0; i < CrawlAck.G2Hubs.size(); i++)
		{
			G2_Frame* pNH = WritePacket(pCRAWLA, "NH");
			Encode_G2CrawlInfo(pNH, CrawlAck.G2Hubs[i], CrawlAck);
		}

		// G2 Leaves
		for(i = 0; i < CrawlAck.G2Leaves.size(); i++)
		{
			G2_Frame* pNL = WritePacket(pCRAWLA, "NL");
			Encode_G2CrawlInfo(pNL, CrawlAck.G2Leaves[i], CrawlAck);
		}
	}


	if(CrawlAck.Network == NETWORK_GNUTELLA)
	{
		// Gnu Self
		G2_Frame* pGnuSELF = WritePacket(pCRAWLA, "G1SELF");

		if(CrawlAck.GnuSelf.Mode == GNU_ULTRAPEER)
			WritePacket(pGnuSELF, "G1UP");
		if(CrawlAck.GnuSelf.Mode == GNU_LEAF)
			WritePacket(pGnuSELF, "G1LEAF");
		
		Encode_GnuCrawlInfo(pGnuSELF, CrawlAck.GnuSelf, CrawlAck, true);

		// Gnu Hubs
		for(i = 0; i < CrawlAck.GnuUPs.size(); i++)
		{
			G2_Frame* pG1NH = WritePacket(pCRAWLA, "G1NH");			
			Encode_GnuCrawlInfo(pG1NH, CrawlAck.GnuUPs[i], CrawlAck, false);
		}

		// Gnu Leaves
		for(i = 0; i < CrawlAck.GnuLeaves.size(); i++)
		{
			G2_Frame* pG1NL = WritePacket(pCRAWLA, "G1NL");
			Encode_GnuCrawlInfo(pG1NL, CrawlAck.GnuLeaves[i], CrawlAck, false);
		}
	}

	// ID
	if(CrawlAck.OrigRequest.ReqID)
		WritePacket(pCRAWLA, "ID", &CrawlAck.OrigRequest.ReqID, 4);

	
	WriteFinish();
}

void CG2Protocol::Encode_G2CrawlInfo(G2_Frame* pNode,  G2NodeInfo &G2Node,   G2_CRAWLA &CrawlAck)
{
	byte assm[32];

	// Network Address
	WritePacket(pNode, "NA", &G2Node.Address, 6);
	
	// Hub Status
	if(G2Node.LeafCount || G2Node.LeafMax)
	{
		memcpy(assm, &G2Node.LeafCount, 2);
		memcpy(assm + 2, &G2Node.LeafMax, 2);
		WritePacket(pNode, "HS", assm, 4);
	}

	// Username
	//if(CrawlAck.OrigRequest.ReqNames && !NodeInfo.Node.Name.IsEmpty())
	//	WritePacket(pNode, "NAME", (byte*) ((LPCSTR) NodeInfo.Node.Namee), NodeInfo.Node.Name.GetLength());

	// GPS
	if(CrawlAck.OrigRequest.ReqGPS && (G2Node.Latitude || G2Node.Longitude))
	{
		memcpy(assm, &G2Node.Latitude, 2);
		memcpy(assm + 2, &G2Node.Longitude, 2);
		WritePacket(pNode, "GPS", assm, 4);
	}

	// Extended
	if(CrawlAck.OrigRequest.ReqExt)
	{
		// Client version
		WritePacket(pNode, "CV", (byte*) ((LPCSTR) G2Node.Client), G2Node.Client.GetLength());

		// Cpu/Mem
		if(G2Node.Cpu || G2Node.Mem)
		{
			memcpy(assm, &G2Node.Cpu, 2);
			memcpy(assm + 2, &G2Node.Mem, 2);
			WritePacket(pNode, "CM", assm, 4);
		}

		// Firewall
		if(G2Node.Firewall)
			WritePacket(pNode, "FW");

		// Hub Able
		if(G2Node.HubAble)
			WritePacket(pNode, "HA");

		// Router
		if(G2Node.Router)
			WritePacket(pNode, "RTR");

		// Library Statistics
		if(G2Node.LibraryCount || G2Node.LibrarySizeKB)
		{
			memcpy(assm, &G2Node.LibraryCount, 4);
			memcpy(assm + 4, &G2Node.LibrarySizeKB, 4);
			WritePacket(pNode, "LS", assm, 8);
		}

		// Bandwidth
		if(G2Node.NetBpsIn || G2Node.NetBpsOut)
		{
			memcpy(assm, &G2Node.NetBpsIn, 4);
			memcpy(assm + 4, &G2Node.NetBpsOut, 4);
			WritePacket(pNode, "NBW", assm, 8);
		}

		// Udp Bandwidth
		if(G2Node.UdpBpsIn || G2Node.UdpBpsOut)
		{
			memcpy(assm, &G2Node.UdpBpsIn, 4);
			memcpy(assm + 4, &G2Node.UdpBpsOut, 4);
			WritePacket(pNode, "UBW", assm, 8);
		}

		// Uptime
		if(G2Node.UpSince > 0)
		{
			uint32 Uptime = time(NULL) - G2Node.UpSince;
			WritePacket(pNode, "UP", &Uptime, 4);
		}

		// Connect Uptime
		if(G2Node.ConnectUptime > 0)
			WritePacket(pNode, "CUP", &G2Node.ConnectUptime, 4);

		// Avg QKRs
		if(G2Node.PacketsQKR[AVG_TOTAL]) // In self only
		{
			memcpy(assm, &G2Node.PacketsQKR[AVG_DNA], 2);
			memcpy(assm + 2, &G2Node.PacketsQKR[AVG_TOTAL], 2);
			WritePacket(pNode, "qkrs", assm, 4);
		}

		// Avg Q2s
		if(G2Node.PacketsQ2[AVG_TOTAL]) // In self only
		{
			memcpy(assm, &G2Node.PacketsQ2[AVG_DNA], 2);
			memcpy(assm + 2, &G2Node.PacketsQ2[AVG_TOTAL], 2);
			WritePacket(pNode, "q2s", assm, 4);
		}
	}
}

void CG2Protocol::Encode_GnuCrawlInfo(G2_Frame* pNode, GnuNodeInfo &GnuNode, G2_CRAWLA &CrawlAck, bool Self)
{
	byte assm[32];

	// Network Address
	WritePacket(pNode, "NA", &GnuNode.Address, 6);

	
	// Hub Status
	memcpy(assm, &GnuNode.LeafCount, 2);
	memcpy(assm + 2, &GnuNode.LeafMax, 2);
	WritePacket(pNode, "HS", assm, 4);


	// GPS
	if(CrawlAck.OrigRequest.ReqGPS && (GnuNode.Latitude || GnuNode.Longitude))
	{
		memcpy(assm, &GnuNode.Latitude, 2);
		memcpy(assm + 2, &GnuNode.Longitude, 2);
		WritePacket(pNode, "GPS", assm, 4);
	}

	// Extended
	if(CrawlAck.OrigRequest.ReqExt)
	{
		// Client version
		WritePacket(pNode, "CV", (byte*) ((LPCSTR) GnuNode.Client), GnuNode.Client.GetLength());

		// Cpu/Mem
		if(GnuNode.Cpu || GnuNode.Mem)
		{
			memcpy(assm, &GnuNode.Cpu, 2);
			memcpy(assm + 2, &GnuNode.Mem, 2);
			WritePacket(pNode, "CM", assm, 4);
		}

		// Firewall
		if(GnuNode.Firewall)
			WritePacket(pNode, "FW");

		// Hub Able
		if(GnuNode.HubAble)
			WritePacket(pNode, "HA");

		// Router
		if(GnuNode.Router)
			WritePacket(pNode, "RTR");

		// Library Statistics
		if(GnuNode.LibraryCount || GnuNode.LibrarySizeKB)
		{
			memcpy(assm, &GnuNode.LibraryCount, 4);
			memcpy(assm + 4, &GnuNode.LibrarySizeKB, 4);
			WritePacket(pNode, "LS", assm, 8);
		}

		// Bandwidth
		if(GnuNode.NetBpsIn || GnuNode.NetBpsOut)
		{
			memcpy(assm, &GnuNode.NetBpsIn, 4);
			memcpy(assm + 4, &GnuNode.NetBpsOut, 4);
			WritePacket(pNode, "NBW", assm, 8);
		}

		// Udp Bandwidth
		if(GnuNode.UdpBpsIn || GnuNode.UdpBpsOut)
		{
			memcpy(assm, &GnuNode.UdpBpsIn, 4);
			memcpy(assm + 4, &GnuNode.UdpBpsOut, 4);
			WritePacket(pNode, "UBW", assm, 8);
		}

		// Uptime
		if(GnuNode.UpSince > 0)
		{
			uint32 Uptime = time(NULL) - GnuNode.UpSince;
			WritePacket(pNode, "UP", &Uptime, 4);
		}

		// Connect Uptime
		if(GnuNode.ConnectUptime > 0)
			WritePacket(pNode, "CUP", &GnuNode.ConnectUptime, 4);
	}
}

void CG2Protocol::Encode_URN(CString strUrn, byte* urn, int &length)
{
	// send bp and tth over network
	int urnSize = length;

	ASSERT( strUrn.Left(4) == "urn:" );

	strUrn = strUrn.Mid(4);

	if( strUrn.Left(9) == "bitprint:"  && strUrn.GetLength() == 9 + 32 + 1 + 39)
	{	
		length = 3;
		memcpy(urn, "bp\0", 3);

		length += DecodeLengthBase32(32);
		DecodeBase32( strUrn.Mid(9), 32, urn + 3, urnSize - 3);

		length += DecodeLengthBase32(39);
		DecodeBase32( strUrn.Mid(9 + 32 + 1), 39, urn + 3 + 20, urnSize - 3 - 20);
	}

	else if( strUrn.Left(12) == "tree:tiger/:" && strUrn.GetLength() == 12 + 39)
	{
		length = 4;
		memcpy(urn, "tth\0", 4);
		
		length += DecodeLengthBase32(39);
		DecodeBase32( strUrn.Mid(12), 39, urn + 4, urnSize - 4);
	}
	
	else if( strUrn.Left(5) == "sha1:" && strUrn.GetLength() == 5 + 32)
	{
		length = 5;
		memcpy(urn, "sha1\0", 5);

		length += DecodeLengthBase32(32);
		DecodeBase32( strUrn.Mid(5), 32, urn + 5, urnSize - 5);
	}

	else if( strUrn.Left(4) == "md5:" && strUrn.GetLength() == 4 + 32)
	{
		length = 4;
		memcpy(urn, "md5\0", 4);

		length += DecodeLengthBase16(32);
		DecodeBase16( strUrn.Mid(4), 32, urn + 4, urnSize - 4);
	}

	else if( strUrn.Left(5) == "ed2k:" && strUrn.GetLength() == 5 + 32)
	{
		length = 5;
		memcpy(urn, "ed2k\0", 5);

		length += DecodeLengthBase16(32);
		DecodeBase16( strUrn.Mid(5), 32, urn + 5, urnSize - 5);
	}

	else if( strUrn.Find(":") > 0)
	{
		int colonpos = strUrn.Find(":");

		length = colonpos;
		memcpy( urn, strUrn.Left(colonpos), length);

		length++;
		memcpy(urn + colonpos, "\0", 1); 

		strUrn = strUrn.Mid(colonpos + 1);

		length += DecodeLengthBase16( strUrn.GetLength() );
		DecodeBase16( strUrn, strUrn.GetLength(), urn + colonpos + 1, urnSize - colonpos - 1);
	}
	
	else
	{
		length = DecodeLengthBase16(32);
		DecodeBase16( strUrn, 32, urn, urnSize);
	}
}



