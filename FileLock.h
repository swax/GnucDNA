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

	uint64 GetPosition();
	BOOL GetStatus(CFileStatus& rStatus);
	CString GetFileName();
	CString GetFileTitle();
	CString GetFilePath();
	void SetFilePath(LPCTSTR lpszNewName);

// Operations
	BOOL Open(LPCTSTR lpszFileName, UINT nOpenFlags, bool IgnoreID3 = false, CFileException* pError = NULL );

	
	uint64 SeekToEnd();
	void SeekToBegin();

	// backward compatible ReadHuge and WriteHuge
	// DWORD ReadHuge(void* lpBuffer, DWORD dwCount);
	// void WriteHuge(const void* lpBuffer, DWORD dwCount);

// Overridables
	CFile* Duplicate();

	uint64 CFileLock::Seek(int64 lOff, uint32 nFrom);
	void SetLength(uint64 dwNewLen);
	uint64 GetLength();

	UINT Read(void* lpBuf, UINT nCount);
	UINT SeekandRead(int64 lOff, void* lpBuf, UINT nCount);

	void Write(const void* lpBuf, UINT nCount);
	void SeekandWrite(int64 lOff, const void* lpBuf, UINT nCount);

	void LockRange(uint64 dwPos, uint64 dwCount);
	void UnlockRange(uint64 dwPos, uint64 dwCount);

	void Abort();
	void Flush();
	void Close();

	static uint64 CFileLock::ScanFileSize(CString FilePath);

	bool  m_IgnoreID3;
	uint64 m_RealLength;
	uint64 m_IgnoreBegin;
	uint64 m_IgnoreLength;
	uint64 m_IgnoreEnd;

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