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
	license your contribution.

	For support, questions, commercial use, etc...
	E-Mail: swabby@c0re.net

********************************************************************************/

#include "StdAfx.h"
#include "GnuSchemaAudio.h"

REGISTER_SCHEMA_CLASS(CGnuSchemaAudio, "audio.xsd")

CGnuSchemaAudio::CGnuSchemaAudio(void)
{
	SetAttributeReadOnly("BitRate");
	SetAttributeReadOnly("Seconds");
	SetAttributeReadOnly("SampleRate");
}

CGnuSchemaAudio::~CGnuSchemaAudio(void)
{
}

void CGnuSchemaAudio::LoadData(SharedFile &File)
{
	// Load generic external meta file
	CGnuSchema::LoadData(File);
	
	// Read audio file for specific metadata
	LoadID3v1(&File);
}

void CGnuSchemaAudio::SaveData(SharedFile &File)
{
	// Save generic external meta file
	CGnuSchema::SaveData(File);

	// Write audio file
	SaveID3v1(&File);

	// Rehash
	for(int i = 0; i < HASH_TYPES; i++)
		File.HashValues[i] = "";

	File.HashError = false;
}

bool CGnuSchemaAudio::LoadID3v1(SharedFile *pFile)
{
	Clear(); // Make sure all data is blank

	
	HANDLE hFile = NULL;
	if ((hFile = CreateFile(pFile->Dir.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
	{
		int AttributeID = 0;
		int nNextSearch = 0;

		MP3FRAMEHEADER sFrameHeader;
		memset(&sFrameHeader, 0, sizeof(sFrameHeader));

		int		nFrameBR = 0;
		double	dLength  = 0;	// total length of file
		ULONG	nTotalBR = 0;	// total frames bit rate (used to calc. average)
		DWORD	dwNumBytesRead;

		int loop = 0;

		// read frame by frame, only do once, takes to long to find other info
		//while (GetNextFrameHeader(hFile, &sFrameHeader, nNextSearch))
		if (GetNextFrameHeader(hFile, &sFrameHeader, nNextSearch))
		{
			loop++;

			if (m_nFrames < 1)
			{
				// first read the MPEG version
				switch (sFrameHeader.mpegver)
				{
					case 0: m_enMPEGVersion = MPEGVER_25; break;
					case 1: m_enMPEGVersion = MPEGVER_NA; break;
					case 2: m_enMPEGVersion = MPEGVER_2;  break;
					case 3: m_enMPEGVersion = MPEGVER_1;  break;
				}

				// next, read the MPEG layer description
				switch (sFrameHeader.mpeglayer)
				{
					case 0: m_nMPEGLayer = 0; break;
					case 1: m_nMPEGLayer = 3; break;
					case 2: m_nMPEGLayer = 2; break;
					case 3: m_nMPEGLayer = 1; break;
				}

				// read the bit for CRC or no CRC
				m_bHasCRC = sFrameHeader.hascrc;
			}

			// read the bitrate, based on the mpeg layer and version
			if (m_nMPEGLayer > 0)
			{
				if (m_enMPEGVersion == MPEGVER_1)
				{
					switch (m_nMPEGLayer)
					{
						case 1: nFrameBR = g_nMP3BitRate[0][sFrameHeader.bitrate]; break;
						case 2: nFrameBR = g_nMP3BitRate[1][sFrameHeader.bitrate]; break;
						case 3: nFrameBR = g_nMP3BitRate[2][sFrameHeader.bitrate]; break;
					}
				}
				else
				{
					switch (m_nMPEGLayer)
					{
						case 1: nFrameBR = g_nMP3BitRate[3][sFrameHeader.bitrate]; break;
						case 2: nFrameBR = g_nMP3BitRate[4][sFrameHeader.bitrate]; break;
						case 3: nFrameBR = g_nMP3BitRate[5][sFrameHeader.bitrate]; break;
					}
				}
			}

			// if nFrameBR is 0 or -1 then the bitrate is either free or bad
			if (nFrameBR > 0)
				nTotalBR += nFrameBR;

			// read sample rate
			int nSampleRate = 0;

			if (m_enMPEGVersion == MPEGVER_1)
				switch (sFrameHeader.samplerate)
				{
					case 0: nSampleRate = 44100; break;
					case 1: nSampleRate = 48000; break;
					case 2: nSampleRate = 32000; break;
				}
			else if (m_enMPEGVersion == MPEGVER_2)
				switch (sFrameHeader.samplerate)
				{
					case 0: nSampleRate = 22050; break;
					case 1: nSampleRate = 24000; break;
					case 2: nSampleRate = 16000; break;
				}
			else if (m_enMPEGVersion == MPEGVER_25)
				switch (sFrameHeader.samplerate)
				{
					case 0: nSampleRate = 11025; break;
					case 1: nSampleRate = 12000; break;
					case 2: nSampleRate = 8000;  break;
				}


			// Set SampleRate
			if (nSampleRate)
				LoadFileAttribute(pFile, "SampleRate", NumtoStr(nSampleRate));


			// read channel mode
			switch (sFrameHeader.chanmode)
			{
				case 0: m_enChannelMode = MP3CM_STEREO; break;
				case 1: m_enChannelMode = MP3CM_JOINT_STEREO; break;
				case 2: m_enChannelMode = MP3CM_DUAL_CHANNEL; break;
				case 3: m_enChannelMode = MP3CM_SINGLE_CHANNEL; break;
			}

			// read the copyright and original bits
			m_bCopyrighted = sFrameHeader.copyright;
			m_bOriginal    = sFrameHeader.original;

			// read the emphasis
			switch (sFrameHeader.emphasis)
			{
				case 0: m_enEmphasis = MP3EM_NONE; break;
				case 1: m_enEmphasis = MP3EM_50_15_MS; break;
				case 2: m_enEmphasis = MP3EM_RESERVED; break;
				case 3: m_enEmphasis = MP3EM_CCIT_J17; break;
			}

			// don't read the CRC -- maybe in a future version

			// nNextSearch = frame length, in bytes
			if(nFrameBR > 0)
			{
				if (m_nMPEGLayer == 1)
					nNextSearch = (12000 * nFrameBR / nSampleRate + sFrameHeader.padding) * 4;
				else
					nNextSearch = 144000 * nFrameBR / nSampleRate + sFrameHeader.padding;
			}

			nNextSearch -= 4; // the frame header was already read

			m_nFrames++;

			// calculate the length in seconds of this frame and add it to total
			if (nFrameBR)
				dLength += (double)(nNextSearch + 4) * 8 / (nFrameBR * 1000);
		}


		// if at least one frame was read, the MP3 is considered valid
		if (m_nFrames > 0)
		{
			m_dwValidity |= MP3VLD_DATA_VALID;
			
			// Set BitRate
			int nBitRate = nTotalBR / m_nFrames; // average the bitrate
			LoadFileAttribute(pFile, "BitRate", NumtoStr(nBitRate));
			
			// Set Seconds
			//int nLength	 = (int)dLength;
			//LoadFileAttribute(pFile, "Seconds", NumtoStr(nLength));
		}
		
		// read the ID3 tag
		
			
		// set up a structure for reading the ID3 version 1 tag
		MP3ID3V1TAG sID3V1;

		if (SetFilePointer(hFile, -128, NULL, FILE_END) != INVALID_SET_FILE_POINTER &&
			ReadFile(hFile, &sID3V1, 128, &dwNumBytesRead, NULL) &&
			dwNumBytesRead == 128 && memcmp(sID3V1.ident, "TAG", 3) == 0)
		{
			char strTemp[31]; strTemp[30] = 0; // make a temporary null-terminated buffer

			// Set Title
			memcpy(strTemp, sID3V1.title,  30);;
			LoadFileAttribute(pFile, "Title", strTemp);

			// Set Artist
			memcpy(strTemp, sID3V1.artist, 30);
			LoadFileAttribute(pFile, "Artist", strTemp);

			// Set Album
			memcpy(strTemp, sID3V1.album,  30);
			LoadFileAttribute(pFile, "Album", strTemp);

			// Set Year
			strTemp[4] = 0; memcpy(strTemp, sID3V1.year, 4);
			LoadFileAttribute(pFile, "Year", strTemp);


			// now, depending on the reserved byte, the comment is 28 bytes or 30 bytes
			if (sID3V1.reserved)
			{
				// NOTE: even if sID3V1.album is of size 28, the reserved and tracknum
				// are right after, so they'll be included in the comment, which is what
				// we want

				// Set Comments
				memcpy(strTemp, sID3V1.comment, 30);
				LoadFileAttribute(pFile, "Comments", strTemp);
			}
			else
			{
				// Set Comments
				memcpy(strTemp, sID3V1.comment, 28);
				LoadFileAttribute(pFile, "Comments", strTemp);

				// Set Track
				int nTrack = sID3V1.tracknum;
				LoadFileAttribute(pFile, "Track", NumtoStr(nTrack));
				
				// Set Genre
				int nGenre = sID3V1.genre;
				if(nGenre < g_nMP3GenreCount)
					LoadFileAttribute(pFile, "Genre", g_arrMP3Genre[nGenre]);
			}

			m_dwValidity |= MP3VLD_ID3V1_VALID;
		}

		CloseHandle(hFile);
	}
	else
		return false;

	return true;
}

// helper functions
bool CGnuSchemaAudio::GetNextFrameHeader(HANDLE hFile, MP3FRAMEHEADER* pHeader, int nPassBytes)
{
	memset(pHeader, 0, sizeof(*pHeader));
	if (nPassBytes > 0)
		SetFilePointer(hFile, nPassBytes, NULL, FILE_CURRENT);

	int   n = 0;
	bool  bReadOK;
	DWORD dwNumBytesRead;
	do
	{
		bReadOK = ReadFile(hFile, pHeader, 4, &dwNumBytesRead, NULL);
		ChangeEndian(pHeader, 4); // convert from big-endian to little-endian

		// only search in 10kb
		if (!bReadOK || dwNumBytesRead != 4 ||
			pHeader->framesync == 0x7FF || ++n > 10000)
			break;

		SetFilePointer(hFile, -3, NULL, FILE_CURRENT);
	}
	while (1);

	return (	((pHeader->framesync  & 0x7FF) == 0x7FF) &&
				((pHeader->mpegver    & 0x003) != 0x001) &&		
				((pHeader->mpeglayer  & 0x003) != 0x000) &&		
				((pHeader->bitrate    & 0x00F) != 0x000) &&		
				((pHeader->bitrate    & 0x00F) != 0x00F) &&		
				((pHeader->samplerate & 0x003) != 0x003) &&		
				((pHeader->emphasis   & 0x003) != 0x002) 	);	//Added some more tests, 
																//especially mpegver is import since it can cause
																//a division by zero (nSampleRate=0)
}

bool CGnuSchemaAudio::SaveID3v1(SharedFile *pFile)
{
	HANDLE hFile = NULL;
	if ((hFile = CreateFile(pFile->Dir.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL)) != INVALID_HANDLE_VALUE)
	{
		// write the ID3
		MP3ID3V1TAG sID3V1;
		DWORD dwNumBytesReadWritten;

		// first check if the ID3V1 exists in the file
		if (SetFilePointer(hFile, -128, NULL, FILE_END) != INVALID_SET_FILE_POINTER &&
			ReadFile(hFile, &sID3V1, 128, & dwNumBytesReadWritten, NULL) &&
			dwNumBytesReadWritten == 128 && memcmp(sID3V1.ident, "TAG",3) == 0)
			SetFilePointer(hFile, -128, NULL, FILE_END); // tag exists, overwrite it
		else
			SetFilePointer(hFile, 0, NULL, FILE_END);  // tag doesnt exists

		// write the ID3 tag (or clear it)
		CString strTitle  = GetFileAttribute(pFile, "Title");
		CString strArtist = GetFileAttribute(pFile, "Artist");
		CString strAlbum  = GetFileAttribute(pFile, "Album");
		CString strYear   = GetFileAttribute(pFile, "Year");

		memset(&sID3V1, 0, sizeof(sID3V1));

		memcpy(sID3V1.ident,"TAG",3);
		memcpy(sID3V1.title,  strTitle,  min(strTitle.GetLength(),  30));
		memcpy(sID3V1.artist, strArtist, min(strArtist.GetLength(), 30));
		memcpy(sID3V1.album,  strAlbum,  min(strAlbum.GetLength(),  30));
		memcpy(sID3V1.year,   strYear,   min(strYear.GetLength(),    4));

		// NOTE: copying 30 bytes into sID3V1.comment will set reserved and tracknum
		int nTrack = atoi(GetFileAttribute(pFile, "Track"));

		CString strComment = GetFileAttribute(pFile, "Comments");
		memcpy(sID3V1.comment, strComment, min(strComment.GetLength(), nTrack ? 28 : 30)); // if a track is specified, max length is 28, otherwise 30

		if (nTrack)
			sID3V1.tracknum = max(0, min(255, nTrack));

		sID3V1.genre = atoi(GetFileAttribute(pFile, "Genre"));

		if (!WriteFile(hFile, &sID3V1, 128, &dwNumBytesReadWritten, NULL) ||
			dwNumBytesReadWritten != 128)
		{
			CloseHandle(hFile);
			return FALSE;
		}
		

		CloseHandle(hFile);
	}
	else
		return false;

	return true;
}

void CGnuSchemaAudio::Clear()
{
	m_strFile	  = "";
	m_dwValidity  = MP3VLD_INVALID;

	m_nFrames		= 0;
	m_enMPEGVersion = MPEGVER_NA;
	m_nMPEGLayer	= 0;
	m_bHasCRC		= FALSE;

	m_enChannelMode = MP3CM_STEREO;
	m_enEmphasis	= MP3EM_NONE;
	m_bCopyrighted	= FALSE;
	m_bOriginal		= FALSE;
}

// functions
CString CGnuSchemaAudio::GetGenreString(int nIndex)
{
	if (nIndex > g_nMP3GenreCount)
		return "";
	
	return g_arrMP3Genre[nIndex];
}

CString CGnuSchemaAudio::GetLengthString(int nSeconds)
{
    int nMin = nSeconds / 60;
    int nSec = nSeconds % 60;

	CString str;
    str.Format("%d:%02d", nMin, nSec);

	return str;
}

// helper functions
void CGnuSchemaAudio::ChangeEndian(void* pBuffer, int nBufSize)
{
	if (!pBuffer || !nBufSize)
		return;

	char temp;
	for (int i = 0; i < nBufSize / 2; i++)
	{
		temp = ((char*) pBuffer)[i];
		((char*) pBuffer)[i] = ((char*)pBuffer)[nBufSize - i - 1];
		((char*) pBuffer)[nBufSize - i - 1] = temp;
	}
}