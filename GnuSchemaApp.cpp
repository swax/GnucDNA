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

#include "StdAfx.h"

#include "GnuSchemaApp.h"

REGISTER_SCHEMA_CLASS(CGnuSchemaApp, "application.xsd")

CGnuSchemaApp::CGnuSchemaApp(void)
{
	m_pVersionInfo = NULL;
	m_dwTransInfo  = 0;

	SetAttributeReadOnly("Title");
	SetAttributeReadOnly("Version");
	SetAttributeReadOnly("FileDescription");
	SetAttributeReadOnly("FileVersion");
	SetAttributeReadOnly("OriginalFilename");
	SetAttributeReadOnly("Company");
	SetAttributeReadOnly("Copyright");
	SetAttributeReadOnly("OS");
	SetAttributeReadOnly("Comments");
}

CGnuSchemaApp::~CGnuSchemaApp(void)
{
}

void CGnuSchemaApp::LoadData(SharedFile &File)
{
	// Load generic external meta file
	CGnuSchema::LoadData(File);


	m_strFilename = File.Dir.c_str();

	// Clear
	m_dwTransInfo = 0L;
	memset(&m_fixedFileInfo, 0, sizeof(VS_FIXEDFILEINFO));
	if (m_pVersionInfo)
	{
		delete m_pVersionInfo;
		m_pVersionInfo = NULL;
	}


	ReadVersionBlock(File);
}


void CGnuSchemaApp::ReadVersionBlock(SharedFile &File)
{
	// Get Info Size
	DWORD	dwDummy;
	UINT	nSize;
	LPVOID	lpVoid;

	nSize = GetFileVersionInfoSize((LPTSTR) (LPCTSTR) m_strFilename, &dwDummy);
	if (!nSize) 
		return;	// no version information available

	
	// Get Version block
	m_pVersionInfo = new BYTE[nSize];

	if ( !GetFileVersionInfo((LPTSTR) (LPCTSTR) m_strFilename, 0L, nSize, m_pVersionInfo) ) 
	{
		delete m_pVersionInfo;
		m_pVersionInfo = NULL;
		return;
	}


	// Get language independant info
	if ( !VerQueryValue(m_pVersionInfo, _T("\\"), &lpVoid, &nSize) )
	{
		delete m_pVersionInfo;
		m_pVersionInfo = NULL;
		return;
	}
	memcpy(&m_fixedFileInfo, lpVoid, nSize);


	// Get language info
	
	VerQueryValue(m_pVersionInfo, TEXT("\\VarFileInfo\\Translation"), (LPVOID*) &m_pTranslate, &nSize);

	for(int i = 0; i < (nSize / sizeof(struct LANGANDCODEPAGE)); i++ )
	{
		LoadFileAttribute(&File, "Title", ExtractString("ProductName"));
		LoadFileAttribute(&File, "Version", ExtractString("ProductVersion"));
		LoadFileAttribute(&File, "FileDescription", ExtractString("FileDescription"));
		LoadFileAttribute(&File, "FileVersion", ExtractString("FileVersion"));
		LoadFileAttribute(&File, "OriginalFilename", ExtractString("OriginalFilename"));
		LoadFileAttribute(&File, "Company", ExtractString("CompanyName"));
		LoadFileAttribute(&File, "Copyright", ExtractString("LegalCopyright"));
		LoadFileAttribute(&File, "OS", GetOS());
		LoadFileAttribute(&File, "Comments", ExtractString("Comments"));


		// Just do the first one
		break;
	}
	
}

CString CGnuSchemaApp::ExtractString(CString VersQuery)
{
	CString SubBlock;
	SubBlock.Format("\\StringFileInfo\\%04x%04x\\", m_pTranslate->wLanguage, m_pTranslate->wCodePage);
	SubBlock += VersQuery;

	// Retrieve file description for language and code page "i". 
	LPSTR pBuffer;
	UINT  nBytes;

	if (VerQueryValue(m_pVersionInfo, (LPSTR) (LPCSTR) SubBlock, (LPVOID*) &pBuffer, &nBytes)) 
    	return CString(pBuffer, nBytes);
    return CString("");
}

CString CGnuSchemaApp::GetOS()
{
	switch (m_fixedFileInfo.dwFileOS)
	{
	case VOS_DOS:			
		return "DOS";

	case VOS_DOS_WINDOWS16:	
		return "DOS";

	case VOS_DOS_WINDOWS32:	
		return "DOS";

	case VOS__WINDOWS16:	
		return "Windows 3.1";

	case VOS__WINDOWS32:	
		return "Windows 95/98/Me";

	case VOS_OS216:			
		return "OS/2";

	case VOS_OS232:			
		return "OS/2";

	case VOS_NT:			
		return "Windows NT/2000/XP";

	case VOS_NT_WINDOWS32:	
		return "Windows NT/2000/XP";
	}
	
	return "";
}

