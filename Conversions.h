#pragma once

#include "Packet.h"

namespace gdna 
{

// Functions
CString NumtoStr(int);		// Convert DWORD to a CString

CString IPtoStr(IP);			// Convert IP to CString
IP      StrtoIP(CString in);	// Convert CString to an IP

CTime	StrToCTime(CString& str);	// Converts a string representation to a CTime
CString CTimeToStr(CTime& time);	// reverse of above

CString EncodeBase16(byte* buffer, unsigned int bufLen);
void    DecodeBase16(const char *base16Buffer, unsigned int base16BufLen, byte *buffer, unsigned int bufLen);
int		EncodeLengthBase16(int rawLength);
int		DecodeLengthBase16(int base16Length);

CString EncodeBase32(const byte* buffer, unsigned int bufLen);
void    DecodeBase32(const char *base32Buffer, unsigned int base32BufLen, byte *buffer, unsigned int bufLen);
int		EncodeLengthBase32(int rawLength);
int		DecodeLengthBase32(int base32Length);

DWORD   GetSpeedinBytes(CString Speed);
CString GetSpeedString(DWORD dwSpeed);

CString HashIDtoTag(int HashID);
int     TagtoHashID(CString Tag);

uint16 GeotoWord(double Coord, bool Lat);
double WordtoGeo(uint16 word, bool Lat);

bool    ValidVendor(CString VendorID);
CString GetVendor(CString VendorID);

CString CommaIze(CString in); // Add commas to big numbers
CString InsertDecimal(double);
CString GetPercentage(DWORD , DWORD );
CString IncrementName(CString);

IPv4 AltLoctoAddress(CString AltLoc);

CString SockErrortoString(int ErrorCode);

int VersiontoInt(CString Version);

} // end gdna namespace

