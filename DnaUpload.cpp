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
#include "GnuTransfers.h"
#include "GnuUploadShell.h"
#include "GnuUpload.h"

#include "DnaUpload.h"



CDnaUpload::CDnaUpload()
{
	m_dnaCore  = NULL;
	m_gnuTrans = NULL;
}

void CDnaUpload::InitClass(CDnaCore* dnaCore)
{
	m_dnaCore  = dnaCore;
	m_gnuTrans = dnaCore->m_gnuCore->m_pTrans;
}

CDnaUpload::~CDnaUpload()
{
}


// CDnaUpload message handlers

LONG CDnaUpload::GetStatus(LONG UploadID)
{

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->GetStatus();

	return 0;
}

CString CDnaUpload::GetName(LONG UploadID)
{
	

	CString strResult;

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		strResult = itUp->second->m_Name;

	return strResult;
}

DATE CDnaUpload::GetChangeTime(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
	{
		COleDateTime OleTime(itUp->second->m_ChangeTime.GetTime());

		return (DATE) OleTime;
	}

	return 0;
}

LONG CDnaUpload::GetBytesCompleted(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->m_BytesSent;

	return 0;
}

LONG CDnaUpload::GetFileLength(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->m_FileLength;

	return 0;
}

LONG CDnaUpload::GetBytesPerSec(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->GetBytesPerSec();

	return 0;
}

LONG CDnaUpload::GetSecETD(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->GetETD();

	return 0;
}

std::vector<int> CDnaUpload::GetUploadIDs(void)
{
	std::vector<int> UploadIDs;

	for(int i = 0; i < m_gnuTrans->m_UploadList.size(); i++)
		UploadIDs.push_back( m_gnuTrans->m_UploadList[i]->m_UploadID );

	return UploadIDs;
}

void CDnaUpload::RunFile(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		itUp->second->RunFile();
}

void CDnaUpload::Remove(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		m_gnuTrans->RemoveUpload(itUp->second);
}

CString CDnaUpload::GetErrorStr(LONG UploadID)
{
	

	CString strResult;

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		strResult = itUp->second->m_Error;

	return strResult;
}

LONG CDnaUpload::GetIndex(LONG UploadID)
{
	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->m_Index;

	return 0;
}

ULONG CDnaUpload::GetIP(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->m_Host.S_addr;

	return 0;
}

LONG CDnaUpload::GetPort(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->m_Port;

	return 0;
}

CString CDnaUpload::GetHandshake(LONG UploadID)
{
	

	CString strResult;

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		strResult = itUp->second->m_Handshake;

	return strResult;
}

LONG CDnaUpload::GetAttempts(LONG UploadID)
{
	

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return itUp->second->m_Attempts;

	return 0;
}

LONG CDnaUpload::GetQueuePos(LONG UploadID)
{
	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		return m_gnuTrans->m_UploadQueue.GetHostPos(itUp->second);

	return 0;
}

CString CDnaUpload::GetFilePath(LONG UploadID)
{
	CString strResult;

	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
		strResult = itUp->second->GetFilePath();

	return strResult;
}

void CDnaUpload::SendChallenge(LONG UploadID, LPCTSTR Challenge, LPCTSTR Answer )
{
	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end())
	{
		itUp->second->m_Challenge       = Challenge; 
		itUp->second->m_ChallengeAnswer = Answer;
	}
}

CString CDnaUpload::GetFileHash(LONG UploadID, LONG HashID)
{
	std::map<int, CGnuUploadShell*>::iterator itUp = m_gnuTrans->m_UploadMap.find(UploadID);

	if(itUp != m_gnuTrans->m_UploadMap.end() && HashID == HASH_SHA1)
		return itUp->second->m_Sha1Hash;
	
	return "";
}