#pragma once

#include "stdafx.h"
#include "GnuUploadShell.h"

#define FULL_POLL  600
#define MAX_POLL    60

class CGnuUploadShell;

struct UploadQueueItem
{
	int		UploadID;
	IP      Host;
	CString RequestURI;
	int     SecsLeft;


	UploadQueueItem()
	{	
	};

	UploadQueueItem(CGnuUploadShell* p)
	{
		UploadID	= p->m_UploadID;
		Host        = p->m_Host;
		RequestURI  = p->m_RequestURI;
		SecsLeft    = MAX_POLL;
	};
};


class CGnuPrefs;
class CGnuTransfers;

class CUploadQueue
{
public:
	CUploadQueue ();

	void Init(CGnuTransfers*);
	
	bool CheckReady(CGnuUploadShell* p);
	
	bool ReadyItem(CGnuUploadShell* p);
	void UpdatePassList();
	
	bool PollItem(UploadQueueItem &);
	
	int  GetHostPos(CGnuUploadShell* p);
	
	void Timer();

	void DebugReport();

	int  m_MaxLength; // Max length of whole queue
	int  m_SameHostLimit;     // Max queues by one host

	int m_Minute;

	//Note that this queue counts from 0. All member functions add 1 when returning pos
	std::list<UploadQueueItem> m_WaitList;
	std::list<UploadQueueItem> m_Queue;
	std::list<UploadQueueItem> m_PassList;

	CGnuTransfers* m_pTrans;
	CGnuPrefs*     m_pPrefs;
};




