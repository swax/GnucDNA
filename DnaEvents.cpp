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


#include "StdAfx.h"
#include "DnaCore.h"

#include "DnaEvents.h"

CDnaEvents::CDnaEvents(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;

	m_dnaCore->m_dnaEvents = this;
}

CDnaEvents::~CDnaEvents(void)
{
	m_dnaCore->m_dnaEvents = NULL;
}


void CDnaEvents::NetworkChange(int NodeID)
{
}

void CDnaEvents::NetworkPacketIncoming(int NetworkID, bool TCP, uint32 IP, int Port, byte* packet, int size, bool Local, int ErrorCode)
{ 
}

void CDnaEvents::NetworkPacketOutgoing(int NetworkID, bool TCP, uint32 IP, int Port, byte* packet, int size, bool Local)
{
}

void CDnaEvents::NetworkAuthenticate(int NodeID)
{
}

void CDnaEvents::NetworkChallenge(int NodeID, LPCTSTR Challenge)
{
}


void CDnaEvents::SearchUpdate(LONG SearchID, LONG ResultID)
{
}

void CDnaEvents::SearchResult(LONG SearchID, LONG ResultID)
{
}

void CDnaEvents::SearchRefresh(LONG SearchID)
{
}

void CDnaEvents::SearchBrowseUpdate(LONG SearchID, LONG State, LONG Progress)
{
}

void CDnaEvents::SearchProgress(LONG SearchID)
{
}

void CDnaEvents::SearchPaused(LONG SearchID)
{
}

void CDnaEvents::ShareUpdate(LONG FileID)
{
}

void CDnaEvents::ShareReload()
{
}

void CDnaEvents::DownloadUpdate(long DownloadID)
{
}

void CDnaEvents::DownloadChallenge(long DownloadID, long SourceID, CString Challenge)
{
}

void CDnaEvents::UploadUpdate(long UploadID)
{
}

void CDnaEvents::UploadAuthenticate(long UploadID)
{
}

void CDnaEvents::ChatRecvDirectMessage(LPCTSTR Address, LPCTSTR Message)
{
}

void CDnaEvents::UpdateFound(LPCTSTR Version)
{
}

void CDnaEvents::UpdateFailed(LPCTSTR Reason)
{
}

void CDnaEvents::UpdateVersionCurrent()
{
}

void CDnaEvents::UpdateComplete()
{
}
