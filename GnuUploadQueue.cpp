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

#include "GnuPrefs.h"
#include "GnuTransfers.h"

#include "GnuUploadQueue.h"


CUploadQueue::CUploadQueue()
{
	m_pTrans = NULL;
	m_pPrefs = NULL;

	m_MaxLength     = 30;
	m_SameHostLimit = 5;

	m_Minute = 0;
};

void CUploadQueue::Init(CGnuTransfers* pTrans)
{
	m_pTrans = pTrans;
	m_pPrefs = pTrans->m_pPrefs;
}


bool CUploadQueue::CheckReady(CGnuUploadShell* p)
{
	if(p == NULL)
		return false;

	std::list<UploadQueueItem>::iterator  itItem;

	// Check if host in wait list
	for(itItem = m_WaitList.begin(); itItem != m_WaitList.end(); itItem++)
		if(p->m_Host.S_addr == (*itItem).Host.S_addr &&
			p->m_RequestURI == (*itItem).RequestURI)
			return false;

	// Add item to queue
	if( !ReadyItem(p) )
		return false;

	// Return true for items that are in pass list
	for(itItem = m_PassList.begin(); itItem != m_PassList.end(); itItem++)
		if(p->m_Host.S_addr == (*itItem).Host.S_addr &&
			p->m_RequestURI == (*itItem).RequestURI)
			return true;


	// Item still in queue
	return false;
}

bool CUploadQueue::ReadyItem(CGnuUploadShell* p)
{
	std::list<UploadQueueItem>::iterator  itItem;

	bool HostActive = false;

	// See if upload already in pass list
	for(itItem = m_PassList.begin(); itItem != m_PassList.end(); itItem++)
		if(p->m_Host.S_addr == (*itItem).Host.S_addr)
		{
			HostActive = true;

			if(p->m_RequestURI == (*itItem).RequestURI)
			{
				(*itItem).SecsLeft = MAX_POLL;
				return true;
			}
		}

	if(m_PassList.size() < m_pPrefs->m_MaxUploads && !HostActive)
	{
		m_PassList.push_back( UploadQueueItem(p) );
		TRACE0("Q> Pass Added: " + IPtoStr(p->m_Host) + p->m_RequestURI + "\n");
		return true;
	}

	
	// Host must have queue ability, to go on queue
	if(!p->m_QueueRequest)
		return false;


	// Cant get on queue if its too big
	if(m_Queue.size() >= m_MaxLength)
		return false;


	// Put Host on queue
	int FilesQueued = 0;


	for(itItem = m_Queue.begin(); itItem != m_Queue.end(); itItem++)
		if(p->m_Host.S_addr == (*itItem).Host.S_addr)
		{
			// Host already in Queue
			if(p->m_RequestURI == (*itItem).RequestURI)
			{
				// Check if host is updating to fast
				int LastUpdate = MAX_POLL - (*itItem).SecsLeft;
				int FloodMark  = MAX_POLL / 4;

				if(LastUpdate < FloodMark)
				{
					p->m_QueueRequest = false;
					return false;
				}

				(*itItem).SecsLeft = MAX_POLL;
				return true;
			}

			// Different file queued, same host
			else
				FilesQueued++;
		}

	if(FilesQueued < m_SameHostLimit)
	{
		m_Queue.push_back( UploadQueueItem(p) );
		TRACE0("Q> Queue Added: " + IPtoStr(p->m_Host) + p->m_RequestURI + "\n");
		return true;
	}
	

	return false;
}

void CUploadQueue::Timer()
{
	std::list<UploadQueueItem>::iterator  itItem;

	// Go through wait list
	itItem = m_WaitList.begin();
	while( itItem != m_WaitList.end() )
		if(  (*itItem).SecsLeft == 0 )
		{
			TRACE0("Q> Wait Removed: " + IPtoStr((*itItem).Host) + (*itItem).RequestURI + "\n");

			itItem = m_WaitList.erase(itItem);
		}
		else
		{
			(*itItem).SecsLeft--;
			itItem++;
		}

	// Go through queue list
	itItem = m_Queue.begin();
	while( itItem != m_Queue.end() )
		if( !PollItem(*itItem) )
		{
			TRACE0("Q> Queue Removed: " + IPtoStr((*itItem).Host) + (*itItem).RequestURI + "\n");

			itItem = m_Queue.erase(itItem);
		}
		else
			itItem++;

	// Go through pass list
	itItem = m_PassList.begin();
	while( itItem != m_PassList.end() )
		if( !PollItem(*itItem) )
		{
			// Dont let this host retry same file after leaving pass for some time
			//(*itItem).SecsLeft   = MAX_POLL * 5;
			//m_WaitList.push_back(*itItem);
			
			//TRACE0("Q> Pass Removed: " + IPtoStr((*itItem).Host) + (*itItem).RequestURI + "\n");
			//TRACE0("Q> Wait Added: " + IPtoStr((*itItem).Host) + (*itItem).RequestURI + "\n");

			itItem = m_PassList.erase(itItem);	
		}
		else
			itItem++;


	UpdatePassList();

	DebugReport();
}

bool CUploadQueue::PollItem(UploadQueueItem &Item)
{
	Item.SecsLeft--;

	if(Item.SecsLeft == 0)
		return false;

	
	for(int i = 0; i < m_pTrans->m_UploadList.size(); i++)
	{
		CGnuUploadShell* p = m_pTrans->m_UploadList[i];

		if(p->m_Host.S_addr == Item.Host.S_addr &&
			  p->m_RequestURI == Item.RequestURI)
		{
			if(p->m_Status == TRANSFER_SENDING)
				Item.SecsLeft = MAX_POLL;
		
			if(p->m_Status == TRANSFER_CLOSED)
				return false;
		}
	}

	// Keep waiting for disconnected hosts
	return true;
}

void CUploadQueue::UpdatePassList()
{
	// Add items from Queue to the PassList if there is room
	std::list<UploadQueueItem>::iterator  itQueueItem;
	std::list<UploadQueueItem>::iterator  itPassItem;

	while(m_PassList.size() < m_pPrefs->m_MaxUploads)
	{
		bool ItemAdded = false;
		
		for(itQueueItem = m_Queue.begin(); itQueueItem != m_Queue.end(); itQueueItem++)
		{
			bool AddQueueItem = true;

			// Prevent duplicate hosts from being in the PassList
			for(itPassItem = m_PassList.begin(); itPassItem != m_PassList.end(); itPassItem++)
				if((*itQueueItem).Host.S_addr == (*itPassItem).Host.S_addr)
					AddQueueItem = false;

			if(AddQueueItem)
			{
				(*itQueueItem).SecsLeft   = MAX_POLL;
				m_PassList.push_back(*itQueueItem);
				
				TRACE0("Q> Pass Added from Queue: " + IPtoStr((*itQueueItem).Host) + (*itQueueItem).RequestURI + "\n");

				m_Queue.erase(itQueueItem);
				
				ItemAdded = true;
				break;
			}
		}
				
		if(!ItemAdded)
			break;
	}
}
int CUploadQueue::GetHostPos(CGnuUploadShell* p)
{
	std::list<UploadQueueItem>::iterator  itItem;

	int i = 0;
	for(itItem = m_Queue.begin(); itItem != m_Queue.end(); itItem++, i++)
		if (p->m_Host.S_addr == (*itItem).Host.S_addr &&
			p->m_RequestURI == (*itItem).RequestURI)
			return i + 1;	//Count from 1

	return 0;	//Not found
}

void CUploadQueue::DebugReport()
{
	// disabled
	//return;

	if(m_Minute < 60)
	{
		m_Minute++;
		return;
	}
	
	m_Minute = 0;

	if(m_WaitList.size() || m_Queue.size() || m_PassList.size())
	{
		std::list<UploadQueueItem>::iterator  itItem;
		
		TRACE0("\nQ> Minute Report\n");

		int i = 0;
		for(itItem = m_WaitList.begin(); itItem != m_WaitList.end(); itItem++, i++)
			TRACE0("Q> Wait " + NumtoStr(i + 1) + ": " + IPtoStr((*itItem).Host) + (*itItem).RequestURI + " ttl=" + NumtoStr((*itItem).SecsLeft) + "\n");

		i = 0;
		for(itItem = m_Queue.begin(); itItem != m_Queue.end(); itItem++, i++)
			TRACE0("Q> Queue " + NumtoStr(i + 1) + ": " + IPtoStr((*itItem).Host) + (*itItem).RequestURI + " ttl=" + NumtoStr((*itItem).SecsLeft) + "\n");

		i = 0;
		for(itItem = m_PassList.begin(); itItem != m_PassList.end(); itItem++, i++)
			TRACE0("Q> Pass " + NumtoStr(i + 1) + ": " + IPtoStr((*itItem).Host) + (*itItem).RequestURI + " ttl=" + NumtoStr((*itItem).SecsLeft) + "\n");
		
		TRACE0("\n");
	}
}



