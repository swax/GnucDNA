#pragma once

#include "G2Packets.h"

class CGnuCore;

#define ROUTE_MAX_SIZE  500
#define UNIQUE_MAX_SIZE 500

struct MessageRoutes;

class CGnuChat
{
public:
	CGnuChat(CGnuCore* pCore);
	~CGnuChat(void);
	
	void SendDirectMessage(CString Address, CString Message);
	void RecvDirectMessage(IPv4 Source, G2_PM PrivateMessage);

	void Timer();

	std::map<uint32, MessageRoutes> m_RouteMap;
	uint32 m_RouteAge;
	uint32 m_RouteCut;

	std::map<uint32, uint32> m_UniqueIDMap;
	uint32 m_UniqueAge;
	uint32 m_UniqueCut;

	CGnuCore* m_pCore;
};

struct MessageRoutes
{
	int Age;
	std::vector<IPv4> Hubs;
};
