#pragma once


#define TABLE_SIZE			1300	// Size of each hash table
#define MAX_REHASH			12		// How many rehashes we try before we consider the current
									// table too full, flush the old table, and swap tables.

class CGnuRouting;


struct key_Value
{
	GUID Guid;
	int  OriginID;
};


class CGnuRouting  
{
public:
	CGnuRouting();
	virtual ~CGnuRouting();

	void Insert(GUID &, int);
	int  FindValue(GUID &);

	int m_nHashEntries;

	CTime	  RefreshTime;
	CTimeSpan HashTimeSpan;


private:
	DWORD CreateKey(GUID &);
	bool  CompareGuid(GUID &, GUID &);
	void  ClearTable(int which);
	
	std::vector<key_Value> m_Table[2][TABLE_SIZE];

	CCriticalSection m_AccessTable;

	int m_nCurrent, 
		m_nOld;
};
