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
  
#include "DnaCore.h"

#include "GnuNetworks.h"
#include "GnuCore.h"
#include "GnuControl.h"
#include "GnuNode.h"
#include "GnuPrefs.h"
#include "GnuLocal.h"
#include "GnuCache.h"
#include "G2Control.h"
#include "G2Node.h"

#include "DnaNetwork.h"


CDnaNetwork::CDnaNetwork()
{
	m_dnaCore    = NULL;
	m_gnuNetwork = NULL;


}

void CDnaNetwork::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;

	m_gnuNetwork = dnaCore->m_gnuCore->m_pNet;
	m_gnuCore    = dnaCore->m_gnuCore;
}

CDnaNetwork::~CDnaNetwork()
{
	
}


// CDnaNetwork message handlers

std::vector<int> CDnaNetwork::GetNodeIDs(void)
{
	std::vector<int> NodeIDs;

	if( m_gnuNetwork->m_pGnu )
		for(int i = 0; i < m_gnuNetwork->m_pGnu->m_NodeList.size(); i++)
		{
			if(m_gnuNetwork->m_pGnu->m_NodeList[i]->m_Status == SOCK_CONNECTED)
				if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER && m_gnuNetwork->m_pGnu->m_NodeList[i]->m_GnuNodeMode == GNU_LEAF)
					continue;

			NodeIDs.push_back(m_gnuNetwork->m_pGnu->m_NodeList[i]->m_NodeID);
		}

	if( m_gnuNetwork->m_pG2 )
		for(int i = 0; i < m_gnuNetwork->m_pG2->m_G2NodeList.size(); i++)
		{
			if( m_gnuNetwork->m_pG2->m_G2NodeList[i]->m_Status == SOCK_CONNECTED)
				if( m_gnuNetwork->m_pG2->m_ClientMode == G2_HUB && m_gnuNetwork->m_pG2->m_G2NodeList[i]->m_NodeMode == G2_CHILD)
					continue;

			NodeIDs.push_back(m_gnuNetwork->m_pG2->m_G2NodeList[i]->m_G2NodeID);
		}
		
	return NodeIDs;
}


LONG CDnaNetwork::GetNodeState(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_Status;
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			return itNode->second->m_Status;
	}

	return SOCK_UNKNOWN;
}

LONG CDnaNetwork::ClientMode(void)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_LEAF)
			return CLIENT_GNU_LEAF;
		if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER)
			return CLIENT_GNU_ULTRAPEER;
	}

	return CLIENT_UNKNOWN;
}

LONG CDnaNetwork::GetNodeMode(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			if(itNode->second->m_GnuNodeMode == GNU_LEAF)
				return CLIENT_GNU_LEAF;
			else if(itNode->second->m_GnuNodeMode == GNU_ULTRAPEER)
				return CLIENT_GNU_ULTRAPEER;
		}
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
		{
			if(itNode->second->m_NodeMode == G2_CHILD)
				return CLIENT_G2_CHILD;
			else if(itNode->second->m_NodeMode == G2_HUB)
				return CLIENT_G2_HUB;
		}
	}

	return CLIENT_UNKNOWN;
}

ULONG CDnaNetwork::GetNodeIP(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_Address.Host.S_addr;
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			return itNode->second->m_Address.Host.S_addr;
	}

	return 0;
}

LONG CDnaNetwork::GetNodePort(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_Address.Port;
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			return itNode->second->m_Address.Port;
	}

	return 0;
}


LONG CDnaNetwork::GetNodeBytesUp(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_AvgBytes[1].GetAverage(); 
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			return itNode->second->m_AvgBytes[1].GetAverage();
	}

	return 0;
}

LONG CDnaNetwork::GetNodeBytesDown(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_AvgBytes[0].GetAverage(); 
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			return itNode->second->m_AvgBytes[0].GetAverage();
	}

	return 0;
}

LONG CDnaNetwork::GetNodeBytesDropped(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_AvgBytes[2].GetAverage();
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			return itNode->second->m_AvgBytes[2].GetAverage();
	}

	return 0;
}

void CDnaNetwork::ConnectNode(LPCTSTR Host, LONG Port)
{
	

	if( m_gnuNetwork->m_pGnu )
		m_gnuNetwork->m_pGnu->AddNode(Host, Port);
}

void CDnaNetwork::RemoveNode(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			m_gnuNetwork->m_pGnu->RemoveNode(itNode->second);
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			m_gnuNetwork->m_pG2->RemoveNode(itNode->second);
	}
}

LONG CDnaNetwork::GetNormalConnectedCount(void)
{
	LONG Count = 0;

	if( m_gnuNetwork->m_pGnu )
		Count += m_gnuNetwork->m_pGnu->CountUltraConnects();

	if ( m_gnuNetwork->m_pG2 )
		Count += m_gnuNetwork->m_pG2->CountHubConnects();

	return Count;
}

CString CDnaNetwork::GetNodeHandshake(LONG NodeID)
{
	

	CString strResult;

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			strResult = itNode->second->m_WholeHandshake;
	}
	
	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			strResult = itNode->second->m_WholeHandshake;
	}

	return strResult;
}

DATE CDnaNetwork::GetNodeConnectTime(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			COleDateTime OleTime(itNode->second->m_ConnectTime);

			return (DATE) OleTime;
		}
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
		{
			COleDateTime OleTime(itNode->second->m_ConnectTime);

			return (DATE) OleTime;
		}
	}

	return 0;
}

ULONG CDnaNetwork::GetLocalIP(void)
{
	

	return m_gnuNetwork->m_CurrentIP.S_addr;
}

LONG CDnaNetwork::GetLocalPort(void)
{
	

	return m_gnuNetwork->m_CurrentPort;
}


std::vector<int> CDnaNetwork::GetLanNodeIDs(void)
{
	std::vector<int> LanNodeIDs;

	if( m_gnuNetwork->m_pGnu )
	{
		int i = 0;
		std::map<int, LanNode>::iterator itNode;

		for(itNode = m_gnuNetwork->m_pGnu ->m_LanSock->m_LanNodeIDMap.begin(); itNode != m_gnuNetwork->m_pGnu ->m_LanSock->m_LanNodeIDMap.end(); itNode++)
			LanNodeIDs.push_back( itNode->first );
	}

	return LanNodeIDs;
}

void CDnaNetwork::LanModeOn(void)
{
	

	if( m_gnuNetwork->m_pGnu )
		m_gnuNetwork->m_pGnu->m_LanSock->LanModeOn();
}

void CDnaNetwork::JoinLan(LPCTSTR LanName)
{
	

	if( m_gnuNetwork->m_pGnu )
		m_gnuNetwork->m_pGnu->m_LanSock->JoinLan(LanName);
}

void CDnaNetwork::LanModeOff(void)
{
	

	if( m_gnuNetwork->m_pGnu )
		m_gnuNetwork->m_pGnu->m_LanSock->LanModeOff();
}



CString CDnaNetwork::GetLanNodeName(LONG LanNodeID)
{
	

	CString strResult;

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, LanNode>::iterator itLanNode = m_gnuNetwork->m_pGnu->m_LanSock->m_LanNodeIDMap.find(LanNodeID);

		if(itLanNode != m_gnuNetwork->m_pGnu->m_LanSock->m_LanNodeIDMap.end())
			strResult = itLanNode->second.Name;
	}

	return strResult;
}

LONG CDnaNetwork::GetLanNodeLeaves(LONG LanNodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, LanNode>::iterator itLanNode = m_gnuNetwork->m_pGnu->m_LanSock->m_LanNodeIDMap.find(LanNodeID);

		if(itLanNode != m_gnuNetwork->m_pGnu->m_LanSock->m_LanNodeIDMap.end())
			return itLanNode->second.Leaves;
	}

	return 0;
}


void CDnaNetwork::GetNodePacketsPing(LONG NodeID, LONG* Good, LONG* Total)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			*Total = itNode->second->m_StatPings[0];
			*Good  = itNode->second->m_StatPings[1];
		}
	}
}

void CDnaNetwork::GetNodePacketsPong(LONG NodeID, LONG* Good, LONG* Total)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			*Total = itNode->second->m_StatPongs[0];
			*Good  = itNode->second->m_StatPongs[1];
		}
	}
}

void CDnaNetwork::GetNodePacketsQuery(LONG NodeID, LONG* Good, LONG* Total)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			*Total = itNode->second->m_StatQueries[0];
			*Good  = itNode->second->m_StatQueries[1];
		}
	}
}

void CDnaNetwork::GetNodePacketsQueryHit(LONG NodeID, LONG* Good, LONG* Total)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			*Total = itNode->second->m_StatQueryHits[0];
			*Good  = itNode->second->m_StatQueryHits[1];
		}
	}
}

void CDnaNetwork::GetNodePacketsPush(LONG NodeID, LONG* Good, LONG* Total)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			*Total = itNode->second->m_StatPushes[0];
			*Good  = itNode->second->m_StatPushes[1];
		}
	}
}

void CDnaNetwork::GetNodePacketsTotal(LONG NodeID, LONG* Good, LONG* Total)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			*Total = itNode->second->m_StatPings[0] + 
								itNode->second->m_StatPongs[0] + 
								itNode->second->m_StatQueries[0] + 
								itNode->second->m_StatQueryHits[0] + 
								itNode->second->m_StatPushes[0];

			*Good = itNode->second->m_StatPings[1] + 
							itNode->second->m_StatPongs[1] + 
							itNode->second->m_StatQueries[1] + 
							itNode->second->m_StatQueryHits[1] + 
							itNode->second->m_StatPushes[1];
		}
	}
}



DOUBLE CDnaNetwork::GetNodePacketsDown(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_AvgPackets[0].GetAverage();
	}

	return 0;
}

DOUBLE CDnaNetwork::GetNodePacketsUp(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_AvgPackets[1].GetAverage();
	}

	return 0;
}

DOUBLE CDnaNetwork::GetNodePacketsDropped(LONG NodeID)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			return itNode->second->m_AvgPackets[2].GetAverage();
	}

	return 0;
}

LONG CDnaNetwork::GetLocalSpeed(void)
{
	

	return m_gnuNetwork->m_RealSpeedDown * 8 / 1024;

	return 0;
}

void CDnaNetwork::ForceUltrapeer(BOOL Enabled)
{
	

	if( m_gnuNetwork->m_pGnu )
	{
		if(Enabled)
		{
			if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_LEAF)
				m_gnuNetwork->m_pGnu->SwitchGnuClientMode(GNU_ULTRAPEER);

			m_gnuNetwork->m_pGnu->m_ForcedUltrapeer = true;
		}
		else
			m_gnuNetwork->m_pGnu->m_ForcedUltrapeer = false;
	}
}



std::vector<int> CDnaNetwork::GetChildNodeIDs(void)
{
	std::vector<int> ChildNodeIDs;
	
	if( m_gnuNetwork->m_pGnu )
	{
		for(int i = 0; i < m_gnuNetwork->m_pGnu->m_NodeList.size(); i++)
			if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER && m_gnuNetwork->m_pGnu->m_NodeList[i]->m_GnuNodeMode == GNU_LEAF)
				ChildNodeIDs.push_back(m_gnuNetwork->m_pGnu->m_NodeList[i]->m_NodeID);
	}

	if( m_gnuNetwork->m_pG2 )
		for(int i = 0; i < m_gnuNetwork->m_pG2->m_G2NodeList.size(); i++)
		{
			if(m_gnuNetwork->m_pG2->m_G2NodeList[i]->m_NodeMode == G2_CHILD)
				ChildNodeIDs.push_back(m_gnuNetwork->m_pG2->m_G2NodeList[i]->m_G2NodeID);
		}

	return ChildNodeIDs;
}

void CDnaNetwork::SendChallenge(LONG NodeID, LPCTSTR Challenge, LPCTSTR Answer)
{
	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			itNode->second->m_Challenge		  = Challenge;
			itNode->second->m_ChallengeAnswer = Answer;
		}
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
		{
			itNode->second->m_Challenge		  = Challenge;
			itNode->second->m_ChallengeAnswer = Answer;
		}
	}
}

void CDnaNetwork::AnswerChallenge(LONG NodeID, LPCTSTR Answer)
{
	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			itNode->second->m_RemoteChallengeAnswer = Answer;
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			itNode->second->m_RemoteChallengeAnswer = Answer;
	}
}

CString CDnaNetwork::GetNodeAgent(LONG NodeID)
{
	

	CString strResult;

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
			strResult = itNode->second->m_RemoteAgent;
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
			strResult = itNode->second->m_RemoteAgent;
	}

	return strResult;
}

LONG CDnaNetwork::GetChildConnectedCount(void)
{
	

	if( m_gnuNetwork->m_pGnu )
		return m_gnuNetwork->m_pGnu->CountLeafConnects();

	return 0;
}

CString CDnaNetwork::GetNodeStatus(LONG NodeID)
{
	CString strStatus;

	if( m_gnuNetwork->m_pGnu )
	{
		std::map<int, CGnuNode*>::iterator itNode = m_gnuNetwork->m_pGnu->m_NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pGnu->m_NodeIDMap.end())
		{
			CGnuNode* pNode = itNode->second;

			strStatus += pNode->m_StatusText;

			CString State = m_gnuNetwork->NetStat.StatetoStr(pNode->m_LastState);

			if( !State.IsEmpty())
				strStatus += " - " + State;
		}
	}

	if( m_gnuNetwork->m_pG2 )
	{
		std::map<int, CG2Node*>::iterator itNode = m_gnuNetwork->m_pG2->m_G2NodeIDMap.find(NodeID);

		if(itNode != m_gnuNetwork->m_pG2->m_G2NodeIDMap.end())
		{
			CG2Node* pNode = itNode->second;
			
			strStatus += pNode->m_StatusText;

			CString State = m_gnuNetwork->NetStat.StatetoStr(pNode->m_LastState);

			if( !State.IsEmpty())
				strStatus += " - " + State;
		}
	}

	return strStatus;
}

LONG CDnaNetwork::ClientMode2(LONG NetworkID)
{
	

	if( NetworkID == NETWORK_GNUTELLA && m_gnuNetwork->m_pGnu)
	{
		if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_LEAF)
			return CLIENT_GNU_LEAF;
		if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER)
			return CLIENT_GNU_ULTRAPEER;
	}

	else if( NetworkID == NETWORK_G2 && m_gnuNetwork->m_pG2 )
	{
		if(m_gnuNetwork->m_pG2->m_ClientMode == G2_HUB)
			return CLIENT_G2_HUB;

		if(m_gnuNetwork->m_pG2->m_ClientMode == G2_CHILD)
			return CLIENT_G2_CHILD;
	}

	return CLIENT_UNKNOWN;
}

void CDnaNetwork::ConnectNode2(LPCTSTR Host, LONG Port, LONG NetworkID)
{
	

	if( NetworkID == NETWORK_GNUTELLA && m_gnuNetwork->m_pGnu )
		m_gnuNetwork->m_pGnu->AddNode(Host, Port);

	else if( NetworkID == NETWORK_G2 && m_gnuNetwork->m_pG2 )
		m_gnuNetwork->m_pG2->CreateNode( Node(Host, Port, NETWORK_G2) );
}

LONG CDnaNetwork::GetNormalConnectedCount2(LONG NetworkID)
{
	

	if( NetworkID == NETWORK_GNUTELLA && m_gnuNetwork->m_pGnu )
		return m_gnuNetwork->m_pGnu->CountUltraConnects();

	if( NetworkID == NETWORK_G2 && m_gnuNetwork->m_pG2 )
		return m_gnuNetwork->m_pG2->CountHubConnects();


	return 0;
}

void CDnaNetwork::ForceUltrapeer2(BOOL Enabled, LONG NetworkID)
{
	

	// Gnutella
	if( NetworkID == NETWORK_GNUTELLA && m_gnuNetwork->m_pGnu )
	{
		if(Enabled)
		{
			if(m_gnuNetwork->m_pGnu->m_GnuClientMode == GNU_LEAF)
				m_gnuNetwork->m_pGnu->SwitchGnuClientMode(GNU_ULTRAPEER);

			m_gnuNetwork->m_pGnu->m_ForcedUltrapeer = true;
		}
		else
			m_gnuNetwork->m_pGnu->m_ForcedUltrapeer = false;
	}

	// G2
	if( NetworkID == NETWORK_G2 && m_gnuNetwork->m_pG2 )
	{
		if(Enabled)
		{
			if(m_gnuNetwork->m_pG2->m_ClientMode == G2_CHILD)
				m_gnuNetwork->m_pG2->SwitchG2ClientMode(G2_HUB);

			m_gnuNetwork->m_pG2->m_ForcedHub = true;
		}
		else
			m_gnuNetwork->m_pG2->m_ForcedHub = false;
	}
}

LONG CDnaNetwork::GetChildConnectedCount2(LONG NetworkID)
{
	if( NetworkID == NETWORK_GNUTELLA && m_gnuNetwork->m_pGnu )
		return m_gnuNetwork->m_pGnu->CountLeafConnects();

	if( NetworkID == NETWORK_G2 && m_gnuNetwork->m_pG2 )
		return m_gnuNetwork->m_pG2->CountChildConnects();

	return 0;
}

void CDnaNetwork::SetClientMode(LONG Mode)
{
	switch(Mode)
	{
	case CLIENT_GNU_ULTRAPEER:
		if(m_gnuNetwork->m_pGnu)
			m_gnuNetwork->m_pGnu->SwitchGnuClientMode(GNU_ULTRAPEER);
		break;
	case CLIENT_GNU_LEAF:
		if(m_gnuNetwork->m_pGnu)
			m_gnuNetwork->m_pGnu->SwitchGnuClientMode(GNU_LEAF);
		break;
	case CLIENT_G2_HUB:
		if(m_gnuNetwork->m_pG2)
			m_gnuNetwork->m_pG2->SwitchG2ClientMode(G2_HUB);
		break;
	case CLIENT_G2_CHILD:
		if(m_gnuNetwork->m_pG2)
			m_gnuNetwork->m_pG2->SwitchG2ClientMode(G2_CHILD);
		break;
	}
}
