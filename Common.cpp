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
#include "Common.h"

DWORD AssignThreadToCPU(CWinThread *pThread, DWORD cpuNumber)
{
	DWORD dwErr = 0;
	
	// Legal thread pointer is needed ;) and Only Windows NT/2000/XP support multiple CPU
	if(pThread != NULL && GetVersion() < 0x80000000)			 
	{
	
		// Do the simple way if checking the number of CPUs: Get it from the environment ;)
		long lNrOfCpus = 0;
		char *pEnvData = getenv("NUMBER_OF_PROCESSORS");
	
		
		if(pEnvData != NULL) 
			lNrOfCpus = atoi(pEnvData);

		if(lNrOfCpus == 0) 
			lNrOfCpus = 1;


		// If only one Cpu, then forget it ;)
		// Otherwise assign the affinity (Note: We assume there are max 4 Cpus
		if (lNrOfCpus > 1) 
		{
			DWORD dwProcAffinityMask = cpuNumber;
	
			if (!SetThreadAffinityMask(pThread->m_hThread, dwProcAffinityMask)) 
			{
				// Failure will be returned as errorcode ;)
				dwErr = GetLastError();
			}
		}
	}

	return dwErr;
}

void GnuStartThread(CWinThread* &pTarget, AFX_THREADPROC pWorkFunc, void* pCaller)
{
	pTarget = AfxBeginThread(pWorkFunc, pCaller, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	pTarget->m_bAutoDelete = false;
	AssignThreadToCPU(pTarget, CPU_0); 
	pTarget->ResumeThread();
}

void GnuEndThread(CWinThread* &pTarget)
{
	if(pTarget)
	{
		WaitForSingleObject(pTarget->m_hThread, INFINITE);

		delete pTarget;
		pTarget = NULL;
	}
}

void GnuCreateGuid(GUID *pGuid)
{
	memset(pGuid, 0, sizeof(GUID));
	CoCreateGuid(pGuid);

	//byte* bGuid = (byte*) pGuid;
	//bGuid[8]  = 0xFF;
	//bGuid[15] = 0x00;
}

void SetRandColor(COLORREF &Color)
{
	int red   = rand() % 255 + 0;
	int green = rand() % 255 + 0;
	int blue  = rand() % 255 + 0;
	
	Color = RGB(red, green, blue);
}

CString	GetFileError(CFileException* error)
{
	if(!error)
		return "File Error Unknown";

	switch( error->m_cause )
	{

	case CFileException::none:
        return "Error Not Detected";
		break;
	case CFileException::generic:
        return "Generic Error";
		break;
	case CFileException::fileNotFound:
        return "File Not Found";
		break;
	case CFileException::badPath:
        return "Bad Path";
		break;
	case CFileException::tooManyOpenFiles:
        return "Too Many Open Files";
		break;
	case CFileException::accessDenied:
        return "Access Denied";
		break;
	case CFileException::invalidFile:
        return "Invalid File";
		break;
	case CFileException::removeCurrentDir:
        return "Cannot Remove Directory";
		break;
	case CFileException::directoryFull:
        return "Directory Full";
		break;
	case CFileException::badSeek:
        return "Error Seeking";
		break;
	case CFileException::hardIO:
        return "Hardware Error";
		break;
	case CFileException::sharingViolation:
        return "Sharing Violation";
		break;
	case CFileException::lockViolation:
        return "Lock Violation";
		break;
	case CFileException::diskFull:
        return "Disk Drive Full";
		break;
	case CFileException::endOfFile:
        return "End of File";
		break;
	}

	return "File Error Unknown";
}

// Return the first string before the delim char
// and remove the string from the main string
// Repeat calls to return all items until empty string is returned
CString ParseString( CString &Str, char delim /* = ',' */)
{
	CString RetStr;

	if (!Str.IsEmpty())
	{
		int delimpos = Str.Find(delim);
        if (delimpos == -1)
        {
			RetStr = Str;
			Str = "";
		}
        else
        {
            RetStr = Str.Left(delimpos);
            Str = Str.Mid(delimpos + 1);
        }
    }
   
    return RetStr;
}

//
// Build an AltLocation from a string
//
AltLocation& AltLocation::operator=(CString& str)
{
	Clear();

	str.TrimLeft();

	str.Remove('\"');

	CString temp, newStr = str;
	newStr.MakeLower();

	// Strip the http: bit
	temp = ParseString(newStr, '/');
	if (temp != "http:")
		return *this;

	// Strip the second '/'
	temp = ParseString(newStr, '/');

	// Next should be the "host:Port" section
	HostPort = ParseString(newStr, '/');
	if (HostPort.Host.IsEmpty())
		return *this;

	temp = ParseString(newStr, '/');
	if (temp == "get")
	{
		// this is a /get/<index>/<FileName> string
		Index = atol(ParseString(newStr,'/'));
		Name  = ParseString(newStr, ' ');
		Name.Replace("%20", " ");
	}
	else if (temp == "uri-res")
	{
		// this is a /uri-res/N2R?urn:sha1:<hash> string
		temp = ParseString(newStr, ':');
		temp = ParseString(newStr, ':');
		Sha1Hash = ParseString(newStr, ' ');
	}

	if (!newStr.IsEmpty())
	{
		// we still have stuff left hopefully it is a timestamp
		HostPort.LastSeen = StrToCTime(newStr);
	}

	return *this;
}

//
// Build an String from an AltLocation
//
CString AltLocation::GetString()
{
	// Start with the address
	CString str;

	if (HostPort.Host.IsEmpty() || (Name.IsEmpty() && Sha1Hash.IsEmpty()))
	{
		// Invalid Altlocation
		return "";
	}
		
	if(Sha1Hash.IsEmpty())
	{
		str = "http://" + HostPort.GetString() + "/get/" + NumtoStr(Index) + "/" + Name;
		str.Replace(" ", "%20");
	}
	else
		str = "http://" + HostPort.GetString() + "/uri-res/N2R?urn:sha1:" + Sha1Hash;


	// Timestamp
	str += " " + CTimeToStr(HostPort.LastSeen);

	return str;
}

CString DecodeURL(CString URL)
{
	//Decode URL. Replace all %XY with 0xXY
	//For example "%20" --> " "
	CString tmp;
	int LowByte;
	int HiByte;

	for (int c = 0; c < URL.GetLength() - 2; c++)
	{
		if (URL[c] == '%')
		{
			HiByte = URL[c+1];
			LowByte = URL[c+2];
			if (isxdigit(HiByte) && isxdigit(LowByte))
			{
				if ('0' <= HiByte && HiByte <= '9') {
					HiByte -= '0';
				} else
				if ('a' <= HiByte && HiByte <= 'f') {
					HiByte -= ('a'-10);
				} else
				if ('A' <= HiByte && HiByte <= 'F') {
					HiByte -= ('A'-10);
				}

				if ('0' <= LowByte && LowByte <= '9') {
					LowByte -= '0';
				} else
				if ('a' <= LowByte && LowByte <= 'f') {
					LowByte -= ('a'-10);
				} else
				if ('A' <= LowByte && LowByte <= 'F') {
					LowByte -= ('A'-10);
				}
				char NewChar = (16 * HiByte) + LowByte;
				tmp = NewChar;
				URL.Replace(URL.Mid(c, 3), tmp);
			}
		}
	}
	return URL;
}


CString GetTimeZoneStr(void)
{
	CString TZStr;

	int TZHours = abs(_timezone) / (60 * 60);
	int TZMins = ((abs(_timezone) / 60) - (TZHours * 60));

	TZStr.Format("%c%2.2d:%2.2d", (_timezone < 0 ? '-' : '+'), TZHours, TZMins);

	return TZStr;
}

CTimeSpan LocalTimeZone()
{
	int TZHours = _timezone / (60 * 60);
	int TZMins = ((abs(_timezone) / 60) - (abs(TZHours) * 60));

	return CTimeSpan(0, TZHours, TZMins, 0);
}


/*char tolowerex(char letter)
{
	if(65 <= letter && letter <= 90)
		return letter + 32;
	
	return letter;
}*/

int memfind(byte* mem, int length, byte value)
{
	ASSERT(length);

	for(int i = 0; i < length; i++)
		if( mem[i] == value)
			return i;

	return -1;
}

uint32 HashGuid(GUID &Guid)
{
	byte* pGuidRaw = (byte*) &Guid;

	// XOR every 4 bytes together ...
	uint32 hash =  (pGuidRaw[0] ^ pGuidRaw[4] ^ pGuidRaw[8] ^ pGuidRaw[12]) +
				   256 * (pGuidRaw[1] ^ pGuidRaw[5] ^ pGuidRaw[9] ^ pGuidRaw[13]) +
				   256 * 256 * (pGuidRaw[2] ^ pGuidRaw[6] ^ pGuidRaw[10] ^ pGuidRaw[14]) +
				   256 * 256 * 256 * (pGuidRaw[3] ^ pGuidRaw[7] ^ pGuidRaw[11] ^ pGuidRaw[15]);

	return hash;
}

CString HexDump(byte* buffer, int length)
{
	CString dump;

	for(int i = 0; i < length; i++)
		dump += EncodeBase16(&buffer[i], 1) + " ";

	return dump;
}

bool IsPrivateIP(IP Address)
{
	uint32 nboAddr = htonl(Address.S_addr);

	// 10.0.0.0 - A class A network. Can have over 16 million computers on this network. 
	if(  htonl(StrtoIP("10.0.0.0").S_addr) <= nboAddr && nboAddr <= htonl(StrtoIP("10.255.255.255").S_addr) )
		return true;

	// 172.16.0.0 through 172.31.0.0 - 16 class B networks. Can have over 64 thousand computers on each network
	if(  htonl(StrtoIP("172.16.0.0").S_addr) <= nboAddr && nboAddr <= htonl(StrtoIP("172.31.255.255").S_addr) )
		return true;

	// 192.168.0.0 through 192.168.255.0 - 256 class C networks. Can have over 250 computers on each network
	if(  htonl(StrtoIP("192.168.0.0").S_addr) <= nboAddr && nboAddr <= htonl(StrtoIP("192.168.255.255").S_addr) )
		return true;


	return false;
}