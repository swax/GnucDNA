/*
 *  CFileLock
 *	Performs thread safe file access
 *  Makslane Araujo Rodrigues
 *	makslane@hotmail.com
 *	jun/2002
 */

#pragma once

#define IGNORE_ID3 true

#include <afxmt.h>

// Pre-definitions
class CAutoLock;
class CFileLock;


// Definitions
class CAutoLock
{
public:
	CAutoLock(CCriticalSection *pCriticalSection)
	{
		this->pCriticalSection = pCriticalSection;
		pCriticalSection->Lock();
	}

	~CAutoLock()
	{
		pCriticalSection->Unlock();
	}

	CCriticalSection *pCriticalSection;
};

class CFileLock : public CFile  
{
public:
	void Unlock();
	void Lock();
	
	CFileLock();
	virtual ~CFileLock();

	ULONGLONG GetPosition();
	BOOL GetStatus(CFileStatus& rStatus);
	CString GetFileName();
	CString GetFileTitle();
	CString GetFilePath();
	void SetFilePath(LPCTSTR lpszNewName);

// Operations
	BOOL Open(LPCTSTR lpszFileName, UINT nOpenFlags, bool IgnoreID3 = false, CFileException* pError = NULL );

	
	ULONGLONG SeekToEnd();
	void SeekToBegin();

	// backward compatible ReadHuge and WriteHuge
	// DWORD ReadHuge(void* lpBuffer, DWORD dwCount);
	// void WriteHuge(const void* lpBuffer, DWORD dwCount);

// Overridables
	CFile* Duplicate();

	ULONGLONG Seek(LONGLONG lOff, UINT nFrom);
	void SetLength(ULONGLONG dwNewLen);
	ULONGLONG GetLength();

	UINT Read(void* lpBuf, UINT nCount);
	void Write(const void* lpBuf, UINT nCount);

	void LockRange(ULONGLONG dwPos, ULONGLONG dwCount);
	void UnlockRange(ULONGLONG dwPos, ULONGLONG dwCount);

	void Abort();
	void Flush();
	void Close();

	bool  m_IgnoreID3;
	ULONGLONG m_RealLength;
	ULONGLONG m_IgnoreBegin;
	ULONGLONG m_IgnoreLength;
	ULONGLONG m_IgnoreEnd;

	CCriticalSection m_CriticalSection;
};

#pragma pack (push, 1)
struct ID3v2
{
	byte tag[3];
	byte version[2];
	byte flags;
	byte size[4];

	ID3v2()
	{
		memset(tag, 0, 3);
		memset(version, 0, 2);
		flags = 0;
		memset(size, 0, 4);
	};
};
#pragma pack (pop)