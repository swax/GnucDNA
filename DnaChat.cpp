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
#include "GnuCore.h"
#include "DnaCore.h"
#include "GnuChat.h"

#include "DnaChat.h"


// CDnaChat

CDnaChat::CDnaChat()
{
	m_dnaCore  = NULL;
	m_gnuChat  = NULL;

}

void CDnaChat::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;
	m_gnuChat = dnaCore->m_gnuCore->m_pChat;
}

CDnaChat::~CDnaChat()
{
}

// CDnaChat message handlers


void CDnaChat::SendDirectMessage(LPCTSTR Address, LPCTSTR Message)
{

	m_gnuChat->SendDirectMessage(Address, Message);
}
