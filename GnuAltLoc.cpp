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

#include "GnuPrefs.h"
#include "GnuShare.h"

#include "GnuAltLoc.h"


CGnuAltLoc::CGnuAltLoc(CGnuShare *pShare)
{
	m_pCore   = pShare->m_pCore;
	m_pShare  = pShare;
	m_pPrefs  = m_pCore->m_pPrefs; 
}

CGnuAltLoc::~CGnuAltLoc()
{

}

void CGnuAltLoc::AddAltLocation(CString& locStr, CString& Sha1Hash)
{
	// Need to tidy up the string first
	locStr.Replace("\r\n", " "); // Change EOL to space
	locStr.Replace("\t", " ");	 // Change TAB to space
	locStr.Replace("  "," ");	 // Change Double space to single space

	// Parse first altlocations by comma
	CString temp = ParseString(locStr);
	
	while (!temp.IsEmpty())
	{
		// we have something so try to convert to an AltLocation
		AltLocation AltLoc = temp;
		AltLoc.Sha1Hash = Sha1Hash;

		if( AltLoc.isValid() && m_pPrefs->AllowedIP(StrtoIP(AltLoc.HostPort.Host)) ) 
		{
			m_pShare->m_FilesAccess.Lock();
		
			for(int i = 0; i < m_pShare->m_SharedFiles.size(); i++)
				if(AltLoc.Sha1Hash == m_pShare->m_SharedFiles[i].HashValues[HASH_SHA1].c_str())
				{
					bool found = false;

					for(int j = 0; j < m_pShare->m_SharedFiles[i].AltHosts.size(); j++)
						if (AltLoc == m_pShare->m_SharedFiles[i].AltHosts[j])
							{
								found = true;
								break;
							}

					if(!found)
						m_pShare->m_SharedFiles[i].AltHosts.push_back(AltLoc);


					if(m_pShare->m_SharedFiles[i].AltHosts.size() > 15)
						m_pShare->m_SharedFiles[i].AltHosts.pop_front();
				}

			m_pShare->m_FilesAccess.Unlock();
		}

		temp = ParseString(locStr);
	}

}

// Return an Alternate Location Header to insert into handshake
// TODO: keep track of whats been returned and return only different
//       AltLocations to the same client
CString CGnuAltLoc::GetAltLocationHeader(CString Sha1Hash, CString Host, int HostCount)
{
	CString AltLocHeader;

	m_pShare->m_FilesAccess.Lock();
		
	for(int i = 0; i < m_pShare->m_SharedFiles.size(); i++)
		if(Sha1Hash == m_pShare->m_SharedFiles[i].HashValues[HASH_SHA1].c_str())
		{
			int j = 0;

			if(m_pShare->m_SharedFiles[i].AltHosts.size() < HostCount)
				HostCount = m_pShare->m_SharedFiles[i].AltHosts.size(); 

			std::vector<int> HostIndexes;

			// Get random indexes to send to host
			while(HostCount > 0)
			{
				HostCount--;

				int NewIndex = rand() % m_pShare->m_SharedFiles[i].AltHosts.size() + 0;

				if(Host == m_pShare->m_SharedFiles[i].AltHosts[NewIndex].HostPort.Host)
					continue;

				bool found = false;

				for(j = 0; j < HostIndexes.size(); j++)
					if(HostIndexes[j] == NewIndex)
						found = true;

				if(!found)
					HostIndexes.push_back(NewIndex);
			}

			for(j = 0; j < HostIndexes.size(); j++)
			{
				CString AltStr = m_pShare->m_SharedFiles[i].AltHosts[ HostIndexes[j] ].GetString();

				if(!AltStr.IsEmpty())
					AltLocHeader += "Alt-Location: " + AltStr + "\r\n";
			}

			break;
		}

	m_pShare->m_FilesAccess.Unlock();

	return AltLocHeader;
}

