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

#include "GnuPrefs.h"
#include "GnuNetworks.h"
#include "GnuControl.h"
#include "GnuNode.h"
#include "G2Packets.h"
#include "G2Control.h"
#include "GnuShare.h"
#include "GnuWordHash.h"
#include "GnuTransfers.h"
#include "GnuDownloadShell.h"
#include "GnuMeta.h"
#include "GnuSchema.h"

#include "DnaCore.h"
#include "DnaEvents.h"
#include "DnaSearch.h"

#include "gnusearch.h"

CGnuSearch::CGnuSearch(CGnuNetworks* pNet)
{
	m_pNet      = pNet;
	m_pCore     = pNet->m_pCore;
	m_pPrefs    = m_pCore->m_pPrefs;
	m_pShare	= m_pCore->m_pShare;
	m_pTrans    = m_pCore->m_pTrans;
	m_pMeta		= m_pCore->m_pMeta;
	

	if(pNet->m_NextSearchID < 1)
		pNet->m_NextSearchID = 1;

	m_SearchID = pNet->m_NextSearchID++;
	pNet->m_SearchIDMap[m_SearchID] = this;

	GnuCreateGuid(&m_QueryID);

	m_BrowseNode = NULL;
	m_BrowseWaiting = 0;

	m_NextResultID = 1;

	m_MetaID = 0;

	m_ResultStep   = SEARCH_RESULT_STEP;
	m_NextTimeout  = time(NULL) + SEARCH_TIMOUT_STEP;
	m_SearchPaused = false;

	m_SizeFilterMode   = LIMIT_NONE;
	m_SizeFilterValue  = 0;

	m_SpeedFilterMode  = LIMIT_NONE;
	m_SpeedFilterValue = 0;

	memset(m_GnuPacket, 0, 4096);
	m_GnuPacketLength = 0;

	m_BrowseNode = NULL;
}

CGnuSearch::~CGnuSearch(void)
{
	if(m_BrowseNode)
	{
		delete m_BrowseNode;
		m_BrowseNode = NULL;
	}

	m_pNet->m_SearchIDMap.erase( m_pNet->m_SearchIDMap.find(m_SearchID) );

	if(m_pNet->m_pG2)
		m_pNet->m_pG2->EndSearch(m_QueryID);
}

void CGnuSearch::SendQuery(CString Query)
{
	m_Search = Query;

	if(BUILD_GNUTELLA)
	{
		// Build packet	
		int Payload = 25;
		memcpy(m_GnuPacket + Payload, m_Search, m_Search.GetLength());
		Payload += m_Search.GetLength();

		// Request for hash reply
		m_GnuPacket[Payload] = '\0';
		Payload += 1;

		// Send query through network
		m_GnuPacketLength = Payload;
		memcpy(&m_GnuPacket, &m_QueryID, 16);

		if(m_pNet->m_pGnu)
			m_pNet->m_pGnu->Broadcast_LocalQuery(m_GnuPacket, m_GnuPacketLength);
	}

	if( m_pNet->m_pG2 )
	{
		G2_Search* pSearch = new G2_Search;

		pSearch->Query.SearchGuid = m_QueryID;
		pSearch->Query.DescriptiveName = m_Search;

		m_pNet->m_pG2->StartSearch(pSearch);
	}
}

void CGnuSearch::SendMetaQuery(CString Query, int MetaID, std::vector<CString> Metadata)
{
	m_Search = Query;

	std::map<int, CGnuSchema*>::iterator itMeta = m_pMeta->m_MetaIDMap.find(MetaID);
	if(itMeta != m_pMeta->m_MetaIDMap.end())
	{
		CGnuSchema* pSchema = itMeta->second;
		
		m_MetaID = MetaID;

		// <videos><video title="my video" year="2002"/></videos>
		//m_MetaSearch = "<" + pSchema->m_NamePlural + ">";
		m_MetaSearch += "<" + pSchema->m_Name;

		for(int i = 0; i < Metadata.size(); i++)
		{
			int ColonPos = Metadata[i].Find(":");

			if(ColonPos != -1)
			{
				CString Attribute = Metadata[i].Left(ColonPos);
				CString Value     = Metadata[i].Mid(ColonPos + 1);

				if( !Attribute.IsEmpty() && !Value.IsEmpty() )
					m_MetaSearch += " " + Attribute + "=\"" + Value + "\"";


				// Save pair for reference with results
				int AttrID = pSchema->VerifyAttribute(Attribute, Value);

				if(AttrID)
					m_SearchAttributes[AttrID] = Value;
			}
		}

		// Close tag and add to packet
		m_MetaSearch += " />";
		//m_MetaSearch += </" + pSchema->m_NamePlural + ">"; 
	}

	if(BUILD_GNUTELLA)
	{
		// Build packet	
		int Payload = 25;
		if(m_Search.GetLength())
		{
			memcpy(m_GnuPacket + Payload, m_Search, m_Search.GetLength());
			Payload += m_Search.GetLength();		
		}

		m_GnuPacket[Payload] = '\0';
		Payload += 1;


		// Add meta-data
		if( m_MetaSearch.GetLength() )
		{
			memcpy(m_GnuPacket + Payload, m_MetaSearch, m_MetaSearch.GetLength());
			Payload += m_MetaSearch.GetLength();

			m_GnuPacket[Payload] = '\0';
			Payload += 1;
		}


		// Send query through network
		m_GnuPacketLength = Payload;
		memcpy(&m_GnuPacket, &m_QueryID, 16);
		
		if(m_pNet->m_pGnu)
			m_pNet->m_pGnu->Broadcast_LocalQuery(m_GnuPacket, m_GnuPacketLength);
	}

	if( m_pNet->m_pG2 )
	{
		G2_Search* pSearch = new G2_Search;

		pSearch->Query.SearchGuid      = m_QueryID;
		pSearch->Query.DescriptiveName = m_Search;
		pSearch->Query.Metadata        = m_MetaSearch;

		m_pNet->m_pG2->StartSearch(pSearch);
	}
}

void CGnuSearch::SendHashQuery(CString Query, int HashID, CString Hash)
{
	m_Search = Query;
	m_Hash = HashIDtoTag(HashID) + Hash;

	if( m_Hash.Left(4) != "urn:")
		m_Hash = "urn:" + m_Hash;

	if(BUILD_GNUTELLA)
	{
		// Build packet	
		int Payload = 25;
		if(m_Search.GetLength())
		{
			memcpy(m_GnuPacket + Payload, m_Search, m_Search.GetLength());
			Payload += m_Search.GetLength();		
		}

		m_GnuPacket[Payload] = '\0';
		Payload += 1;

		if(m_Hash.GetLength())
		{
			memcpy(m_GnuPacket + Payload, m_Hash, m_Hash.GetLength());
			Payload += m_Hash.GetLength();

			m_GnuPacket[Payload] = '\0';
			Payload += 1;
		}


		// Send query through network
		m_GnuPacketLength = Payload;
		memcpy(&m_GnuPacket, &m_QueryID, 16);
		
		if( m_pNet->m_pGnu )
			m_pNet->m_pGnu->Broadcast_LocalQuery(m_GnuPacket, m_GnuPacketLength);
	}

	if( m_pNet->m_pG2 )
	{
		G2_Search* pSearch = new G2_Search;

		pSearch->Query.SearchGuid      = m_QueryID;
		pSearch->Query.DescriptiveName = m_Search;
		pSearch->Query.URNs.push_back(m_Hash);

		m_pNet->m_pG2->StartSearch(pSearch);
	}
}

void CGnuSearch::SendBrowseRequest(CString Host, int Port)
{
	if( m_pNet->m_pGnu )
	{
		m_BrowseNode = new CGnuNode(m_pNet->m_pGnu, Host, Port);
		m_BrowseNode->m_BrowseID = m_SearchID;

		m_BrowseSearch = true;
		GnuCreateGuid((GUID*) m_GnuPacket);

		//Attempt to connect to node
		if(!m_BrowseNode->Create())
		{
			m_BrowseNode->m_Status = SOCK_CLOSED;
			return;
		}
		
		if( !m_BrowseNode->Connect(Host, Port) )
			if (m_BrowseNode->GetLastError() != WSAEWOULDBLOCK)
			{
				m_BrowseNode->m_Status = SOCK_CLOSED;
				return;
			}
	}
}

void CGnuSearch::IncomingSource(FileSource &Source)
{
	// Send to connected downloads
	if( !Source.Sha1Hash.IsEmpty() )
	{
		std::map<CString, CGnuDownloadShell*>::iterator itDown = m_pTrans->m_DownloadHashMap.find( Source.Sha1Hash );
		if(itDown != m_pTrans->m_DownloadHashMap.end())
			itDown->second->AddHost(Source);
	}

	// Check for duplicates in master list
	for(int i = 0; i < m_WholeList.size(); i++)
		if(Source.Address.Host.S_addr == m_WholeList[i].Address.Host.S_addr && 
		   Source.Address.Port == m_WholeList[i].Address.Port && 
		   Source.Sha1Hash == m_WholeList[i].Sha1Hash)
			return;


	m_WholeList.push_back(Source);

	// Screen Item to user's preferences
	if( !Inspect(Source) )
		return;
	
	m_CurrentList.push_back(Source);
	ResultGroup* pGroup = AddtoGroup(Source);

	if(pGroup && m_pCore->m_dnaCore->m_dnaEvents)
	{
		if(pGroup->ResultList.size() > 1)
			m_pCore->m_dnaCore->m_dnaEvents->SearchUpdate(m_SearchID, pGroup->ResultID);
		else
			m_pCore->m_dnaCore->m_dnaEvents->SearchResult(m_SearchID, pGroup->ResultID);
	}
}

bool CGnuSearch::Inspect(FileSource &Item)
{
	// Refine filter
	if(!RefinedQuery.IsEmpty())
		if(!ResultDoubleCheck(Item.NameLower, RefinedQuery))
			return false;


	// If filtering is active
	if(m_FilteringActive)
	{
		// Node Filter
		if(m_pPrefs->m_ScreenNodes)
			if(!m_pPrefs->AllowedIP(Item.Address.Host))
				return false;

		// Blocked hosts filter
		if(m_pPrefs->BlockedIP(Item.Address.Host))
			return false;

		// Size Filter
		if(m_SizeFilterMode != LIMIT_NONE)
			if(!CheckLimit(m_SizeFilterMode, m_SizeFilterValue, Item.Size))
				return false;

		// Speed Filter
		if(m_SpeedFilterMode != LIMIT_NONE)
			if(!CheckLimit(m_SpeedFilterMode, m_SpeedFilterValue, Item.Speed / 8))
				return false;

		// Word Filter
		std::vector<CString>::iterator itWord;
		for (itWord = m_pPrefs->m_ScreenedWords.begin(); itWord != m_pPrefs->m_ScreenedWords.end(); itWord++)
			if(Item.NameLower.Find(*itWord) != -1)
				return false;

		// Double Check
		if(m_pPrefs->m_DoubleCheck)
		{
			if(!ResultDoubleCheck(Item.NameLower, m_Search))
				return false;

			if(!ResultDoubleCheckMeta(Item))
				return false;
		}
	}

	return true;
}

bool CGnuSearch::ResultDoubleCheck(CString Result, CString Query)
{
	Result.MakeLower();
	Query.MakeLower();

	// Break Query into words
	std::vector< std::basic_string<char> > Words;
	m_pShare->m_pWordTable->BreakupName( (LPCTSTR) Query, Words);

	
	// Search result for the those words
	for(int i = 0; i < Words.size(); i++)
		if( Result.Find( Words[i].c_str() ) == -1 )
			return false;


	return true;
}

bool CGnuSearch::ResultDoubleCheckMeta(FileSource &Item)
{
	
	// If search has a meta type result should too
	if(m_MetaID)
	{
		if(m_MetaID == Item.MetaID)
		{
			std::map<int, CString>::iterator itSearchAttr = m_SearchAttributes.begin();

			for( ; itSearchAttr != m_SearchAttributes.end(); itSearchAttr++)
			{
				std::map<int, CString>::iterator itResultAttr = Item.AttributeMap.begin();
				
				// Match result attributes with search attributes
				bool AttrFound = false;

				for( ; itResultAttr != Item.AttributeMap.end(); itResultAttr++)
					if(itResultAttr->first == itSearchAttr->first)
					{
						AttrFound = true;

						if( !ResultDoubleCheck(itResultAttr->second, itSearchAttr->second) )
							return false;

						break;
					}

				if(!AttrFound)
					return false;
			}
		}

		else
			return false;
	}


	return true;
}

bool CGnuSearch::CheckLimit(int Limit, DWORD Value, DWORD Compare)
{
	if(Limit == LIMIT_MORE)
	{
		if(Compare < Value)
			return false;
	}
	else if(Limit == LIMIT_EXACTLY)
	{
		if(Compare != Value)
			return false;
	}
	else if(Limit == LIMIT_LESS)
	{
		if(Compare > Value)
			return false;
	}
	
	return true;
}

ResultGroup* CGnuSearch::AddtoGroup(FileSource &Item)
{
	bool NewGroup = true;
	ResultGroup* ReturnGroup = NULL;

	// Check if item is part of any groups
	std::list<ResultGroup>::iterator itGroup;
	for(itGroup = m_GroupList.begin(); itGroup != m_GroupList.end(); itGroup++)
		if((*itGroup).Size == Item.Size && Item.Sha1Hash == (*itGroup).Sha1Hash)
		{
			Item.SourceID = (*itGroup).NextHostID++;
			(*itGroup).HostMap[Item.SourceID] = (*itGroup).ResultList.size();

			(*itGroup).ResultList.push_back(Item);
			
			NewGroup = false;
			break;
		}


	// If not create a new group
	if(NewGroup)
	{
		ResultGroup InsertGroup;

		InsertGroup.ResultID   = m_NextResultID++;

		InsertGroup.Name     = Item.Name;
		InsertGroup.State    = RESULT_INACTIVE;

		// Take spaces out of name
		while(InsertGroup.Name.Find("  ") != -1)
			InsertGroup.Name.Replace("  ", " ");

		InsertGroup.NameLower = InsertGroup.Name;
		InsertGroup.NameLower.MakeLower();

		InsertGroup.Size	 = Item.Size;
		InsertGroup.AvgSpeed = Item.Speed;
		InsertGroup.Sha1Hash = Item.Sha1Hash;
		InsertGroup.State	 = UpdateResultState(Item.Sha1Hash);

		// Add Meta
		InsertGroup.AvgMetaID = 0;

		if(Item.MetaID)
		{
			InsertGroup.AvgMetaID = Item.MetaID;

			std::map<int, CString>::iterator itAttr = Item.AttributeMap.begin();

			for( ; itAttr != Item.AttributeMap.end(); itAttr++)
				InsertGroup.AvgAttributeMap[itAttr->first] = itAttr->second;
		}

		Item.SourceID = InsertGroup.NextHostID++;
		InsertGroup.HostMap[Item.SourceID] = 0;
		InsertGroup.ResultList.push_back(Item);

		m_GroupList.push_back(InsertGroup);
		ReturnGroup = &m_GroupList.back();
		
		m_ResultMap[InsertGroup.ResultID]     = ReturnGroup;
		m_ResultHashMap[InsertGroup.Sha1Hash] = ReturnGroup;
	}	
	else
	{
		int Hosts = 0, SpeedSum = 0;

		std::vector<FileSource>::iterator itResult;
		for(itResult = (*itGroup).ResultList.begin(); itResult != (*itGroup).ResultList.end(); itResult++)
		{
			SpeedSum += (*itResult).Speed;
			Hosts++;
		}

		if(Hosts)
			(*itGroup).AvgSpeed = SpeedSum / Hosts;


		// Average metadata
		if(Item.MetaID)
		{
			if((*itGroup).AvgMetaID == 0)
				(*itGroup).AvgMetaID = Item.MetaID;
			
			if((*itGroup).AvgMetaID == Item.MetaID)
			{
				std::map<int, CString>::iterator itAttr = Item.AttributeMap.begin();

				// Add similar attributes to general result, mis-matches will nullify both
				for( ; itAttr != Item.AttributeMap.end(); itAttr++)
				{
					std::map<int, CString>::iterator itAvgAttr = (*itGroup).AvgAttributeMap.find(itAttr->first);

					if(itAvgAttr != (*itGroup).AvgAttributeMap.end())
					{
						if(itAttr->second.CompareNoCase(itAvgAttr->second) != 0)
							itAvgAttr->second = "";
					}
					else
						(*itGroup).AvgAttributeMap[itAttr->first] = itAttr->second;
				}
			}
		}


		ReturnGroup = &(*itGroup);
	}

	return ReturnGroup;
}

void CGnuSearch::Timer()
{
	// Update browse sock if we are browsing someone else
	if(m_BrowseNode)
	{
		if(m_BrowseNode->m_Status == SOCK_CLOSED || (m_BrowseNode->m_Status == SOCK_CONNECTING && m_BrowseWaiting > 8))
		{
			if( m_pNet->m_pGnu )
				m_pNet->m_pGnu->NodeUpdate(m_BrowseNode);
			
			delete m_BrowseNode;
			m_BrowseNode = NULL;
		}
		else
			m_BrowseWaiting++;

		return;
	}

	
	// Continue G2 Search
	if( !m_SearchPaused )
	{
		// Pause search if step hit
		if(m_WholeList.size() > m_ResultStep || time(NULL) > m_NextTimeout)
		{
			m_SearchPaused = true;

			if(m_pCore->m_dnaCore->m_dnaEvents)
				m_pCore->m_dnaCore->m_dnaEvents->SearchPaused(m_SearchID);

			return;
		}

		// Research Gnutella
		if( m_pNet->m_pGnu)
		{
			std::list<int>::iterator itID;
			for(itID = m_RequeryList.begin(); itID != m_RequeryList.end(); itID++)
			{
				std::map<int,CGnuNode*>::iterator itNode = m_pNet->m_pGnu->m_NodeIDMap.find(*itID);
				if(itNode != m_pNet->m_pGnu->m_NodeIDMap.end())
				{
					CGnuNode* pNode = itNode->second;

					if(pNode->m_NextRequeryWait == 0)
					{
						packet_Query* pQuery = (packet_Query*) m_GnuPacket;
						pNode->SendPacket(m_GnuPacket, m_GnuPacketLength, PACKET_QUERY, pQuery->Header.Hops);

						pNode->m_NextRequeryWait = REQUERY_WAIT;
						m_RequeryList.erase(itID);

						break;
					}
				}
				else
					itID = m_RequeryList.erase(itID);
			}
		}

		// Search G2
		if( m_pNet->m_pG2 )
			m_pNet->m_pG2->StepSearch(m_QueryID);
	}
}

void CGnuSearch::SockUpdate()
{
	// New conenction made, send searches over it
	//if(!m_SearchPaused)
	//	m_DoReQuery = true;
}

void CGnuSearch::TransferUpdate(int ResultID)
{
	std::map<UINT, ResultGroup*>::iterator itResult = m_ResultMap.find(ResultID);

	if(itResult != m_ResultMap.end() && m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->SearchUpdate(m_SearchID, ResultID);		
}

void CGnuSearch::Rebuild()
{
	m_ResultMap.clear();
	m_GroupList.clear();
	m_CurrentList.clear();

	for(int i = 0; i < m_WholeList.size(); i++)
		if(Inspect(m_WholeList[i]))
		{
			m_CurrentList.push_back(m_WholeList[i]);
			AddtoGroup(m_WholeList[i]);
		}

	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->SearchRefresh(m_SearchID);
}

int CGnuSearch::Download(UINT ResultID)
{
	int i = 0;

	// Make sure valid ID
	std::map<UINT, ResultGroup*>::iterator itResult = m_ResultMap.find(ResultID);
	if(itResult == m_ResultMap.end())
		return 0;
	
	ResultGroup* pResult = itResult->second;

	// Check if already downloading
	if( !pResult->Sha1Hash.IsEmpty() )
	{
		std::map<CString, CGnuDownloadShell*>::iterator itDown = m_pTrans->m_DownloadHashMap.find( pResult->Sha1Hash );
		if(itDown != m_pTrans->m_DownloadHashMap.end())
			return itDown->second->m_DownloadID;
	}	

	pResult->State = RESULT_TRYING;


	// Make the research string with the original search and refined search strings and the extension of the file
	CString ReSearch = RefinedQuery;

	if (ReSearch == "")
		ReSearch = m_Search;
	else
		ReSearch = m_Search + " " + RefinedQuery;

	int BreakPos = pResult->Name.ReverseFind('.');
	if(BreakPos > 0)
	{
		CString Extenstion = pResult->Name.Right( pResult->Name.GetLength() - BreakPos - 1);
		
		if(ReSearch.Find(Extenstion) == -1)
			ReSearch += " " + Extenstion;


		if(pResult->Name.GetLength() > 125)
			pResult->Name = pResult->Name.Left(100) + "." + Extenstion;
	}

	// These characters are filtered cause they cant be used in a file name or screw with partials
	ReSearch.Replace('\\', ' ');
	ReSearch.Replace('/', ' ');
	ReSearch.Replace(':', ' ');
	ReSearch.Replace('*', ' ');
	ReSearch.Replace('?', ' ');
	ReSearch.Replace('\"',' ');
	ReSearch.Replace('<', ' ');
	ReSearch.Replace('>', ' ');
	ReSearch.Replace('|', ' ');
	ReSearch.Replace('.', ' ');
	ReSearch.Replace(',', ' ');
	ReSearch.Replace('(', ' ');
	ReSearch.Replace(')', ' ');

	
	// Create new download
	CGnuDownloadShell* Download = new CGnuDownloadShell(m_pTrans);

	// Give download some inital properties
	CString FileName = pResult->Name;
	
	// Fix name if it is in direcory format
	int DirPos = FileName.ReverseFind('/');
	if(DirPos != -1)
		FileName = FileName.Mid(DirPos + 1);
	
	// Fix name if it has invalid chars
	FileName.Replace("/",  "");
	FileName.Replace("\\", "");
	FileName.Replace(":",  "");
	FileName.Replace("*",  "");
	FileName.Replace("?",  "");
	FileName.Replace("\"", "");
	FileName.Replace("<",  "");
	FileName.Replace(">",  "");
	FileName.Replace("|",  "");


	if(m_BrowseSearch)
		Download->m_Search = FileName;
	else
		Download->m_Search = ReSearch;
 	
	Download->m_AvgSpeed	= pResult->AvgSpeed;

	// Transfer metadata to download
	Download->m_MetaID = pResult->AvgMetaID;

	std::map<int, CString>::iterator itAttr;
	for(itAttr = pResult->AvgAttributeMap.begin(); itAttr != pResult->AvgAttributeMap.end(); itAttr++)
		Download->m_AttributeMap[itAttr->first] = itAttr->second;
	
	Download->m_MetaXml = Download->GetMetaXML(false);

	// Add hosts we've searched to the new download
	std::list<int>::iterator itID;
	for(itID = m_RequeryList.begin(); itID != m_RequeryList.end(); itID++)
		Download->m_RequeryList.push_back(*itID);


	// Change download name if there's a duplicate
	bool dups = true;
	while(dups)
	{
		dups = false;

		for(i = 0; i < m_pTrans->m_DownloadList.size(); i++)
			if(m_pTrans->m_DownloadList[i]->m_Name == FileName)
			{
				FileName = IncrementName(FileName);

				dups = true;
				break;
			}
	}

	Download->Init(FileName, pResult->Size, HASH_SHA1, pResult->Sha1Hash);

	m_pTrans->m_DownloadAccess.Lock();
	m_pTrans->m_DownloadList.push_back(Download);
	m_pTrans->m_DownloadAccess.Unlock();


	// Add hosts to the download list
	for(i = 0; i < pResult->ResultList.size(); i++)
		Download->AddHost( pResult->ResultList[i] );


	m_pTrans->DownloadUpdate(Download->m_DownloadID);
	TransferUpdate(ResultID);


	return Download->m_DownloadID;
}

void CGnuSearch::IncomingGnuNode(CGnuNode* pNode)
{
	ASSERT(pNode->m_GnuNodeMode == GNU_ULTRAPEER);

	m_RequeryList.push_back(pNode->m_NodeID);
}

int CGnuSearch::UpdateResultState(CString Hash)
{
	int State = RESULT_INACTIVE;

	std::map<CString, CGnuDownloadShell*>::iterator itDown = m_pTrans->m_DownloadHashMap.find(Hash);
	if(itDown != m_pTrans->m_DownloadHashMap.end())
	{
		CGnuDownloadShell* pShell = itDown->second;

		int DownloadStatus = pShell->GetStatus();
		
		if(DownloadStatus == TRANSFER_NOSOURCES)
			State = RESULT_NOSOURCES;

		if(DownloadStatus == TRANSFER_PENDING ||
		   DownloadStatus == TRANSFER_CONNECTING ||
		   DownloadStatus == TRANSFER_CONNECTED ||
		   DownloadStatus == TRANSFER_COOLDOWN ||
		   DownloadStatus == TRANSFER_QUEUED ||
		   DownloadStatus == TRANSFER_CLOSED )
		{
			State = RESULT_TRYING;
		}

		if(DownloadStatus == TRANSFER_RECEIVING)
			State = RESULT_DOWNLOADING;

		if(DownloadStatus == TRANSFER_COMPLETED)
			State = RESULT_COMPLETED;

		if(State == RESULT_INACTIVE)
			ASSERT(0);
	}

	// Check if file shared
	m_pShare->m_FilesAccess.Lock();

		std::map<CString, int>::iterator itShare = m_pShare->m_SharedHashMap.find(Hash);
		if(itShare != m_pShare->m_SharedHashMap.end())
			State = RESULT_SHARED;

	m_pShare->m_FilesAccess.Unlock();

	return State;

}