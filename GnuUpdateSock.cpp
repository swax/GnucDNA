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


#include "stdafx.h"
#include "GnuCore.h"

#include "GnuUpdate.h"
#include "GnuPrefs.h"

#include "GnuUpdateSock.h"


CGnuUpdateSock::CGnuUpdateSock(CGnuUpdate* pUpdate)
{
	m_pUpdate = pUpdate;
	m_pPrefs  = pUpdate->m_pCore->m_pPrefs;

	m_Status = TRANSFER_CONNECTING;

	m_BytesCompleted = 0;
	m_FileSize       = 0;
}

CGnuUpdateSock::~CGnuUpdateSock()
{
	m_File.Abort();
}

void CGnuUpdateSock::OnConnect(int nErrorCode)
{
	if(nErrorCode)
		return;

	m_Status = TRANSFER_CONNECTED;

	CString GetFile;
	GetFile      = "GET " + m_Path + " HTTP/1.1\r\n";
	GetFile     += "User-Agent: Mozilla/4.0\r\n";
	GetFile     += "Host: " + m_Host + "\r\n";
	GetFile     += "Connection: Keep-Alive\r\n";
	GetFile     += "\r\n";

	Send(GetFile, GetFile.GetLength());


	CAsyncSocketEx::OnConnect(nErrorCode);
}

void CGnuUpdateSock::OnReceive(int nErrorCode)
{
	// Receive Data
	byte* pBuff = new byte[8000];
	int BuffLength = Receive(pBuff, 4096);

	// Check for errors
	switch (BuffLength)
	{
	case 0:
		m_Error = "No Data";
		Close();
		delete [] pBuff;
		return;
		break;
	case SOCKET_ERROR:
		m_Error = "Socket Error";
		Close();
		delete [] pBuff;
		return;
		break;
	}

	// React to different download states
	if(m_Status == TRANSFER_RECEIVING)
	{
		Download(pBuff, BuffLength);
	}
	else if(m_Status == TRANSFER_CONNECTED)
	{
		pBuff[BuffLength] = '\0';
		
		m_Header += CString(pBuff);
		
		if(m_Header.Find("\r\n\r\n") != -1)
		{
			CString Handshake = m_Header;
			CString FirstLine = Handshake.Mid(0, Handshake.Find("\r\n"));

			// Check HTTP header
			int  HttpCode   = 0;
			char okBuff[10] = "";
			::sscanf((LPCTSTR) FirstLine, "%s %d", okBuff, &HttpCode);

			if(200 <= HttpCode && HttpCode < 300)
			{
				Handshake.MakeLower();

				int FileBegin = Handshake.Find("\r\n\r\n") + 4;
				if(FileBegin == 3)
				{
					m_Error = "Bad HTTP Response";
					Close();
				}

				// New download
				else
				{	
					int pos = Handshake.Find("\r\ncontent-length:");

					if(pos != -1)
						sscanf((LPCTSTR) Handshake.Mid(pos), "\r\ncontent-length: %ld\r\n", &m_FileSize);

					if(m_FileSize)
					{
						m_Status = TRANSFER_RECEIVING;
						ReadyFile();
						
						if(BuffLength - FileBegin > 0)
							Download(&pBuff[FileBegin], BuffLength - FileBegin);
					}
					else
					{
						m_Error = "Bad File Size";
						Close();
					}
				}
			}
			else if(300 <= HttpCode && HttpCode < 400)
			{

				// If file moved
				if(HttpCode == 301)
				{
					CParsedHeaders ParsedHeaders(m_Header);

					CString Location = ParsedHeaders.FindHeader("Location");
					if( !Location.IsEmpty() )
						m_pUpdate->AddServer(Location);
				}
				
				Close();
			}
			else if(400 <= HttpCode && HttpCode < 500)
			{
				m_Error = "File Not Found";
				Close();
			}
			else if(500 <= HttpCode && HttpCode < 600)
			{
				m_Error = "Server Busy";
				Close();
			}
			else
			{
				m_Error = "Bad HTTP Response";
				Close();
			}
		}
	}
	else
	{
		m_Error = "Wrong State";
		Close();
	}

	delete [] pBuff;

	CAsyncSocketEx::OnReceive(nErrorCode);
}

void CGnuUpdateSock::OnClose(int nErrorCode)
{
	Close();

	CAsyncSocketEx::OnClose(nErrorCode);
}

void CGnuUpdateSock::Close()
{
	if(m_SocketData.hSocket != INVALID_SOCKET)
	{
		AsyncSelect(0);
		ShutDown(2);

		CAsyncSocketEx::Close();
	}

	m_Status = TRANSFER_CLOSED;
	
	if(m_BytesCompleted)
		if(m_BytesCompleted == m_FileSize)
			m_Status = TRANSFER_COMPLETED;

	m_File.Abort();

	
}

void CGnuUpdateSock::ReadyFile()
{
	CString Name = m_Path;

	int SlashPos = m_Path.ReverseFind('/');
	if(SlashPos != -1)
		Name = m_Path.Mid(SlashPos + 1);


	CreateDirectory(m_pPrefs->m_DownloadPath, NULL);
	m_DownloadPath = m_pPrefs->m_DownloadPath + "\\Update\\";
	CreateDirectory(m_DownloadPath, NULL);
	
	m_DownloadPath += Name;
	m_DownloadPath.Replace("\\\\", "\\");

	if(!m_File.Open(m_DownloadPath, CFile::modeCreate | CFile::modeWrite))
	{
		m_Status = TRANSFER_CLOSED;
		m_Error  = "Unable to Create File";
	}
}

void CGnuUpdateSock::Download(byte* pBuff, int nSize)
{
	if(m_File.m_hFile != CFile::hFileNull)
		m_File.Write(pBuff, nSize);
	else 
	{
		m_Error = "Local File Error";
		Close();
	}

	m_BytesCompleted += nSize;

	if(m_BytesCompleted == m_FileSize)
		Close();
}