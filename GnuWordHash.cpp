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
#include "GnuCore.h"

#include "GnuNetworks.h"
#include "GnuControl.h"
#include "GnuNode.h"
#include "G2Control.h"
#include "G2Node.h"

#include "GnuShare.h"
#include "GnuWordHash.h"


#define A_INT 0x4F1BBCDC;


CGnuWordHash::CGnuWordHash(CGnuShare* pShare)
{
	m_pCore   = pShare->m_pCore;
	m_pShare  = pShare;

	memset(m_GnutellaHitTable, 0xFF, GNU_TABLE_SIZE);
	m_GnuEntries = 0;

	memset(m_G2HitTable, 0xFF, G2_TABLE_SIZE);
	m_G2Entries = 0;
	
	
	m_HashedWords   = 0;
	m_LargestRehash = 0;
	m_UniqueSlots	= 0;	
}

CGnuWordHash::~CGnuWordHash()
{
	ClearLocalTable();
}

void CGnuWordHash::ClearLocalTable()
{
	m_TableLock.Lock();

	for(int i = 0; i < 1 << GNU_TABLE_BITS; i++)
		if(m_HashTable[i].LocalKey)
		{
			delete m_HashTable[i].LocalKey;
			m_HashTable[i].LocalKey = NULL;
		}

	m_TableLock.Unlock();

	m_HashedWords   = 0;
	m_LargestRehash = 0;
	m_UniqueSlots	= 0;
	
	// Gnutella
	memset(m_GnutellaHitTable, 0xFF, GNU_TABLE_SIZE);

	// G2
	memset(m_G2HitTable, 0xFF, G2_TABLE_SIZE);
}

void CGnuWordHash::InsertString(std::basic_string<char> Name, int Index, bool BreakString, std::basic_string<char> MetaTag)
{
	// Breakup file name into keywords
	std::vector< std::basic_string<char> > Keywords;


	// Make sure not a hash value
	if(BreakString)
		BreakupName(Name, Keywords);
	else
		Keywords.push_back(Name);


	// Hash keywords and put the hash in the hash table, and a table in the shared file
	for(int i = 0; i < Keywords.size(); i++)
	{
		std::basic_string<char> CurrentWord = Keywords[i];
		
		if(!MetaTag.empty())
		{
			CurrentWord.insert(0, "=");
			CurrentWord.insert(0, MetaTag);
		}
		
		// Make lower
		for(int j = 0; j < CurrentWord.size(); j++)
			CurrentWord[j] = tolowerex(CurrentWord[j]);

		int WordHash = Hash(CurrentWord, GNU_TABLE_BITS);

		// Add word to hash table
		bool AddWord = true;

		// Check if word is already in table
		if(m_HashTable[WordHash].LocalKey)
			for(j = 0; j < m_HashTable[WordHash].LocalKey->size(); j++)
				if(m_HashTable[WordHash].LocalKey->at(j).Text.compare(CurrentWord) == 0)
				{	
					bool IndexFound = false;
					
					for(int k = 0; k < m_HashTable[WordHash].LocalKey->at(j).Indexes.size(); k++)
						if(m_HashTable[WordHash].LocalKey->at(j).Indexes[k] == Index)
						{
							IndexFound = true;
							break;
						}

					if(!IndexFound)
					{
						m_HashTable[WordHash].LocalKey->at(j).Indexes.push_back(Index);

						m_pShare->m_SharedFiles[Index].Keywords.push_back( CurrentWord );
						m_pShare->m_SharedFiles[Index].HashIndexes.push_back( WordHash );
					}
					
					AddWord = false;
				}

		// If not add word to table
		if(AddWord)
		{
			WordData NewWord;
			NewWord.Text = CurrentWord;
			NewWord.Indexes.push_back(Index);

			m_pShare->m_SharedFiles[Index].Keywords.push_back( CurrentWord );
			m_pShare->m_SharedFiles[Index].HashIndexes.push_back( WordHash );

			m_TableLock.Lock();

			if(m_HashTable[WordHash].LocalKey == NULL)
			{
				m_HashTable[WordHash].LocalKey = new std::vector<WordData>;

				m_UniqueSlots++;
			}

			m_HashTable[WordHash].LocalKey->push_back(NewWord);

			m_TableLock.Unlock();

			m_HashedWords++;

			if(m_HashTable[WordHash].LocalKey->size() > m_LargestRehash)
				m_LargestRehash = m_HashTable[WordHash].LocalKey->size();
			
		}

		// Gnutella Local Hit Table
		WordHash = Hash(CurrentWord, GNU_TABLE_BITS);

		int nByte = ( WordHash >> 3 ); 
		int nBit  = ( WordHash & 7 ); 

		m_GnutellaHitTable[ nByte ] &= ~( 1 << nBit ); // Set to 0 (full)
		m_GnuEntries++;

		// G2 Local Hit Table
		WordHash = Hash(CurrentWord, G2_TABLE_BITS);

		nByte = ( WordHash >> 3 ); 
		nBit  = ( WordHash & 7 ); 

		m_G2HitTable[ nByte ] &= ~( 1 << nBit ); // Set to 0 (full)
		m_G2Entries++;
	}
}

void CGnuWordHash::BreakupName(std::basic_string<char> Name, std::vector< std::basic_string<char> > &Keywords)
{
	// all non-alphanumric characters 0 - 127 are treated as spaces, except for apostraphe
	// 0 - 9 -> ASCII 48 - 57
	// A - Z -> ASCII 65 - 90
	// a - z -> ASCII 97 - 122
	// ' -> ASCII 96
	

	// Break Query into individual words
	std::basic_string<char> BuildWord = "";
	//char LastChar = 0;

	for(int i = 0; i < Name.size(); i++)
	{
		// Check for end of filename
		if(Name[i] == '\0')
		{
			if(BuildWord.size() > 1)
			{
				AddWord(Keywords, BuildWord);
				BuildWord = "";
				//LastChar  = 0;
			}

			break;
		}	

		// Special characters
		else if(Name[i] == '\'')
		{
			if(BuildWord.size() > 1)
				AddWord(Keywords, BuildWord);
		}

		// Break characters
		else if(  Name[i] < 48 || 
			     (Name[i] > 57 && Name[i] < 65) || 
				 (Name[i] > 90 && Name[i] < 97) || 
				  Name[i] > 122)
		{
			if(BuildWord.size() > 1)
				AddWord(Keywords, BuildWord);

			BuildWord = "";
			//LastChar  = 0;
		}

		// Add character
		else
		{
			// Break up alpha and numeric part of filename
			/*if(((LastChar >= 95 && LastChar <= 90) || (LastChar >= 97 && LastChar <= 122)) && 
				(Name[i] >= 48 && Name[i] <= 57))
			{
				if(BuildWord.size() > 2)
					AddWord(Keywords, BuildWord);
			}*/


			BuildWord += tolowerex(Name[i]);
			//LastChar  =  tolower(Name[i]);
		}
	}

	if(BuildWord.size() > 1)
		AddWord(Keywords, BuildWord);


}
  
void CGnuWordHash::AddWord(std::vector< std::basic_string<char> > &Keywords, std::basic_string<char> Word)
{
	bool Add = true;

	for(int i = 0; i < Keywords.size(); i++)
		if(Keywords[i].compare(Word) == 0)
			Add = false;

	if(Add)
		Keywords.push_back(Word);
}


void CGnuWordHash::LookupQuery(GnuQuery &FileQuery, std::list<UINT> &Indexes, std::list<int> &RemoteNodes)
{
	// Break Query into individual words
	std::vector< std::basic_string<char> > Keywords;

	int i, j, k;

	for( i = 0; i < FileQuery.Terms.size(); i++ )
	{
		// Make sure all terms in lower case so compare() with words in table match
		CString Term = FileQuery.Terms[i];
		Term.MakeLower();

		if( Term.Left(4) == "urn:")
		{
			if(Term.GetLength() > 4)
				Keywords.push_back( (LPCTSTR) Term ) ;
		}
		else if( Term.Left(1) == "<" )
			BreakupMeta( Term, Keywords);

		else
			BreakupName( (LPCTSTR) Term, Keywords);
	}

	//ASSERT( Keywords.size() );
	if( Keywords.size() == 0 )
		return;

	// Go through words and match them with indexes in the table
	bool LocalMatch  = true;
	bool RemoteMatch = true;

	for(i = 0; i < Keywords.size(); i++)
	{
		UINT WordHash = Hash(Keywords[i], GNU_TABLE_BITS);


		// Intersect with local files to get results
		if(LocalMatch)
		{
			LocalMatch = false;

			m_TableLock.Lock();

			// See if keyword is in hash table
			if(m_HashTable[WordHash].LocalKey)
				for(j = 0; j < m_HashTable[WordHash].LocalKey->size(); j++)
					if(m_HashTable[WordHash].LocalKey->at(j).Text.compare(Keywords[i]) == 0)
					{
						LocalMatch = true;
						break;
					}

			if(LocalMatch)
			{
				// Intersect indexes in hash table with current results
				if(Indexes.size())
					LocalMatch = IntersectIndexes(Indexes, m_HashTable[WordHash].LocalKey->at(j).Indexes);
				else
				{
					for(k = 0; k < m_HashTable[WordHash].LocalKey->at(j).Indexes.size(); k++)
						Indexes.push_back(m_HashTable[WordHash].LocalKey->at(j).Indexes[k]);

					LocalMatch = true;
				}
			}

			m_TableLock.Unlock();
		
			if(!LocalMatch)
				Indexes.clear();
		}
	}

	// Intersect remote nodes (children) for results
	if(FileQuery.Network == NETWORK_GNUTELLA && FileQuery.Forward && m_pCore->m_pNet->m_pGnu)
	{
		CGnuControl* pGnuComm = m_pCore->m_pNet->m_pGnu;


		// For each Gnutella node run through key hashes for matches
		pGnuComm->m_NodeAccess.Lock();

			std::vector<CGnuNode*>::iterator itNode;
			for(itNode = pGnuComm->m_NodeList.begin(); itNode != pGnuComm->m_NodeList.end(); itNode++)
			{
				bool Match = true;

				for(i = 0; i < Keywords.size(); i++)
				{
					UINT WordHash = Hash(Keywords[i], GNU_TABLE_BITS);

					int nByte = ( WordHash >> 3 ); 
					int nBit  = ( WordHash & 7  ); 

					if( ( ~(*itNode)->m_RemoteHitTable[nByte] & (1 << nBit) ) == 0)
					{
						Match = false;
						break;
					}
				}

				if( Match )
					RemoteNodes.push_back( (*itNode)->m_NodeID );
			}
	
		pGnuComm->m_NodeAccess.Unlock();
	}


	if(FileQuery.Network == NETWORK_G2 && FileQuery.Forward && m_pCore->m_pNet->m_pG2)
	{
		CG2Control* pG2Comm = m_pCore->m_pNet->m_pG2;


		// For each G2 node run through key hashes for matches
		pG2Comm->m_G2NodeAccess.Lock();

			std::vector<CG2Node*>::iterator itNode;
			for(itNode = pG2Comm->m_G2NodeList.begin(); itNode != pG2Comm->m_G2NodeList.end(); itNode++)
			{
				bool Match = true;

				for(i = 0; i < Keywords.size(); i++)
				{
					UINT WordHash = Hash(Keywords[i], G2_TABLE_BITS);

					int nByte = ( WordHash >> 3 ); 
					int nBit  = ( WordHash & 7  ); 

					if( ( ~(*itNode)->m_RemoteHitTable[nByte] & (1 << nBit) ) == 0)
					{
						Match = false;
						break;
					}
				}

				if( Match )
					RemoteNodes.push_back( (*itNode)->m_G2NodeID );
			}
	
		pG2Comm->m_G2NodeAccess.Unlock();
	}
}

bool CGnuWordHash::IntersectIndexes(std::list<UINT> &Index, std::vector<UINT> &CompIndex)
{
	bool Match = false;

	std::list<UINT>::iterator itIndex;

	itIndex = Index.begin();
	while( itIndex != Index.end() )
	{
		Match = false;

		for(int i = 0; i < CompIndex.size(); i++)
			if(*itIndex == CompIndex[i])
				Match = true;

		if(!Match)
			itIndex = Index.erase(itIndex);
		else
			itIndex++;
	}
	
	if(Index.empty())
		return false;

	return true;
}

void CGnuWordHash::BreakupMeta(CString &QueryEx, std::vector< std::basic_string<char> > &Keywords)
{
	// Get rid of <?xml version='1.0'?>
	int TrashPos = QueryEx.Find("?>");
	if(TrashPos != -1)
		QueryEx = QueryEx.Mid(TrashPos + 2);

	// Get rid of xsi:nonamespaceschemalocation
	TrashPos = QueryEx.Find("xsi:");
	if(TrashPos != -1)
	{
		TrashPos = QueryEx.Find(">", TrashPos);
		if(TrashPos != -1)
			QueryEx = QueryEx.Mid(TrashPos + 1);
	}


	int SpacePos = QueryEx.Find(" ");
	int TagPos   = -1;

	if(SpacePos != -1)
		TagPos = QueryEx.Left(SpacePos).ReverseFind('<');
	
	if(TagPos != -1)
	{
		QueryEx = QueryEx.Mid(TagPos + 1);
		QueryEx.Replace("\"", "'");
		QueryEx += " ";

		// Get meta name
		SpacePos = QueryEx.Find(" ");

		if(SpacePos != -1)
		{
			CString MetaName = QueryEx.Left(SpacePos);
			
			if(!MetaName.IsEmpty())
				Keywords.push_back( (LPCTSTR) MetaName );
			else
				return;


			// Get attributes
			int EqualPos = QueryEx.Find("=");

			while(EqualPos != -1 && SpacePos != -1 && SpacePos < EqualPos)
			{
				CString AttributeName = QueryEx.Mid(SpacePos + 1, EqualPos - SpacePos - 1);
				
				int FrontQuotePos = QueryEx.Find("'", EqualPos);
				int BackQuotePos  = QueryEx.Find("'", FrontQuotePos + 1);

				if( !AttributeName.IsEmpty() && FrontQuotePos < BackQuotePos)
				{
					CString AttributeValue = QueryEx.Mid(FrontQuotePos + 1, BackQuotePos - FrontQuotePos - 1);

					// Break up any value into keywords
					std::vector< std::basic_string<char> > KeyValues;
					BreakupName( (LPCTSTR) AttributeValue, KeyValues);

					for(int i = 0; i < KeyValues.size(); i++)
						Keywords.push_back( KeyValues[i].c_str() );
						//Keywords.push_back( (LPCTSTR) (MetaName + "." + AttributeName + "=" + KeyValues[i].c_str()) ) ;
				}
				else
					break;



				SpacePos = QueryEx.Find(" ", BackQuotePos);
				EqualPos = QueryEx.Find("=", BackQuotePos);
			}
		}

	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Code from LimeWire QRP standard of hashing keywords for tables
 
/**
 * Returns the same value as hash(x.substring(start, end), bits),
 * but tries to avoid allocations.  Note that x is lower-cased
 * when hashing.
 *
 * @param x the string to hash
 * @param bits the number of bits to use in the resulting answer
 * @param start the start offset of the substring to hash
 * @param end just PAST the end of the substring to hash
 * @return the hash value 
 */   

UINT CGnuWordHash::Hash(std::basic_string<char> x, byte bits)
{
	int start = 0;
	int end   = x.length();


    //1. First turn x[start...end-1] into a number by treating all 4-byte
    //chunks as a little-endian quadword, and XOR'ing the result together.
    //We pad x with zeroes as needed. 
    //    To avoid having do deal with special cases, we do this by XOR'ing
    //a rolling value one byte at a time, taking advantage of the fact that
    //x XOR 0==x.

    UINT64 xor = 0;  //the running total
    UINT64 j   = 0;  //the byte position in xor.  INVARIANT: j == (i - start) % 4
	UINT64 b   = 0;
    for (int i = start; i < end; i++) 
	{
        b   = tolowerex(x[i]) & 0xFF; 
        b   = b << (j * 8);
        xor = xor ^ b;
        j   = (j + 1) % 4;
    }

    //2. Now map number to range 0 - (2^bits-1).
	/**
	* Returns the n-<b>bit</b> hash of x, where n="bits".  That is, the
	* returned value value can fit in "bits" unsigned bits, and is
	* between 0 and (2^bits)-1.
	*/
	//Multiplication-based hash function.  See Chapter 12.3.2. of CLR.
	j = xor * A_INT; // prod
    b = j << 32;     // ret
    b = b >> (32 + (32 - bits)); // >>> ?

    return (UINT) b;
}

