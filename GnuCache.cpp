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

#include "GnuRouting.h"
#include "GnuControl.h"
#include "GnuNetworks.h"
#include "GnuPrefs.h"

#include "GnuCache.h"


#define MODE_HOSTFILE		1
#define MODE_REQUEST		2
#define MODE_UPDATE			3
#define MODE_ADDCACHE		4
#define MODE_GET			5

#define REQUEST_WAIT		15

#define WEBCACHE_TRIES 5

UINT WebCacheWorker(LPVOID pVoidCache);


CGnuCache::CGnuCache(CGnuNetworks* pNet)
{
	m_pNet   = pNet;
	m_pCore  = pNet->m_pCore;
	m_pPrefs = pNet->m_pCore->m_pPrefs;

	m_MaxCacheSize	  = 200;
	m_MaxWebCacheSize = 500;

	m_pWebCacheThread = NULL;

	m_TriedSites = 0;
	m_TotalSites = 0;

	m_AllowPrivateIPs = false;

	m_ThreadEnded = false;
	m_StopThread = false;

	m_SecsLastRequest = REQUEST_WAIT;
}

CGnuCache::~CGnuCache()
{
	TRACE0("*** CGnuCache Deconstructing\n");
}

void CGnuCache::endThreads()
{
	m_StopThread = true;
	GnuEndThread(m_pWebCacheThread);
}

void CGnuCache::LoadCache(CString HostFile)
{	
	CStdioFile infile;
	
	// Add permanent servers from preferences
	for(int i = 0; i < m_pPrefs->m_HostServers.size(); i++)
		m_GnuPerm.push_back( Node(m_pPrefs->m_HostServers[i].Host, m_pPrefs->m_HostServers[i].Port) );

	Node CachedHost;

	// Load nodes from file cache
	if (infile.Open(HostFile, CFile::modeCreate | CFile::modeNoTruncate	| CFile::modeRead))
	{
		CString NextLine;

		while (infile.ReadString(NextLine))
		{
			Node readNode;
			readNode.Host = ParseString(NextLine, ':');
			readNode.Port = atoi( ParseString(NextLine, ':') );

			if( !NextLine.IsEmpty() )
				readNode.Network = atoi( ParseString(NextLine, ':') );

			if(readNode.Host.IsEmpty() &&
				readNode.Host.Compare("0.0.0.0") == 0 &&
				readNode.Host.Compare("255.255.255.255") == 0 &&
				readNode.Port == 0)
				continue;


			if(readNode.Network == NETWORK_GNUTELLA)
				m_GnuPerm.push_back(readNode);

			if(readNode.Network == NETWORK_G2)
				m_G2Perm.push_back(readNode);
		}

		infile.Abort();
	}
}

void CGnuCache::SaveCache(CString HostFile)
{
	int slashpos = HostFile.ReverseFind('\\');
	if(slashpos != -1)
		CreateDirectory(HostFile.Left(slashpos + 1), NULL);


	CStdioFile outfile;

	// Save first 100 from perm and first 100 from real to equal 200, interlaced
	if( outfile.Open(HostFile, CFile::modeCreate | CFile::modeWrite) )
	{
		WriteCache(outfile, m_GnuPerm);
		WriteCache(outfile, m_G2Perm);

		outfile.Abort();
	}
}

void CGnuCache::WriteCache(CStdioFile& OutFile, std::list<Node> &NodeList)
{
	std::list<Node>::iterator itNode = NodeList.begin();
	for(int i = 0 ; itNode != NodeList.end(); itNode++, i++)
	{
		CString writeNode = (*itNode).Host + ":" + NumtoStr((*itNode).Port) + ":" + NumtoStr((*itNode).Network);
	
		OutFile.WriteString( writeNode + "\n");
	}
}

void CGnuCache::AddKnown(Node KnownNode)
{
	if(KnownNode.Host.Compare("0.0.0.0") == 0 || KnownNode.Host.Compare("255.255.255.255") == 0 || KnownNode.Port == 0)
		return;


	if( m_pNet->NotLocal( Node(KnownNode.Host, KnownNode.Port) ) && m_pPrefs->AllowedIP(StrtoIP(KnownNode.Host)) && !m_pPrefs->BlockedIP(StrtoIP(KnownNode.Host)))
	{
		if( !m_AllowPrivateIPs )
			if( IsPrivateIP(StrtoIP(KnownNode.Host)) )
				return;

		std::list<Node>* ActiveCache = NULL;

		if( KnownNode.Network == NETWORK_GNUTELLA)
			ActiveCache = &m_GnuReal;
		
		if( KnownNode.Network == NETWORK_G2)
			ActiveCache = &m_G2Real;
		

		if(ActiveCache)
		{
			// put hosts tried in last minute at back of list, probably will be popped anyways
			// fresh hosts at top of list

			if( IsRecent(StrtoIP(KnownNode.Host)) )
				ActiveCache->push_back(KnownNode);
			else
				ActiveCache->push_front(KnownNode);

			while(ActiveCache->size() > m_MaxCacheSize)
				ActiveCache->pop_back();
		}
		else
			ASSERT(0);
	}
}

void CGnuCache::AddWorking(Node WorkingNode)
{
	std::list<Node>* ActiveCache = NULL;

	if( WorkingNode.Network == NETWORK_GNUTELLA)
		ActiveCache = &m_GnuPerm;
	
	if( WorkingNode.Network == NETWORK_G2)
		ActiveCache = &m_G2Perm;
	

	if( !m_AllowPrivateIPs )
		if( IsPrivateIP(StrtoIP(WorkingNode.Host)) )
			return;

	if(ActiveCache)
	{
		// Check for duplicates in list
		std::list<Node>::iterator itNode;
		for(itNode = ActiveCache->begin(); itNode != ActiveCache->end(); itNode++)
			if( (*itNode).Host == WorkingNode.Host && (*itNode).Port == WorkingNode.Port)
			{
				ActiveCache->erase(itNode);
				break;
			}

		ActiveCache->push_front(WorkingNode);

		while(ActiveCache->size() > m_MaxCacheSize)
			ActiveCache->pop_back();
	}
}

bool CGnuCache::IsRecent(IP Host)
{
	std::list<IP>::iterator itIP;
	for(itIP = m_RecentIPs.begin(); itIP != m_RecentIPs.end(); itIP++)
		if( (*itIP).S_addr == Host.S_addr)
		{
			m_RecentIPs.erase(itIP);
			m_RecentIPs.push_back(Host);
			return true;
		}

	m_RecentIPs.push_back(Host);

	if(m_RecentIPs.size() > RECENT_SIZE)
		m_RecentIPs.pop_front();

	return false;		
}

void CGnuCache::RemoveIP(CString strIP, int Network)
{
	std::list<Node>::iterator itNode;

	if( Network == NETWORK_GNUTELLA)
	{	
		itNode = m_GnuPerm.begin();
		while(itNode != m_GnuPerm.end())
			if( (*itNode).Host == strIP )
				itNode = m_GnuPerm.erase(itNode);
			else
				itNode++;

		// Write real time node list
		itNode = m_GnuReal.begin();
		while(itNode != m_GnuReal.end())
			if( (*itNode).Host == strIP )
				itNode = m_GnuReal.erase(itNode);
			else
				itNode++;
	}

	if( Network == NETWORK_G2)
	{
		itNode = m_G2Perm.begin();
		while(itNode != m_G2Perm.end())
			if( (*itNode).Host == strIP )
				itNode = m_G2Perm.erase(itNode);
			else
				itNode++;

		// Write real time node list
		itNode = m_G2Real.begin();
		while(itNode != m_G2Real.end())
			if( (*itNode).Host == strIP )
				itNode = m_G2Real.erase(itNode);
			else
				itNode++;

	}
}


///////////////////////////////////////////////////////////////////////
// gCache support

void CGnuCache::Timer()
{
	// Transfer new nodes from thread to cache
	m_TransferAccess.Lock();
	
	while(m_WebTransferList.size())
	{
		// add to both g1/g2 caches
		Node NewNode(m_WebTransferList.back());
		AddKnown( NewNode );

		NewNode.Network = NETWORK_G2;
		AddKnown( NewNode );

		m_WebTransferList.pop_back();
	}
	
	m_TransferAccess.Unlock();


	// If thread terminated, reset thread container
	if(m_ThreadEnded)
	{
		GnuEndThread(m_pWebCacheThread);
		m_ThreadEnded = false;
	}

	m_SecsLastRequest++;
}

void CGnuCache::LoadWebCaches(CString WebHostFile)
{
	CString URL;
	int		State;
	CTime	LastRef;
	int		ErrCount;
	int		GWCVer;

	CString	strState;
	CString strLastRef;
	CString	strErrCount;
	CString	strGWCVer;

	// Loads the list of cache URLs from the specified file
	// m_AltWebCaches.clear(); already checks for duplicates below
	
	CStdioFile infile;
	
	if (infile.Open(WebHostFile, CFile::modeCreate | CFile::modeNoTruncate	| CFile::modeRead))
	{
		CString NextLine;

		while (infile.ReadString(NextLine))
			if (!NextLine.IsEmpty())
			{
				bool AddHost = true;
				
				if ( NextLine.Find("|") != -1 )
				{
					URL = ParseString(NextLine, '|').Trim();
					strState = ParseString(NextLine, '|');
					strLastRef = ParseString(NextLine, '|');
					strErrCount = ParseString(NextLine, '|');
					strGWCVer = ParseString(NextLine, '|');
					if ( !strState.IsEmpty() )
						State = atoi(strState);
					else
						State = UNTESTED;
					if ( !strLastRef.IsEmpty() && strLastRef.GetLength() == 14 ) 
						LastRef = CTime(atoi(strLastRef.Left(4)),
							atoi(strLastRef.Mid(4,2)),
							atoi(strLastRef.Mid(6,2)),
							atoi(strLastRef.Mid(8,2)),
							atoi(strLastRef.Mid(10,2)),
							atoi(strLastRef.Mid(12,2)));
					else
						LastRef = CTime::GetCurrentTime();
					if ( !strErrCount.IsEmpty() )
						ErrCount = atoi(strErrCount);
					else
						ErrCount = 0;
					if ( !strGWCVer.IsEmpty() )
						GWCVer = atoi(strGWCVer);
					else
						GWCVer = GWC_VERSION1;
				} 
				else
				{
					URL      = NextLine.Trim();
					State    = UNTESTED;
					LastRef  = 0;
					ErrCount = 0;
					GWCVer   = GWC_VERSION1;
				}

				for(int i = 0; i < m_AltWebCaches.size(); i++)
				{
					if(m_AltWebCaches[i].URL == URL)
						AddHost = false;
				}

				if(AddHost) 
				{
					// set because cache being added for first time 
					// prevent caches not being tried between runs
					LastRef  = 0; 

					if (ValidURL(URL)) 
					{
						m_AltWebCaches.push_back(AltWebCache(URL,State,LastRef,ErrCount,GWCVer));
					}
					else m_pCore->LogError("Couldn't add cache " + URL + ", invalid URL");
				}
			}

		infile.Close();
	}

}

void CGnuCache::SaveWebCaches(CString WebHostFile)
{
	// Saves the list of web caches to the specified file
	
	CStdioFile outfile;
	CTimeSpan span;
	
	if( outfile.Open(WebHostFile, CFile::modeCreate | CFile::modeWrite) )
	{
		for(int i = 0; i < m_AltWebCaches.size(); i++)
		{
// remove max size check as we're now keeping dead cache's for 60 days
//			if(i > m_MaxWebCacheSize)
//				break;

			// let's save dead web caches for 60 days before we drop them from the list
			// this keeps a dead cache from being added back into our list if we are
			// handed the dead cache via a new GWebCache query result
			span = CTime::GetCurrentTime() - m_AltWebCaches[i].LastRef;
			if( m_AltWebCaches[i].State != DEAD || span.GetDays() < 61 )
				outfile.WriteString( m_AltWebCaches[i].URL + "|" +
					NumtoStr(m_AltWebCaches[i].State) + "|" +
					m_AltWebCaches[i].LastRef.Format("%Y%m%d%H%M%S") + "|" +
					NumtoStr(m_AltWebCaches[i].ErrCount) + "|" +
					NumtoStr(m_AltWebCaches[i].GWCVer) + "\n");
		}

		outfile.Close();
	}
}


void CGnuCache::WebCacheRequest(bool HostFileOnly)
{ 

	// Starts thread to retrieve hostfile (and urlfile)
	if(!m_pWebCacheThread)
	{
		if(m_SecsLastRequest < REQUEST_WAIT)
			return;

		if( HostFileOnly )
			m_WebMode = MODE_HOSTFILE; // gets hostfile only
		else
			m_WebMode = MODE_REQUEST; // gets both (for startup)

		// Dont bother if no web caches 
		if( !GetRandWebCache(false).IsEmpty() )
		{
			m_SecsLastRequest = 0;

			GnuStartThread(m_pWebCacheThread, WebCacheWorker, this);
		}
		else
		{
			m_pCore->LogError("WebCacheRequest can't find random web cache!");
		}
	}
}

void CGnuCache::WebCacheGetRequest(CString network)
{ 

	// Starts thread to retrieve hostfile (and urlfile)
	if(!m_pWebCacheThread)
	{
		if(m_SecsLastRequest < REQUEST_WAIT)
			return;

		m_WebNetwork = network;

		m_WebMode = MODE_GET; // GWC v2 get request

		// Dont bother if no web caches 
		if( !GetRandWebCache(false).IsEmpty() )
		{
			m_SecsLastRequest = 0;

			GnuStartThread(m_pWebCacheThread, WebCacheWorker, this);
		} 
		else
		{
			m_pCore->LogError("WebCacheGetRequest can't find random web cache!");
		}
	}
}


void CGnuCache::WebCacheUpdate()
{
	//TRACE0("### WebCacheUpdate Called\n"); //DB

	// Send an update request to a random cache
	if(!m_pWebCacheThread)
	{
		m_WebMode = MODE_UPDATE;
		
		GnuStartThread(m_pWebCacheThread, WebCacheWorker, this);
	}
}

void CGnuCache::WebCacheAddCache(CString NewURL)
{
	// Add New Cache to our list
	bool AddHost = true;
	for(int i = 0; i < m_AltWebCaches.size(); i++)
		if(m_AltWebCaches[i].URL == NewURL)
			AddHost = false;

	if(ValidURL(NewURL) && AddHost)
		m_AltWebCaches.push_back( AltWebCache(NewURL, ALIVE, 0, 0, GWC_VERSION2) );
}

bool CGnuCache::WebCacheSeedCache(CString NewURL)
{
	//TRACE0("### WebCacheAddCache Called\n"); //DB

	// Add a new web cache to the web cache system
	if(!m_pWebCacheThread)
	{
		if(!ValidURL(NewURL))
		{
			//AfxMessageBox("The cache you attempted to add was not in the correct format.\n\nPlease try again.");
			m_pCore->LogError("Couldn't add cache " + NewURL + ", invalid URL");
	
			return false;
		}

		m_WebMode = MODE_ADDCACHE;
		m_NewSite = NewURL; // the URL we're adding
		
		GnuStartThread(m_pWebCacheThread, WebCacheWorker, this);

		return true;
	}

	m_pCore->LogError("Couldn't add cache " + NewURL + ", currently busy, try again later");
	return false;
}

UINT WebCacheWorker(LPVOID pVoidCache)
{
	//TRACE0("*** WebCache Thread Started\n");
	CGnuCache*	  pCache = (CGnuCache*) pVoidCache;
	CGnuNetworks* pNet   = pCache->m_pNet;
	CGnuCore*     pCore  = pCache->m_pCore;
	int GWCVer;
	
	srand((unsigned)time(NULL));	

	// *** HOSTFILE REQUEST ***
	if(pCache->m_WebMode == MODE_REQUEST || pCache->m_WebMode == MODE_HOSTFILE)
	{
		int attempts = WEBCACHE_TRIES;
		
		CString CacheURL;
		CString strFile;

		bool isGoodCache;
		
		//TRACE0("### Hostfile Request\n"); //DB
		
		while(attempts > 0)
		{
			CacheURL = pCache->GetRandWebCache(false); // get a cache to try

			if(CacheURL.IsEmpty())
				break;

			strFile = pCache->WebCacheDoRequest(CacheURL + "?hostfile=1");

			isGoodCache = pCache->WebCacheParseResponse(CacheURL, strFile);

			if ( isGoodCache )
				attempts = 0;
			else
				attempts--;

			if ( pCache->m_StopThread )
				return 0;
		}
	}

	// *** URLFILE REQUEST ***
	if(pCache->m_WebMode == MODE_REQUEST)
	{
		int attempts = WEBCACHE_TRIES;

		CString CacheURL;
		CString strFile;

		bool isGoodCache;
		
		//TRACE0("### URLFile Request\n"); //DB

		while(attempts > 0) 
		{
			CacheURL = pCache->GetRandWebCache(false);

			if(CacheURL.IsEmpty())
				break;

			strFile = pCache->WebCacheDoRequest(CacheURL + "?urlfile=1");

			isGoodCache = pCache->WebCacheParseResponse(CacheURL, strFile);

			if ( isGoodCache )
				attempts = 0;
			else
				attempts--;

			if(pCache->m_StopThread)
				return 0;
		}
		
	}

	// *** UPDATE REQUEST ***
	else if(pCache->m_WebMode == MODE_UPDATE)
	{
		int attempts = WEBCACHE_TRIES;
		CString Command, CacheURL, strFile;		

		// set-up: get our IP and a random URL to send
		IP HostIP = pNet->m_CurrentIP;
			
		if(pCache->m_pPrefs->m_ForcedHost.S_addr)
			HostIP = pCache->m_pPrefs->m_ForcedHost;
		
		Command = "?ip=" + pCache->EscapeEncode(IPtoStr(HostIP) + ":" + NumtoStr(pNet->m_CurrentPort));
		
		CString AddURL = pCache->GetRandWebCache(true);
		
		// if GetRandWebCache couldn't get us a live cache we can't submit one
		if(!AddURL.IsEmpty()) 
			Command += "&url=" + pCache->EscapeEncode(AddURL);
		
		//TRACE0("### Update Request: "+Command+"\n"); //DB
		
		while(attempts > 0)
		{
			CacheURL = pCache->GetRandWebCache(false);

			if( CacheURL.IsEmpty() )
				break;

			strFile  = pCache->WebCacheDoRequest(CacheURL + Command);

			strFile.MakeUpper();

			if ( strFile.Find("|") == 1 )
				GWCVer = GWC_VERSION2;
			else
				GWCVer = GWC_VERSION1;

			// see if cache gave the right response
			if( !strFile.IsEmpty() && (strFile.Find("OK") == 0) || strFile.Find("I|UPDATE|OK") != -1) 
			{
				//TRACE0("### Cache was Good\n"); //DB
				pCache->MarkWebCache(CacheURL, true, GWCVer);
				attempts = 0;
			}
			// bad cache
			else 
			{
				//TRACE0("### Cache was Bad\n"); //DB
				pCache->MarkWebCache(CacheURL, false, GWCVer);
				attempts--;
			}
			if(pCache->m_StopThread)
				return 0;
		}
	}	
	// *** ADD CACHE ***
	else if(pCache->m_WebMode == MODE_ADDCACHE)
	{
		//TRACE0("### Adding Web Cache...\n"); //DB
		
		pCache->m_TotalSites = pCache->m_AltWebCaches.size() + 2;
		
		// first, ping the cache to see if it is good
		CString strFile = pCache->WebCacheDoRequest(pCache->m_NewSite + "?ping=1");
		
		if(pCache->m_StopThread)
			return 0;

		if ( strFile.Find("|") == 1 )
			GWCVer = GWC_VERSION2;
		else
			GWCVer = GWC_VERSION1;

		strFile.MakeUpper();

		if( strFile.Find("PONG") == 0 || strFile.Find("I|PONG|") ==  0 ) // Good response
		{
			// Add New Cache to all other known caches
			CString Command = "?url=" + pCache->EscapeEncode(pCache->m_NewSite);
			
			pCache->m_TriedSites = 1;
			
			std::vector<AltWebCache>::iterator itCache;
			for(itCache = pCache->m_AltWebCaches.begin(); itCache != pCache->m_AltWebCaches.end(); itCache++)
			{
				CTimeSpan span = CTime::GetCurrentTime() - (*itCache).LastRef; 
				if( (*itCache).URL != pCache->m_NewSite && (*itCache).State != DEAD && span.GetTotalSeconds() > 3600 )
				{
					int tmpVer;
					strFile = pCache->WebCacheDoRequest((*itCache).URL + Command);

					if ( strFile.Find("|") == 1 )
						tmpVer = GWC_VERSION2;
					else
						tmpVer = GWC_VERSION1;

					strFile.MakeUpper();

					// cache gave the right response
					if( !strFile.IsEmpty() && (strFile.Find("OK") == 0 || strFile.Find("I|UPDATE|OK") != -1) ) 
					{
						//TRACE0("### Cache was Good\n"); //DB
						pCache->MarkWebCache((*itCache).URL, true, tmpVer);
					}
					// bad cache
					else 
					{
						//TRACE0("### Cache was Bad\n"); //DB
						pCache->MarkWebCache((*itCache).URL, false, tmpVer);
					}

					pCache->m_TriedSites++;

				}

				if(pCache->m_StopThread)
					return 0;
			}

			// Add New Cache to our list
			bool AddHost = true;
			for(int i = 0; i < pCache->m_AltWebCaches.size(); i++)
				if(pCache->m_AltWebCaches[i].URL == pCache->m_NewSite)
					AddHost = false;

			if(pCache->ValidURL(pCache->m_NewSite) && AddHost)
				pCache->m_AltWebCaches.push_back( AltWebCache(pCache->m_NewSite,ALIVE,0,0,GWCVer) );
		}
		else
		{
			pCore->LogError("Couldn't add cache " + pCache->m_NewSite + ", did not respond correctly");
		}

		pCache->m_TotalSites = 1;
		pCache->m_TriedSites = 1;
	}
	// *** GET REQUEST ***
	else if ( pCache->m_WebMode == MODE_GET )
	{
		int attempts = WEBCACHE_TRIES;

		CString CacheURL;
		CString strFile;

		bool isGoodCache;
		
		//TRACE0("### GET Request\n"); //DB

		while(attempts > 0) 
		{
			CacheURL = pCache->GetRandWebCache(false);

			if(CacheURL.IsEmpty())
				break;

			strFile = pCache->WebCacheDoRequest(CacheURL + "?get=1");
	
			isGoodCache = pCache->WebCacheParseResponse(CacheURL, strFile);

			if ( isGoodCache )
				attempts = 0;
			else
				attempts--;

			if(pCache->m_StopThread)
				return 0;
		}

	}
	
	// Shouldn't save web cache list with hardcoded name since there are methods
	// in the Prefs interface to Load/Save the web cache list.
	// Save the web cache list back to the file
	//if(pCache->m_pPrefs->m_LanMode)
	//	pCache->SaveWebCacheList(pCache->m_pCore->m_RunPath + "LanWebCache.net");
	//else
	//	pCache->SaveWebCacheList(pCache->m_pCore->m_RunPath + "WebCache.net");
	
	

//#ifdef _DEBUG
//	pCache->DebugDumpWebCaches(); //DB
//#endif
	
	//TRACE0("### WebCacheWorker Done\n"); //DB
	
	pCache->m_ThreadEnded = true;

	return 0;
}

// this function actually sends the request
CString CGnuCache::WebCacheDoRequest(CString RequestURL)
{
	CString strFile;
		
	if( !m_pPrefs->m_NetworkName.IsEmpty() )
		RequestURL += "&net=" + m_pPrefs->m_NetworkName;
	else if( !m_WebNetwork.IsEmpty() )
		RequestURL += "&net=" + m_WebNetwork;

	RequestURL += "&client=" + m_pCore->m_ClientCode; //TEST";	// Forced to use client=TEST because GWC version 1.0.0.0 has auth list of clients
	RequestURL += "&version=" + m_pCore->m_DnaVersion;

	if ( RequestURL.Find("?urlfile=1") != -1 || RequestURL.Find("?hostfile=1") != -1 )
		RequestURL += "&get=1";

	if ( RequestURL.Find("?ip=") != -1 )
		RequestURL += "&update=1";


//	m_pCore->DebugLog("---> " + RequestURL);

	//TRACE0("### REQUEST: " + RequestURL + "\n"); //DB

	// Get session ready to connect to web
    CInternetSession GnucleusNet;
	char UserAgent[12] = "Mozilla/4.0"; // Get past blocking some "universities" do
	GnucleusNet.SetOption(INTERNET_OPTION_USER_AGENT, UserAgent, 11); 

	GnucleusNet.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 15000);
	GnucleusNet.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 15000); 
	GnucleusNet.SetOption(INTERNET_OPTION_SEND_TIMEOUT,	   15000);

	CHttpFile* pFile = NULL;
	
	try
	{
		pFile = (CHttpFile*) GnucleusNet.OpenURL(RequestURL, 1 , INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD);
		DWORD StatusCode = 0;
		if(pFile)
		{
			if(pFile->QueryInfoStatusCode(StatusCode))
				if(StatusCode >= 200 && StatusCode < 300)
				{
					BYTE buffer[50];
					int Read = 0;

					do
					{
						Read = pFile->Read(buffer, 50);

						for(int i = 0; i < Read; i++)
							strFile += buffer[i];

					} while(Read != 0);	   
				}
				else
					strFile = NumtoStr(StatusCode) + " ERROR";

			pFile->Close();             
			delete pFile;
			pFile = NULL;
		}
		else
			strFile = "ERROR Open address failed";
	}
	catch(...)
	{
		//TCHAR szCause[255];
		//p->GetErrorMessage(szCause, 255);
		//p->Delete();

		//TRACE0("### CInternetException: " + CString(szCause) + "\n"); //DB
		strFile = "ERROR Caught";
	}
	
	// fix CRLFs - turn into only LF
	if(strFile) 
	{ 
		// are there LFs?
		if( strFile.Find('\n') != -1)
			strFile.Remove('\r');
		// if not, make those CRs into LFs
		else
			strFile.Replace('\r','\n'); 
	}
	
	//TRACE0("### RESPONSE: " + strFile + "\n"); //DB

	return strFile;
}

CString CGnuCache::FindHeader(CString Handshake, CString Name)
{	
	CString Data;
	
	Name += ":";
	Name.MakeLower();

	Handshake.MakeLower();
	
	int keyPos = Handshake.Find(Name);

	if (keyPos != -1)
	{
		keyPos += Name.GetLength();

		Data = Handshake.Mid(keyPos, Handshake.Find("\n", keyPos) - keyPos);
		Data.TrimLeft();
	}
	
	return Data;
}

bool CGnuCache::WebCacheParseResponse(CString CacheURL, CString strResult) {
	bool isGood = true;
	bool AddHost;
	int total = 0;
	int good = 0;
	int pos;
	int GWCVer = GWC_VERSION1;

	if ( strResult.Find("ERROR") == 0 ) {
		isGood = false;
	}

	// break between header and data
	int breakline = strResult.Find("\n\n");
	if(breakline != -1)
	{
		// header
		CString Header = strResult.Mid(0, breakline + 1);

		CString RemoteIP = FindHeader(Header, "X-Remote-IP");
		if(!RemoteIP.IsEmpty())
			m_pNet->m_CurrentIP = StrtoIP(RemoteIP);
		

		// data
		strResult = strResult.Mid(breakline + 2);
	}
	
	CString NextLine = ParseString(strResult, '\n');

	while ( !NextLine.IsEmpty() )
	{
		if(NextLine == " ")
			break;

		total++;
		// see if this is a GWebCache version 2 response
		if ( NextLine.Find("|") == 1 ) 
		{
			GWCVer = GWC_VERSION2;
			CString Line = NextLine;
			CString cmd = ParseString(Line, '|').MakeLower();
			CString data = ParseString(Line, '|');
			CString extra = ParseString(Line, '|');
			if ( cmd.GetLength() == 1 )
			{
				switch ( cmd[0] )
				{
					case 'h':
						pos = data.Find(":");
						if ( pos > 0 && atoi(data.Mid(pos + 1)) > 0 && inet_addr(data.Left(pos)) != INADDR_NONE ) 
						{
							good++;
							m_TransferAccess.Lock();
							m_WebTransferList.push_back(data);
							m_TransferAccess.Unlock();
						} 
						else 
						{
							//m_pCore->DebugLog("Didn't like: " + NextLine);
							//TRACE0("Didn't like: " + NextLine); //DB
						}
						break;
					case 'u':
						if ( ValidURL(data) ) 
						{
							good++;
							AddHost = true;
							for(int i = 0; i < m_AltWebCaches.size(); i++) 
							{
								if(m_AltWebCaches[i].URL == data)
									AddHost = false;
							}
							if ( AddHost )
								m_AltWebCaches.push_back(AltWebCache(data));
						} 
						else 
						{
							//m_pCore->LogError("Didn't like: " + NextLine);
							//TRACE0("Didn't like: " + NextLine); //DB
						}
						break;
					default:
						// skip unknown responses if in x|blahblah| GWC version 2 format
						good++;
						break;
				}
			} 
			else 
			{
				//m_pCore->DebugLog("Didn't like: " + NextLine);
				//TRACE0("Didn't like: " + NextLine); //DB
			}
		} 
		// must be a GWebCache version 1 format response
		else 
		{
			GWCVer = GWC_VERSION1;
			// see if response is a url entry
			if ( ValidURL(NextLine) ) 
			{
				good++;
				AddHost = true;
				for(int i = 0; i < m_AltWebCaches.size(); i++)
					if ( m_AltWebCaches[i].URL == NextLine )
						AddHost = false;
				if ( AddHost ) 
					m_AltWebCaches.push_back(AltWebCache(NextLine));
			} 
			// not a url entry, must be a host:port entry
			else 
			{
				pos = NextLine.Find(":");
				if ( pos > 0 && atoi(NextLine.Mid(pos + 1)) > 0 && inet_addr(NextLine.Left(pos)) != INADDR_NONE ) 
				{
					good++;
					m_TransferAccess.Lock();
					m_WebTransferList.push_back(NextLine);
					m_TransferAccess.Unlock();
				} 
				else 
				{
					//m_pCore->DebugLog("Didn't like: " + NextLine);
				}
			}
		}
		NextLine = ParseString(strResult, '\n');
	}

	if ( total > 0 && total == good ) 
		isGood = true;
	else
		isGood = false;

	// if cache was good and almost empty, let's add ourselves
	if ( isGood && total < 5 ) 
	{
		IP HostIP = m_pNet->m_CurrentIP;

		if ( m_pPrefs->m_ForcedHost.S_addr )
			HostIP = m_pPrefs->m_ForcedHost;

		CString Command = "?ip=" + EscapeEncode(IPtoStr(HostIP) + ":" + NumtoStr(m_pNet->m_CurrentPort));

		CString AddURL = GetRandWebCache(true);

		if ( !AddURL.IsEmpty() && AddURL != CacheURL )
			Command += "&url=" + EscapeEncode(AddURL);

		WebCacheDoRequest(CacheURL + Command);

	}

	// need to add code to add ip and url if cache not full (ie. total < 5 )

	if ( isGood ) 
	{
		MarkWebCache(CacheURL, true, GWCVer);
	} 
	else 
	{
		MarkWebCache(CacheURL, false, GWCVer);
	}

	return isGood;
}

// mark a web cache as either DEAD, ALIVE, or GWCERROR and
// indicate the format of the cache response (version 1 or version 2)
void CGnuCache::MarkWebCache(CString CacheURL, bool IsAlive, int GWCVer)
{
	std::vector<AltWebCache>::iterator itCache;
	for(itCache = m_AltWebCaches.begin(); itCache != m_AltWebCaches.end(); itCache++)
		if((*itCache).URL == CacheURL)
		{
			(*itCache).LastRef = CTime::GetCurrentTime();
			// cache is alive, so clear error count
			if ( IsAlive ) 
			{
				(*itCache).State = ALIVE;
				(*itCache).GWCVer = GWCVer;
				(*itCache).ErrCount = 0;
				m_pCore->LogError("MarkWebCache: " + CacheURL + " (ALIVE)");
			}
			// cache is either dead or bump error count
			else 
			{
				(*itCache).ErrCount++;
				// cache has had too many errors, consider it dead
				if ( (*itCache).ErrCount > 2 ) 
				{
					(*itCache).State = DEAD;
					(*itCache).GWCVer = GWCVer;
					m_pCore->LogError("MarkWebCache: " + CacheURL + " (DEAD)");
				}
				// mark cache in error but not dead yet
				else 
				{
					(*itCache).State = GWCERROR;
					(*itCache).GWCVer = GWCVer;
					m_pCore->LogError("MarkWebCache: " + CacheURL + " (GWCERROR)");
				}
			}
			return;
		}
}

//DB: dump list of web caches
#ifdef _DEBUG
void CGnuCache::DebugDumpWebCaches()
{
	//TRACE0("### Web Cache List Dump\n");

	/*std::vector<AltWebCache>::iterator itCache;
	for(itCache = m_AltWebCaches.begin(); itCache != m_AltWebCaches.end(); itCache++) {
		//TRACE0((*itCache).URL);
		if( (*itCache).State == ALIVE )
			//TRACE0(" ALIVE\n");
		else if( (*itCache).State == DEAD )
			//TRACE0(" DEAD\n");
		else
			//TRACE0(" UNTESTED\n");
	}*/

	//TRACE0("### End Web Cache List Dump\n");
}
#endif

// this function returns a random web cache that is either UNTESTED or ALIVE
CString CGnuCache::GetRandWebCache(bool MustBeAlive)
{
	std::vector<AltWebCache> FoundCaches;
	CTimeSpan span;

	for(int i = 0; i < m_AltWebCaches.size(); i++) 
	{
		AltWebCache testCache = m_AltWebCaches[i];
		
		span = CTime::GetCurrentTime() - m_AltWebCaches[i].LastRef;
		if( ( m_AltWebCaches[i].State == ALIVE || (!MustBeAlive && m_AltWebCaches[i].State != DEAD) ) && 
			( span.GetTotalSeconds() > 3600 || m_AltWebCaches[i].State == UNTESTED ) &&
			( m_WebMode != MODE_GET || ( m_WebMode == MODE_GET && (m_AltWebCaches[i].GWCVer == GWC_VERSION2 || m_AltWebCaches[i].State == UNTESTED) ) ) )
				FoundCaches.push_back(m_AltWebCaches[i]);
	}

	if(FoundCaches.size()) // sometimes we don't have alive caches
		return FoundCaches[ rand() % FoundCaches.size() + 0].URL;
	else
		return "";
}

// "escape encode" values for submission
CString CGnuCache::EscapeEncode(CString &what)
{
	what.Replace(":", "%3A");
	what.Replace("/", "%2F");
	what.Replace(" ", "%20");
	what.Replace("+", "%2B");

	return what;
}

// basic check to see if a URL is valid
bool CGnuCache::ValidURL(CString WebHost)
{
	char c;
	CString validChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:.-/~_";

	WebHost.Replace("%3A",":");
	WebHost.Replace("%2F","/");

	if( WebHost.Find("http://") != 0 )
		return false;

	WebHost.Replace("http://","");
	WebHost.Replace("%7E","~");
	WebHost.Replace("%7e","~");

	if( WebHost.IsEmpty() )
		return false;

	for( int i = 0; i < WebHost.GetLength(); i++ )
	{
		c = WebHost[i];
		if( validChars.Find(c) == -1 )
		{
			return false;
		}
	}

	return true;
}
