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
#include "GnuCore.h"

#include "GnuNode.h"
#include "GnuRouting.h"


CGnuRouting::CGnuRouting()
{
	m_nHashEntries = 0;

	m_nCurrent     = 0;
	m_nOld         = 1;

	RefreshTime  = CTime::GetCurrentTime();
	HashTimeSpan = RefreshTime - RefreshTime;
}

CGnuRouting::~CGnuRouting()
{
	m_AccessTable.Lock();

		for(int i = 0; i < TABLE_SIZE; i++)
		{
			while(m_Table[0][i].size())
				m_Table[0][i].clear();

			while(m_Table[1][i].size())
				m_Table[1][i].clear();
		}

	m_AccessTable.Unlock();
}


void CGnuRouting::Insert(GUID &Guid, int NodeID)
{
	DWORD key = CreateKey(Guid);

	m_AccessTable.Lock();

		// Check to see if table is full
		if(m_Table[m_nCurrent][key].size() >= MAX_REHASH || m_nHashEntries >= TABLE_SIZE * 4)  
		{
			// Time to clean out the old and switch
			ClearTable(m_nOld);

			int temp   = m_nCurrent;
			m_nCurrent = m_nOld;
			m_nOld     = temp;
		}
		
		key_Value new_val;
		new_val.Guid     = Guid;
		new_val.OriginID = NodeID;


		m_Table[m_nCurrent][key].push_back(new_val);

		m_nHashEntries++;

		//if(m_nHashEntries % 50 == 0)
		//	TRACE0( "Hash " + NumtoStr(m_nHashEntries) + "\n");

	m_AccessTable.Unlock();
}


int CGnuRouting::FindValue(GUID &Guid)
{
	DWORD key = CreateKey(Guid);

	m_AccessTable.Lock();

		int nAttempts = m_Table[m_nCurrent][key].size();  

		// Search the current table
		while(nAttempts--)
		{
			if(CompareGuid(m_Table[m_nCurrent][key][nAttempts].Guid, Guid))
			{
				m_AccessTable.Unlock();
				return m_Table[m_nCurrent][key][nAttempts].OriginID;
			}
		}

		// Try the old table
		nAttempts = m_Table[m_nOld][key].size();  
		while(nAttempts--)
		{
			if(CompareGuid(m_Table[m_nOld][key][nAttempts].Guid, Guid))
			{
				m_AccessTable.Unlock();
				return m_Table[m_nOld][key][nAttempts].OriginID;
			}
		}

	m_AccessTable.Unlock();

	// not found ...
	return -1;
}


DWORD CGnuRouting::CreateKey(GUID &Guid)
{
	DWORD key =  HashGuid(Guid);

	// Modulo it down to size
	key %= TABLE_SIZE;

	return key;
}


bool  CGnuRouting::CompareGuid(GUID &Guid1, GUID &Guid2)
{
	if(memcmp(&Guid1, &Guid2, 16) == 0)
		return true;

	return false;
}


// Secure (the function that calls this is locked)
void  CGnuRouting::ClearTable(int nWhich)
{
	for(int i = 0; i < TABLE_SIZE; i++)
		if(m_Table[nWhich][i].size())
			m_Table[nWhich][i].clear();


	HashTimeSpan = CTime::GetCurrentTime() - RefreshTime;
	RefreshTime  = CTime::GetCurrentTime();

	m_nHashEntries = 0;
}