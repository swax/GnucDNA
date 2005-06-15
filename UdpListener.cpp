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
#include "GnuControl.h"
#include "GnuDatagram.h"
#include "G2Control.h"
#include "G2Datagram.h"
#include "UdpListener.h"

CUdpListener::CUdpListener(CGnuNetworks* pNet)
{
	m_pNet = pNet;

	// prevent sending to bad ports from triggering onreceive
	DWORD bNewBehavior = FALSE;
	int result = ioctlsocket(m_hSocket, SIO_UDP_CONNRESET, &bNewBehavior);
}

CUdpListener::~CUdpListener(void)
{
	Close();

	if(m_hSocket != INVALID_SOCKET)
		AsyncSelect(0);
}

void CUdpListener::OnReceive(int nErrorCode)
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

	IPv4 Address;
	Address.Host = StrtoIP(Host);
	Address.Port = Port;

	// Route to G2
	if( memcmp(m_pRecvBuff, "GND", 3) == 0 )
	{
		if( m_pNet->m_pG2 )
			m_pNet->m_pG2->m_pDispatch->OnReceive(Address, m_pRecvBuff, RecvLength);
	}

	// Route to Gnutella
	else if(RecvLength >= 23)
	{
		// gnutella transfer
		if(m_pRecvBuff[16] == 0x41) // 0x41 is the function of the gnutella packet
		{
			std::multimap<uint32, CReliableSocket*>::iterator itSocket = m_TransferMap.find(Address.Host.S_addr);

			while(itSocket != m_TransferMap.end() && itSocket->first == Address.Host.S_addr)
			{
				Gnu_RecvdPacket Packet( Address, (packet_Header*) m_pRecvBuff, RecvLength);
				itSocket->second->RudpReceive(Packet);
				itSocket++;
			}
		}

		// gnutella network
		else if( m_pNet->m_pGnu )
			m_pNet->m_pGnu->m_pDatagram->OnReceive(Address, m_pRecvBuff, RecvLength);
	}
}
