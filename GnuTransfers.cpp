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


#include "stdafx.h"
#include "GnuCore.h"

#include "GnuNetworks.h"
#include "GnuPrefs.h"
#include "GnuControl.h"
#include "G2Control.h"
#include "GnuShare.h"
#include "GnuMeta.h"
#include "GnuSchema.h"
#include "GnuUploadShell.h"
#include "GnuDownloadShell.h"
#include "GnuDownload.h"

#include "DnaCore.h"
#include "DnaDownload.h"
#include "DnaUpload.h"
#include "DnaEvents.h"

#include "gnutransfers.h"

CGnuTransfers::CGnuTransfers()
{
	m_DownloadSecBytes	= 0;
	m_UploadSecBytes	= 0;

	m_NextDownloadID = 1;
	m_NextUploadID = 1;

	m_NextReSearch = 0;
}

void CGnuTransfers::InitTransfers(CGnuCore* pCore)
{
	m_pCore  = pCore;
	m_pPrefs = pCore->m_pPrefs;
	m_pNet   = pCore->m_pNet;


	LoadDownloads();

	m_UploadQueue.Init(this);

	m_Minute = 0;
}

CGnuTransfers::~CGnuTransfers(void)
{
	// Destroy Upload List
	m_UploadAccess.Lock();

		while( m_UploadList.size() )
		{
			delete m_UploadList.back();
			m_UploadList.pop_back();
		}

	m_UploadAccess.Unlock();


	// Destroy Download List	
	m_DownloadAccess.Lock();
		
		while( m_DownloadList.size() )
		{
			delete m_DownloadList.back();
			m_DownloadList.pop_back();
		}

	m_DownloadAccess.Unlock();
}

int CGnuTransfers::CountUploading()
// Use by thread so we are careful
{
	int NumUploading = 0;

	m_UploadAccess.Lock();

		for(int i = 0; i < m_UploadList.size(); i++)	
		{
			CGnuUploadShell *p = m_UploadList[i];

			if(p->m_Status == TRANSFER_SENDING)
				NumUploading++;
		}

	m_UploadAccess.Unlock();

	

	return NumUploading;
}

int CGnuTransfers::CountDownloading()
{
	int NumDownloading = 0;

	m_DownloadAccess.Lock();

		for(int i = 0; i < m_DownloadList.size(); i++)	
		{
			CGnuDownloadShell *p = m_DownloadList[i];

			if(p->m_ShellStatus == CGnuDownloadShell::eActive)
				NumDownloading++;
		}

	m_DownloadAccess.Unlock();

	return NumDownloading;
}

void CGnuTransfers::DownloadUpdate(int DownloadID)
{
	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->DownloadUpdate(DownloadID);
}

void CGnuTransfers::UploadUpdate(int UploadID)
{
	if(m_pCore->m_dnaCore->m_dnaEvents)
		m_pCore->m_dnaCore->m_dnaEvents->UploadUpdate(UploadID);
}

void CGnuTransfers::Timer()
{
	ManageDownloads();
	ManageUploads();

	m_Minute++;
	if(m_Minute == 60)
	{
		MinuteTimer();
		m_Minute = 0; 
	}
}


void CGnuTransfers::MinuteTimer()
{
	//	Make sure 5 mins have passed since last research
	if(time(NULL) < m_NextReSearch)
		return;


	CGnuDownloadShell *pSelected = NULL;

	uint64 LowestNextResearch = 0;

	for(int i = 0; i < m_DownloadList.size(); i++)
	{
		CGnuDownloadShell *pDown = m_DownloadList[i];
		
		// Dont research pending or done
		if(pDown->m_ShellStatus != CGnuDownloadShell::ePending &&
		   pDown->m_ShellStatus != CGnuDownloadShell::eDone)
		{
			ASSERT(pDown->m_NextReSearch != 0);

			// Find the download most over due for a re-search
			if(LowestNextResearch == 0 || pDown->m_NextReSearch < LowestNextResearch)
			{
				pSelected = pDown;
				LowestNextResearch =  pDown->m_NextReSearch;
			}
		}
	}

	// check time to see if good to do re-search
	if(pSelected && time(NULL) > pSelected->m_NextReSearch)
	{
		pSelected->ReSearch();

		m_NextReSearch = time(NULL) + (5*60); // dont any researching for 5 mins

	}     
}

void CGnuTransfers::ManageDownloads()
{
	int NetBytes  = 0;
	
	if( m_pNet->m_pGnu )
		NetBytes += m_pNet->m_pGnu->m_NetSecBytesDown;

	if( m_pNet->m_pG2 )
		NetBytes += m_pNet->m_pG2->m_NetSecBytesDown;

	m_DownloadSecBytes = 0;

	int BytesLeft     = 0;
	int BytesAlloc    = 0;
	int DownloadCount = 0;
	int MaxBytes      = 0;

	for(int i = 0; i < m_DownloadList.size(); i++)
	{
		CGnuDownloadShell *pDown = m_DownloadList[i];

		if(pDown->IsDownloading())
		{
			for(int j = 0; j < pDown->m_Sockets.size(); j++)
				m_DownloadSecBytes += pDown->m_Sockets[j]->m_AvgRecvBytes.GetAverage();

			DownloadCount++;

			/*if(m_pPrefs->m_BandwidthDown)
			{
				BytesLeft  += pDown->m_AllocBytes;
				BytesAlloc += pDown->m_AllocBytesTotal;
			}*/
		}

		pDown->Timer();
	}
	
	// Manage Download Bandwidth
	/*if(m_pPrefs->m_BandwidthDown)
	{
		MaxBytes = m_pPrefs->m_BandwidthDown * 1024 - NetBytes;

		if(BytesAlloc > MaxBytes)
			BytesLeft -= BytesAlloc - MaxBytes;

		else if(BytesAlloc < MaxBytes)
			BytesLeft += MaxBytes - BytesAlloc;

		if(BytesLeft < 0)
			BytesLeft = 0;
	}*/
	
	for(i = 0; i < m_DownloadList.size(); i++)
	{
		CGnuDownloadShell *pDown = m_DownloadList[i];

		if(pDown->IsDownloading() && m_pPrefs->m_BandwidthDown && DownloadCount)
		{
			pDown->m_AllocBytes = m_pPrefs->m_BandwidthDown * 1024 / DownloadCount;	


			/*if(pDown->m_AllocBytesTotal > MaxBytes / DownloadCount)
			{
				int RePos = MaxBytes / DownloadCount / DownloadCount;
				
				BytesLeft += RePos;
				pDown->m_AllocBytesTotal -= RePos;
			}*/

			//// bytes download can use = bytes download used last second plus the bytes never used from the last second divided by the total active downloads
			////pDown->m_AllocBytesTotal = (pDown->m_AllocBytesTotal - pDown->m_AllocBytes) + (BytesLeft / DownloadCount);
			
			//pDown->m_AllocBytesTotal = MaxBytes / DownloadCount;
			//pDown->m_AllocBytes = pDown->m_AllocBytesTotal;	
		}
	}

	UINT TotalDownSpeed = NetBytes + m_DownloadSecBytes;

	if(TotalDownSpeed > m_pNet->m_RealSpeedDown)
		m_pNet->m_RealSpeedDown = TotalDownSpeed;	


	// If client is downloading more than 20 KB/s flag it as a high bandwidth node
	if( !m_pNet->m_HighBandwidth && TotalDownSpeed > 20 * 1024)
	{
		m_pNet->m_HighBandwidth = true;
	}
}

void CGnuTransfers::ManageUploads()
{
	int NetBytes     = 0;
	
	if( m_pNet->m_pGnu )
		NetBytes += m_pNet->m_pGnu->m_NetSecBytesUp;

	if( m_pNet->m_pG2 )
		NetBytes += m_pNet->m_pG2->m_NetSecBytesUp;


	m_UploadSecBytes = 0;

	int BytesLeft   = 0;
	int BytesAlloc  = 0;
	int UploadCount = 0;

	for(int i = 0; i < m_UploadList.size(); i++)
	{
		CGnuUploadShell *pUp = m_UploadList[i];

		if(TRANSFER_SENDING == pUp->m_Status)
		{
			int SpeedUp = pUp->m_AvgSentBytes.GetAverage();

			if(m_pNet->m_RealSpeedUp < SpeedUp)
				m_pNet->m_RealSpeedUp = SpeedUp;

			m_UploadSecBytes += pUp->m_AvgSentBytes.GetAverage();

			UploadCount++;

			/*if(m_pPrefs->m_BandwidthUp)
			{
				BytesLeft  += pUp->m_AllocBytes;
				BytesAlloc += pUp->m_AllocBytesTotal;
			}*/
		}
	}

	// Manage Upload Bandwidth
	/*if(m_pPrefs->m_BandwidthUp)
	{
		int MaxBytes = m_pPrefs->m_BandwidthUp * 1024 - NetBytes;

		if(BytesAlloc > MaxBytes)
			BytesLeft -= BytesAlloc - MaxBytes;

		else if(BytesAlloc < MaxBytes)
			BytesLeft += MaxBytes - BytesAlloc;

		if(BytesLeft < 0)
			BytesLeft = 0;
	}*/


	for(i = 0; i < m_UploadList.size(); i++)
	{
		CGnuUploadShell *pUp = m_UploadList[i];

		if(TRANSFER_SENDING == pUp->m_Status && m_pPrefs->m_BandwidthUp && UploadCount)
		{
			// bytes upload can use = bytes upload used last second plus the bytes never used from the last second divided by the total active uploads
			//pUp->m_AllocBytesTotal = (pUp->m_AllocBytesTotal - pUp->m_AllocBytes) + (BytesLeft / UploadCount);
			
			//pUp->m_AllocBytes = pUp->m_AllocBytesTotal;
			pUp->m_AllocBytes = m_pPrefs->m_BandwidthUp * 1024 / UploadCount;	
		}
		
		pUp->Timer();
	}

	//Manage upload queues
	m_UploadQueue.Timer();


	// If client is uploading more than 20 KB/s flag it as a high bandwidth node
	if( !m_pNet->m_HighBandwidth && (NetBytes + m_UploadSecBytes) > 20 * 1024)
	{
		m_pNet->m_HighBandwidth = true;
	}
}

void CGnuTransfers::LoadDownloads()
{
	CFileFind finder;
	CString   Header;

	// Start looking for files
	CString PartialPath = m_pPrefs->m_PartialDir;
	int bWorking = finder.FindFile(PartialPath + "\\*");

	while (bWorking)
	{
		bWorking = finder.FindNextFile();

		// Skip . and .. files
		if (finder.IsDots() || finder.IsDirectory())
			continue;


		CString FileDir  = finder.GetFilePath();

		if( FileDir.Right(5).Compare(".info") == 0 )
		{
			CGnuDownloadShell* pDownload = LoadDownloadHosts(FileDir);

			if(pDownload)
			{
				int dotpos = FileDir.ReverseFind('.');
				FileDir = FileDir.Left(dotpos) + ".part";
				
				LoadDownloadParts(FileDir, pDownload);
			}
		}
	}

	finder.Close();
}

CGnuDownloadShell* CGnuTransfers::LoadDownloadHosts(CString FilePath)
{
	// Check if file already loaded
	for(int i = 0; i < m_DownloadList.size(); i++)
		if( m_DownloadList[i]->m_BackupPath.CompareNoCase(FilePath) == 0 )
			return NULL;


	CStdioFile BackupFile;

	CString NextLine;
	CString Backup;
	
	if (BackupFile.Open(FilePath, CFile::modeRead))
	{
		while (BackupFile.ReadString(NextLine))
			Backup += NextLine + "\n";

		BackupFile.Abort();
	}

	if(Backup.IsEmpty() || Backup.Find("[Download]") == -1)
		return NULL;

	int CurrentPos = Backup.Find("[Download]");

	CGnuDownloadShell* Download = new CGnuDownloadShell(this);
	
	Download->m_ShellStatus		= (CGnuDownloadShell::Status) atoi(GetBackupString("Status", CurrentPos, Backup));
	Download->m_Name			= GetBackupString("Name", CurrentPos, Backup);
	Download->m_FileLength		= atoi(GetBackupString("FileLength", CurrentPos, Backup));
	Download->m_PartSize		= atoi(GetBackupString("PartSize", CurrentPos, Backup));
	Download->m_OverrideName	= GetBackupString("OverrideName", CurrentPos, Backup);
	Download->m_OverridePath	= GetBackupString("OverridePath", CurrentPos, Backup);
	Download->m_PartialPath		= GetBackupString("PartialPath", CurrentPos, Backup);
	Download->m_BackupPath  	= FilePath;
	Download->m_Sha1Hash		= GetBackupString("Sha1Hash", CurrentPos, Backup);
	Download->m_Search			= GetBackupString("Search", CurrentPos, Backup);
	Download->m_AvgSpeed		= atoi(GetBackupString("AvgSpeed", CurrentPos, Backup));
	Download->m_HashComputed	= atoi(GetBackupString("HashComputed", CurrentPos, Backup));
	Download->m_HashVerified	= atoi(GetBackupString("HashVerified", CurrentPos, Backup));
	Download->m_FileMoved		= atoi(GetBackupString("FileMoved", CurrentPos, Backup));
	Download->m_ReasonDead		= GetBackupString("ReasonDead", CurrentPos, Backup);
	Download->m_MetaXml         = GetBackupString("Meta", CurrentPos, Backup);

	Download->m_UseProxy		= atoi(GetBackupString("UseProxy", CurrentPos, Backup));
	Download->m_DefaultProxy	= GetBackupString("DefaultProxy", CurrentPos, Backup);

	Download->m_TigerHash		= GetBackupString("TigerHash", CurrentPos, Backup);
	Download->m_TreeSize		= atoi(GetBackupString("TreeSize", CurrentPos, Backup));
	Download->m_TreeRes			= atoi(GetBackupString("TreeRes", CurrentPos, Backup));

	if(Download->m_TreeSize)
	{
		Download->m_TigerTree = new byte[Download->m_TreeSize];
		memset(Download->m_TigerTree, 0, Download->m_TreeSize);
	}

	if(Download->m_TigerTree)
	{
		CString Value = GetBackupString("TigerTree", CurrentPos, Backup);

		int buffPos = 0;
		int dotPos  = Value.Find(".");

		while(dotPos != -1 && buffPos < Download->m_TreeSize)
		{
			DecodeBase32( Value.Mid(dotPos - 39, 39), 39, Download->m_TigerTree + buffPos );

			buffPos += 24;
			dotPos = Value.Find(".", dotPos + 1);
		}
	}


	Download->Init(Download->m_Name, Download->m_FileLength, HASH_SHA1, Download->m_Sha1Hash);

	
	// Load Host info
	if( !Download->m_FileMoved )
		for(int i = 0; ; i++)
		{
			CurrentPos = Backup.Find("[Host " + NumtoStr(i) + "]");

			if(CurrentPos == -1)
				break;

			CurrentPos += 5; // Host in header and value conflict

			FileSource nResult;
			nResult.Name = GetBackupString("Name", CurrentPos, Backup);
			nResult.NameLower = nResult.Name;
			nResult.NameLower.MakeLower();

			nResult.Sha1Hash	 = GetBackupString("Sha1Hash", CurrentPos, Backup);
			//nResult.BitprintHash = GetBackupString("BitprintHash", CurrentPos, Backup);

			nResult.FileIndex	= atoi(GetBackupString("FileIndex", CurrentPos, Backup));
			nResult.Size		= atoi(GetBackupString("Size", CurrentPos, Backup));

			nResult.Address.Host = StrtoIP(GetBackupString("Host", CurrentPos, Backup));
			nResult.Address.Port = atoi(GetBackupString("Port", CurrentPos, Backup));
			nResult.Network      = atoi(GetBackupString("Network", CurrentPos, Backup));
			nResult.HostStr		 = GetBackupString("HostStr", CurrentPos, Backup);
			nResult.Path		 = GetBackupString("Path", CurrentPos, Backup);
			nResult.Speed		 = atoi(GetBackupString("Speed", CurrentPos, Backup));
			nResult.Vendor		 = GetBackupString("Vendor", CurrentPos, Backup);

			nResult.Firewall	= atoi(GetBackupString("Firewall", CurrentPos, Backup)) != 0;
			nResult.OpenSlots	= atoi(GetBackupString("OpenSlots", CurrentPos, Backup)) != 0;
			nResult.Busy		= atoi(GetBackupString("Busy", CurrentPos, Backup)) != 0;
			nResult.Stable		= atoi(GetBackupString("Stable", CurrentPos, Backup)) != 0;
			nResult.ActualSpeed = atoi(GetBackupString("ActualSpeed", CurrentPos, Backup)) != 0;
			DecodeBase16(GetBackupString("PushID", CurrentPos, Backup), 32, (byte*) &nResult.PushID);

			CString Nodes = GetBackupString("Direct", CurrentPos, Backup);
			while(!Nodes.IsEmpty())
				nResult.DirectHubs.push_back( StrtoIPv4(ParseString(Nodes, ',')) );

			nResult.GnuRouteID = 0;
			nResult.Distance = 7;
			//nResult.Icon     = m_pCore->GetIconIndex(nResult.Name);
		
			

			Download->AddHost(nResult);
		}

	//Download->m_DoReQuery = true;


	// Add Download to list
	m_DownloadAccess.Lock();
	m_DownloadList.push_back(Download);
	m_DownloadAccess.Unlock();

	TransferLoadMeta();

	if(Download->m_ShellStatus == CGnuDownloadShell::eActive)
		Download->Start();

	return Download;
}

void CGnuTransfers::LoadDownloadParts(CString FilePath, CGnuDownloadShell* pDownload)
{
	CStdioFile PartsFile;

	CString NextLine;
	CString PartsData;
	
	if (PartsFile.Open(FilePath, CFile::modeRead))
	{
		while (PartsFile.ReadString(NextLine))
			PartsData += NextLine + "\n";

		PartsFile.Abort();
	}

	if( PartsData.IsEmpty() )
		return;


	// Load each part
	for(int i = 0; ; i++)
	{
		int CurrentPos = PartsData.Find("[Part " + NumtoStr(i) + "]");

		if(CurrentPos == -1)
			break;

		CurrentPos += 5; // Host in header and value conflict

		if(i < pDownload->m_PartList.size() )
		{
			pDownload->m_PartList[i].BytesCompleted = atoi(GetBackupString("BytesCompleted", CurrentPos, PartsData));
			pDownload->m_PartList[i].Verified       = atoi(GetBackupString("Verified", CurrentPos, PartsData));
			pDownload->m_PartList[i].SourceHostID   = atoi(GetBackupString("SourceHostID", CurrentPos, PartsData));
		}
	}

}

CString CGnuTransfers::GetBackupString(CString Property, int &StartPos, CString &Backup)
{
	int PropPos = Backup.Find(Property, StartPos);
	
	if(PropPos != -1)
	{
		PropPos += Property.GetLength() + 1;
		int EndPos = Backup.Find("\n", PropPos);

		if(EndPos != -1)
			return Backup.Mid(PropPos, EndPos - PropPos);
	}

	return "";
}

void CGnuTransfers::RemoveDownload(CGnuDownloadShell* p)
{
	std::vector<CGnuDownloadShell*>::iterator itDown;

	itDown = m_DownloadList.begin(); 
	while(itDown != m_DownloadList.end())
		if(*itDown == p)
		{	
			m_DownloadAccess.Lock();

				(*itDown)->Erase();
				delete *itDown;

				itDown = m_DownloadList.erase(itDown);

			m_DownloadAccess.Unlock();
		}
		else
			itDown++;

}

void CGnuTransfers::RemoveUpload(CGnuUploadShell* p)
{
	// Upload Sockets
	std::vector<CGnuUploadShell*>::iterator itUp;

	itUp = m_UploadList.begin();
	while( itUp != m_UploadList.end())
		if(*itUp == p)
		{
			m_UploadAccess.Lock();

				delete *itUp;
				
				itUp = m_UploadList.erase(itUp);
			
			m_UploadAccess.Unlock();
		}
		else
			itUp++;
}

void CGnuTransfers::RemoveCompletedDownloads()
{
	std::vector<CGnuDownloadShell*>::iterator itDown;

	itDown = m_DownloadList.begin();
	while( itDown != m_DownloadList.end())
	{
		CGnuDownloadShell* p = *itDown;

		if(p->m_FileLength && p->m_FileLength == p->GetBytesCompleted())
		{
			m_DownloadAccess.Lock();

				(*itDown)->Erase();
				delete *itDown;
			
				itDown = m_DownloadList.erase(itDown);

			m_DownloadAccess.Unlock();
		}
		else
			itDown++;
	}

}

void CGnuTransfers::DoPush(GnuPush &Push)
{
	if(m_pPrefs->BlockedIP(Push.Address.Host))
		return;


	// Make sure not already pushing file
	for(int i = 0; i < m_UploadList.size(); i++)
		if(m_UploadList[i]->m_Host.S_addr == Push.Address.Host.S_addr && m_UploadList[i]->m_Status == TRANSFER_PUSH)
			return;

	
	CGnuUploadShell* Upload = new CGnuUploadShell(this);

	Upload->m_Host		= Push.Address.Host;
	Upload->m_Port		= Push.Address.Port;
	Upload->m_Network   = Push.Network;
	Upload->m_Index		= Push.FileID;


	m_UploadAccess.Lock();
		m_UploadList.push_back(Upload);
	m_UploadAccess.Unlock();


	Upload->PushFile();
}

void CGnuTransfers::TransferLoadMeta()
{
	for(int i = 0; i < m_DownloadList.size(); i++)
	{
		CGnuDownloadShell* pDownload = m_DownloadList[i];

		if( !pDownload->m_MetaXml.IsEmpty() )
		{
			pDownload->m_MetaID = m_pCore->m_pShare->m_pMeta->MetaIDfromXml( pDownload->m_MetaXml );

			std::map<int, CGnuSchema*>::iterator itSchema = m_pCore->m_pShare->m_pMeta->m_MetaIDMap.find(pDownload->m_MetaID);
			if(itSchema != m_pCore->m_pShare->m_pMeta->m_MetaIDMap.end() )
				itSchema->second->SetResultAttributes(pDownload->m_AttributeMap, pDownload->m_MetaXml);
		}
	}
}