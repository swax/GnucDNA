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
#include "Headers.h"


CParsedHeaders::CParsedHeaders(CString RawHeaderBlock)
{
	m_Warning = 0;

	
	//Allow header continuations
	RawHeaderBlock.Replace("\r\n\t", " ");
	RawHeaderBlock.Replace("\r\n ", " ");
	
	
	//Make sure RawHeaderBlock ends with ONE "\r\n"
	RawHeaderBlock.TrimRight("\r\n");
	RawHeaderBlock += "\r\n";
	
	
	Header  NewHeader;

	CString Line;
	CString Name;
	CString Value;

	int delimpos = 0;
	int colonpos = 0;
	int commapos = 0;

	while (!RawHeaderBlock.IsEmpty())
	{
		//Get next line
		delimpos = RawHeaderBlock.Find("\r\n");
		Line = RawHeaderBlock.Left(delimpos);
		RawHeaderBlock = RawHeaderBlock.Mid(delimpos + 2);

		//Split Line into Name and Value
		colonpos = Line.Find(":");
		if (colonpos != -1)
		{
			Name = Line.Left(colonpos);
			Name.TrimLeft();
			Name.TrimRight();

			if (!Name.IsEmpty())
			{
				Line = Line.Mid(colonpos + 1);

				while (!Line.IsEmpty())
				{
					commapos = Line.Find(",");

					if (Line.Left(6) == "bytes ")	//A header "Name: bytes 0-10,20-30" is just one header.
						commapos = -1;

					if (commapos != -1)
					{
						//Trick to avoid splitting on timestamps like Thu, 27 Jun 2002 20:22:26 GMT work
						CString PossibleDay = Line.Mid(commapos - 4, 4);
						if (PossibleDay == " Sun" || PossibleDay == " Mon" || PossibleDay == " Tue" || PossibleDay == " Wed" || PossibleDay == " Thu" || PossibleDay == " Fri" || PossibleDay == " Sat")
						{
							//Skip to next comma
							commapos = Line.Find(",", commapos + 1);
						}

						//Dont split inside quotes ("")
						int FirstQuote = Line.Find('\"');
						if (FirstQuote != -1 && FirstQuote < commapos)
						{
							//There is a " before the comma. Check where the quoted string ends
							int SecondQuote = Line.Find('\"', FirstQuote + 1);
							if (SecondQuote > commapos)
							{
								//Comma is inside quotes
								commapos = Line.Find(",", SecondQuote + 1);
							}
						}
					}

					//If comma is still there
					if (commapos != -1)	
					{
						Value = Line.Left(commapos);
						Line = Line.Mid(commapos+1);
					}
					else
					{
						Value = Line;
						Line = "";
					}
					
					Value.TrimLeft();
					Value.TrimRight();
					//Value.TrimRight(','); // Remove the comma from value

					if (!Value.IsEmpty())
					{
						NewHeader.Name = Name;
						NewHeader.Value = Value;
						m_Headers.push_back(NewHeader);
					}
					else 
						m_Warning = 1;	//No value
				}
			}
			else 
				m_Warning = 1;	//No header name
		}
		else
		{
			//Line has no colon
			m_Warning = 1;
			//Continue with next line anyway
		}
    }
};


CString CParsedHeaders::FindHeader (CString HeaderName)
{
	HeaderName.MakeLower();
	CString tmp;

	for (int i = 0; i < m_Headers.size(); i++)
	{
		tmp = m_Headers[i].Name;
		tmp.MakeLower();

		if (tmp == HeaderName)
		{
			return m_Headers[i].Value;
		}
	}
	return "";	//Not found
}

bool ValidHttpHeader(CString Header, CString Version, int Code, CString Reason)
{
	return false;
}

CString LimitHeadersLength( CString Headers )
{
	if (Headers.GetLength() > 4000)	//4000 - \r\n and some margin
	{
		Headers = Headers.Left(4000);
		Headers = Headers.Left(Headers.ReverseFind('\n') + 1);	//Remove last line
	}
	return Headers;
}
