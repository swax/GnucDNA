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

#include "GnuNetworks.h"
#include "GnuControl.h"
#include "GnuProtocol.h"
#include "GnuCore.h"

#include "DnaCore.h"
#include "DnaEvents.h"

#include "GnuDatagram.h"

CGnuDatagram::CGnuDatagram(CGnuControl* pComm)
{
	m_pComm = pComm;

	m_AvgUdpDown.SetRange(30);
	m_AvgUdpUp.SetRange(30);

	m_UdpSecBytesDown = 0;
	m_UdpSecBytesUp   = 0;

}

CGnuDatagram::~CGnuDatagram()
{

}

void CGnuDatagram::Init()
{
	m_pProtocol = m_pComm->m_pProtocol;

	Close();

	// Cant use same port, g2 already using it
	m_pComm->m_UdpPort = m_pComm->m_pNet->m_CurrentPort + 13;

	if(!Create(m_pComm->m_UdpPort, SOCK_DGRAM))
	{
		int error = GetLastError();
		ASSERT(0);
	}
}

void CGnuDatagram::Timer()
{
	m_AvgUdpDown.Update(m_UdpSecBytesDown);
	m_AvgUdpUp.Update(m_UdpSecBytesUp);

	m_UdpSecBytesDown = 0;
	m_UdpSecBytesUp   = 0;
}

void CGnuDatagram::OnReceive(int nErrorCode)
{
	CString Host;
	UINT    Port;
	int RecvLength = ReceiveFrom(m_pRecvBuff, GNU_RECV_BUFF, Host, Port);

	if(RecvLength == 0)
	{
		// Connection Closed
		return;
	}
	else if(RecvLength == SOCKET_ERROR)
	{
		int ErrorCode = GetLastError();
		return;
	}

	m_UdpSecBytesDown += RecvLength;

	IPv4 Address;
	Address.Host = StrtoIP(Host);
	Address.Port = Port;

	if(RecvLength >= 23)
	{
		Gnu_RecvdPacket Packet( Address, (packet_Header*) m_pRecvBuff, RecvLength);

		if(RecvLength == 23 + Packet.Header->Payload)
			m_pProtocol->ReceivePacket( Packet );
	}
}

void CGnuDatagram::SendPacket(IPv4 Address, byte* packet, uint32 length)
{
	if(m_pComm->m_pCore->m_dnaCore->m_dnaEvents)
		m_pComm->m_pCore->m_dnaCore->m_dnaEvents->NetworkPacketOutgoing(NETWORK_GNUTELLA, false , Address.Host.S_addr, Address.Port, packet, length, false);
	
	m_pComm->m_pNet->AddNatDetect(Address.Host);

	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_port   = htons(Address.Port);
	sa.sin_addr.S_un.S_addr = Address.Host.S_addr;

	int UdpSent = SendTo( packet, length, (SOCKADDR*) &sa, sizeof(SOCKADDR) );

	m_UdpSecBytesUp += length;
}