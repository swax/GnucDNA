#pragma once

#include "stdafx.h"
#include <iprtrmib.h>
#include <iphlpapi.h>


typedef struct 
{
  DWORD   dwState;        // state of the connection
  DWORD   dwLocalAddr;    // address on local computer
  DWORD   dwLocalPort;    // port number on local computer
  DWORD   dwRemoteAddr;   // address on remote computer
  DWORD   dwRemotePort;   // port number on remote computer
  DWORD	  dwProcessId;
} MIB_TCPEXROW;

// Gets information about TCP Sockets on this machine
typedef struct 
{
  DWORD			dwNumEntries;
  MIB_TCPEXROW	table[ANY_SIZE];
} MIB_TCPEXTABLE;


typedef DWORD (WINAPI *PALLOCATEANDGETTCPEXTABLEFROMSTACK)(MIB_TCPEXTABLE** pTcpTable, BOOL bOrder, HANDLE heap, DWORD zero, DWORD flags);

enum TCPSTATES
{ 
  NONEXISTENT = -1,
  OPEN, CLOSED, LISTENING, SYN_SENT, SYN_RCVD, ESTABLISHED, FIN_WAIT1, FIN_WAIT2,
  CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT, DELETE_TCB
};


/* Constructing one of these gathers all the TCP socket information into the class where it can
be instantly fetched. Consider it a "snapshot". To get a new snapshot run Query again or reinstantiate
the object. */

struct TcpEntry
{
	IPv4  Address;
	int   State;

	TcpEntry()
	{
		State = -1;
	}
};

class TcpStatus
{
	MIB_TCPEXTABLE* tcpExTable;
	HANDLE Heap;

	bool LoadExApis(VOID); //get pointers to the tcp iphelper function
	void Cleanup(void);    //delete previous query info without destructing the object;
	
	static PALLOCATEANDGETTCPEXTABLEFROMSTACK pAllocateAndGetTcpExTableFromStack;

public:
	TcpStatus(void);
	~TcpStatus(void);

	bool IsLoaded () const
	{ 
		return tcpExTable; 
	}

	int Query(void); //re-initialize the object and returns total number of TCP sockets it gathered.
	int GetCount( TCPSTATES status = SYN_SENT ); //get count of how many entries of this type
	
	/* Get status will retrieve the status of a specific socket given the remote address and port that 
	socket is related to. 0 means "I don't care" so for example you can get the first socket it 
	finds to port 80 of ANY address by saying this:
	GetStatus(0, 80); */
	int GetStatus( IPv4 address); //get status of specific connection -1 means "no socket exists"
	CString StatetoStr(int state);

	void Timer();

	std::vector<TcpEntry> TcpEntries;
};

