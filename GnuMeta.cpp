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
#include "GnuCore.h"
#include "GnuTransfers.h"
#include "GnuShare.h"

#include "GnuSchema.h"
#include "GnuMeta.h"



CGnuMeta::CGnuMeta(CGnuCore* pCore)
{
	m_pCore = pCore;

	m_NextMetaID = 1;
}

CGnuMeta::~CGnuMeta(void)
{
	for(int i = 0; i < m_MetaList.size(); i++)
	{
		delete m_MetaList[i];
		m_MetaList[i] = NULL;
	}
}

void CGnuMeta::LoadSchemaDir(CString DirPath)
{
	// Add wild card to directory path
	CString strWildcard = DirPath;
	strWildcard.Replace("\\\\", "\\");

	if(strWildcard.GetAt( strWildcard.GetLength() - 1) != '\\')
		strWildcard += "\\*";
	else
		strWildcard += "*";

	// Go through files in directory
	CFileFind Finder;

	bool bWorking = Finder.FindFile(strWildcard);

	while (bWorking)
	{
		bWorking = Finder.FindNextFile();

		if (Finder.IsDots() || Finder.IsDirectory())
			continue;

		CString FilePath  = Finder.GetFilePath();

		int DotPos = FilePath.ReverseFind('.');
		if(DotPos != -1)
			if(FilePath.Mid(DotPos).CompareNoCase(".xsd") == 0)
				LoadSchemaFile(FilePath);
	}

	Finder.Close();

	m_pCore->m_pShare->m_UpdateShared = true;
	m_pCore->m_pTrans->TransferLoadMeta();
}	

void CGnuMeta::LoadSchemaFile(CString FilePath)
{
	// Get Name of file from path
	int SlashPos = FilePath.ReverseFind('\\');

	CString SchemaFile;
	if(SlashPos != -1)
		SchemaFile = FilePath.Mid(SlashPos + 1);


	// Determine which schema class to use
	CGnuSchema* pSchema = NULL;

	pSchema = CGnuSchema::GetSchemaInstance((LPCSTR)SchemaFile);

	// Check if definition already loaded
	for(int i = 0; i < m_MetaList.size(); i++)
		if(m_MetaList[i]->m_Name.CompareNoCase(pSchema->m_Name) == 0)
		{
			delete pSchema;
			return;
		}


	// Load schema file
	if( !pSchema->LoadXMLFile(FilePath))
	{
		delete pSchema;
		return;
	}


	// Load def file
	FilePath.Replace(".xsd", ".xml");
	pSchema->LoadXMLFile(FilePath);


	// Add to schema list and give ID
	if(m_NextMetaID < 1)
		m_NextMetaID = 1;
	
	pSchema->m_MetaID = m_NextMetaID++;
	m_MetaIDMap[pSchema->m_MetaID] = pSchema;

	pSchema->m_pMeta = this;

	m_MetaList.push_back(pSchema);
}

void CGnuMeta::LoadFileMeta(SharedFile &File)
{
	for(int i = 0; i < m_MetaList.size(); i++)
		if(m_MetaList[i]->MatchExtension(File.Name.c_str()))
		{
			m_MetaList[i]->LoadData(File);

			if (File.MetaID != 0)
				break;
		}
}

CString CGnuMeta::GetMetaName(int MetaID)
{	
	std::map<int, CGnuSchema*>::iterator itMeta = m_MetaIDMap.find(MetaID);

	if(itMeta != m_MetaIDMap.end())
		return itMeta->second->m_Name;

	return "";
}

bool CGnuMeta::DecompressMeta(CString &MetaLoad, byte* MetaBuff, int MetaLength)
{
	if(MetaLength < 12)
		return false;


	// Plain text
	if(MetaLoad.Left(11).CompareNoCase("{plaintext}") == 0)
		MetaLoad = MetaLoad.Mid(11);


	// zlib compressed
	else if(MetaLoad.Left(9).CompareNoCase("{deflate}") == 0)
	{
		char* UncompressedBuff = new char[16384];
		DWORD UncompressedSize = 16384;

		if(uncompress((byte*) UncompressedBuff, &UncompressedSize, MetaBuff + 9, MetaLength - 9) == Z_OK)
		{
			MetaLoad = CString(UncompressedBuff, UncompressedSize);
			delete [] UncompressedBuff;
		}
		else
		{
			delete [] UncompressedBuff;
			return false;
		}
	}

	MetaLoad.Replace("<?xml version=\"1.0\"?>", "");

	return true;
}

void CGnuMeta::ParseMeta(CString MetaLoad, std::map<int, int> &MetaIDMap, std::map<int, CString> &MetaValueMap)
{
	// Loop trough meta types
	for(int i = 0; i < m_MetaList.size(); i++)
	{
		int StartPos = MetaLoad.Find("<" + m_MetaList[i]->m_Name + " ");

		while(StartPos != -1)
		{
			// Get end tag pos
			int EndPos = MetaLoad.Find("/>", StartPos);

			if(EndPos == -1)
			{
				EndPos = MetaLoad.Find("</" + m_MetaList[i]->m_Name + ">", StartPos);
				
				if(EndPos == -1)
					break;
			}

			CString MetaLine = MetaLoad.Mid(StartPos, EndPos - StartPos);
			MetaLine += " >";

			// Get what file index this tag is associated with
			CString MetaLineLow = MetaLine;
			MetaLineLow.MakeLower();
			
			int IndexPos = MetaLineLow.Find("index=");
			
			if(IndexPos != -1)
			{
				IndexPos += 6;

				int IndexBackPos = MetaLineLow.Find(" ", IndexPos);

				if(IndexBackPos != -1)
				{
					CString IndexStr = MetaLineLow.Mid(IndexPos, IndexBackPos - IndexPos);

					IndexStr.Remove('\"');

					int FileIndex = atoi(IndexStr);

					// Add line to MetaMap
					MetaIDMap[FileIndex]    = m_MetaList[i]->m_MetaID;
					MetaValueMap[FileIndex] = MetaLine;
				}
			}

			StartPos = MetaLoad.Find("<" + m_MetaList[i]->m_Name, EndPos);
		}
	}
}

int CGnuMeta::MetaIDfromXml( CString MetaXml )
{
	for(int i = 0; i < m_MetaList.size(); i++)
	{
		int StartPos = MetaXml.Find("<" + m_MetaList[i]->m_Name + " ");

		if(StartPos != -1)
			return m_MetaList[i]->m_MetaID;
	}

	return 0;
}



