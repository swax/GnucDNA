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


#include "stdafx.h"
#include "FileLock.h"


CFileLock::CFileLock()
{
	m_RealLength	 = 0;
	m_IgnoreID3		 = false;
	m_IgnoreBegin    = 0;
	m_IgnoreLength   = 0;
	m_IgnoreEnd      = 0;
}

CFileLock::~CFileLock()
{

}

uint64 CFileLock::GetPosition()
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		uint64 pos = CFile::GetPosition() - m_IgnoreBegin;
		ASSERT(0 <= pos && pos < m_IgnoreLength);
		return pos;
	}

	return CFile::GetPosition();
}

BOOL CFileLock::GetStatus(CFileStatus& rStatus)
{
	CAutoLock lock(&m_CriticalSection);

	return CFile::GetStatus(rStatus);
}

CString CFileLock::GetFileName()
{
	CAutoLock lock(&m_CriticalSection);

	return CFile::GetFileName();
}

CString CFileLock::GetFileTitle()
{
	CAutoLock lock(&m_CriticalSection);

	return CFile::GetFileTitle();
}

CString CFileLock::GetFilePath()
{
	CAutoLock lock(&m_CriticalSection);

	return CFile::GetFilePath();
}

void CFileLock::SetFilePath(LPCTSTR lpszNewName)
{
	CAutoLock lock(&m_CriticalSection);

	CFile::SetFilePath(lpszNewName);
}

BOOL CFileLock::Open(LPCTSTR lpszFileName, UINT nOpenFlags, bool IgnoreID3, CFileException* pError )
{
	// Re-Init vars
	m_RealLength	 = 0;
	m_IgnoreID3		 = false;
	m_IgnoreBegin    = 0;
	m_IgnoreLength   = 0;
	m_IgnoreEnd      = 0;

	CAutoLock lock(&m_CriticalSection);

	BOOL FileOpened = CFile::Open(lpszFileName, nOpenFlags, pError);

	if(IGNORE_ID3 && IgnoreID3 && FileOpened && CString(lpszFileName).Right(4).CompareNoCase(".mp3") == 0)
	{
		m_RealLength   = GetLength();
		m_IgnoreLength = m_RealLength;
		m_IgnoreEnd    = m_RealLength;

		// Check for back id3v1 tag
		if(m_RealLength > 128)
		{
			Seek(-128, CFile::end);
			
			byte tag[3];
			Read(tag, 3);

			if( memcmp(tag, "TAG", 3) == 0)
			{
				m_IgnoreEnd    = m_RealLength - 128;
				m_IgnoreLength = m_IgnoreEnd;
				m_IgnoreID3    = true;
			}
		}

		// Check for front id3v2 tag
		if(m_RealLength > 10)
		{
			ID3v2 v2header;

			SeekToBegin();
			Read(&v2header, 10);

			if( memcmp(v2header.tag, "ID3", 3) == 0 && v2header.version[0] <= 3)
			{
				m_IgnoreBegin  = 10 + (v2header.size[0] << 21 | v2header.size[1] << 14 | v2header.size[2] << 7 | v2header.size[3]);
				m_IgnoreLength -= m_IgnoreBegin;
				m_IgnoreID3 = true;
			}
		}

		SeekToBegin();
	}

	return FileOpened;
}



uint64 CFileLock::SeekToEnd()
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		return CFile::Seek(m_IgnoreEnd, CFile::begin);
	}

	return CFile::SeekToEnd();
}

void CFileLock::SeekToBegin()
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		CFile::Seek(m_IgnoreBegin, CFile::begin);
		return;
	}

	CFile::SeekToBegin();
}


/*DWORD CFileLock::ReadHuge(void* lpBuffer, DWORD dwCount)
{
	CAutoLock lock(&m_CriticalSection);

	return CFile::ReadHuge(lpBuffer, dwCount);
}

void CFileLock::WriteHuge(const void* lpBuffer, DWORD dwCount)
{
	CAutoLock lock(&m_CriticalSection);

	CFile::WriteHuge(lpBuffer, dwCount);
}*/


CFile* CFileLock::Duplicate()
{
	CAutoLock lock(&m_CriticalSection);

	return CFile::Duplicate();
}

uint64 CFileLock::Seek(int64 lOff, uint32 nFrom)
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		switch(nFrom)
		{
		case CFile::begin:
			return CFile::Seek(m_IgnoreBegin + lOff, CFile::begin);
			break;
		case CFile::current:
			// Handled outside switch
			break;
		case CFile::end:
			return CFile::Seek(m_IgnoreEnd + lOff, CFile::begin); // lOff should be negetive
			break;
		}
	}

	return CFile::Seek(lOff, nFrom);
}

void CFileLock::SetLength(uint64 dwNewLen)
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		ASSERT(0);
	}

	CFile::SetLength(dwNewLen);
}

uint64 CFileLock::GetLength()
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		return m_IgnoreLength;
	}

	return CFile::GetLength();
}

UINT CFileLock::Read(void* lpBuf, UINT nCount)
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		uint64 pos = CFile::GetPosition();

		if(pos + nCount > m_IgnoreEnd)
			nCount = m_IgnoreEnd - pos;
	}

	return CFile::Read(lpBuf, nCount);
}

UINT CFileLock::SeekandRead(int64 lOff, void* lpBuf, UINT nCount)
{
	CAutoLock lock(&m_CriticalSection);

	CFile::Seek(lOff, CFile::begin);
	
	if(m_IgnoreID3)
	{
		uint64 pos = CFile::GetPosition();

		if(pos + nCount > m_IgnoreEnd)
			nCount = m_IgnoreEnd - pos;
	}

	return CFile::Read(lpBuf, nCount);
}

void CFileLock::Write(const void* lpBuf, UINT nCount)
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		ASSERT(0);
	}

	CFile::Write(lpBuf, nCount);
}

void CFileLock::SeekandWrite(int64 lOff, const void* lpBuf, UINT nCount)
{
	CAutoLock lock(&m_CriticalSection);

	CFile::Seek(lOff, CFile::begin);
	CFile::Write(lpBuf, nCount);
}

void CFileLock::LockRange(uint64 dwPos, uint64 dwCount)
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		CFile::LockRange(m_IgnoreBegin + dwPos, dwCount);
		return;
	}

	CFile::LockRange(dwPos, dwCount);
}

void CFileLock::UnlockRange(uint64 dwPos, uint64 dwCount)
{
	CAutoLock lock(&m_CriticalSection);

	if(m_IgnoreID3)
	{
		CFile::UnlockRange(m_IgnoreBegin + dwPos, dwCount);
		return;
	}

	CFile::UnlockRange(dwPos, dwCount);
}

void CFileLock::Abort()
{
	CAutoLock lock(&m_CriticalSection);

	CFile::Abort();
}

void CFileLock::Flush()
{
	CAutoLock lock(&m_CriticalSection);

	CFile::Flush();
}

void CFileLock::Close()
{
	CAutoLock lock(&m_CriticalSection);

	CFile::Close();
}

void CFileLock::Lock()
{
	m_CriticalSection.Lock();
}

void CFileLock::Unlock()
{
	m_CriticalSection.Unlock();
}

uint64 CFileLock::ScanFileSize(CString FilePath)
{
	CFileLock scanFile;
	
	uint64 FileSize = 0;
	if( scanFile.Open(FilePath, CFile::modeRead, true) )
	{
		FileSize = scanFile.GetLength();
		scanFile.Close();
	}

	return FileSize;
}

 