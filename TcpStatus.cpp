#include "stdafx.h"
#include "tcpstatus.h"


PALLOCATEANDGETTCPEXTABLEFROMSTACK TcpStatus::pAllocateAndGetTcpExTableFromStack;

TcpStatus::TcpStatus(void) : tcpExTable(NULL), Heap(0)
{
	Query();
}

TcpStatus::~TcpStatus(void)
{
	Cleanup();
}

bool TcpStatus::LoadExApis(VOID) //get pointers to the tcp iphelper function
{
	if ( pAllocateAndGetTcpExTableFromStack )
		return true;

	pAllocateAndGetTcpExTableFromStack = (PALLOCATEANDGETTCPEXTABLEFROMSTACK) GetProcAddress( LoadLibrary( "iphlpapi.dll"), "AllocateAndGetTcpExTableFromStack" );

	if( !pAllocateAndGetTcpExTableFromStack )
		return false;

	return true;
}

void TcpStatus::Cleanup(void)
{
	if ( tcpExTable )
		HeapFree(Heap, 0, tcpExTable);

	tcpExTable = NULL;
}

int TcpStatus::Query(void)
{
	if ( !LoadExApis() ) //make sure functions are loaded
		return 0;

	Cleanup();

	pAllocateAndGetTcpExTableFromStack( &tcpExTable, TRUE, Heap = GetProcessHeap(), 2, 2 );

	return tcpExTable->dwNumEntries;
}

int TcpStatus::GetCount( TCPSTATES status ) //get count of how many entries of this type
{
	int count = 0;

	for( int i = 0; i < TcpEntries.size(); i++)
	{
		if( TcpEntries[i].State != status)
			continue;

		count++;
	}

	return count;
}

int TcpStatus::GetStatus( IPv4 address )
{
	for( int i = 0; i < TcpEntries.size(); i++)
	{
		if( address.Host.S_addr != TcpEntries[i].Address.Host.S_addr)
			continue;

		if( address.Port != TcpEntries[i].Address.Port)
			continue;

		return TcpEntries[i].State;
	}

	return NONEXISTENT;
}

CString TcpStatus::StatetoStr(int state)
{
	CString text;

	switch(state)
	{
	case OPEN:
		text = "Open";
		break;
	case CLOSED:
		text = "Closed";
		break;
	case SYN_SENT:
		text = "Syn Sent";
		break;
	case SYN_RCVD:
		text = "Syn Recvd";
		break;
	case ESTABLISHED:
		text = "Established";
		break;
	case FIN_WAIT1:
		text = "Fin Wait 1";
		break;
	case FIN_WAIT2:
		text = "Fin Wait 2";
		break;
	case CLOSE_WAIT:
		text = "Close Wait";
		break;
	case CLOSING:
		text = "Closing";
		break;
	case LAST_ACK:
		text = "Last Ack";
		break;
	case TIME_WAIT:
		text = "Time Wait";
		break;
	case DELETE_TCB:
		text = "Delete Tcb";
		break;
	}

	return text;
}

void TcpStatus::Timer()
{
	TcpEntries.clear();

	Query();

	if( !tcpExTable)
		return;

	for( int i = 0; i < tcpExTable->dwNumEntries; i++)
	{
		TcpEntry Entry;
		Entry.Address.Host.S_addr = tcpExTable->table[i].dwRemoteAddr;
		Entry.Address.Port        = ntohs(tcpExTable->table[i].dwRemotePort);

		Entry.State = tcpExTable->table[i].dwState;

		TcpEntries.push_back(Entry);
	}
}