/********************************************************************************

	GnucDNA - A Gnutella Library
    Copyright (C) 2000-2004 John Marshall

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

#include "StdAfx.h"
#include "Dime.h"

namespace gdna 
{


DIME::DIME(byte* pData, int length)
{
	m_pData  = pData;
	m_Length = length;

	m_pNextPos  = pData;
	m_BytesLeft = length;
}

DIME::ReadResult DIME::ReadNextRecord(DimeRecord &Record)
{
	Record = DimeRecord();

	if(m_BytesLeft < 12)
		return DIME::READ_INCOMPLETE;

	// byte 0
	if(*m_pNextPos >> 3 != 1) // version error
		return DIME::READ_ERROR;

	Record.First   = (*m_pNextPos >> 2) & 0x01;
	Record.Last    = (*m_pNextPos >> 1) & 0x01;
	Record.Chunked = *m_pNextPos & 0x01;

	m_pNextPos++;
	m_BytesLeft--;

	// byte 1
	Record.tType = *m_pNextPos >> 4;
	m_pNextPos++;
	m_BytesLeft--;

	// byte 2 - 3
	Record.OptionsLength = ( m_pNextPos[0] << 8 ) + m_pNextPos[1];
	m_pNextPos  += 2;
	m_BytesLeft -= 2;

	// byte 4 - 5
	Record.IDLength = ( m_pNextPos[0] << 8 ) + m_pNextPos[1];
	m_pNextPos  += 2;
	m_BytesLeft -= 2;

	// byte 6 - 7
	Record.TypeLength = ( m_pNextPos[0] << 8 ) + m_pNextPos[1];
	m_pNextPos  += 2;
	m_BytesLeft -= 2;

	// byte 8 - 11
	Record.DataLength = ( m_pNextPos[0] << 24 ) + ( m_pNextPos[1] << 16 ) + ( m_pNextPos[2] << 8 ) + m_pNextPos[3];
	m_pNextPos  += 4;
	m_BytesLeft -= 4;


	// Get options
	if(	ReadSetting(Record.Options, Record.OptionsLength) != DIME::READ_GOOD)
		return DIME::READ_INCOMPLETE;

	if(	ReadSetting(Record.ID, Record.IDLength) != DIME::READ_GOOD)
		return DIME::READ_INCOMPLETE;

	if(	ReadSetting(Record.Type, Record.TypeLength) != DIME::READ_GOOD)
		return DIME::READ_INCOMPLETE;


	// Get data
	if(Record.DataLength)
	{
		uint16 PadLength = Record.DataLength;
		while(PadLength % 4 != 0)
			PadLength++;

		if(m_BytesLeft < PadLength)
			return DIME::READ_INCOMPLETE;

		Record.Data = m_pNextPos;

		m_pNextPos  += PadLength;
		m_BytesLeft -= PadLength;
	}

	return DIME::READ_GOOD;
}

DIME::ReadResult DIME::ReadSetting(CString &Setting, int Length)
{
	if(Length)
	{
		int PadLength = Length;
		while(PadLength % 4 != 0)
			PadLength++;

		if(m_BytesLeft < PadLength)
			return DIME::READ_INCOMPLETE;

		Setting = CString((char*) m_pNextPos, Length);

		m_pNextPos  += PadLength;
		m_BytesLeft -= PadLength;
	}

	return DIME::READ_GOOD;
}

int DIME::WriteRecord(byte Flags, byte tType, CString ID, CString Type, const void* Data, int DataLength)
{
	int BytesWritten = 0;

	if(m_BytesLeft < 12)
		return 0;

	*m_pNextPos++ = Flags; // flags byte 0

	*m_pNextPos++ = tType; // tType byte 1

	*((uint16*) m_pNextPos) = 0; // options length byte 2 - 3
	m_pNextPos += 2;

	*((uint16*) m_pNextPos) = htons( ID.GetLength() );
	m_pNextPos += 2;

	*((uint16*) m_pNextPos) = htons( Type.GetLength() );
	m_pNextPos += 2;

	*((uint32*) m_pNextPos) = htonl( DataLength );
	m_pNextPos += 4;

	m_BytesLeft  -= 12;
	BytesWritten += 12;

	BytesWritten += WriteField(ID, ID.GetLength());
	BytesWritten += WriteField(Type, Type.GetLength());
	BytesWritten += WriteField(Data, DataLength);

	return BytesWritten;
}

int DIME::WriteField(const void* Data, int DataLength)
{
	int PadLength = DataLength;
	while(PadLength % 4 != 0)
		PadLength++;

	if(m_BytesLeft < PadLength)
		return 0;

	memset(m_pNextPos, 0, PadLength);
	memcpy(m_pNextPos, Data, DataLength);

	m_pNextPos  += PadLength;
	m_BytesLeft -= PadLength;

	return PadLength;
}

} // end dna namespace