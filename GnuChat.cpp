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
#include "DnaCore.h"
#include "DnaChat.h"
#include "DnaEvents.h"

#include "GnuCore.h"
#include "GnuNetworks.h"
#include "G2Control.h"
#include "G2Node.h"
#include "G2Protocol.h"
#include "GnuSearch.h"

#include "GnuChat.h"

CGnuChat::CGnuChat(CGnuCore* pCore)
{
	m_pCore = pCore;

	m_RouteCut  = ROUTE_MAX_SIZE;
	m_UniqueCut = UNIQUE_MAX_SIZE;
}

CGnuChat::~CGnuChat(void)
{
	m_RouteAge  = 0;
	m_UniqueAge = 0;
}

void CGnuChat::Timer()
{
	// Not pretty should make a map class with built in age

	if(m_RouteAge > m_RouteCut)
	{
		m_RouteCut += ROUTE_MAX_SIZE;

		// Keep half of old routes
		std::map<uint32, MessageRoutes>::iterator itRoute;
		for(itRoute = m_RouteMap.begin(); itRoute != m_RouteMap.end(); itRoute++)
			if(itRoute->second.Age < m_RouteCut - (ROUTE_MAX_SIZE + ROUTE_MAX_SIZE/2) )
				itRoute = m_RouteMap.erase(itRoute);
	}

	if(m_UniqueAge > m_UniqueCut)
	{
		m_UniqueCut += UNIQUE_MAX_SIZE;

		// Keep half of old routes
		std::map<uint32, uint32>::iterator itUnique;
		for(itUnique = m_UniqueIDMap.begin(); itUnique != m_UniqueIDMap.end(); itUnique++)
			if(itUnique->second < m_UniqueCut - (UNIQUE_MAX_SIZE + UNIQUE_MAX_SIZE/2) )
				itUnique = m_UniqueIDMap.erase(itUnique);
	}
}

void CGnuChat::SendDirectMessage(CString Address, CString Message)
{
	CGnuNetworks* pNet = m_pCore->m_pNet;
	CG2Control* pG2    = pNet->m_pG2;

	// G2 required to use chat
	if(pG2 == NULL)
		return;


	G2_PM PrivateMessage;
	PrivateMessage.Destination.Host = StrtoIP(ParseString(Address, ':'));
	PrivateMessage.Destination.Port = atoi(Address);

	if(PrivateMessage.Destination.Host.S_addr == 0 || PrivateMessage.Destination.Port == 0)
		return;

	// Look through searches to see if node firewalled
	for(int i = 0; i < pNet->m_SearchList.size(); i++)
		for(int x = 0; x < pNet->m_SearchList[i]->m_WholeList.size(); x++)
			// If firewalled try route through alternate path
			if(pNet->m_SearchList[i]->m_WholeList[x].Firewall)
				if(pNet->m_SearchList[i]->m_WholeList[x].DirectHubs.size())
				{
					MessageRoutes Routes;
					Routes.Age = m_RouteAge++;
					for(int p = 0; p < pNet->m_SearchList[i]->m_WholeList[x].DirectHubs.size(); p++)
						Routes.Hubs.push_back(pNet->m_SearchList[i]->m_WholeList[x].DirectHubs[p]);
	
					m_RouteMap[PrivateMessage.Destination.Host.S_addr] = Routes;
				}

	// Set local info so messages can be sent back
	for(int i = 0; i < pG2->m_G2NodeList.size(); i++)
		if( pG2->m_G2NodeList[i]->m_NodeMode == G2_HUB )
			if( pG2->m_G2NodeList[i]->m_NodeInfo.Address.Host.S_addr != 0)
				PrivateMessage.Neighbours.push_back( pG2->m_G2NodeList[i]->m_NodeInfo.Address );

	PrivateMessage.Firewall = pNet->m_UdpFirewall;
	PrivateMessage.Message = Message;

	// Check if directly connected
	CG2Node* pTCP = NULL;
	std::map<uint32, CG2Node*>::iterator itNode = pG2->m_G2NodeAddrMap.find(PrivateMessage.Destination.Host.S_addr);
	if(itNode != pG2->m_G2NodeAddrMap.end())
		pTCP = itNode->second;

	// Send to alternate routes if availale
	if(pTCP == NULL)
	{
		std::map<uint32, MessageRoutes>::iterator itRoutes = m_RouteMap.find(PrivateMessage.Destination.Host.S_addr);
		if(itRoutes != m_RouteMap.end())
			for(i = 0; i < itRoutes->second.Hubs.size(); i++)
				pG2->Send_PM(itRoutes->second.Hubs[i], PrivateMessage);
	}

	// Either sends direct udp or tcp
	pG2->Send_PM(PrivateMessage.Destination, PrivateMessage, pTCP);
}

void CGnuChat::RecvDirectMessage(IPv4 Source, G2_PM PrivateMessage)
{
	IPv4 Sender = Source;
	if(PrivateMessage.SendingAddress.Host.S_addr)
		Sender = PrivateMessage.SendingAddress;

	// Add or update routemap 
	if(PrivateMessage.Firewall)
	{
		MessageRoutes Routes;
		for(int i = 0; i < PrivateMessage.Neighbours.size(); i++)
			Routes.Hubs.push_back(PrivateMessage.Neighbours[i]);

		m_RouteMap[Sender.Host.S_addr] = Routes;
	}

	// Check for dupe ids
	std::map<uint32, uint32>::iterator itID = m_UniqueIDMap.find(PrivateMessage.UniqueID);
	if(itID != m_UniqueIDMap.end())
		return;
	
	m_UniqueIDMap[PrivateMessage.UniqueID] = m_UniqueAge++;

	// Fire event
	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->ChatRecvDirectMessage(IPtoStr(Sender.Host) + ":" + NumtoStr(Sender.Port), PrivateMessage.Message);
}