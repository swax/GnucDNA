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
	license your contribution.

	For support, questions, commercial use, etc...
	E-Mail: swabby@c0re.net

********************************************************************************/

#include "stdafx.h"

#include "GnuNetworks.h"
#include "GnuControl.h"
#include "GnuProtocol.h"
#include "GnuCore.h"
#include "UdpListener.h"

#include "DnaCore.h"
#include "DnaEvents.h"

#include "GnuDatagram.h"

CGnuDatagram::CGnuDatagram(CGnuControl* pComm)
{
	m_pComm     = pComm;
	m_pProtocol = pComm->m_pProtocol;

	m_AvgUdpDown.SetRange(30);
	m_AvgUdpUp.SetRange(30);

	m_UdpSecBytesDown = 0;
	m_UdpSecBytesUp   = 0;

}

CGnuDatagram::~CGnuDatagram()
{

}

void CGnuDatagram::Timer()
{
	m_AvgUdpDown.Update(m_UdpSecBytesDown);
	m_AvgUdpUp.Update(m_UdpSecBytesUp);

	m_UdpSecBytesDown = 0;
	m_UdpSecBytesUp   = 0;
}

void CGnuDatagram::OnReceive(IPv4 Address, byte* pRecvBuff, int RecvLength)
{

	m_UdpSecBytesDown += RecvLength;

	if(RecvLength >= 23)
	{
		Gnu_RecvdPacket Packet( Address, (packet_Header*) pRecvBuff, RecvLength);

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

	int UdpSent = 0;

	if(m_pComm->m_pNet->m_pUdpSock)
		UdpSent = m_pComm->m_pNet->m_pUdpSock->SendTo( packet, length, (SOCKADDR*) &sa, sizeof(SOCKADDR) );

	m_UdpSecBytesUp += length;
}