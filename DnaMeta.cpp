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
#include "GnuMeta.h"
#include "GnuSchema.h"

#include "DnaMeta.h"


CDnaMeta::CDnaMeta()
{
	m_dnaCore   = NULL;
	m_gnuMeta = NULL;

 
}

void CDnaMeta::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore = dnaCore;
	m_gnuMeta = dnaCore->m_gnuCore->m_pMeta;
}

CDnaMeta::~CDnaMeta()
{
}



// CDnaMeta message handlers

void CDnaMeta::LoadSchemaDir(LPCTSTR DirPath)
{
	

	m_gnuMeta->LoadSchemaDir(DirPath);
}

std::vector<int> CDnaMeta::GetMetaIDs(void)
{
	std::vector<int> MetaIDs;

	for(int i = 0; i < m_gnuMeta->m_MetaList.size(); i++)
		MetaIDs.push_back( m_gnuMeta->m_MetaList[i]->m_MetaID );

	return MetaIDs;
}

CString CDnaMeta::GetMetaName(LONG MetaID)
{
	

	return m_gnuMeta->GetMetaName(MetaID);
}

std::vector<int> CDnaMeta::GetAttributeIDs(LONG MetaID)
{
	std::vector<int> AttributeIDs;

	std::map<int, CGnuSchema*>::iterator itMeta = m_gnuMeta->m_MetaIDMap.find(MetaID);
	if(itMeta != m_gnuMeta->m_MetaIDMap.end())
	{
		for(int i = 0; i < itMeta->second->m_Attributes.size(); i++)
			AttributeIDs.push_back( itMeta->second->m_Attributes[i].AttributeID );
	}

	return AttributeIDs;
}

CString CDnaMeta::GetAttributeName(LONG MetaID, LONG AttributeID)
{
	

	CString strResult;

	std::map<int, CGnuSchema*>::iterator itMeta = m_gnuMeta->m_MetaIDMap.find(MetaID);

	if(itMeta != m_gnuMeta->m_MetaIDMap.end())
	{
		strResult = itMeta->second->GetAttributeName(AttributeID);
	}

	return strResult;
}

BOOL CDnaMeta::GetAttributeReadOnly(LONG MetaID, LONG AttributeID)
{
	

	std::map<int, CGnuSchema*>::iterator itMeta = m_gnuMeta->m_MetaIDMap.find(MetaID);

	if(itMeta != m_gnuMeta->m_MetaIDMap.end())
	{
		std::map<int, int>::iterator itAttr = itMeta->second->m_AttributeMap.find(AttributeID);

		if(itAttr != itMeta->second->m_AttributeMap.end())
			return itMeta->second->m_Attributes[itAttr->second].ReadOnly;
	}

	return FALSE;
}

CString CDnaMeta::GetAttributeType(LONG MetaID, LONG AttributeID)
{
	

	CString strResult;

	std::map<int, CGnuSchema*>::iterator itMeta = m_gnuMeta->m_MetaIDMap.find(MetaID);

	if(itMeta != m_gnuMeta->m_MetaIDMap.end())
	{
		std::map<int, int>::iterator itAttr = itMeta->second->m_AttributeMap.find(AttributeID);

		if(itAttr != itMeta->second->m_AttributeMap.end())
			strResult = itMeta->second->m_Attributes[itAttr->second].Type;
	}

	return strResult;
}

std::vector<CString> CDnaMeta::GetAttributeEnums(LONG MetaID, LONG AttributeID)
{
	std::map<int, CGnuSchema*>::iterator itMeta = m_gnuMeta->m_MetaIDMap.find(MetaID);
	if(itMeta != m_gnuMeta->m_MetaIDMap.end())
	{
		std::map<int, int>::iterator itAttr = itMeta->second->m_AttributeMap.find(AttributeID);

		if(itAttr != itMeta->second->m_AttributeMap.end())
			return itMeta->second->m_Attributes[itAttr->second].Enums;
	}

	return std::vector<CString>();
}

std::vector<CString> CDnaMeta::GetMetaExtensions(LONG MetaID)
{
	std::map<int, CGnuSchema*>::iterator itMeta = m_gnuMeta->m_MetaIDMap.find(MetaID);

	if(itMeta != m_gnuMeta->m_MetaIDMap.end())
		return itMeta->second->m_Extensions;

	return std::vector<CString>();
}
