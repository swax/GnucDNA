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
#include "Conversions.h"

// Get rid of ugly warnings
#pragma warning (disable : 4786)

namespace gdna
{

static byte base16Chars[17] = "0123456789ABCDEF";
static byte base32Chars[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

#define BASE16_LOOKUP_MAX 23
static byte base16Lookup[BASE16_LOOKUP_MAX][2] =
{
    { '0', 0x0 },
    { '1', 0x1 },
    { '2', 0x2 },
    { '3', 0x3 },
    { '4', 0x4 },
    { '5', 0x5 },
    { '6', 0x6 },
    { '7', 0x7 },
    { '8', 0x8 },
    { '9', 0x9 },
	{ ':', 0x9 },
    { ';', 0x9 },
    { '<', 0x9 },
    { '=', 0x9 },
    { '>', 0x9 },
    { '?', 0x9 },
    { '@', 0x9 },
    { 'A', 0xA },
    { 'B', 0xB },
    { 'C', 0xC },
    { 'D', 0xD },
    { 'E', 0xE },
    { 'F', 0xF }
};

#define BASE32_LOOKUP_MAX 43
static byte base32Lookup[BASE32_LOOKUP_MAX][2] =
{
    { '0', 0xFF },
    { '1', 0xFF },
    { '2', 0x1A },
    { '3', 0x1B },
    { '4', 0x1C },
    { '5', 0x1D },
    { '6', 0x1E },
    { '7', 0x1F },
    { '8', 0xFF },
    { '9', 0xFF },
    { ':', 0xFF },
    { ';', 0xFF },
    { '<', 0xFF },
    { '=', 0xFF },
    { '>', 0xFF },
    { '?', 0xFF },
    { '@', 0xFF },
    { 'A', 0x00 },
    { 'B', 0x01 },
    { 'C', 0x02 },
    { 'D', 0x03 },
    { 'E', 0x04 },
    { 'F', 0x05 },
    { 'G', 0x06 },
    { 'H', 0x07 },
    { 'I', 0x08 },
    { 'J', 0x09 },
    { 'K', 0x0A },
    { 'L', 0x0B },
    { 'M', 0x0C },
    { 'N', 0x0D },
    { 'O', 0x0E },
    { 'P', 0x0F },
    { 'Q', 0x10 },
    { 'R', 0x11 },
    { 'S', 0x12 },
    { 'T', 0x13 },
    { 'U', 0x14 },
    { 'V', 0x15 },
    { 'W', 0x16 },
    { 'X', 0x17 },
    { 'Y', 0x18 },
    { 'Z', 0x19 }
};

CString NumtoStr(int in)
{
	char buff[16];

	::sprintf (buff, "%d", in);

	return buff;
}

CString IPtoStr(IP in)
{
	char* buffer;
	buffer = inet_ntoa(*(in_addr *)&in);

	return buffer;
}

IP StrtoIP(CString in)
{
	IP out;

	out.S_addr = inet_addr(in);		// standard WinSock2 function

	return out;	
}



CString EncodeBase16(unsigned char* buffer, unsigned int bufLen)
{
	CString Base16Buff;

	for(int i = 0; i < bufLen; i++)
	{
		Base16Buff += base16Chars[buffer[i] >> 4];
		Base16Buff += base16Chars[buffer[i] & 0xf];
	}

    return Base16Buff;
}

void DecodeBase16(const char *base16Buffer, unsigned int base16BufLen, byte *buffer)
{
    memset(buffer, 0, DecodeLengthBase16(base16BufLen));
  
    for(int i = 0; i < base16BufLen; i++)
    {
		int lookup = toupper(base16Buffer[i]) - '0';

        // Check to make sure that the given word falls inside a valid range
		byte word = 0;
        
		if ( lookup < 0 || lookup >= BASE16_LOOKUP_MAX)
           word = 0xFF;
        else
           word = base16Lookup[lookup][1];

		if(i % 2 == 0)
		{
			buffer[i/2] = word << 4;
		} 
		else
		{
			buffer[(i-1)/2] |= word;
		}
	}
}

int EncodeLengthBase16(int rawLength)
{
    return rawLength * 2;
}

int	DecodeLengthBase16(int base16Length)
{
	return base16Length / 2;
}

CString EncodeBase32(const unsigned char* buffer, unsigned int bufLen)
{
	int bufflen = EncodeLengthBase32(bufLen);
	int buffpos = 0;
    
	char* encbuff = new char[bufflen];
	int   encsize = 0;
	
	unsigned int   i, index;
    unsigned char  word;

    for(i = 0, index = 0; i < bufLen;)
    {
        /* Is the current word going to span a byte boundary? */
        if (index > 3)
        {
            word = (buffer[i] & (0xFF >> index));
            index = (index + 5) % 8;
            word <<= index;
            if (i < bufLen - 1)
                word |= buffer[i + 1] >> (8 - index);

            i++;
        }
        else
        {
            word = (buffer[i] >> (8 - (index + 5))) & 0x1F;
            index = (index + 5) % 8;
            if (index == 0)
               i++;
        }

       // assert(word < 32);

		if(buffpos < bufflen)
		{
			encbuff[buffpos++] = (char) base32Chars[word];
			encsize++;
		}
		else
			ASSERT(0);
        
		//*(base32Buffer++) = (char) base32Chars[word];
    }

	CString encStr(encbuff, encsize);

	delete [] encbuff;

    return encStr;
}

void DecodeBase32(const char *base32Buffer, unsigned int base32BufLen, byte *buffer)
{
    int            i, index, max, lookup, offset;
    unsigned char  word;

    memset(buffer, 0, DecodeLengthBase32(base32BufLen));
    max = strlen(base32Buffer);
    for(i = 0, index = 0, offset = 0; i < max; i++)
    {
        lookup = toupper(base32Buffer[i]) - '0';
        /* Check to make sure that the given word falls inside
           a valid range */
        if ( lookup < 0 || lookup >= BASE32_LOOKUP_MAX)
           word = 0xFF;
        else
           word = base32Lookup[lookup][1];

        /* If this word is not in the table, ignore it */
        if (word == 0xFF)
           continue;

        if (index <= 3)
        {
            index = (index + 5) % 8;
            if (index == 0)
            {
               buffer[offset] |= word;
               offset++;
            }
            else
               buffer[offset] |= word << (8 - index);
        }
        else
        {
            index = (index + 5) % 8;
            buffer[offset] |= (word >> index);
            offset++;

            buffer[offset] |= word << (8 - index);
        }
    }
}

int EncodeLengthBase32(int rawLength)
{
    return ((rawLength * 8) / 5) + ((rawLength * 8 % 5) ? 1 : 0);
}

int DecodeLengthBase32(int base32Length)
{
   return ((base32Length * 5) / 8);
}

// Converts a string representation to a CTime
CTime StrToCTime(CString& str)
{
	CTime temp(0);
	
	if (str.GetLength() < 17 || str.GetLength() > 22) 
		return temp;

	// 0         1         2 
	// 0123456789012345678901
	// YYYY-MM-DDTHH:MM+HH:MM
	// or
	// YYYY-MM-DDTHH:MMZ

	if (str[4] != '-'           || str[7] != '-' || 
		tolowerex(str[10]) != 't' || str[13] != ':')
	{
		return temp;
	}

	int Year  = atoi(str.Mid(0,4));
	int Month = atoi(str.Mid(5,2));
	int Day   = atoi(str.Mid(8,2));
	int Hour  = atoi(str.Mid(11,2));
	int Min   = atoi(str.Mid(14,2));

	// Check for valid values
	CTime CurrentTime = CTime::GetCurrentTime();

	if(Year  < 1970 || Year  > CurrentTime.GetYear() ||
	   Month < 1    || Month > 12 ||
	   Day   < 1    || Day   > 31 )
	{
	   return temp;
	}

	temp = CTime(Year, Month, Day, Hour, Min, 0, 0);

	if (tolowerex(str[16]) == 'z')
	{
		return temp;
	}

	int tzh   = atoi(str.Mid(16,3));
	int tzm   = atoi(str.Mid(20,2));


	return temp;
}

// Convert Ctime to String
CString CTimeToStr(CTime& time)
{
	// The Format as per http://www.w3.org/TR/NOTE-datetime 
	// YYYY-MM-DDTHH:MMTZD
	CString TimeStr;

	TimeStr = time.Format("%Y-%m-%dT%H:%MZ");

	// CTime doesn't store time zone so have to convert all times to UTC
	//	TimeStr += GetTimeZoneStr();

	return TimeStr;
}

CString HashIDtoTag(int HashID)
{
	switch(HashID)
	{
	case HASH_SHA1:
		return "sha1:";
		break;
	case HASH_MD5:
		return "md5:";
		break;
	case HASH_MD4_ED2K:
		return "ed2k:";
		break;
	case HASH_TIGERTREE:
		return "tree:tiger/:";
		break;
	case HASH_BITPRINT:
		return "bitprint:";
		break;
	}

	return "";
}

int TagtoHashID(CString Tag)
{
	if(Tag.CompareNoCase("sha1:") == 0)
		return HASH_SHA1;

	if(Tag.CompareNoCase("md5:") == 0)
		return HASH_MD5;

	if(Tag.CompareNoCase("ed2k:") == 0)
		return HASH_MD4_ED2K;

	if(Tag.CompareNoCase("tree:tiger/:") == 0 || Tag.CompareNoCase("tree:tiger:") == 0)
		return HASH_TIGERTREE;

	if(Tag.CompareNoCase("bitprint:") == 0)
		return HASH_BITPRINT;

	return HASH_UNKNOWN;
}

uint16 GeotoWord(double Coord, bool Lat)
{
	double offset = 180;
	if(Lat)
		offset = 90;

	uint16 word = (uint16) (Coord + offset) * 65535.0f / (offset * 2);

	word = word < 0 ? 0 : ( word > 65535 ? 65535 : word );

	return word;
}

double WordtoGeo(uint16 word, bool Lat)
{
	double offset = 180;
	if(Lat)
		offset = 90;

	double geo =  ((double) word * (offset * 2) / 65535.0f) - offset;

	return geo;
}

CString CommaIze(CString in)
{
	if (in.GetLength() > 3)
		return CommaIze(in.Left(in.GetLength() - 3)) + "," + in.Right(3);
	else
		return in;
}

CString InsertDecimal(double dNumber)
{
	if(dNumber <= 0.00)
		return "0.00";

	int    decimal, sign;
	//char*  buffer;

	CString strNumber( _fcvt( dNumber, 2, &decimal, &sign));
	
	if(decimal == 0)
		strNumber.Insert(0, "0.");
	else if(decimal == -1)
		strNumber.Insert(0, "0.0");
	else if(decimal < -1)
		return "0.00";
	else
		strNumber.Insert(decimal, '.');

	return strNumber;
}

CString GetPercentage(DWORD dWhole, DWORD dPart)
{
	CString result = "0.00";

	if(dPart > dWhole)
		dPart = dWhole;

	if(dWhole)
	{
		result = NumtoStr(dPart * 10000 / dWhole);

		if(result.GetLength() > 2)
			result.Insert( result.GetLength() - 2, ".");
		else
		{
			switch(result.GetLength())
			{
			case 2:
				result.Insert(0, "0.");
				break;
			case 1:
				result.Insert(0, "0.0");
				break;
			default:
				result = "0.00";
				break;
			}
		}
	}

	return result + " %";
}

CString IncrementName(CString FileName)
{
	CString Front = FileName;
				
	int dotpos = FileName.ReverseFind('.');
	if(dotpos != -1)
		Front = FileName.Left(dotpos);
	
	int copy = 1;
	int spacepos = Front.ReverseFind(' ');

	if(spacepos != -1)
	{
		copy = atoi( Front.Right(Front.GetLength() - spacepos - 1));
		
		if(copy == 0)
			Front += " 1";
		else
		{
			copy++;
			Front = Front.Left(spacepos) + " " + NumtoStr(copy);
		}
	}
	else
		Front += " 1";

	CString NewFile = Front;
	
	if(dotpos != -1)
		Front += FileName.Right( FileName.GetLength() - dotpos);

	return Front;
}

DWORD GetSpeedinBytes(CString Speed)
// The protocol is messed, bytes are bits 
{
	if(Speed == "Cellular Modem")
		return 0;
	if(Speed == "14.4 Modem")
		return 14;
	if(Speed == "28.8 Modem")
		return 28;
	if(Speed == "56K Modem")
		return 53;
	if(Speed == "ISDN")
		return 128;
	if(Speed == "Cable")
		return 384;
	if(Speed == "DSL")
		return 768;
	if(Speed == "T1")
		return 1500;
	if(Speed == "T3 (or Greater)")
		return 45000;
	
	return atol(Speed);
}

CString GetSpeedString(DWORD dwSpeed)
// The protocol is messed, bytes are bits 
{
	if(dwSpeed >= 45000)
		return "T3 (or Greater)";
	if(dwSpeed >= 1500)
		return "T1";
	if(dwSpeed >= 768)
		return "DSL";
	if(dwSpeed >= 384)
		return "Cable";
	if(dwSpeed >= 128)
		return "ISDN";
	if(dwSpeed >= 53)
		return "56K Modem";
	if(dwSpeed >= 28)
		return "28.8 Modem";
	if(dwSpeed >= 14)
		return "14.4 Modem";
	else
		return "Cellular Modem";
}

bool ValidVendor(CString VendorID)
{
	if(GetVendor(VendorID) == "")
		return false;
	
	return true;
}

CString GetVendor(CString VendorID)
{
	VendorID.MakeUpper();

	if(VendorID == "ARES") return "Ares	";
	
	if(VendorID == "BEAR") return "BearShare";

	if(VendorID == "CULT") return "Cultiv8r";
	
	if(VendorID == "GNOT") return "Gnotella";
	
	if(VendorID == "GNUC") return "Gnucleus";

	if(VendorID == "GNUT") return "Gnut";

	if(VendorID == "GTKG") return "Gtk-Gnutella";

	if(VendorID == "HSLG") return "Hagelslag";
	
	if(VendorID == "LIME") return "LimeWire";
	
	if(VendorID == "MACT") return "Mactella";
	
	if(VendorID == "MNAP") return "MyNapster";

	if(VendorID == "MMMM") return "Morpheus v2";

	if(VendorID == "MRPH") return "Morpheus";
	
	if(VendorID == "NAPS") return "NapShare";

	if(VendorID == "OCFG") return "OCFolders";

	if(VendorID == "QTEL") return "Qtella";

	if(VendorID == "RAZA") return "Shareaza";

	if(VendorID == "SNUT") return "SwapNut";

	if(VendorID == "TOAD") return "ToadNode";

	if(VendorID == "XOLO") return "Xolox";

	return VendorID;
}

} // end gdna namespace
