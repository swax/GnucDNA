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


#include "StdAfx.h"

#include "DnaCore.h"
#include "GnuNetworks.h"
#include "GnuPrefs.h"
#include "GnuShare.h"
#include "GnuMeta.h"
#include "GnuLocal.h"
#include "GnuTransfers.h"
#include "GnuUpdate.h"
#include "GnuChat.h"

//#include "psapi.h"
//#include "process.h"

#include "GnuCore.h"


CGnuCore::CGnuCore(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;

	
	//DNALog_Date.y04.m03.d14._Time.h20.m16.txt
	CTime NowTime = CTime::GetCurrentTime();
	m_DebugFilename = NowTime.Format("DNALOG_Date.y%y.m%m.d%d._Time.h%H.m%M.txt");

	// Debugging
	/*#ifdef _DEBUG
	m_OldMemState.Checkpoint();
	#endif*/

	srand( (unsigned)time(NULL) );

	SetLicense();
	SetCredits();

	m_DnaVersion	= DNA_VERSION;

	m_ClientName    = "GnuApp";
	m_ClientVersion = m_DnaVersion;
	m_ClientCode    = "GDNA";

	// Path to running exe
	TCHAR szModule[_MAX_PATH];
	GetModuleFileName(NULL, szModule, _MAX_PATH);
	m_RunPath = szModule;
	m_RunPath = m_RunPath.Mid(0, m_RunPath.ReverseFind('\\') + 1);

	m_StartTime = CTime::GetCurrentTime();

	// Determine system type memory and cpu
	ScanPerformance();


	// Initialize components
	m_pShare  = new CGnuShare(this);
	m_pMeta   = new CGnuMeta(this);
	m_pPrefs  = new CGnuPrefs(this);
	m_pTrans  = new CGnuTransfers();
	m_pNet    = new CGnuNetworks(this);
	m_pUpdate = new CGnuUpdate(this);
	m_pChat   = new CGnuChat(this);
	
	
	// Start sharing and load files
	m_pShare->InitShare();
	m_pTrans->InitTransfers(this);


	//CFile TestFile;
	//TestFile.Open( m_pPrefs->m_DownloadPath + "\\testsend.mp3", CFile::modeCreate | CFile::modeReadWrite);
	//TestFile.SetLength(1024 * 1024 * 5); // 5 megs
	//TestFile.Abort();
}

CGnuCore::~CGnuCore()
{
	m_pShare->endThreads();
	m_pUpdate->endThreads();

	delete m_pChat;
	m_pChat = NULL;

	delete m_pUpdate;
	m_pUpdate = NULL;

	delete m_pTrans;
	m_pTrans = NULL;

	delete m_pNet;
	m_pNet  = NULL;
	
	delete m_pPrefs;
	m_pPrefs = NULL;

	delete m_pMeta;
	m_pMeta = NULL;

	delete m_pShare;
	m_pShare = NULL;
}


void CGnuCore::Connect(int NetworkID)
{
	if(NetworkID == NETWORK_GNUTELLA)
		m_pNet->Connect_Gnu();

	if(NetworkID == NETWORK_G2)
		m_pNet->Connect_G2();
}


void CGnuCore::Disconnect(int NetworkID)
{
	if(NetworkID == NETWORK_GNUTELLA)
		m_pNet->Disconnect_Gnu();

	if(NetworkID == NETWORK_G2)
		m_pNet->Disconnect_G2();
}

void CGnuCore::SecondTimer()
{
	m_pNet->Timer();

	m_pTrans->Timer();
	
	m_pShare->Timer();

	m_pUpdate->Timer();

	m_pChat->Timer();
}

void CGnuCore::HourlyTimer()
{
	m_pNet->HourlyTimer();

	// Check for new updates
	m_pUpdate->Check();
}

CString CGnuCore::GetUserAgent()
{
	// GnuApp 2.0 (GnucDNA 1.0)
	return m_ClientName + " " + m_ClientVersion + " (GnucDNA " + m_DnaVersion + ")";
}

void CGnuCore::ScanPerformance()
{
	// Determine what kind of system we're running
	OSVERSIONINFO osv;
	osv.dwOSVersionInfoSize = sizeof(osv);
	GetVersionEx(&osv);
	
	m_IsKernalNT = (osv.dwPlatformId == VER_PLATFORM_WIN32_NT) ? true : false;
	
	m_IsSp2 = false;
	if(osv.dwMajorVersion == 5 && osv.dwMinorVersion == 1)
	{
		OSVERSIONINFOEX osvEx;
		osvEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		GetVersionEx((OSVERSIONINFO*)&osvEx);

		if (osvEx.wServicePackMajor >= 2)
			m_IsSp2 = true;
	}

	// Get total system memory
	MEMORYSTATUS memstat;
	GlobalMemoryStatus(&memstat);
	m_SysMemory = memstat.dwTotalPhys / 1024 / 1024;


	// Get CPU speed
	m_SysSpeed = _GetCPUSpeed();

	// Sometimes error
	if(m_SysSpeed > 100000)
		m_SysSpeed = 888;
}

int CGnuCore::_GetCPUSpeed()
{
    unsigned __int64 start, stop;
    unsigned __int64 nCtr, nFreq, nCtrStop;
    QueryPerformanceFrequency((LARGE_INTEGER *)&nFreq);

    _asm _emit 0x0F
    _asm _emit 0x31
    _asm mov DWORD PTR start, eax
    _asm mov DWORD PTR [start+4], edx

    QueryPerformanceCounter((LARGE_INTEGER *)&nCtrStop);
    nCtrStop += nFreq;
    do
    {
        QueryPerformanceCounter((LARGE_INTEGER *)&nCtr);
    } while (nCtr < nCtrStop);

    _asm _emit 0x0F
    _asm _emit 0x31
    _asm mov DWORD PTR stop, eax
    _asm mov DWORD PTR [stop+4], edx

    // stop-start is speed in Hz
    // divided by 1,000,000 is speed in MHz
    return (UINT)((stop-start)/1000000);
} 

void CGnuCore::SetLicense()
{
	m_License = "Copyright (C) 2000-2005 John Marshall Group - GnucDNA comes with ABSOLUTELY \
NO WARRANTY.  This is free software, and you are welcome to redistribute \
it under certain conditions.  Read GPL.txt in the library directory for \
further information concerning licensing.";
}	

void CGnuCore::SetCredits()
{
	// Author
	m_Credits.push_back("John Marshall");

	// Gnutella creators
	m_Credits.push_back("Justin Frankel");
	m_Credits.push_back("Tom Pepper");

	// Programmers
	m_Credits.push_back("Mark Dennehy");
	m_Credits.push_back("Aaron Putnam");
	m_Credits.push_back("Scott Kirkwood");
	m_Credits.push_back("Heiner Eichman");
	m_Credits.push_back("Makslane Rodrigues");
	m_Credits.push_back("Tor Klingberg");
	m_Credits.push_back("Nathan Brown");
	m_Credits.push_back("Nigel Heath");
	m_Credits.push_back("Justin Marrese");
}

void CGnuCore::LogError(CString Error)
{
	//TRACE0("*** " + Error + "\n");

	//while(m_ErrorList.size() > 1000)
	//	m_ErrorList.pop_back();

	//ErrorInfo ErrorRpt;
	//ErrorRpt.Description = Error;
	//ErrorRpt.Time = CTime::GetCurrentTime();
	//	
	//m_ErrorList.push_front(ErrorRpt);
}

void CGnuCore::DebugLog(CString Section, CString Entry)
{
#ifdef _DEBUG
	
	CStdioFile LogFile;

	if( LogFile.Open(m_RunPath + m_DebugFilename, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite) )
	{
		LogFile.SeekToEnd();

		CTimeSpan Running = CTime::GetCurrentTime() - m_StartTime;
		CString Timestamp = NumtoStr(Running.GetTotalMinutes()) + ":" + NumtoStr(Running.GetSeconds());
		
		// 12:34 Network, Bad Packet
		LogFile.WriteString(Timestamp + " " + Section + ", " + Entry + "\r\n");
	
		LogFile.Abort();
	}
	
#endif
}

/*#ifdef _DEBUG
uint32 CGnuCore::GetVirtualMemUsage()
{
	HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, _getpid());

	if(handle)
	{
		PROCESS_MEMORY_COUNTERS pmemory;
		GetProcessMemoryInfo(handle, &pmemory, sizeof(PROCESS_MEMORY_COUNTERS));

		CloseHandle(handle);

		return pmemory.PagefileUsage;
	}

	return 0;
}
#endif*/

void CGnuCore::DebugTrigger(bool details)
{
	/*#ifdef _DEBUG

	m_NewMemState.Checkpoint();

	m_DiffMemState.Difference(m_OldMemState, m_NewMemState);


	TRACE0("\n\n*** Current Mem State...\n");
	m_NewMemState.DumpStatistics();
	TRACE0("*** Memory Difference...\n");
	m_DiffMemState.DumpStatistics();

	if(details)
	{	
		TRACE0("*** Allocated Memory...\n");
		m_OldMemState.DumpAllObjectsSince();
	}


	TRACE0("\n\n");


	m_OldMemState.Checkpoint();

	#endif*/
}


