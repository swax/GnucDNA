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

#include "GnuCore.h"
#include "GnuUpdate.h"

#include "DnaUpdate.h"


 
CDnaUpdate::CDnaUpdate()
{
	m_dnaCore   = NULL;
	m_gnuUpdate = NULL;
}

void CDnaUpdate::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore   = dnaCore;
	m_gnuUpdate = dnaCore->m_gnuCore->m_pUpdate;
}

CDnaUpdate::~CDnaUpdate()
{
}

// CDnaUpdate members

void CDnaUpdate::AddServer(LPCTSTR Server)
{
	 

	m_gnuUpdate->AddServer(Server);
}

void CDnaUpdate::Check(void)
{
	 

	m_gnuUpdate->Check();
}

void CDnaUpdate::StartDownload(void)
{
	 

	m_gnuUpdate->StartDownload();
}

void CDnaUpdate::CancelUpdate(void)
{
	 

	m_gnuUpdate->CancelUpdate();
}

std::vector<int> CDnaUpdate::GetFileIDs(void)
{
	std::vector<int> FileIDs;

	for(int i = 0; i < m_gnuUpdate->m_FileList.size(); i++)
		FileIDs.push_back( m_gnuUpdate->m_FileList[i].FileID );

	return FileIDs;
}

LONG CDnaUpdate::GetTotalCompleted(void)
{
	 

	return m_gnuUpdate->TotalUpdateCompleted();
}

LONG CDnaUpdate::GetTotalSize(void)
{
	 

	return m_gnuUpdate->TotalUpdateSize();
}

CString CDnaUpdate::GetFileName(LONG FileID)
{
	 

	CString strResult;

	for(int i = 0; i < m_gnuUpdate->m_FileList.size(); i++)
		if(m_gnuUpdate->m_FileList[i].FileID == FileID)
			strResult = m_gnuUpdate->m_FileList[i].Name;

	return strResult;
}

LONG CDnaUpdate::GetFileSize(LONG FileID)
{
	

	for(int i = 0; i < m_gnuUpdate->m_FileList.size(); i++)
		if(m_gnuUpdate->m_FileList[i].FileID == FileID)
			return m_gnuUpdate->m_FileList[i].Size;

	return 0;
}

LONG CDnaUpdate::GetFileCompleted(LONG FileID)
{
	 

	for(int i = 0; i < m_gnuUpdate->m_FileList.size(); i++)
		if(m_gnuUpdate->m_FileList[i].FileID == FileID)
			return m_gnuUpdate->GetFileCompleted(m_gnuUpdate->m_FileList[i]);

	return 0;
}

void CDnaUpdate::LaunchUpdate(void)
{
	 

	m_gnuUpdate->LaunchUpdate();
}
