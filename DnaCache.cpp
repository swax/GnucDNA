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
	license to your contribution.

	For support, questions, commercial use, etc...
	E-Mail: swabby@c0re.net

********************************************************************************/


#include "stdafx.h"
  
#include "DnaCore.h"

#include "GnuNetworks.h"
#include "GnuCore.h"
#include "GnuCache.h"
#include "GnuControl.h"

#include "DnaCache.h"


CDnaCache::CDnaCache()
{
	m_dnaCore  = NULL;
	m_gnuCache = NULL;

	
}

void CDnaCache::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;
	m_gnuCache = dnaCore->m_gnuCore->m_pNet->m_pCache;
}

CDnaCache::~CDnaCache()
{
	
}

// CDnaCache message handlers

void CDnaCache::LoadCache(LPCTSTR FilePath)
{
	

	m_gnuCache->LoadCache(FilePath);
}

void CDnaCache::LoadUltraCache(LPCTSTR FilePath)
{
	

	//m_gnuCache->LoadUltraNodes(FilePath);
}

void CDnaCache::LoadWebCache(LPCTSTR FilePath)
{
	m_gnuCache->LoadWebCaches(FilePath);
}

void CDnaCache::AddWebCache(LPCTSTR WebAddress)
{
	m_gnuCache->WebCacheAddCache(WebAddress);
}

void CDnaCache::SaveCache(LPCTSTR FilePath)
{
	

	m_gnuCache->SaveCache(FilePath);
}

void CDnaCache::SaveUltraCache(LPCTSTR FilePath)
{
	

	//m_gnuCache->SaveUltraNodes(FilePath);
}

void CDnaCache::SaveWebCache(LPCTSTR FilePath)
{
	

	m_gnuCache->SaveWebCaches(FilePath);
}



LONG CDnaCache::GetNodeCacheSize(void)
{
	

	return m_gnuCache->m_GnuPerm.size() + m_gnuCache->m_GnuReal.size() + 
		   m_gnuCache->m_G2Perm.size() + m_gnuCache->m_G2Real.size();
}

LONG CDnaCache::GetNodeCacheMaxSize(void)
{
	

	return m_gnuCache->m_MaxCacheSize * 4;
}

LONG CDnaCache::GetUltraNodeCacheSize(void)
{
	

	return 0; //m_gnuCache->m_UltraNodes.size();
}

LONG CDnaCache::GetUltraNodeCacheMaxSize(void)
{
	

	return m_gnuCache->m_MaxCacheSize;
}

LONG CDnaCache::GetWebCacheSize(void)
{
	

	return m_gnuCache->m_AltWebCaches.size();
}

LONG CDnaCache::GetWebCacheMaxSize(void)
{
	

	return m_gnuCache->m_MaxWebCacheSize;
}


void CDnaCache::AddNode(LPCTSTR HostPort, BOOL SuperNode)
{
	

	Node addNode = HostPort;

	m_gnuCache->AddKnown( addNode );
}

void CDnaCache::RemoveIP(LPCTSTR Host, int Port)
{
	m_gnuCache->RemoveIP( Host, Port );
}

void CDnaCache::SeedWebCache(LPCTSTR WebAddress)
{
	m_gnuCache->WebCacheSeedCache(WebAddress);
}

void CDnaCache::TryWebCache(LPCTSTR Network)
{
	m_gnuCache->WebCacheGetRequest(Network);
}