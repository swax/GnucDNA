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
#include "DnaCore.h"

#include "GnuCore.h"
#include "GnuPrefs.h"
#include "GnuNetworks.h"
#include "GnuControl.h"

#include "DnaPrefs.h"


CDnaPrefs::CDnaPrefs()
{
	m_dnaCore  = NULL;
	m_gnuPrefs = NULL;

	
}

void CDnaPrefs::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;

	m_gnuPrefs = dnaCore->m_gnuCore->m_pPrefs;
}

CDnaPrefs::~CDnaPrefs()
{
	
}

void CDnaPrefs::LoadConfig(LPCTSTR FilePath)
{
	 
	m_gnuPrefs->LoadConfig(FilePath);
}

void CDnaPrefs::SaveConfig(LPCTSTR FilePath)
{
	 

	m_gnuPrefs->SaveConfig(FilePath);
}

void CDnaPrefs::LoadBlocked(LPCTSTR FilePath)
{
	 
	m_gnuPrefs->LoadBlocked(FilePath);
}

void CDnaPrefs::SaveBlocked(LPCTSTR FilePath)
{
	 

	m_gnuPrefs->SaveBlocked(FilePath);
}

ULONG CDnaPrefs::GetForcedHost(void)
{
	 
	return m_gnuPrefs->m_ForcedHost.S_addr;
}

void CDnaPrefs::SetForcedHost(ULONG newVal)
{
	 

	m_gnuPrefs->m_ForcedHost.S_addr = newVal;
}

LONG CDnaPrefs::GetForcedPort(void)
{
	 

	return m_gnuPrefs->m_ForcedPort;
}

void CDnaPrefs::SetForcedPort(LONG newVal)
{
 

	if(m_dnaCore->m_gnuCore->m_pNet)
		m_dnaCore->m_gnuCore->m_pNet->StartListening();

	m_gnuPrefs->m_ForcedPort = newVal;
}

LONG CDnaPrefs::GetSpeedStat(void)
{
	 

	return m_gnuPrefs->m_SpeedStatic;
}

void CDnaPrefs::SetSpeedStat(LONG newVal)
{
 

	m_gnuPrefs->m_SpeedStatic = newVal;
}

LONG CDnaPrefs::GetUpdate(void)
{
	  

	return m_gnuPrefs->m_Update;
}

void CDnaPrefs::SetUpdate(LONG newVal)
{
	  

	m_gnuPrefs->m_Update = newVal;
}

void CDnaPrefs::GetClientID(byte ClientID[16])
{
	memcpy(ClientID, &m_gnuPrefs->m_ClientID, 16);
}

void CDnaPrefs::SetClientID(byte ClientID[16])
{
	memcpy(&m_gnuPrefs->m_ClientID, ClientID, 16);
}

BOOL CDnaPrefs::GetSuperNodeAble(void)
{
	  

	if(m_gnuPrefs->m_SupernodeAble)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetSuperNodeAble(BOOL newVal)
{
	  

	// If downgrading
	if(newVal == false)
		if(m_dnaCore->m_gnuCore->m_pNet->m_pGnu)
			m_dnaCore->m_gnuCore->m_pNet->m_pGnu->DowngradeClient();

	m_gnuPrefs->m_SupernodeAble = newVal;
}

LONG CDnaPrefs::GetMaxLeaves(void)
{
	  

	return m_gnuPrefs->m_MaxLeaves;
}

void CDnaPrefs::SetMaxLeaves(LONG newVal)
{
	  

	if(newVal > 1500)
		newVal = 1500;

	m_gnuPrefs->m_MaxLeaves = newVal;
}

BOOL CDnaPrefs::GetLanMode(void)
{
	  

	if(m_gnuPrefs->m_LanMode)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetLanMode(BOOL newVal)
{
	  

	m_gnuPrefs->m_LanMode = newVal;
}

CString CDnaPrefs::GetLanName(void)
{

	CString strResult = m_gnuPrefs->m_LanName;

	return strResult;
}

void CDnaPrefs::SetLanName(LPCTSTR newVal)
{
	  

	m_gnuPrefs->m_LanName = newVal;
}

BOOL CDnaPrefs::GetBehindFirewall(void)
{
	  

	if(m_gnuPrefs->m_BehindFirewall)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetBehindFirewall(BOOL newVal)
{
	  

	m_gnuPrefs->m_BehindFirewall = newVal;
}

LONG CDnaPrefs::GetLeafModeConnects(void)
{
	  

	return m_gnuPrefs->m_LeafModeConnects;
}

void CDnaPrefs::SetLeafModeConnects(LONG newVal)
{
	  

	m_gnuPrefs->m_LeafModeConnects = newVal;
}

LONG CDnaPrefs::GetMinConnects(void)
{
	  

	return m_gnuPrefs->m_MinConnects;
}

void CDnaPrefs::SetMinConnects(LONG newVal)
{
	  

	m_gnuPrefs->m_MinConnects = newVal;
	
	if(m_gnuPrefs->m_MinConnects != -1 && m_gnuPrefs->m_MaxConnects != -1)
		if(m_gnuPrefs->m_MinConnects > m_gnuPrefs->m_MaxConnects)
			m_gnuPrefs->m_MinConnects = m_gnuPrefs->m_MaxConnects;

}

LONG CDnaPrefs::GetMaxConnects(void)
{
	  

	return m_gnuPrefs->m_MaxConnects;
}

void CDnaPrefs::SetMaxConnects(LONG newVal)
{
	  

	m_gnuPrefs->m_MaxConnects = newVal;
	
	if(m_gnuPrefs->m_MinConnects != -1 && m_gnuPrefs->m_MaxConnects != -1)
		if(m_gnuPrefs->m_MinConnects > m_gnuPrefs->m_MaxConnects)
		{
			m_gnuPrefs->m_MaxConnects = m_gnuPrefs->m_MinConnects;
			return;
		}

	
}

std::vector<CString> CDnaPrefs::GetHostServers(void)
{
	std::vector<CString> HostServers;

	for(int i = 0; i < m_gnuPrefs->m_HostServers.size(); i++)
		HostServers.push_back( m_gnuPrefs->m_HostServers[i].GetString() );

	return HostServers;
}

void CDnaPrefs::SetHostServers(std::vector<CString> &HostServers)
{
	m_gnuPrefs->m_HostServers.clear();

	for(int i = 0; i < HostServers.size(); i++)
		 m_gnuPrefs->m_HostServers.push_back( Node(HostServers[i]) );
}

std::vector<CString> CDnaPrefs::GetScreenedNodes(void)
{
	std::vector<CString> ScreenedNodes;

	for(int i = 0; i < m_gnuPrefs->m_ScreenedNodes.size(); i++)
		ScreenedNodes.push_back( IPRuletoStr(m_gnuPrefs->m_ScreenedNodes[i]) );

	return ScreenedNodes;
}

void CDnaPrefs::SetScreenedNodes(std::vector<CString> &ScreenedNodes)
{
	m_gnuPrefs->m_ScreenedNodes.clear();

	for(int i = 0; i < ScreenedNodes.size(); i++)
		 m_gnuPrefs->m_ScreenedNodes.push_back( StrtoIPRule( ScreenedNodes[i]) );
}

CString CDnaPrefs::GetDownloadPath(void)
{
	  

	CString strResult = m_gnuPrefs->m_DownloadPath;

	return strResult;
}

void CDnaPrefs::SetDownloadPath(LPCTSTR newVal)
{
	  

	MoveFile(m_gnuPrefs->m_DownloadPath, newVal);
	MoveFile(m_gnuPrefs->m_DownloadPath + "\\Partials", CString(newVal) + "\\Partials");

	m_gnuPrefs->m_DownloadPath = newVal;
}

BOOL CDnaPrefs::GetDoubleCheck(void)
{
	  

	if(m_gnuPrefs->m_DoubleCheck)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetDoubleCheck(BOOL newVal)
{
	  

	m_gnuPrefs->m_DoubleCheck = newVal;
}

BOOL CDnaPrefs::GetScreenNodes(void)
{
	  

	if(m_gnuPrefs->m_ScreenNodes)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetScreenNodes(BOOL newVal)
{
	  

	m_gnuPrefs->m_ScreenNodes = newVal;
}

std::vector<CString> CDnaPrefs::GetScreenedWords(void)
{
	return m_gnuPrefs->m_ScreenedWords;
}

void CDnaPrefs::SetScreenedWords(std::vector<CString> &ScreenedWords)
{
	m_gnuPrefs->m_ScreenedWords.clear();

	for(int i = 0; i < ScreenedWords.size(); i++)
		 m_gnuPrefs->m_ScreenedWords.push_back( ScreenedWords[i] );
}

std::vector<CString> CDnaPrefs::GetBlockList(void)
{
	std::vector<CString> BlockList;

	for(int i = 0; i < m_gnuPrefs->m_BlockList.size(); i++)
		BlockList.push_back( BlockedtoStr(m_gnuPrefs->m_BlockList[i]) );

	return BlockList;
}

void CDnaPrefs::SetBlockList(std::vector<CString> &BlockList)
{
	m_gnuPrefs->m_BlockList.clear();

	for(int i = 0; i < BlockList.size(); i++)
		 m_gnuPrefs->m_BlockList.push_back( StrtoBlocked(BlockList[i]) );
}

BOOL CDnaPrefs::GetReplyFilePath(void)
{
	  

	if(m_gnuPrefs->m_ReplyFilePath)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetReplyFilePath(BOOL newVal)
{
	  

	m_gnuPrefs->m_ReplyFilePath = newVal;
}

LONG CDnaPrefs::GetMaxReplies(void)
{
	  

	return m_gnuPrefs->m_MaxReplies;
}

void CDnaPrefs::SetMaxReplies(LONG newVal)
{
	  

	m_gnuPrefs->m_MaxReplies = newVal;
}

BOOL CDnaPrefs::GetSendOnlyAvail(void)
{
	  

	if(m_gnuPrefs->m_SendOnlyAvail)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetSendOnlyAvail(BOOL newVal)
{
	  

	m_gnuPrefs->m_SendOnlyAvail = newVal;
}

LONG CDnaPrefs::GetMaxDownloads(void)
{
	  

	return m_gnuPrefs->m_MaxDownloads;
}

void CDnaPrefs::SetMaxDownloads(LONG newVal)
{
	  

	m_gnuPrefs->m_MaxDownloads = newVal;
}

LONG CDnaPrefs::GetMaxUploads(void)
{
	  

	return m_gnuPrefs->m_MaxUploads;
}

void CDnaPrefs::SetMaxUploads(LONG newVal)
{
	  

	m_gnuPrefs->m_MaxUploads = newVal;
}

BOOL CDnaPrefs::GetMultisource(void)
{
	  

	if(m_gnuPrefs->m_Multisource)
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetMultisource(BOOL newVal)
{
	  

	m_gnuPrefs->m_Multisource = newVal;
}

FLOAT CDnaPrefs::GetBandwidthUp(void)
{
	  

	return m_gnuPrefs->m_BandwidthUp;
}

void CDnaPrefs::SetBandwidthUp(FLOAT newVal)
{
	  

	m_gnuPrefs->m_BandwidthUp = newVal;
}

FLOAT CDnaPrefs::GetBandwidthDown(void)
{
	  

	return m_gnuPrefs->m_BandwidthDown;
}

void CDnaPrefs::SetBandwidthDown(FLOAT newVal)
{
	  

	m_gnuPrefs->m_BandwidthDown = newVal;
}

FLOAT CDnaPrefs::GetMinDownSpeed(void)
{
	  

	return m_gnuPrefs->m_MinDownSpeed;
}

void CDnaPrefs::SetMinDownSpeed(FLOAT newVal)
{
	  

	m_gnuPrefs->m_MinDownSpeed = newVal;
}

FLOAT CDnaPrefs::GetMinUpSpeed(void)
{
	  

	return m_gnuPrefs->m_MinUpSpeed;
}

void CDnaPrefs::SetMinUpSpeed(FLOAT newVal)
{
	  

	m_gnuPrefs->m_MinUpSpeed = newVal;
}

CString CDnaPrefs::GetPartialsDir(void)
{
	  

	CString strResult;

	strResult = m_gnuPrefs->m_PartialDir;

	return strResult;
}

void CDnaPrefs::SetPartialsDir(LPCTSTR newVal)
{
	  

	m_gnuPrefs->m_PartialDir = newVal;

	if(m_gnuPrefs->m_PartialDir.Right(1) == "\\")
	{
		int newSize = m_gnuPrefs->m_PartialDir.GetLength() - 1;
		m_gnuPrefs->m_PartialDir = m_gnuPrefs->m_PartialDir.Left(newSize);

		if(m_gnuPrefs->m_PartialDir == m_gnuPrefs->m_DownloadPath)
			m_gnuPrefs->m_PartialDir = m_gnuPrefs->m_DownloadPath + "\\Partials";
	}
}

void CDnaPrefs::LoadProxies(LPCTSTR FilePath)
{
	  

	m_gnuPrefs->LoadProxies(FilePath);
}

void CDnaPrefs::SaveProxies(LPCTSTR FilePath)
{
	  

	m_gnuPrefs->SaveProxies(FilePath);
}

std::vector<CString> CDnaPrefs::GetProxyList(void)
{
	std::vector<CString> ProxyList;

	CString temp;
	for(int i = 0; i < m_gnuPrefs->m_ProxyList.size(); i++)
	{
		temp.Format("%s:%i", (m_gnuPrefs->m_ProxyList[i]).host, (m_gnuPrefs->m_ProxyList[i]).port);
		ProxyList.push_back( temp );
	}

	return ProxyList;
}

void CDnaPrefs::SetProxyList(std::vector<CString> &ProxyList)
{
	m_gnuPrefs->m_ProxyList.clear();

	for(int i = 0; i < ProxyList.size(); i++)
	{
		ProxyAddr proxy = StrtoProxy(ProxyList[i]);
		m_gnuPrefs->m_ProxyList.push_back(proxy);
	}

}

BOOL CDnaPrefs::GetAntivirusEnabled(void)
{
	  

	if( m_gnuPrefs->m_AntivirusEnabled )
		return TRUE;

	return FALSE;
}

void CDnaPrefs::SetAntivirusEnabled(BOOL newVal)
{
	  

	if( newVal == TRUE )
		m_gnuPrefs->m_AntivirusEnabled = true;
	else
		m_gnuPrefs->m_AntivirusEnabled = false;
}

CString CDnaPrefs::GetAntivirusPath(void)
{
	  

	CString strResult;

	strResult = m_gnuPrefs->m_AntivirusPath;

	return strResult;
}

void CDnaPrefs::SetAntivirusPath(LPCTSTR newVal)
{
	  

	m_gnuPrefs->m_AntivirusPath = newVal;
}

DOUBLE CDnaPrefs::GetLatitude(void)
{
	  

	return WordtoGeo(m_gnuPrefs->m_GeoLatitude, true);
}

void CDnaPrefs::SetLatitude(DOUBLE newVal)
{
	  

	m_gnuPrefs->m_GeoLatitude = GeotoWord(newVal, true);
}

DOUBLE CDnaPrefs::GetLongitude(void)
{
	  

	return WordtoGeo(m_gnuPrefs->m_GeoLongitude, false);
}

void CDnaPrefs::SetLongitude(DOUBLE newVal)
{
	m_gnuPrefs->m_GeoLongitude = GeotoWord(newVal, false);
}

CString CDnaPrefs::GetNetworkName(void)
{
	return m_gnuPrefs->m_NetworkName;
}

void CDnaPrefs::SetNetworkName(LPCTSTR newVal)
{
	m_gnuPrefs->m_NetworkName = newVal;
}

