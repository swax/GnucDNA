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

#include "GnuDatagram.h"

CGnuDatagram::CGnuDatagram(CGnuControl* pComm)
{
	m_pComm = pComm;
}

CGnuDatagram::~CGnuDatagram()
{

}

void CGnuDatagram::Init()
{
	Close();

	// Cant use same port, g2 already using it
	m_pComm->m_UdpPort = m_pComm->m_pNet->m_CurrentPort + 1;

	if(!Create(m_pComm->m_UdpPort, SOCK_DGRAM))
	{
		int error = GetLastError();
		ASSERT(0);
	}
}

void CGnuDatagram::Timer()
{

}

void CGnuDatagram::OnReceive(int nErrorCode)
{

}
