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
#include "DnaCore.h"
#include "DnaUpdate.h"
#include "DnaEvents.h"

#include "GnuCore.h"
#include "GnuPrefs.h"
#include "GnuTransfers.h"
#include "GnuDownloadShell.h"
#include "GnuUpdateSock.h"

#include "GnuUpdate.h"


UINT ResolveWorker(LPVOID pVoidUpdate);


CGnuUpdate::CGnuUpdate(CGnuCore* pCore)
{
	m_pCore     = pCore;
	m_pPrefs    = pCore->m_pPrefs;
	m_pTrans	= pCore->m_pTrans;

	m_NextFileID = 1;

	m_pResolveThread = NULL;

	m_CheckPending  = false;
	m_DownloadMode  = false;
	m_TryingNetwork = 0;

	m_Socket = NULL;
}

CGnuUpdate::~CGnuUpdate(void)
{
	if(m_Socket)
	{
		delete m_Socket;
		m_Socket = NULL;
	}

	for(int i = 0; i < m_FileList.size(); i++)
		if(m_FileList[i].Socket)
		{
			delete m_FileList[i].Socket;
			m_FileList[i].Socket = NULL;
		}
}

void CGnuUpdate::endThreads()
{
	GnuEndThread(m_pResolveThread);
}

void CGnuUpdate::AddServer(CString Server)
{
	if(Server.Find("http://") == 0)
		Server = Server.Mid(7);


	// Seperate Host from path
	CString HostPort = Server;
	CString Path;

	int SlashPos = Server.Find("/");

	if(SlashPos != -1)
	{
		HostPort = Server.Left(SlashPos);
		Path     = Server.Mid(SlashPos);
	}
	else
		return;

	
	// Create update server object
	UpdateServer NewServer;
	NewServer.Path = Path;


	// Seperate host and port
	NewServer.HostIP.S_addr = 0;
	NewServer.Host = HostPort;
	NewServer.Port = 80;

	int ColonPos = HostPort.Find(":");

	if(ColonPos != -1)
	{
		NewServer.Host = HostPort.Left(ColonPos);
		NewServer.Port = atoi( HostPort.Mid(ColonPos + 1));
	}


	// Check to see if server already in list
	m_ServerLock.Lock();
	
	for(int i = 0; i < m_ServerList.size(); i++)
		if(m_ServerList[i].Host.CompareNoCase( NewServer.Host ) == 0)
		{
			m_ServerLock.Unlock();
			return;
		}

	// Add server to list
	m_ServerList.push_back(NewServer);

	m_ServerLock.Unlock();
	
	// Resolve Host to IP
	GnuEndThread(m_pResolveThread);

	GnuStartThread(m_pResolveThread, ResolveWorker, this);
}

UINT ResolveWorker(LPVOID pVoidUpdate)
{
	CGnuUpdate* pUpdate = (CGnuUpdate*) pVoidUpdate;


	bool FreshHost = false;
	
	// Loop while there are hosts to resolve
	do
	{
		FreshHost = false;

		CString ResolveHost;

		// Get next host to resolve
		pUpdate->m_ServerLock.Lock();
			for(int i = 0; i < pUpdate->m_ServerList.size(); i++)
				if(pUpdate->m_ServerList[i].HostIP.S_addr == 0)
				{
					ResolveHost = pUpdate->m_ServerList[i].Host;
					FreshHost = true;
					break;
				}
		pUpdate->m_ServerLock.Unlock();

		if(!FreshHost)
			break;

		hostent* Host = gethostbyname(ResolveHost);

		std::vector<UpdateServer>::iterator itServer;

		pUpdate->m_ServerLock.Lock();

		// If host successfully resolved
		if(Host)
		{
			itServer = pUpdate->m_ServerList.begin();
			while(itServer != pUpdate->m_ServerList.end())
				if( (*itServer).HostIP.S_addr == 0)
				{
					memcpy(&(*itServer).HostIP.S_addr, Host->h_addr_list[0], 4);
					
					if((*itServer).HostIP.S_addr == 0)
						itServer = pUpdate->m_ServerList.erase(itServer);
					else
						itServer++;
				}
				else
					itServer++;
		}

		// Else, erase host
		else
		{
			itServer = pUpdate->m_ServerList.begin();
			while(itServer != pUpdate->m_ServerList.end() )
				if( (*itServer).HostIP.S_addr == 0)
					itServer = pUpdate->m_ServerList.erase(itServer);
				else
					itServer++;
		}

		pUpdate->m_ServerLock.Unlock();

	} while(FreshHost);

	return 0;
}
	

void CGnuUpdate::Check()
{
	m_ServerLock.Lock();


	// Download update xml file
	for(int i = 0; i < m_ServerList.size(); i++)
	{
		// See if host resolved to ip
		if(m_ServerList[i].HostIP.S_addr)
		{
			m_CheckPending = false;

			// Try to connect
			if(!m_Socket)
			{
				m_Socket = new CGnuUpdateSock(this);

				m_Socket->m_Host = m_ServerList[i].Host;
				m_Socket->m_Port = m_ServerList[i].Port;
				m_Socket->m_Path = m_ServerList[i].Path;

				m_Socket->Create();
				if(!m_Socket->Connect(IPtoStr(m_ServerList[i].HostIP), m_ServerList[i].Port))
					if (m_Socket->GetLastError() != WSAEWOULDBLOCK)
					{
						delete m_Socket;
						m_Socket = NULL;
					}
			}
			

			break;
		}
		else 
			m_CheckPending = true;
	}

	m_ServerLock.Unlock();
}

void CGnuUpdate::Timer()
{

	if(m_CheckPending)
	{
		m_CheckPending = false;

		Check();
	}

	// If we have an open connection to the update server
	if(m_Socket)
	{
		if(m_Socket->m_Status == TRANSFER_CLOSED)
		{
			// Delete server from list
			std::vector<UpdateServer>::iterator itServer;

			itServer = m_ServerList.begin();
			while(itServer != m_ServerList.end())
				if( (*itServer).Host.CompareNoCase(m_Socket->m_Host)  == 0)
					itServer = m_ServerList.erase(itServer);
				else
					itServer++;

			// This host failed, try next
			if(m_pCore->m_dnaCore->m_dnaEvents)
				m_pCore->m_dnaCore->m_dnaEvents->UpdateFailed(m_Socket->m_Error);

			m_CheckPending = true;

			delete m_Socket;
			m_Socket = NULL;
			return;
		}

		if(m_Socket->m_Status == TRANSFER_COMPLETED)
		{
			// Read update file
			ParseUpdateFile(m_Socket->m_DownloadPath);

			delete m_Socket;
			m_Socket = NULL;
		}
	}


	// If there are files in the list we are updating
	if(m_DownloadMode)
	{
		if(m_TryingNetwork)
		{
			if(DownloadProgress())
				m_TryingNetwork = 90;

			// Check if download is complete
			if(DownloadComplete())
			{
				if(m_pCore->m_dnaCore->m_dnaEvents)
					m_pCore->m_dnaCore->m_dnaEvents->UpdateComplete();
				
				m_DownloadMode  = false;
				m_TryingNetwork = 0;
				return;
			}

			m_TryingNetwork--;


			// Update over network failed, setup download to come from server
			if(m_TryingNetwork == 0)
			{
				// Cancel network downloads
				for(int i = 0; i < m_FileList.size(); i++)
				{
					std::map<int, CGnuDownloadShell*>::iterator itDL = m_pTrans->m_DownloadMap.find(m_FileList[i].DownloadID);

					if(itDL != m_pTrans->m_DownloadMap.end())
						m_pTrans->RemoveDownload(itDL->second);
				}

				// Start downloading 1 file from update server
				for(int i = 0; i < m_FileList.size(); i++)
					if(m_FileList[i].Socket)
					{
						if(m_FileList[i].Socket->m_Status == TRANSFER_COMPLETED)
							continue;
						else
							return;
					}
					else if(!m_FileList[i].Completed)
					{
						m_FileList[i].Socket = ServerDownload(m_FileList[i]);
						return;
					}
					
			}
		}

		// Else couldnt get files from network, try update server
		else
		{
			// Download files 1 at a time
			for(int i = 0; i < m_FileList.size(); i++)
				if(m_FileList[i].Socket)
				{
					if(m_FileList[i].Socket->m_Status == TRANSFER_COMPLETED)
						continue;
					else
						return;
				}
				else if(!m_FileList[i].Completed)
				{
					m_FileList[i].Socket = ServerDownload(m_FileList[i]);
					return;
				}

			// If code gets down here, all files completed
			if(m_pCore->m_dnaCore->m_dnaEvents)
				m_pCore->m_dnaCore->m_dnaEvents->UpdateComplete();
			
			m_DownloadMode  = false;
			return;
		}
	}

}

void CGnuUpdate::ParseUpdateFile(CString FilePath)
{
	CFile UpdateFile(FilePath, CFile::modeRead);

	// Copy file to string
	CString UpdateXML;

	char pBuff[4096];
	int BytesRead = UpdateFile.Read(pBuff, 4096);

	while(BytesRead > 0)
	{
		UpdateXML += CString(pBuff, BytesRead);
		BytesRead = UpdateFile.Read(pBuff, 4096);
	}

	ScanComponent(m_pCore->m_ClientName, m_pCore->m_ClientVersion, UpdateXML);
	ScanComponent("GnucDNA", m_pCore->m_DnaVersion, UpdateXML);	
	
	if(m_pCore->m_dnaCore->m_dnaEvents)
		if(m_FileList.size())
		{
			// Send update found event
			m_pCore->m_dnaCore->m_dnaEvents->UpdateFound(m_NewVers);
		}
		else
			m_pCore->m_dnaCore->m_dnaEvents->UpdateVersionCurrent();
}

void CGnuUpdate::ScanComponent(CString Component, CString LocalVersion, CString File)
{
	// Old, but tested code =/

	// Interpret XML file
	bool NewVersion = false;

	CString tmpVersion = LocalVersion;
	tmpVersion.Remove('.');
	int localVers = atoi(tmpVersion);

	int pos       = File.Find( "<" + Component + " version='", 0);
	int	endPos    = 0;
	int taglength = Component.GetLength() + 11;


	// Loop through Component blocks in XML
	while(pos < File.GetLength() && pos != -1)
	{
		// Extract version
		//tmpVersion = File.Mid(pos + taglength, 7);
		endPos = File.Find( "'", pos + taglength);
		tmpVersion = File.Mid(pos + taglength, endPos - ( pos + taglength ));

		tmpVersion.Remove('.');
		int nextVers = atoi(tmpVersion);

		// Compare with current
		if(nextVers > localVers)
		{
			if( m_pPrefs->m_Update == UPDATE_BETA || nextVers % 10 == 0)
			{
				// Target version is what the final upgrade will be
				if(m_NewVers == "")
					m_NewVers = Component + " " + File.Mid(pos + taglength, endPos - (pos + taglength) );

				endPos = File.Find( "</" + Component + ">", pos);

				// Extract File blocks just from this version
				pos = File.Find( "<File ", pos);
				while(pos < endPos && pos != -1)
				{
					int endtag = File.Find("</File>", pos);
					AddFile( File.Mid(pos, endtag - pos) );
					NewVersion = true;

					pos = File.Find( "<File ", pos + 1);
				}
			}
		}
		else
			break;

		pos    = File.Find( "<" + Component + " version='", endPos);
		endPos = pos + 1;
	}
}

void CGnuUpdate::AddFile(CString FileTag)
{
	UpdateFile Item;

	Item.FileID     = m_NextFileID++;
	Item.DownloadID = 0;
	Item.Socket     = NULL;
	Item.Completed  = false;

	// Extract data from <File> block
	int frontPos = FileTag.Find("src='") + 5;
	int backPos  = FileTag.Find("'", frontPos);
	Item.Source  = FileTag.Mid(frontPos, backPos - frontPos);
	Item.Name    = Item.Source;

	frontPos = Item.Source.ReverseFind('/');
	if(frontPos != -1)
		Item.Name = Item.Source.Mid(frontPos + 1);

	frontPos   = FileTag.Find("sha1='") + 6;
	backPos    = FileTag.Find("'", frontPos);
	Item.Hash  = FileTag.Mid(frontPos, backPos - frontPos);

	frontPos   = FileTag.Find("size='") + 6;
	backPos    = FileTag.Find("'", frontPos);
	Item.Size  = atoi(FileTag.Mid(frontPos, backPos - frontPos));


	// Check for duplicates
	for(int i = 0; i < m_FileList.size(); i++)
		if(Item.Name.CompareNoCase( m_FileList[i].Name ) == 0)
			return;

	m_FileList.push_back(Item);
}

void CGnuUpdate::StartDownload()
{
	// Search for file over gnutella network

	for(int i = 0; i < m_FileList.size(); i++)
	{
		// Create new download
		CGnuDownloadShell* Download = new CGnuDownloadShell(m_pTrans);
		
		m_FileList[i].DownloadID = Download->m_DownloadID;

		Download->m_Search = m_FileList[i].Name;
		
		Download->Init(m_FileList[i].Name, m_FileList[i].Size, HASH_SHA1, m_FileList[i].Hash);

		m_pTrans->m_DownloadAccess.Lock();
		m_pTrans->m_DownloadList.push_back(Download);
		m_pTrans->m_DownloadAccess.Unlock();

		Download->ReSearch();
	}

	
	// Wait 90 seconds for results

	m_DownloadMode  = true;
	m_TryingNetwork = 90;
}

bool CGnuUpdate::DownloadProgress()
{
	for(int i = 0; i < m_FileList.size(); i++)
	{
		std::map<int, CGnuDownloadShell*>::iterator itDL = m_pTrans->m_DownloadMap.find(m_FileList[i].DownloadID);

		if(itDL != m_pTrans->m_DownloadMap.end())
			if(itDL->second->GetStatus() == TRANSFER_RECEIVING)
				return true;
	}

	return false;
}

bool CGnuUpdate::DownloadComplete()
{
	bool DownloadComplete = true;

	for(int i = 0; i < m_FileList.size(); i++)
		if(!m_FileList[i].Completed)
		{
			std::map<int, CGnuDownloadShell*>::iterator itDL = m_pTrans->m_DownloadMap.find(m_FileList[i].DownloadID);

			if(itDL != m_pTrans->m_DownloadMap.end())
			{
				if(itDL->second->GetStatus() == TRANSFER_COMPLETED)
				{
					CString UpdateDir = itDL->second->GetFinalPath();
					
					int SlashPos = UpdateDir.ReverseFind('\\');
					if(SlashPos != -1)
						UpdateDir = UpdateDir.Left(SlashPos) + "\\Update" + UpdateDir.Mid(SlashPos);
					

					// Move file to update dir
					DeleteFile(UpdateDir);
					if(MoveFile(itDL->second->GetFinalPath(), UpdateDir))
					{
						m_FileList[i].Completed = true;
						m_pTrans->RemoveDownload(itDL->second);
					}

				}
				else
					DownloadComplete = false;
			}
			else
				DownloadComplete = false;
		}

	return DownloadComplete;
}

CGnuUpdateSock* CGnuUpdate::ServerDownload(UpdateFile &File)
{
	// Use last working server
	for(int i = 0; i < m_ServerList.size(); i++)
		if(m_ServerList[i].HostIP.S_addr)
		{
			File.Socket = new CGnuUpdateSock(this);

			File.Socket->m_Host = m_ServerList[i].Host;
			File.Socket->m_Port = m_ServerList[i].Port;
			File.Socket->m_Path = m_ServerList[i].Path;
		
			int pos = File.Socket->m_Path.ReverseFind('/');
			if(pos != -1)
				File.Socket->m_Path = File.Socket->m_Path.Left(pos + 1);
			
			File.Socket->m_Path += File.Source;
			File.Socket->m_Path.Replace("//", "/");

			File.Socket->Create();
			if(!File.Socket->Connect(IPtoStr(m_ServerList[i].HostIP), m_ServerList[i].Port))
				if (File.Socket->GetLastError() != WSAEWOULDBLOCK)
				{
					delete File.Socket;
					File.Socket = NULL;
				}

			return File.Socket;
		}

	return NULL;
}

void CGnuUpdate::CancelUpdate()
{
	if(m_Socket)
	{
		delete m_Socket;
		m_Socket = NULL;
	}

	// Cancel network downloads
	for(int i = 0; i < m_FileList.size(); i++)
	{
		std::map<int, CGnuDownloadShell*>::iterator itDL = m_pTrans->m_DownloadMap.find(m_FileList[i].DownloadID);

		if(itDL != m_pTrans->m_DownloadMap.end())
			m_pTrans->RemoveDownload(itDL->second);
	}

	for(int i = 0; i < m_FileList.size(); i++)
		if(m_FileList[i].Socket)
		{
			delete m_FileList[i].Socket;
			m_FileList[i].Socket = NULL;
		}


	m_CheckPending  = false;
	m_DownloadMode  = false;
	m_TryingNetwork = 0;
}

int CGnuUpdate::TotalUpdateCompleted()
{
	int CompletedSize = 0;

	for(int i = 0; i < m_FileList.size(); i++)
		CompletedSize += GetFileCompleted(m_FileList[i]);

	return CompletedSize;
}

int CGnuUpdate::TotalUpdateSize()
{
	int TotalSize = 0;

	for(int i = 0; i < m_FileList.size(); i++)
		TotalSize += m_FileList[i].Size;

	return TotalSize;
}

int CGnuUpdate::GetFileCompleted(UpdateFile &File)
{
	// Check if file is downloaded already
	if(File.Completed)
		return File.Size;

	// Check if file is being downloaded from network
	std::map<int, CGnuDownloadShell*>::iterator itDL = m_pTrans->m_DownloadMap.find(File.DownloadID);

	if(itDL != m_pTrans->m_DownloadMap.end())
		return itDL->second->GetBytesCompleted();

	// Check if file is being downloaded from central server
	if(File.Socket)
		return File.Socket->m_BytesCompleted;

	return 0;
}

void CGnuUpdate::LaunchUpdate()
{
	for(int i = 0; i < m_FileList.size(); i++)
		if(m_FileList[i].Name.Find(".exe") != -1)
		{
			CString FilePath  = m_pPrefs->m_DownloadPath + "\\Update\\";
			FilePath += m_FileList[i].Name;
			FilePath.Replace("\\\\", "\\");

			ShellExecute(NULL, "open", FilePath, "/SILENT", NULL, SW_SHOWNORMAL);
			
			break;
		}

	
}
