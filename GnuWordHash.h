#pragma once

#include "GnuShare.h"

#define GNU_TABLE_BITS	16
#define GNU_TABLE_SIZE  (1 << GNU_TABLE_BITS) / 8

#define TABLE_INFINITY   2

#define G2_TABLE_BITS	20
#define G2_TABLE_SIZE   (1 << G2_TABLE_BITS) / 8


class CGnuShare;
class CGnuNode;
class CGnuCore;

struct WordData
{
	std::basic_string<char> Text;

	std::vector<UINT> Indexes;
};

struct WordKey
{
	std::vector<WordData>* LocalKey;  // Locally indexed	

	WordKey() 
	{ 
		LocalKey  = NULL; 
	};

	~WordKey() 
	{ 
		if(LocalKey)
		{
			delete LocalKey;
			LocalKey = NULL;
		}
	};
};


class CGnuWordHash  
{
public:
	CGnuWordHash(CGnuShare* pShare);
	virtual ~CGnuWordHash();

	void ClearLocalTable();
	
	void InsertString(std::basic_string<char> Name, int Index, bool BreakString=true, std::basic_string<char> MetaTag="");

	void BreakupName(std::basic_string<char> Name, std::vector< std::basic_string<char> > &Keywords);
	void BreakupMeta(CString &QueryEx, std::vector< std::basic_string<char> > &Keywords);

	void AddWord(std::vector< std::basic_string<char> > &Keywords, std::basic_string<char> Word);

	void LookupQuery(GnuQuery &FileQuery, std::list<UINT> &Indexes, std::list<int> &RemoteNodes);

	bool IntersectIndexes(std::list<UINT> &Index, std::vector<UINT> &CompIndex);
	
	UINT Hash(std::basic_string<char> x, byte bits);
	

	WordKey m_HashTable[1 << GNU_TABLE_BITS];

	UINT m_HashedWords;
	UINT m_LargestRehash;
	UINT m_UniqueSlots;

	// Gnutella
	byte m_GnutellaHitTable[GNU_TABLE_SIZE];
	uint32 m_GnuEntries;

	// G2
	byte m_G2HitTable[G2_TABLE_SIZE];
	uint32 m_G2Entries;

	CGnuShare*  m_pShare;
	CGnuCore*	m_pCore;

	CCriticalSection m_TableLock;
};

