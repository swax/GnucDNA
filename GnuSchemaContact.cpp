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

#include "StdAfx.h"
#include "GnuSchemaContact.h"

REGISTER_SCHEMA_CLASS(CGnuSchemaContact, "contact.xsd")

CGnuSchemaContact::CGnuSchemaContact(void)
{
}

CGnuSchemaContact::~CGnuSchemaContact(void)
{
}

void CGnuSchemaContact::LoadData(SharedFile &File)
{
	// Load generic external meta file
	CGnuSchema::LoadData(File);
	
	// Read audio file for specific metadata
	LoadContact(&File);
}

void CGnuSchemaContact::SaveData(SharedFile &File)
{
	// Save generic external meta file
	CGnuSchema::SaveData(File);

	// Write audio file
	SaveContact(&File);

	// Rehash
	for(int i = 0; i < HASH_TYPES; i++)
		File.HashValues[i] = "";

	File.HashError = false;
}

void CGnuSchemaContact::LoadContact(SharedFile *pFile)
{
	CStdioFile ContactFile;

	// Load nodes from file cache
	if (ContactFile.Open(pFile->Dir.c_str(), CFile::modeRead))
	{
		CString NextLine;

		while (ContactFile.ReadString(NextLine))
		{
			ParseLine(pFile, NextLine);
		}

		ContactFile.Abort();
	}

}

void CGnuSchemaContact::ParseLine(SharedFile *pFile, CString &Line)
{
	CString Property;
	CString Params;
	CString Value;

	int ColonPos = Line.Find(":");

	// Seperate property and value
	if(ColonPos != -1)
	{
		Property = Line.Left(ColonPos);
		Value    = Line.Mid(ColonPos + 1);

		
		// Seperate property and parameters
		int SemiPos = Property.Find(";");

		if(SemiPos != -1)
		{
			Params   = Property.Mid(SemiPos + 1);
			Property = Property.Left(SemiPos);
		}


		// Take out grouping
		int DotPos = Property.Find(".");

		if(DotPos != -1)
			Property = Property.Mid(DotPos + 1);

		
		

		Property.MakeUpper();
		Params.MakeUpper();


		//if(Property.Compare("N") == 0)
		//{
		//	// last; first; middle; prefix; suffix
		//	
		//	int pos = 0;
		//	while(!Value.IsEmpty())
		//	{
		//		pos++;

		//		CString SubValue = ParseString(Value, ';');

		//		if(!SubValue.IsEmpty())
		//			switch(pos)
		//			{
		//			case 1:
		//				LoadFileAttribute(pFile, "name-last", SubValue);
		//				break;
		//			case 2:
		//				LoadFileAttribute(pFile, "name-first", SubValue);
		//				break;
		//			case 3:
		//				LoadFileAttribute(pFile, "name-middle", SubValue);
		//				break;
		//			case 4:
		//				LoadFileAttribute(pFile, "name-prefix", SubValue);
		//				break;
		//			}
		//	}

		//}

		Value.Replace("=0D=0A", " ");
		Value.Replace(";", ", ");


		// Handle properties
		if(Property.Compare("FN") == 0)
			LoadFileAttribute(pFile, "Name", Value);

		if(Property.Compare("NICKNAME") == 0)
			LoadFileAttribute(pFile, "Nick", Value);

		if(Property.Compare("BDAY") == 0)
			LoadFileAttribute(pFile, "BDay", Value);

		if(Property.Compare("EMAIL") == 0)
			LoadFileAttribute(pFile, "EMail", Value);

		if(Property.Compare("TITLE") == 0)
			LoadFileAttribute(pFile, "title", Value);

		if(Property.Compare("ROLE") == 0)
			LoadFileAttribute(pFile, "Role", Value);

		if(Property.Compare("ORG") == 0)
			LoadFileAttribute(pFile, "Company", Value);

		if(Property.Compare("NOTE") == 0)
			LoadFileAttribute(pFile, "Note", Value);

		if(Property.Compare("URL") == 0)
			LoadFileAttribute(pFile, "url", Value);

		if(Property.Compare("TZ") == 0)
			LoadFileAttribute(pFile, "tz", Value);

		if(Property.Compare("GEO") == 0)
			LoadFileAttribute(pFile, "geo", Value);


		if(Property.Compare("ADR") == 0)
		{
			//if(Params.Find("HOME") != -1)
			//	LoadFileAttribute(pFile, "adr-home", Value);

			if(Params.Find("WORK") != -1)
				LoadFileAttribute(pFile, "address", Value);
		}

		if(Property.Compare("TEL") == 0)
		{
			//if(Params.Find("HOME") != -1)
			//	LoadFileAttribute(pFile, "tel-home", Value);

			if(Params.Find("WORK") != -1)
				LoadFileAttribute(pFile, "telephone", Value);
		}

		
	}


}

void CGnuSchemaContact::SaveContact(SharedFile *pFile)
{
	// Saving is a little tricky, let the user use an external writer
}


