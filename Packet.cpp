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
#include "Packet.h"

// Get rid of ugly warnings
#pragma warning (disable : 4786)


Node::Node()
{ 
	Network = NETWORK_GNUTELLA;
	Host = "";
	Port = 0;
	LastSeen = 0;
}

Node::Node(CString HostPort)
{ 
	*this = HostPort; 

	Network = NETWORK_GNUTELLA;
	LastSeen = 0;
}

Node::Node(CString nHost, UINT nPort, int nNetwork, CTime tLastSeen)
{
	Network = nNetwork;
	Host = nHost;
	Port = nPort;
	LastSeen = tLastSeen;
}
	
// Allow Node = "host:port" assignment
Node& Node::operator=(CString &rhs)
{
	int pos = rhs.Find(":") + 1;

	// check for a valid string
	if (pos <= 1 || pos >= rhs.GetLength())
	{
		Host = "";
		Port = 0;
	}
	else
	{
		Host = rhs.Left(pos - 1);
		Port = atoi(rhs.Mid(pos, rhs.GetLength() - pos));
	}

	Network = NETWORK_GNUTELLA;
	LastSeen = 0;

	return *this;
}

CString Node::GetString()
{
	return (Host + ":" + NumtoStr(Port));
}
