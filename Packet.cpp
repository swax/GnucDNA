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
#include "Packet.h"

// Get rid of ugly warnings
#pragma warning (disable : 4786)


Node::Node()
{ 
	Network  = NETWORK_GNUTELLA;
	Host     = "";
	Port     = 0;
	LastSeen = 0;
	DNA		 = false;
}

Node::Node(CString HostPort)
{ 
	*this = HostPort; 

	Network  = NETWORK_GNUTELLA;
	LastSeen = 0;
	DNA		 = false;
}

Node::Node(CString nHost, UINT nPort, int nNetwork, CTime tLastSeen, bool bDNA)
{
	Network  = nNetwork;
	Host	 = nHost;
	Port	 = nPort;
	LastSeen = tLastSeen;
	DNA		 = bDNA;
}
	
// Allow Node = "host:port" assignment
Node& Node::operator=(CString &rhs)
{
	CString Address = rhs;

	Host = ParseString(Address, ':');
	Port = atoi(Address);

	Network  = NETWORK_GNUTELLA;
	LastSeen = 0;
	DNA		 = false;

	return *this;
}

CString Node::GetString()
{
	return (Host + ":" + NumtoStr(Port));
}
