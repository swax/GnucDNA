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
#include "GnuNetworks.h"
#include "GnuControl.h"
#include "G2Control.h"
#include "GnuSearch.h"

#include "DnaSearch.h"


CDnaSearch::CDnaSearch()
{
	m_dnaCore    = NULL;
	m_gnuNetwork = NULL;

	
}

CDnaSearch::~CDnaSearch()
{
	
}

void CDnaSearch::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore    = dnaCore;
	m_gnuNetwork = dnaCore->m_gnuCore->m_pNet;
}


// CDnaSearch message handlers

LONG CDnaSearch::StartSearch(LPCTSTR Query)
{
	  

	CGnuSearch* pSearch = new CGnuSearch(m_gnuNetwork);
	m_gnuNetwork->m_SearchList.push_back(pSearch);

	pSearch->SendQuery(Query);

	return pSearch->m_SearchID;
}

LONG CDnaSearch::StartMetaSearch(LPCTSTR Query, LONG MetaID, std::vector<CString> &AttributeList)
{
	CGnuSearch* pSearch = new CGnuSearch(m_gnuNetwork);
	m_gnuNetwork->m_SearchList.push_back(pSearch);

	pSearch->SendMetaQuery(Query, MetaID, AttributeList);

	return pSearch->m_SearchID;
}

LONG CDnaSearch::StartHashSearch(LPCTSTR Query, LONG HashID, LPCTSTR Hash)
{
	  

	if(HashID < 0 || HashID >= HASH_TYPES)
		return 0;

	CGnuSearch* pSearch = new CGnuSearch(m_gnuNetwork);
	m_gnuNetwork->m_SearchList.push_back(pSearch);

	pSearch->SendHashQuery(Query, HashID, Hash);

	return pSearch->m_SearchID;
}

LONG CDnaSearch::SendBrowseRequest(LPCTSTR Host, LONG Port)
{
	  


	CGnuSearch* pSearch = new CGnuSearch(m_gnuNetwork);
	m_gnuNetwork->m_SearchList.push_back(pSearch);

	pSearch->SendBrowseRequest(Host, Port);

	return pSearch->m_SearchID;
}

void CDnaSearch::EndSearch(LONG SearchID)
{
	  

	m_gnuNetwork->EndSearch(SearchID);
}

void CDnaSearch::RefineSearch(LONG SearchID, LPCTSTR RefinedQuery)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		pSearch->RefinedQuery = RefinedQuery;
		pSearch->Rebuild();
	}
}

void CDnaSearch::SetFiltering(LONG SearchID, BOOL Enabled)
{
	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		if( !Enabled )
			pSearch->m_FilteringActive = false;
		else
			pSearch->m_FilteringActive = true;

		pSearch->Rebuild();
	}
}

void CDnaSearch::FilterSize(LONG SearchID, LONG Mode, LONG Value)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;
			
		pSearch->m_SizeFilterMode   = Mode;
		pSearch->m_SizeFilterValue  = Value;

		pSearch->Rebuild();
	}
}

void CDnaSearch::FilterSpeed(LONG SearchID, LONG Mode, LONG Value)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;
		
		pSearch->m_SpeedFilterMode  = Mode;
		pSearch->m_SpeedFilterValue = Value;

		pSearch->Rebuild();
	}	
}

void CDnaSearch::PauseSearch(LONG SearchID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		pSearch->m_SearchPaused = true;
	}
}

LONG CDnaSearch::CountGoodResults(LONG SearchID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		return pSearch->m_CurrentList.size();
	}

	return 0;
}

LONG CDnaSearch::CountTotalResults(LONG SearchID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		return pSearch->m_WholeList.size();
	}

	return 0;
}



CString CDnaSearch::GetResultName(LONG SearchID, LONG ResultID)
{
	  

	CString strResult;

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
			strResult = itResult->second->Name;

		return strResult;
	}

	return strResult;
}

LONG CDnaSearch::GetResultSize(LONG SearchID, LONG ResultID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
			return itResult->second->Size;
	}

	return 0;
}

LONG CDnaSearch::GetResultSpeed(LONG SearchID, LONG ResultID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;
		
		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
			return itResult->second->AvgSpeed / 8;
	}

	return 0;
}

LONG CDnaSearch::GetResultHostCount(LONG SearchID, LONG ResultID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;
		
		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
			return itResult->second->ResultList.size();
	}

	return 0;
}

CString CDnaSearch::GetResultHash(LONG SearchID, LONG ResultID, LONG HashID)
{
	  

	CString strResult;

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
			if(HashID == HASH_SHA1)
				strResult = itResult->second->Sha1Hash;

		return strResult;
	}

	return strResult;
}



std::vector<int> CDnaSearch::GetResultIDs(LONG SearchID)
{
	std::vector<int> ResultIDs;

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);
	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		int j = 0;
		std::list<ResultGroup>::iterator itGroup;
		for(itGroup = pSearch->m_GroupList.begin(); itGroup != pSearch->m_GroupList.end(); itGroup++)
			ResultIDs.push_back( (*itGroup).ResultID );
	}

	return ResultIDs;
}

LONG CDnaSearch::DownloadResult(LONG SearchID, LONG ResultID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		return pSearch->Download(ResultID);
	}

	return 0;
}

std::vector<int> CDnaSearch::GetHostIDs(LONG SearchID, LONG ResultID)
{
	std::vector<int> HostIDs;

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);
	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);
		if(itResult != pSearch->m_ResultMap.end())
		{
			for(int j = 0; j < itResult->second->ResultList.size(); j++)
				HostIDs.push_back( itResult->second->ResultList[j].SourceID );
		}
	}

	return HostIDs;
}

ULONG CDnaSearch::GetHostIP(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].Address.Host.S_addr;
		}
	}

	return 0;
}

LONG CDnaSearch::GetHostPort(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].Address.Port;
		}
	}

	return 0;
}

LONG CDnaSearch::GetHostSpeed(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].Speed / 8;
		}
	}

	return 0;
}

LONG CDnaSearch::GetHostDistance(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;
		
		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].Distance;
		}
	}

	return 0;
}

BOOL CDnaSearch::GetHostFirewall(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].Firewall;
		}
	}

	return FALSE;
}

BOOL CDnaSearch::GetHostStable(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].Stable;
		}
	}

	return FALSE;
}

BOOL CDnaSearch::GetHostBusy(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].Busy;
		}
	}

	return FALSE;
}

CString CDnaSearch::GetHostVendor(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	CString strResult;

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				strResult = itResult->second->ResultList[itHost->second].Vendor;
		}
	}

	return strResult;
}

std::vector<CString> CDnaSearch::GetHostExtended(LONG SearchID, LONG ResultID, LONG HostID)
{
	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;
		
		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);
		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].GnuExtraInfo;
		}
	}

	return std::vector<CString>();
}


LONG CDnaSearch::GetResultState(LONG SearchID, LONG ResultID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
			return itResult->second->State;
	}

	return 0;
}



LONG CDnaSearch::GetResultMetaID(LONG SearchID, LONG ResultID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
			return itResult->second->AvgMetaID;
	}

	return 0;
}

CString CDnaSearch::GetResultAttributeValue(LONG SearchID, LONG ResultID, LONG AttributeID)
{
	  

	CString strResult;

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{
			std::map<int, CString>::iterator itAttr = itResult->second->AvgAttributeMap.find(AttributeID);

			if(itAttr != itResult->second->AvgAttributeMap.end())
				strResult = itAttr->second;
		}
	}

	return strResult;
}

LONG CDnaSearch::GetHostMetaID(LONG SearchID, LONG ResultID, LONG HostID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
				return itResult->second->ResultList[itHost->second].MetaID;
		}
	}

	return 0;
}

CString CDnaSearch::GetHostAttributeValue(LONG SearchID, LONG ResultID, LONG HostID, LONG AttributeID)
{
	  

	CString strResult;

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);

	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

		if(itResult != pSearch->m_ResultMap.end())
		{ 
			std::map<UINT, UINT>::iterator itHost = itResult->second->HostMap.find(HostID);
			
			if(itHost != itResult->second->HostMap.end())
			{
				std::map<UINT, ResultGroup*>::iterator itResult = pSearch->m_ResultMap.find(ResultID);

				if(itResult != pSearch->m_ResultMap.end())
				{
					std::map<int, CString>::iterator itAttr = itResult->second->ResultList[itHost->second].AttributeMap.find(AttributeID);

					if(itAttr != itResult->second->ResultList[itHost->second].AttributeMap.end())
						strResult = itAttr->second;
				}
			}
		}
	}

	return strResult;
}



LONG CDnaSearch::CountHostsSearched(LONG SearchID)
{
	  

	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);
	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;
		
		if( m_gnuNetwork->m_pG2)
		{
			std::list<G2_Search*>::iterator itG2Query;
			for(itG2Query = m_gnuNetwork->m_pG2->m_G2Searches.begin(); itG2Query != m_gnuNetwork->m_pG2->m_G2Searches.end(); itG2Query++)
				if(pSearch->m_QueryID == (*itG2Query)->Query.SearchGuid)
				{
					return (*itG2Query)->SearchedHubs + (*itG2Query)->SearchedChildren;
				}
		}
	}

	return 0;
}

void CDnaSearch::ContinueSearch(LONG SearchID)
{
	std::map<int, CGnuSearch*>::iterator itSearch = m_gnuNetwork->m_SearchIDMap.find(SearchID);
	if(itSearch != m_gnuNetwork->m_SearchIDMap.end())
	{
		CGnuSearch* pSearch = itSearch->second;

		pSearch->m_SearchPaused = false;
		pSearch->m_ResultStep   = pSearch->m_WholeList.size() + SEARCH_RESULT_STEP;
		pSearch->m_NextTimeout  = time(NULL) + SEARCH_TIMOUT_STEP;
	}
}
