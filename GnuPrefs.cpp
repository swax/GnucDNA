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
#include "GnuCore.h"

#include "GnuNetworks.h"
#include "GnuShare.h"
#include "GnuControl.h"
#include "GnuTransfers.h"

#include "GnuPrefs.h"

CGnuPrefs::CGnuPrefs(CGnuCore* pCore)
{
	m_pCore  = pCore;
	m_pShare = pCore->m_pShare;

	LoadDefaults();
}

CGnuPrefs::~CGnuPrefs()
{
}

void CGnuPrefs::LoadDefaults()
{
	// Local
	m_ForcedHost		= StrtoIP("0.0.0.0"); 
	m_ForcedPort		= 0;

	m_SpeedStatic		= 0;
	m_SpeedDown			= 0;
	m_SpeedUp			= 0;

	m_Update			= UPDATE_RELEASE;
	GnuCreateGuid(&m_ClientID);

	// Local Network
	m_SupernodeAble		= true;
	m_MaxLeaves			= CalcMaxLeaves();

	m_LanMode			= false;

	// Local Firewall
	m_BehindFirewall	= false;

	// Connect
	m_LeafModeConnects	= 1;
	m_MinConnects		= 4;
	m_MaxConnects		= 6;

	// G2 Connect Prefs
	m_G2ChildConnects = 1;
	m_G2MinConnects   = 6;
	m_G2MaxConnects   = 9;

	// Connect Servers

	// Connect Screen

	// Search
	m_DownloadPath		= m_pCore->m_RunPath + "\\Downloads";
	m_DoubleCheck		= true;
	m_ScreenNodes		= false;

	// Search Screen

	// Share
	m_ReplyFilePath		= false;
	m_MaxReplies		= 64;
	m_SendOnlyAvail		= false;

	// Transfer
	m_PartialDir     = m_pCore->m_RunPath + "\\Partials";
	m_MaxDownloads	 = 4;
	m_MaxUploads     = 8;
	m_Multisource	 = true;

	// Bandwidth
	m_BandwidthUp	    = 0;
	m_BandwidthDown		= 0;
	m_MinDownSpeed		= 0;
	m_MinUpSpeed		= 0;

	// Antivirus
	m_AntivirusEnabled = false;

	// Geo location
	m_GeoLatitude  = 0;
	m_GeoLongitude = 0;
}

void CGnuPrefs::LoadConfig(CString ConfigFile)
{
	char buffer[256];

    // Local
	GetPrivateProfileString("Local",  "ForcedHost",		"0.0.0.0",	buffer, 256, ConfigFile);
	CString strIP(buffer);
	m_ForcedHost = StrtoIP(buffer);
	GetPrivateProfileString("Local",  "ForcedPort",		"0",		buffer, 256, ConfigFile);
	m_ForcedPort = atol(buffer);

	GetPrivateProfileString("Local",  "SpeedStatic",	"0",		buffer, 256, ConfigFile);
	m_SpeedStatic = atol(buffer); 
	GetPrivateProfileString("Local",  "SpeedDown",	"0",		buffer, 256, ConfigFile);
	m_SpeedDown = atof(buffer); 
	GetPrivateProfileString("Local",  "SpeedUp",	"0",		buffer, 256, ConfigFile);
	m_SpeedUp = atof(buffer); 

	GetPrivateProfileString("Local",  "UpdateMode",		"1",		buffer, 256, ConfigFile);
	m_Update = atoi(buffer);
	GetPrivateProfileString("Local", "ClientID", EncodeBase16((byte*) &m_ClientID, 16), buffer, 256, ConfigFile);
	DecodeBase16( CString(buffer), 16, (byte*) &m_ClientID);

	
	// Local Network
	GetPrivateProfileString("LocalNetwork", "Lan",				"0",	buffer, 256, ConfigFile);
	m_LanMode = (0 != atoi(buffer));
	GetPrivateProfileString("LocalNetwork",	"LanName",			"",		buffer, 256, ConfigFile);
	m_LanName = buffer;
	GetPrivateProfileString("LocalNetwork",	"NetworkName",			"",		buffer, 256, ConfigFile);
	m_NetworkName = buffer;

	GetPrivateProfileString("LocalNetwork", "SupernodeAble",	"1",	buffer, 256, ConfigFile);
	m_SupernodeAble	 = (0 != atoi(buffer));
	//GetPrivateProfileString("LocalNetwork", "MaxLeaves",	NumtoStr(CalcMaxLeaves()),	buffer, 256, ConfigFile);
	//m_MaxLeaves = atoi(buffer);
	

	// Local Firewall
	GetPrivateProfileString("LocalFirewall", "BehindFirewall", "0", buffer, 256, ConfigFile);
	m_BehindFirewall = (0 != atoi(buffer));

	// Connect
	GetPrivateProfileString("Connect",  "LeafModeConnects",	"1",		buffer, 256, ConfigFile);
	m_LeafModeConnects = atoi(buffer);
	GetPrivateProfileString("Connect",  "MinConnects",	"4",		buffer, 256, ConfigFile);
	m_MinConnects = atoi(buffer);
	GetPrivateProfileString("Connect",  "MaxConnects",	"6",		buffer, 256, ConfigFile);
	m_MaxConnects = atoi(buffer);

	// Connect Servers
	for(int i = 1; ; i++)
	{
		CString KeyName;
		KeyName.Format("Server%ld", i);

		if(GetPrivateProfileString( "Connect Servers", KeyName, "",	buffer, 256, ConfigFile) > 0)
		{
			int     pos;
			CString Host(buffer);

			if( (pos = Host.Find(":", 0)) != -1)
			{
				Node Server;
				Server.Port		  = atoi(Host.Mid(pos + 1, Host.GetLength() - pos));
				Server.Host		  = Host.Mid(0, pos);
				
				m_HostServers.push_back(Server);
			}
		}
		else
			break;
	}

	// Connect Screen
	CString strScreen;
	IPRule NewScreen; 

	for(int i = 1; ; i++)
	{
		CString KeyName;
		KeyName.Format("Host%ld", i);

		if(GetPrivateProfileString( "Connect Screen", KeyName, "",	buffer, 256, ConfigFile) > 0)
		{
			NewScreen = StrtoIPRule(buffer);

			if(NewScreen.mode == ALLOW)
				m_ScreenedNodes.push_back(NewScreen);
			else
				m_ScreenedNodes.insert(m_ScreenedNodes.begin(), NewScreen);
		}
		else
			break;
	}

	
	// Search
	GetPrivateProfileString("Search",	"DownloadPath", m_pCore->m_RunPath + "Downloads",	buffer, 256, ConfigFile);
	m_DownloadPath = buffer;
	GetPrivateProfileString("Search",	"DoubleCheck",	"1",				buffer, 256, ConfigFile);
	m_DoubleCheck = (0 != atoi(buffer));
	GetPrivateProfileString("Search",	"ScreenNodes",	"0",				buffer, 256, ConfigFile);
	m_ScreenNodes = (0 != atoi(buffer));


	// Search Screen
	CString NewWord; 
	
	for(i = 1; ; i++)
	{
		CString KeyName;
		KeyName.Format("Word%ld", i);

		if(GetPrivateProfileString( "Search Screen", KeyName, "",	buffer, 256, ConfigFile) > 0)
		{
			NewWord = buffer;

			m_ScreenedWords.push_back(NewWord);
		}
		else
			break;
	}

	// Share
	CString DirName;

	m_pShare->m_FilesAccess.Lock();

		m_pShare->m_DirIDMap.clear();
		m_pShare->m_SharedDirectories.clear();

		for (i = 0; ; ++i)
		{
			DirName.Format ("Dir%ld", i);
			
			if (GetPrivateProfileString ("Share", DirName, "", buffer, 256, ConfigFile) > 0)
			{
				// Check if recursive
				CString DirName  = buffer;
				CString RealName = DirName;
				DirName.MakeLower();

				bool Recurse = false;
				if(DirName.Find("recursive") != -1)
				{
					DirName  = DirName.Left( DirName.ReverseFind(','));
					RealName = RealName.Left( RealName.ReverseFind(','));
					Recurse = true;
				}

				// Insert directory
				SharedDirectory Directory;
				Directory.Name		= RealName;
				Directory.Recursive = Recurse;
				Directory.Size		= 0;
				Directory.FileCount = 0;

				if(!RealName.IsEmpty())
				{
					Directory.DirID = m_pShare->m_NextDirID++;
					m_pShare->m_DirIDMap[Directory.DirID] = m_pShare->m_SharedDirectories.size();

					m_pShare->m_SharedDirectories.push_back(Directory);
				}
				else
					continue;
			}
			else
				break;
		}

	m_pShare->m_FilesAccess.Unlock();

	GetPrivateProfileString("Share",	"ReplyFilePath",		"0",	buffer, 256, ConfigFile);
	m_ReplyFilePath = (0 != atoi(buffer));
	GetPrivateProfileString("Share",	"MaxReplies",			"64",	buffer, 256, ConfigFile);
	m_MaxReplies = atoi(buffer);
	GetPrivateProfileString("Share",	"SendOnlyAvail",		"1",	buffer, 256, ConfigFile);
	m_SendOnlyAvail = (0 != atoi(buffer));

	// Transfer
	GetPrivateProfileString("Transfer",	"PartialDir",		m_pCore->m_RunPath + "Partials",	buffer, 256, ConfigFile);
	m_PartialDir = buffer;
	GetPrivateProfileString("Transfer",  "MaxDownloads",		"5",	buffer, 256, ConfigFile);
	m_MaxDownloads = atoi(buffer);
	GetPrivateProfileString("Transfer",  "MaxUploads",			"10",	buffer, 256, ConfigFile);
	m_MaxUploads = atoi(buffer);
	GetPrivateProfileString("Transfer",	"Multisource",			"1",	buffer, 256, ConfigFile);
	m_Multisource = (0 != atoi(buffer));

	GetPrivateProfileString("Transfer",	"AntivirusEnabled",		"0",	buffer, 256, ConfigFile);
	m_AntivirusEnabled = (0 != atoi(buffer));
	GetPrivateProfileString("Transfer",	"AntivirusPath",		"",		buffer, 256, ConfigFile);
	m_AntivirusPath = buffer;


	// Bandwidth
	GetPrivateProfileString("Bandwidth",  "BandwidthUp",		"0",	buffer, 256, ConfigFile);
	m_BandwidthUp = atof(buffer);
	GetPrivateProfileString("Bandwidth",  "BandwidthDown",		"0",	buffer, 256, ConfigFile);
	m_BandwidthDown = atof(buffer);
	GetPrivateProfileString("Bandwidth",  "MinDownloadSpeed",	"0",	buffer, 256, ConfigFile);
	m_MinDownSpeed = atof(buffer);
	GetPrivateProfileString("Bandwidth",  "MinUploadSpeed",		"0",	buffer, 256, ConfigFile);
	m_MinUpSpeed = atof(buffer);
	
	if(m_ForcedHost.S_addr)
		m_pCore->m_pNet->m_CurrentIP.S_addr = m_ForcedHost.S_addr;

	if(m_ForcedPort)
		m_pCore->m_pNet->m_CurrentPort = m_ForcedPort;

	// Geo Location
	GetPrivateProfileString("Geo",  "Latitude",		"0",	buffer, 256, ConfigFile);
	m_GeoLatitude = atoi(buffer);
	GetPrivateProfileString("Geo",  "Longitude",	"0",	buffer, 256, ConfigFile);
	m_GeoLongitude = atoi(buffer);

	// Check loaded
	if(m_PartialDir == m_DownloadPath)
		m_PartialDir = m_DownloadPath + "\\Partials";


	m_pCore->m_pTrans->LoadDownloads();


	m_pShare->m_UpdateShared = true;
	m_pShare->m_TriggerThread.SetEvent(); 
}

void CGnuPrefs::SaveConfig(CString ConfigFile)
{
	int slashpos = ConfigFile.ReverseFind('\\');
	if(slashpos != -1)
		CreateDirectory(ConfigFile.Left(slashpos + 1), NULL);

	// Local
	WritePrivateProfileString("Local", "ForcedHost",	  IPtoStr(m_ForcedHost),			ConfigFile);
	WritePrivateProfileString("Local", "ForcedPort",	  NumtoStr(m_ForcedPort),			ConfigFile);

	WritePrivateProfileString("Local", "SpeedStatic",     NumtoStr(m_SpeedStatic),			ConfigFile);
	WritePrivateProfileString("Local", "SpeedDown",		  InsertDecimal( (double) m_SpeedDown),			ConfigFile);
	WritePrivateProfileString("Local", "SpeedUp",		  InsertDecimal( (double) m_SpeedUp),				ConfigFile);

	WritePrivateProfileString("Local", "UpdateMode",	  NumtoStr(m_Update),				ConfigFile);
	WritePrivateProfileString("Local", "ClientID",        EncodeBase16((byte*) &m_ClientID, 16),		ConfigFile);

	// Local Network
	WritePrivateProfileString("LocalNetwork", "Lan",				NumtoStr(m_LanMode),		ConfigFile);
	WritePrivateProfileString("LocalNetwork", "LanName",			m_LanName,					ConfigFile);
	WritePrivateProfileString("LocalNetwork", "NetworkName",		m_NetworkName,					ConfigFile);


	WritePrivateProfileString("LocalNetwork", "SupernodeAble",		NumtoStr(m_SupernodeAble),		ConfigFile);
	//WritePrivateProfileString("LocalNetwork", "MaxLeaves",			NumtoStr(m_MaxLeaves),			ConfigFile);
	

	// Local Firewall
	WritePrivateProfileString("LocalFirewall", "BehindFirewall",     NumtoStr(m_BehindFirewall),	ConfigFile);

	// Connect
	WritePrivateProfileString("Connect", "LeafModeConnects",NumtoStr(m_LeafModeConnects),	ConfigFile);
	WritePrivateProfileString("Connect", "MinConnects",		NumtoStr(m_MinConnects),		ConfigFile);
	WritePrivateProfileString("Connect", "MaxConnects",		NumtoStr(m_MaxConnects),		ConfigFile);
	
	// Connect Servers
	WritePrivateProfileString("Connect Servers", NULL, NULL, ConfigFile); 
	
	CString KeyName;
	std::vector<Node>::iterator itNode;
	
	int i = 1;
	for (i = 1, itNode = m_HostServers.begin(); itNode != m_HostServers.end(); i++, itNode++)
	{
		KeyName.Format("Server%ld", i);
		
		CString Insert = (*itNode).Host;
				Insert += ":" + NumtoStr((*itNode).Port);
		
		WritePrivateProfileString ("Connect Servers", KeyName, Insert, ConfigFile);
	}

	// Connect Screen
	WritePrivateProfileString("Connect Screen", NULL, NULL, ConfigFile); // First clear out the IPFilters ini file section
	
	std::vector<IPRule>::iterator itIP;
	i = 1;

	for (itIP = m_ScreenedNodes.begin(); itIP != m_ScreenedNodes.end(); itIP++)
	{
		KeyName.Format("Host%ld", i);
		
		CString Insert = IPRuletoStr( (*itIP) );
		
		WritePrivateProfileString ("Connect Screen", KeyName, Insert, ConfigFile);

		i++;
	}

	// Search
	WritePrivateProfileString("Search", "DownloadPath",		m_DownloadPath,						ConfigFile);
	WritePrivateProfileString("Search", "DoubleCheck",		NumtoStr(m_DoubleCheck),			ConfigFile);
	WritePrivateProfileString("Search", "ScreenNodes",		NumtoStr(m_ScreenNodes),			ConfigFile);
	

	// Search Screen
	WritePrivateProfileString("Search Screen", NULL, NULL, ConfigFile); // First clear out the IPFilters ini file section
	
	std::vector<CString>::iterator itWord;
	
	for (i = 1, itWord = m_ScreenedWords.begin(); itWord != m_ScreenedWords.end(); i++, itWord++)
	{
		KeyName.Format("Word%ld", i);
		
		CString Insert = *itWord;
		
		WritePrivateProfileString ("Search Screen", KeyName, Insert, ConfigFile);
	}

	// Share
	CString DirName;
	WritePrivateProfileString("Share", NULL, NULL, ConfigFile); 
	
	for (i = 0; i < m_pShare->m_SharedDirectories.size(); i++)
	{
		DirName.Format ("Dir%ld", i);
		
		CString Directory = m_pShare->m_SharedDirectories[i].Name;
		
		if(m_pShare->m_SharedDirectories[i].Recursive)
			Directory += ", Recursive";

		WritePrivateProfileString ("Share", DirName, Directory, ConfigFile);
	}

	WritePrivateProfileString("Share",		"ReplyFilePath",		NumtoStr(m_ReplyFilePath),	ConfigFile);
	WritePrivateProfileString("Share",		"MaxReplies",			NumtoStr(m_MaxReplies),	ConfigFile);
	WritePrivateProfileString("Share",		"SendOnlyAvail",		NumtoStr(m_SendOnlyAvail),	ConfigFile);
	
	// Transfer
	WritePrivateProfileString("Transfer",		"PartialDir",			m_PartialDir,					ConfigFile);
	WritePrivateProfileString("Transfer",		"MaxDownloads",			NumtoStr(m_MaxDownloads),		ConfigFile);
	WritePrivateProfileString("Transfer",		"MaxUploads",			NumtoStr(m_MaxUploads),			ConfigFile);
	WritePrivateProfileString("Transfer",		"Multisource",			NumtoStr(m_Multisource),		ConfigFile);
	
	WritePrivateProfileString("Transfer",		"AntivirusEnabled",		NumtoStr(m_AntivirusEnabled),	ConfigFile);
	WritePrivateProfileString("Transfer",		"AntivirusPath",		m_AntivirusPath,				ConfigFile);
	
	// Bandwidth
	if(m_BandwidthUp)
		WritePrivateProfileString("Bandwidth",	"BandwidthUp",			InsertDecimal( (double) m_BandwidthUp),			ConfigFile);
	else
		WritePrivateProfileString("Bandwidth",	"BandwidthUp",			"0",													ConfigFile);

	if(m_BandwidthDown)
		WritePrivateProfileString("Bandwidth",	"BandwidthDown",		InsertDecimal( (double) m_BandwidthDown),		ConfigFile);
	else
		WritePrivateProfileString("Bandwidth",	"BandwidthDown",		"0",													ConfigFile);
	
	if(m_MinDownSpeed)
		WritePrivateProfileString("Bandwidth",	"MinDownloadSpeed",		InsertDecimal( (double) m_MinDownSpeed),		ConfigFile);
	else
		WritePrivateProfileString("Bandwidth",	"MinDownloadSpeed",		"0",													ConfigFile);

	if(m_MinUpSpeed)
		WritePrivateProfileString("Bandwidth",	"MinUploadSpeed",		InsertDecimal( (double) m_MinUpSpeed),			ConfigFile);
	else
		WritePrivateProfileString("Bandwidth",	"MinUploadSpeed",		"0",	ConfigFile);

	// Geo location
	WritePrivateProfileString("Geo", "Latitude",	NumtoStr( m_GeoLatitude ),	ConfigFile);
	WritePrivateProfileString("Geo", "Longitude",	NumtoStr( m_GeoLongitude ),	ConfigFile);

}

void CGnuPrefs::LoadBlocked(CString LoadPath)
{
	CStdioFile infile;
	
	if(infile.Open(LoadPath, CFile::modeCreate | CFile::modeNoTruncate	| CFile::modeRead))
	{
		BlockedHost BadHost;

		CString strLine;
		CString HostRange;

		int BreakPos;
		int HyphenPos;
		
		CString NextLine;

		while(infile.ReadString(NextLine))
		{
			strLine = NextLine;

			BreakPos = strLine.Find(':');

			if(BreakPos != -1)
			{
				 HostRange = strLine.Left(BreakPos);

				 HyphenPos = HostRange.Find('-');

				 if(HyphenPos != -1)
				 {
					 BadHost.StartIP = StrtoIP( HostRange.Left(HyphenPos) );
					 BadHost.EndIP   = StrtoIP( HostRange.Mid(HyphenPos + 1) );
				 }
				 else
				 {
					BadHost.StartIP = StrtoIP(HostRange);
					BadHost.EndIP   = StrtoIP(HostRange);
				}

				 strLine = strLine.Mid(BreakPos + 1);
				 
				 BreakPos = strLine.Find(':');

				if(BreakPos != -1)
				{
					BadHost.Reason = strLine.Left(BreakPos);

					m_BlockList.push_back(BadHost);
				}
			}
		}

		infile.Abort();
	}
}

void CGnuPrefs::SaveBlocked(CString SavePath)
{
	CStdioFile outfile;

	if( outfile.Open(SavePath, CFile::modeCreate | CFile::modeWrite) )
	{
		for(int i = 0; i < m_BlockList.size(); i++)
		{
			outfile.WriteString( IPtoStr(m_BlockList[i].StartIP));

			if(m_BlockList[i].StartIP.S_addr != m_BlockList[i].EndIP.S_addr)
				outfile.WriteString( "-" + IPtoStr(m_BlockList[i].EndIP));

			outfile.WriteString(":" + m_BlockList[i].Reason + ":\n");
		}

		outfile.Abort();
	}
}

bool CGnuPrefs::AllowedIP(IP ipTest)
{
	// Default is allow
	// NOTE: Denied IPs are always at the front of the list, allowed are at the end

	bool allowIP = true;

	// Check with screened nodes
	std::vector<IPRule>::iterator itIP;
	for (itIP = m_ScreenedNodes.begin(); itIP != m_ScreenedNodes.end(); itIP++)
	{
		if( (*itIP).mode == DENY && MatchIP(ipTest, (*itIP)) )
			allowIP = false;

		if( (*itIP).mode == ALLOW && MatchIP(ipTest, (*itIP)) )
			allowIP = true;
	}

	return allowIP;
}

bool CGnuPrefs::BlockedIP(IP TestIP)
{
	for(int i = 0; i < m_BlockList.size(); i++)
		if(ntohl(TestIP.S_addr) >= ntohl(m_BlockList[i].StartIP.S_addr) && ntohl(TestIP.S_addr) <= ntohl(m_BlockList[i].EndIP.S_addr))
			return true;

	return false;
}

bool CGnuPrefs::MatchIP(IP ipTest, IPRule &ipCompare)
{
	// -1 is a wildcard

	if(ipTest.a == ipCompare.a || ipCompare.a == -1)
		if(ipTest.b == ipCompare.b || ipCompare.b == -1)
			if(ipTest.c == ipCompare.c || ipCompare.c == -1)
				if(ipTest.d == ipCompare.d || ipCompare.d == -1)
					return true;

	return false;
}

int CGnuPrefs::CalcMaxLeaves()
{
	int maxLeaves = 10;	

	if(m_pCore->m_SysSpeed > 500 && m_pCore->m_SysMemory > 100)
		maxLeaves = 100;

	if(m_pCore->m_SysSpeed > 1000 && m_pCore->m_SysMemory > 200)
		maxLeaves = 200;

	if(m_pCore->m_SysSpeed > 1500 && m_pCore->m_SysMemory > 300)
		maxLeaves = 300;

	if(m_pCore->m_SysSpeed > 2000 && m_pCore->m_SysMemory > 400)
		maxLeaves = 400;

	return maxLeaves;
}

void CGnuPrefs::LoadProxies(CString FilePath)
{
	CStdioFile infile;

	m_ProxyList.clear();
	
	if(infile.Open(FilePath, CFile::modeCreate | CFile::modeNoTruncate	| CFile::modeRead))
	{
		CString NextLine;
		while(infile.ReadString(NextLine))
		{
			ProxyHost proxy = StrtoProxy(NextLine);
			if (proxy.host != "" && proxy.port != 0) m_ProxyList.push_back(proxy);
		}

		infile.Abort();
	}
}

void CGnuPrefs::SaveProxies(CString FilePath)
{
	CStdioFile outfile;

	CString temp;

	if (outfile.Open(FilePath, CFile::modeCreate | CFile::modeWrite))
	{
		for (int i = 0; i < m_ProxyList.size(); i++)
		{
			temp.Format("%s:%i", (m_ProxyList[i]).host, (m_ProxyList[i]).port);
			outfile.WriteString(temp+"\n");
		}
		outfile.Abort();
	}
}

ProxyHost CGnuPrefs::GetRandProxy()
{
	int index = rand() % m_ProxyList.size();

	return m_ProxyList[index];
}

CString IPRuletoStr(IPRule in)
{
	char buffer[20];
	::sprintf(buffer, "%d.%d.%d.%d:", in.a, in.b, in.c, in.d);

	CString out = buffer;

	out.Replace("-1", "*");
	if(in.mode == DENY)
		out.Replace(":", ":Deny");
	else
		out.Replace(":", ":Allow");

	return out;
}

IPRule StrtoIPRule(CString in)
{
	char rawMode[8] = "";

	in.Replace("*", "-1");

	int  a = 0, b = 0, c = 0, d = 0;

	::sscanf ((LPCTSTR) in, "%d.%d.%d.%d:%s", &a, &b, &c, &d, rawMode);

	IPRule out;
	out.a = a;	out.b = b;	out.c = c;	out.d = d;
	
	CString mode(rawMode);

	mode.MakeUpper();
	if(mode == "ALLOW")
		out.mode = ALLOW;
	else
		out.mode = DENY;

	return out;
}

BlockedHost StrtoBlocked(CString strBlocked)
{
	BlockedHost BadHost;
	BadHost.StartIP.S_addr = 0;
	BadHost.EndIP.S_addr	= 0;

	int BreakPos = strBlocked.Find(':');

	if(BreakPos != -1)
	{
		CString HostRange = strBlocked.Left(BreakPos);

		int HyphenPos = HostRange.Find('-');

		if(HyphenPos != -1)
		{
			BadHost.StartIP = StrtoIP( HostRange.Left(HyphenPos) );
			BadHost.EndIP   = StrtoIP( HostRange.Mid(HyphenPos + 1) );
		}
		else
		{
			BadHost.StartIP = StrtoIP(HostRange);
			BadHost.EndIP   = StrtoIP(HostRange);
		}

		CString strLine = strBlocked.Mid(BreakPos + 1);
		
		BreakPos = strLine.Find(':');

		if(BreakPos != -1)
			BadHost.Reason = strLine.Left(BreakPos);
	}

	return BadHost;
}

CString	BlockedtoStr(BlockedHost badHost)
{
	CString strBlocked;

	strBlocked = IPtoStr(badHost.StartIP);

	if(badHost.StartIP.S_addr != badHost.EndIP.S_addr)
		strBlocked += "-" + IPtoStr(badHost.EndIP);

	strBlocked += ":" + badHost.Reason + ":\n";

	return strBlocked;
}

ProxyHost StrtoProxy(CString strProxy)
{
	ProxyHost Proxy;

	strProxy.Remove(' ');
	int BreakPos = strProxy.Find(':');

	if(BreakPos != -1)
	{
		Proxy.host = strProxy.Left(BreakPos);
		Proxy.port = atoi(strProxy.Mid(BreakPos + 1));
	}
	else
		Proxy.host = strProxy;

	return Proxy;
}