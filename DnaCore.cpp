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
#include "GnuNetworks.h"
#include "GnuControl.h"
#include "G2Control.h"
#include "GnuTransfers.h"

#include "DnaPrefs.h"
#include "DnaNetwork.h"
#include "DnaCache.h"
#include "DnaShare.h"
#include "DnaSearch.h"
#include "DnaDownload.h"
#include "DnaUpload.h"
#include "DnaUpdate.h"
#include "DnaMeta.h"
#include "DnaChat.h"
#include "DnaEvents.h"

#include "DnaCore.h"


CDnaCore*  ComCore; // Needed for call back functions
void CALLBACK SecTimerProc(HWND hwnd, UINT message,UINT idTimer, DWORD dwTime);
void CALLBACK HourTimerProc(HWND hwnd, UINT message,UINT idTimer, DWORD dwTime);


CDnaCore::CDnaCore()
{
	AfxSocketInit();

	CreateMutex(NULL, false, "GnucDNA_Mutex");


	m_Loaded = false;

	Load();
}

CDnaCore::~CDnaCore()
{
	Unload();
}

void CDnaCore::Load(void)
{
	if(m_Loaded)
		return;

	m_Loaded = true;

	ComCore = this;

	m_dnaEvents = NULL;

	m_gnuCore  = new CGnuCore(this); 
	
	m_dnaPrefs = new CDnaPrefs();
	m_dnaPrefs->InitClass(this);

	m_dnaNetwork = new CDnaNetwork();
	m_dnaNetwork->InitClass(this);

	m_dnaCache = new CDnaCache();
	m_dnaCache->InitClass(this);

	m_dnaShare = new CDnaShare();
	m_dnaShare->InitClass(this);

	m_dnaSearch = new CDnaSearch();
	m_dnaSearch->InitClass(this);

	m_dnaDownload  = new CDnaDownload();
	m_dnaDownload->InitClass(this);

	m_dnaUpload  = new CDnaUpload();
	m_dnaUpload->InitClass(this);

	m_dnaUpdate  = new CDnaUpdate();
	m_dnaUpdate->InitClass(this);

	m_dnaMeta  = new CDnaMeta();
	m_dnaMeta->InitClass(this);

	m_dnaChat  = new CDnaChat();
	m_dnaChat->InitClass(this);

	m_LastTickCount = 0;

	m_SecTimerID  = SetTimer(NULL, NULL, 1000,  (TIMERPROC) SecTimerProc);
	m_HourTimerID = SetTimer(NULL, NULL, 3600000, (TIMERPROC) HourTimerProc);
}

void CDnaCore::Unload(void)
{
	if(!m_Loaded)
		return;

	m_Loaded = false;

	KillTimer(NULL, m_SecTimerID);
	KillTimer(NULL, m_HourTimerID);

	UINT m_SecTimerID    = 0;
	UINT m_HourTimerID   = 0;
	UINT m_LastTickCount = 0;

	if(m_dnaEvents) // cant delete becuase created by app, but can prevent destructor from crashing
		m_dnaEvents->m_dnaCore = NULL;

	delete m_dnaChat;
	m_dnaChat = NULL;

	delete m_dnaMeta;
	m_dnaMeta = NULL;

	delete m_dnaUpdate;
	m_dnaUpdate = NULL;

	delete m_dnaUpload;
	m_dnaUpload = NULL;

	delete m_dnaDownload;
	m_dnaDownload = NULL;

	delete m_dnaSearch;
	m_dnaSearch = NULL;

	delete m_dnaShare;
	m_dnaShare = NULL;

	delete m_dnaCache;
	m_dnaCache = NULL;

	delete m_dnaNetwork;
	m_dnaNetwork = NULL;

	delete m_dnaPrefs;
	m_dnaPrefs = NULL;

	
	delete m_gnuCore;
	m_gnuCore = NULL;


	ComCore = NULL;
}


//bool CDnaCore::SendCallback(CGnuCallbackType CalType, void* Param1, void* Param2, void *Param3)
//{
  //  if(!m_pCalWin)
   //     return false;
   // CGnucCallbackMsg m_CallMsg(CalType,Param1, Param2, Param3);

   // SendMessage(m_pCalWin,WM_GNUCDNA_CALLBACK,(WPARAM)&m_CallMsg,(LPARAM)NULL);

    //return true;
//}
// CCore message handlers

void CALLBACK SecTimerProc( 
    HWND hwnd,        // handle to window for timer messages 
    UINT message,     // WM_TIMER message 
    UINT idTimer,     // timer identifier 
    DWORD dwTime)     // current system time 
{ 
	// Prevent timer from over flowing
	UINT TickCount = GetTickCount();

	if(TickCount - ComCore->m_LastTickCount < 500)
		return;
	else
		ComCore->m_LastTickCount = TickCount;
   

	ComCore->m_gnuCore->SecondTimer();
} 

void CALLBACK HourTimerProc( 
    HWND hwnd,        // handle to window for timer messages 
    UINT message,     // WM_TIMER message 
    UINT idTimer,     // timer identifier 
    DWORD dwTime)     // current system time 
{ 

	ComCore->m_gnuCore->HourlyTimer();
} 

CString CDnaCore::GetRunPath(void)
{

	return m_gnuCore->m_RunPath;
}

void CDnaCore::SetRunPath(LPCTSTR newVal)
{

	m_gnuCore->m_RunPath = newVal;
}

void CDnaCore::Connect(void)
{

	m_gnuCore->Connect(NETWORK_GNUTELLA);
	m_gnuCore->Connect(NETWORK_G2);
}

void CDnaCore::Disconnect(void)
{

	m_gnuCore->Disconnect(NETWORK_GNUTELLA);
	m_gnuCore->Disconnect(NETWORK_G2);
}



BOOL CDnaCore::IsConnecting(void)
{
	if(m_gnuCore->m_pNet->m_pGnu || m_gnuCore->m_pNet->m_pG2)
		return TRUE;

	return FALSE;
}

DATE CDnaCore::GetUptime(void)
{

	if(m_gnuCore->m_pNet->m_pGnu)
	{
		CTimeSpan Uptime(CTime::GetCurrentTime() - m_gnuCore->m_pNet->m_pGnu->m_ClientUptime);
		COleDateTimeSpan OleTime(Uptime.GetDays(), Uptime.GetHours(), Uptime.GetMinutes(), Uptime.GetSeconds());
		return (DATE) OleTime;
	}

	return (DATE) 0;
}

LONG CDnaCore::GetBytesPerSecDown(void)
{
	int BytesDown = 0;

	if(m_gnuCore->m_pNet->m_pGnu)
		BytesDown += m_gnuCore->m_pNet->m_pGnu->m_NetSecBytesDown;

	if(m_gnuCore->m_pNet->m_pG2)
		BytesDown += m_gnuCore->m_pNet->m_pG2->m_NetSecBytesDown;

	BytesDown += m_gnuCore->m_pTrans->m_DownloadSecBytes;

	return BytesDown;
}

LONG CDnaCore::GetBytesPerSecUp(void)
{
	int BytesUp = 0;

	if(m_gnuCore->m_pNet->m_pGnu)
		BytesUp += m_gnuCore->m_pNet->m_pGnu->m_NetSecBytesUp;

	if(m_gnuCore->m_pNet->m_pG2)
		BytesUp += m_gnuCore->m_pNet->m_pG2->m_NetSecBytesUp;

	BytesUp += m_gnuCore->m_pTrans->m_UploadSecBytes;

	return BytesUp;
}

CString CDnaCore::GetClientName(void)
{
	CString strResult;

	strResult = m_gnuCore->m_ClientName;

	return strResult;
}

void CDnaCore::SetClientName(LPCTSTR newVal)
{
	m_gnuCore->m_ClientName = newVal;
}

CString CDnaCore::GetClientVersion(void)
{

	CString strResult;
	strResult = m_gnuCore->m_ClientVersion;
	return strResult;
}

void CDnaCore::SetClientVersion(LPCTSTR newVal)
{

	m_gnuCore->m_ClientVersion = newVal;
}

CString CDnaCore::GetCoreVersion(void)
{
	CString strResult;
	strResult = m_gnuCore->m_DnaVersion;
	m_gnuCore->DebugTrigger(false);
	return strResult;
}

CString CDnaCore::GetClientCode(void)
{

	CString strResult;

	strResult = m_gnuCore->m_ClientCode;

	return strResult;
}

void CDnaCore::SetClientCode(LPCTSTR newVal)
{
	CString NewCode = newVal;

	if(NewCode.GetLength() < 4)
		return;
	if(NewCode.GetLength() > 4)
		NewCode = NewCode.Left(4);

	NewCode.MakeUpper();

	m_gnuCore->m_ClientCode = NewCode;
}

std::vector<CString> CDnaCore::GetCoreCredits(void)
{
	return m_gnuCore->m_Credits;
}

CString CDnaCore::GetCoreLicense(void)
{

	CString strResult;

	strResult = m_gnuCore->m_License;
	
	m_gnuCore->DebugTrigger(true);

	return strResult;
}


void CDnaCore::Connect2(LONG NetworkID)
{

	m_gnuCore->Connect(NetworkID);
}

void CDnaCore::Disconnect2(LONG NetworkID)
{

	m_gnuCore->Disconnect(NetworkID);
}

BOOL CDnaCore::IsConnecting2(LONG NetworkID)
{

	if(NetworkID == NETWORK_GNUTELLA && m_gnuCore->m_pNet->m_pGnu)
		return TRUE;

	if(NetworkID == NETWORK_G2 && m_gnuCore->m_pNet->m_pG2)
		return TRUE;

	return FALSE;
}

DATE CDnaCore::GetUptime2(LONG NetworkID)
{

	if(NetworkID == NETWORK_GNUTELLA && m_gnuCore->m_pNet->m_pGnu)
	{
		CTimeSpan Uptime(CTime::GetCurrentTime() - m_gnuCore->m_pNet->m_pGnu->m_ClientUptime);
		COleDateTimeSpan OleTime(Uptime.GetDays(), Uptime.GetHours(), Uptime.GetMinutes(), Uptime.GetSeconds());
		return (DATE) OleTime;
	}

	if(NetworkID == NETWORK_G2 && m_gnuCore->m_pNet->m_pG2)
	{
		CTimeSpan Uptime(CTime::GetCurrentTime() - m_gnuCore->m_pNet->m_pG2->m_ClientUptime);
		COleDateTimeSpan OleTime(Uptime.GetDays(), Uptime.GetHours(), Uptime.GetMinutes(), Uptime.GetSeconds());
		return (DATE) OleTime;
	}

	return (DATE)0;
}



