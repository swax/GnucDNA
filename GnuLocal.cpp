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


#include "stdafx.h"
#include "GnuCore.h"

#include "GnuPrefs.h"
#include "GnuNetworks.h"
#include "GnuCache.h"
#include "GnuRouting.h"
#include "GnuControl.h"

#include "GnuLocal.h"


CGnuLocal::CGnuLocal(CGnuControl* pComm)
{
	m_pComm  = pComm;
	m_pNet   = pComm->m_pNet;
	m_pCore  = pComm->m_pCore;
	m_pPrefs = pComm->m_pPrefs;

	m_Broadcasted = true;

	m_NextLanID = 1;
}

CGnuLocal::~CGnuLocal()
{
	if(m_hSocket != INVALID_SOCKET)
		AsyncSelect(0);
}


// Do not edit the following lines, which are needed by ClassWizard.
#if 0
BEGIN_MESSAGE_MAP(CGnuLocal, CAsyncSocketEx)
	//{{AFX_MSG_MAP(CGnuLocal)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
#endif	// 0


void CGnuLocal::Init()
{
	/*if(!Create(UDP_PORT, SOCK_DGRAM))
	{
		m_pCore->LogError("Socket Create Error #" + NumtoStr(GetLastError()));
		return;
	}

	if(!SetSockOpt(SO_BROADCAST, &m_Broadcasted, sizeof(int)))
	{
		m_pCore->LogError("Socket Set Error #" + NumtoStr(GetLastError()));
		return;
	}

	SendPing();*/
}

void CGnuLocal::LanModeOn()
{
	m_pCore->Disconnect(NETWORK_GNUTELLA);
	m_pCore->Disconnect(NETWORK_G2);

	m_pCore->Connect(NETWORK_GNUTELLA);

	/*m_pComm->m_pCache->m_GnuPerm.clear();
	m_pComm->m_pCache->m_GnuReal.clear();
	m_pComm->m_pCache->m_AltWebCaches.clear();
	m_pPrefs->m_BlockList.clear();*/

	m_pPrefs->m_LanMode = true;

	m_pComm->m_NetworkName	= "GNUCLEAR";

	SendPing();
}

void CGnuLocal::JoinLan(CString LanName)
{
	m_pPrefs->m_LanName = LanName;


	CString LocalHost;
	UINT nPort;
	GetSockName(LocalHost, nPort);

	std::map<int, LanNode>::iterator itLanNode;
	for(itLanNode = m_LanNodeIDMap.begin(); itLanNode != m_LanNodeIDMap.end(); itLanNode++)
		if(itLanNode->second.Name == LanName)
		{
			Node PermNode;
			PermNode.Host = itLanNode->second.Host;
			PermNode.Port = itLanNode->second.Port;
			
			if(PermNode.Host != LocalHost)
				m_pComm->m_pCache->m_GnuPerm.push_back(PermNode);
		}

	m_pCore->Disconnect(NETWORK_GNUTELLA);
	m_pCore->Disconnect(NETWORK_G2);

	m_pCore->Connect(NETWORK_GNUTELLA);
}

void CGnuLocal::LanModeOff(void)
{
	//m_pComm->m_pCache->m_GnuPerm.clear();
	//m_pComm->m_pCache->m_GnuReal.clear();
	//m_pComm->m_pCache->m_AltWebCaches.clear();
	//m_pPrefs->m_BlockList.clear();

	m_pComm->m_NetworkName	= "GNUTELLA";

	m_pPrefs->m_LanMode = false;
}

void CGnuLocal::SendPing()
{
	m_LanNodeIDMap.clear();

	CString NetworkPing;
	NetworkPing  = m_pComm->m_NetworkName;
	NetworkPing += " PING\r\n";
	NetworkPing += "Port: " + NumtoStr(m_pNet->m_CurrentPort) + "\r\n";


	if(m_pPrefs->m_LanMode && m_pPrefs->m_LanName != "")
	{
		NetworkPing += "LAN: " + m_pPrefs->m_LanName + "\r\n";

		CString RandCache = m_pComm->m_pCache->GetRandWebCache(true);
		
		if(!RandCache.IsEmpty())
			NetworkPing += "WebCache: " + RandCache + "\r\n";
	}



	NetworkPing += "\r\n";

	// Send initial ping out to LAN network
	SendTo(NetworkPing, NetworkPing.GetLength(), UDP_PORT, NULL);

	m_pCore->LogError("UDP: Ping Sent");
}

void CGnuLocal::OnReceive(int nErrorCode) 
{
	byte buffer[1024];

	CString Host, LocalHost, NetworkPing, NetworkPong;
	CString LanName, IRCAddr, InfoURL;

	UINT    Port, LocalPort, Leaves = 0;

	int buffLength = ReceiveFrom(buffer, 1024, Host, Port);

	GetSockName(LocalHost, LocalPort);

	// Handle Errors
	if(!buffLength || buffLength == SOCKET_ERROR)
		return;
	
	CString strBuffer((char*) buffer, 128);

	NetworkPing =  m_pComm->m_NetworkName;
	NetworkPing += " PING\r\n";

	NetworkPong =  m_pComm->m_NetworkName;
	NetworkPong += " PONG\r\n";


	// Handle Ping over LAN
	if(strBuffer.Find(NetworkPing) == 0)
	{
		if(Host == LocalHost)
		{
			m_pCore->LogError("UDP: Ping received from localhost");
			//return;
		}

		// Send back pong only if not a leaf
		NetworkPong += "Port: " + NumtoStr(m_pNet->m_CurrentPort) + "\r\n";

		if(m_pPrefs->m_LanMode)
		{
			NetworkPong += "LAN: " + m_pPrefs->m_LanName + "\r\n";

			CString RandCache = m_pComm->m_pCache->GetRandWebCache(true);
		
			if(!RandCache.IsEmpty())
				NetworkPing += "WebCache: " + RandCache + "\r\n";
		}

		// Leaves header
		if(m_pComm->m_GnuClientMode ==GNU_ULTRAPEER)
		{
			int Leaves = m_pComm->CountLeafConnects();

			if(Leaves)
				NetworkPong += "Leaves: " + NumtoStr(Leaves) + "\r\n";
		}


		NetworkPong += "\r\n";

		int pos = strBuffer.Find("\r\nPort: ");
		if(pos != -1)
		{
			pos += 2;
			sscanf((char*)buffer + pos, "Port: %d\r\n", &Port);
		}
		
		SendTo(NetworkPong, NetworkPong.GetLength(), UDP_PORT, Host);
		m_pCore->LogError("UDP: Pong Sent to " + Host + ":" + NumtoStr(Port));
	
	}

	// Extract Data from ping/pong
	if(strBuffer.Find(NetworkPing) == 0 || strBuffer.Find(NetworkPong) == 0)
	{
		int pos, backpos;

		pos = strBuffer.Find("\r\nPort: ");
		if(pos != -1)
		{
			pos += 2;
			sscanf((char*)buffer + pos, "Port: %d\r\n", &Port);
		}

		pos = strBuffer.Find("\r\nLAN: ");
		if(pos != -1)
		{
			pos += 2;
			backpos = strBuffer.Find("\r\n", pos);
			LanName = strBuffer.Mid(pos + 5, backpos - pos - 5);
		}

		pos = strBuffer.Find("\r\nWebCache: ");
		if(pos != -1)
		{
			pos += 2;
			backpos = strBuffer.Find("\r\n", pos);
			CString NewWebCache = strBuffer.Mid(pos + 10, backpos - pos - 10);

			m_pComm->m_pCache->WebCacheAddCache(NewWebCache);
		}

		pos = strBuffer.Find("\r\nIRC: ");
		if(pos != -1)
		{
			pos += 2;
			backpos = strBuffer.Find("\r\n", pos);
			IRCAddr = strBuffer.Mid(pos + 5, backpos - pos - 5);
		}

		pos = strBuffer.Find("\r\nInfoURL: ");
		if(pos != -1)
		{
			pos += 2;
			backpos = strBuffer.Find("\r\n", pos);
			InfoURL = strBuffer.Mid(pos + 9, backpos - pos - 9);
		}

		pos = strBuffer.Find("\r\nLeaves: ");
		if(pos != -1)
		{
			pos += 2;
			backpos = strBuffer.Find("\r\n", pos);
			Leaves  = atoi( strBuffer.Mid(pos + 8, backpos - pos - 8) );
		}
	}

	
	// Handle Pong over LAN
	if(strBuffer.Find(NetworkPong) == 0)
	{
		CString Extra;

		if(Leaves)
			Extra = " with " + NumtoStr(Leaves) + " leaves";

		m_pCore->LogError("UDP: Pong Received from " + Host + ":" + NumtoStr(Port) + Extra);
	}


	std::map<int, LanNode>::iterator itNode;
	for(itNode = m_LanNodeIDMap.begin(); itNode != m_LanNodeIDMap.end(); itNode++)
		if( itNode->second.Host == Host && itNode->second.Port == Port)
			return;


	LanNode LocalNode;
	LocalNode.Host		= Host;
	LocalNode.Port		= Port;
	LocalNode.Name		= LanName;
	LocalNode.IRCserver = IRCAddr;
	LocalNode.InfoPage	= InfoURL;
	LocalNode.Leaves	= Leaves;

	int LanNodeID = m_NextLanID++;
	m_LanNodeIDMap[LanNodeID] = LocalNode;


	// If we're not in lan mode, just add to cache
	if(!m_pPrefs->m_LanMode)
	{
		Node PermNode;
		PermNode.Host = Host;
		PermNode.Port = Port;
		
		if(PermNode.Host != LocalHost)
			m_pComm->m_pCache->m_GnuPerm.push_back(PermNode);
	}


	CAsyncSocket::OnReceive(nErrorCode);
}
