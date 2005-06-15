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


#include "stdafx.h"
#include "GnuCore.h"

#include "GnuNetworks.h"
#include "GnuTransfers.h"
#include "GnuShare.h"
#include "GnuFileHash.h"
#include "GnuRouting.h"
#include "GnuControl.h"
#include "GnuDownloadShell.h"
#include "GnuPrefs.h"
#include "Dime.h"

#include "GnuUpload.h"
#include "GnuUploadQueue.h"
#include "GnuUploadShell.h"

CGnuUploadShell::CGnuUploadShell(CGnuTransfers* pTrans)
{
	m_pTrans = pTrans;
	m_pNet  = pTrans->m_pNet;
	m_pShare = pTrans->m_pCore->m_pShare;
	m_pPrefs = pTrans->m_pPrefs;


	if(pTrans->m_NextUploadID < 1)
		pTrans->m_NextUploadID = 1;

	m_UploadID = pTrans->m_NextUploadID++;
	pTrans->m_UploadMap[m_UploadID] = this;


	m_Host.S_addr = 0;
	m_Port		  = 0;
	m_Attempts	  = 1;

	m_Index	  = 0;
	m_Network = NETWORK_GNUTELLA;

	m_UpdatedInSecond = false;

	m_ChangeTime = CTime::GetCurrentTime();
 
	m_KeepAlive		   = false;
	m_RequsetPending   = false;

	m_TigerTreeRequest = false;
	m_TigerThexRequest = false;
	m_TigerTree	= NULL;
	m_TreeSize  = 0;

	m_Status         = TRANSFER_CLOSED;

	m_StartPos       = 0;
	m_CurrentPos     = 0;
	m_StopPos		 = 0;
	m_FileLength     = 0;
	m_BytesSent		 = 0;

	m_IsPartial = false;
	m_PartialID = 0;

	m_nSecsUnderLimit = 0;
	m_nSecsDead       = 0;
	m_SecsClosed      = 0;

	m_QueueRequest  = false;
	m_QueuePos		= 0;

	// Bandwidth
	m_AvgSentBytes.SetSize(30);

	
	m_AllocBytes	  = 0;
	m_AllocBytesTotal = 0;


	m_Socket = NULL;
}

CGnuUploadShell::~CGnuUploadShell()
{
	std::map<int, CGnuUploadShell*>::iterator itUp = m_pTrans->m_UploadMap.find(m_UploadID);

	if(itUp != m_pTrans->m_UploadMap.end())
		m_pTrans->m_UploadMap.erase(itUp);

	if(m_Socket)
	{
		delete m_Socket;
		m_Socket = NULL;
	}

	if(m_File.m_hFile != CFile::hFileNull)
		m_File.Abort();

	if(m_TigerTree)
		delete [] m_TigerTree;
	m_TigerTree = NULL;
	m_TreeSize  = 0;

	//m_CheckFile.Abort();
	//m_MirrorFile.Abort();
}

void CGnuUploadShell::ParseRequest(CString Handshake)
{
	if(!m_Socket)
		return;
	
	m_Index     = 0;
	m_Name      = "Unknown";
	m_Sha1Hash  = "";
	m_TigerHash = "";

	m_TigerTreeRequest = false;
	if(m_TigerTree)
		delete [] m_TigerTree;
	m_TigerTree = NULL;
	m_TreeSize  = 0;

	m_IsPartial = false;
	m_PartialID = 0;

	
	int i = 0;
	bool HeadRequest = false;
	
	

	//Parse Request-Line
	CString FirstLine = Handshake.Left(Handshake.Find("\r\n") );
	FirstLine.MakeLower();
	
	Handshake = Handshake.Mid(Handshake.Find("\r\n") + 2);
	

	// Get Http Method
	int spacepos = FirstLine.Find(" ");
	if (spacepos != -1)
	{
		m_HTTPMethod = FirstLine.Left(spacepos);
		m_HTTPMethod.MakeUpper();
	}


	// Check if bad http method
	if (m_HTTPMethod != "GET" && m_HTTPMethod != "HEAD")
	{		
		m_Name = "Not Specified";
		m_Socket->Send_HttpBadRequest();
		return;
	}

	if(m_HTTPMethod == "HEAD")
		HeadRequest = true;


	// Get Request URI
	int endpos = FirstLine.ReverseFind(' ');

	if (endpos == -1)
		m_RequestURI = FirstLine.Mid(spacepos + 1);
	else
	{
		m_RequestURI = FirstLine.Mid(spacepos + 1, endpos - spacepos -1);
		m_HTTPVersion = FirstLine.Mid(endpos + 1);
		m_HTTPVersion.MakeUpper();
	}

	m_RequestURI = DecodeURL(m_RequestURI);

	CString LowRequestURI = m_RequestURI;
	LowRequestURI.MakeLower();

	// Find requested file info
	if (LowRequestURI.Left(22).CompareNoCase("/uri-res/n2r?urn:sha1:") == 0)
	{
		m_Sha1Hash = m_RequestURI.Mid(22);
		m_Sha1Hash.MakeUpper();

		m_Index = m_pShare->m_pHash->GetHashIndex(HASH_SHA1, m_Sha1Hash);
		m_Name  = m_Sha1Hash;
	}

	else if (LowRequestURI.Left(5).CompareNoCase("/get/") == 0)
	{
		CString IndexString = m_RequestURI.Mid(5, m_RequestURI.Find("/", 5) - 5);
		
		m_Index = atoi(IndexString);
		m_Name  = m_RequestURI.Mid(5 + IndexString.GetLength() + 1); 
	}

	else if (LowRequestURI.Left(39).CompareNoCase("/gnutella/tigertree/v3?urn:tree:tiger/:") == 0)
	{
		m_TigerHash = m_RequestURI.Mid(39);
		m_TigerHash.MakeUpper();

		m_Index = m_pShare->m_pHash->GetHashIndex(HASH_TIGERTREE, m_TigerHash);
		m_Name  = m_TigerHash;

		m_TigerTreeRequest = true;
	}

	else if (LowRequestURI.Left(34).CompareNoCase("/gnutella/thex/v1?urn:tree:tiger/:") == 0)
	{
		m_TigerHash = m_RequestURI.Mid(34);
		m_TigerHash.MakeUpper();

		m_Index = m_pShare->m_pHash->GetHashIndex(HASH_TIGERTREE, m_TigerHash);
		m_Name  = m_TigerHash;

		m_TigerTreeRequest = true;
		m_TigerThexRequest = true;
	}

	else if (LowRequestURI.Left(22).CompareNoCase("/uri-res/N2X?urn:sha1:") == 0)
	{
		CString HashString = m_RequestURI.Mid(22);
		HashString.MakeUpper();

		int semipos = HashString.Find(';');

		if(semipos == -1)
			m_Sha1Hash = HashString;
		else
		{
			m_Sha1Hash  = HashString.Left(semipos);
			m_TigerHash = HashString.Mid(semipos + 1);
		}

		m_Index = m_pShare->m_pHash->GetHashIndex(HASH_SHA1, m_Sha1Hash);
		m_Name  = m_Sha1Hash;

		m_TigerTreeRequest = true;
		m_TigerThexRequest = true; 
	}

	else
	{
		m_Socket->Send_HttpNotFound();
		return;
	}


	//Parse headers
	CParsedHeaders ParsedHeaders (Handshake);


	// Parse X-Gnutella-Content-URN, its a mess we need standards arg
	CString urn = ParsedHeaders.FindHeader("X-Gnutella-Content-URN");
	if( urn.IsEmpty() )
	{
		urn = ParsedHeaders.FindHeader("Content-URN");

		if( urn.IsEmpty() )
			urn = ParsedHeaders.FindHeader("X-Content-URN");
		
		if( !urn.IsEmpty() )
			m_Network = NETWORK_G2;
	}

	urn.MakeLower();

	if (urn.Left(9) == "urn:sha1:")
	{
		m_Sha1Hash = urn.Mid(9);
		m_Sha1Hash.MakeUpper();

		if( m_Index == 0 )
			m_Index = m_pShare->m_pHash->GetHashIndex(HASH_SHA1, m_Sha1Hash);
	}


	// Get Handle to requested file, now that all needed data is collected
	if(m_Index)
	{
		CString UploadPath = m_pShare->GetFilePath(m_Index);
		m_Name = m_pShare->GetFileName(m_Index);
		
		if( UploadPath.IsEmpty())
		{
			m_Socket->Send_HttpNotFound();
			return;
		}

		m_File.Abort();
		if( !m_File.Open( UploadPath, CFile::modeRead | CFile::shareDenyWrite, true) )
		{
			m_Socket->Send_HttpInternalError();
			return;
		}

		m_Sha1Hash  = m_pShare->GetFileHash(m_Index, HASH_SHA1);
		m_TigerHash = m_pShare->GetFileHash(m_Index, HASH_TIGERTREE);

		/*if( !m_Sha1Hash.IsEmpty() )
		{
			if(m_CheckFile.m_hFile == CFile::hFileNull)
				m_CheckFile.Open( m_pPrefs->m_DownloadPath + "\\" + m_Sha1Hash.Left(4) + ".mp3", CFile::modeRead);
			
			if(m_MirrorFile.m_hFile == CFile::hFileNull)
				m_MirrorFile.Open( m_pPrefs->m_DownloadPath + "\\" + m_Name + ".MIRROR", CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite);
		}*/
	}

	// Else, no file index, try to find download with same hash
	else
	{ 
		for(i = 0; i < m_pTrans->m_DownloadList.size(); i++)
		{
			CGnuDownloadShell* p = m_pTrans->m_DownloadList[i];

			if (p->m_Sha1Hash == m_Sha1Hash)
			{
				m_Name = p->m_Name;

				// If downloading not yet finished but has at least started
				if (p->GetBytesCompleted() > 0 && p->m_File.m_hFile != CFile::hFileNull)
				{
					m_IsPartial = true;
					m_PartialID = p->m_DownloadID;
				}
			}
		}

		// If no download found..
		if(!m_IsPartial)
		{
			m_Socket->Send_HttpNotFound();
			return;
		}
	}


	// Get file length
	m_StartPos   = 0;
	m_CurrentPos = 0;
	m_StopPos    = GetFileLength();
	m_FileLength = GetFileLength();
	
	if( m_TigerTreeRequest )
		if( !LoadTigerTree() )
		{
			m_Socket->Send_HttpInternalError();
			return;
		}
	
	// Parse Keep-Alive, set as default in HTTP/1.1
	m_KeepAlive = (m_HTTPVersion == "HTTP/1.1") ? true : false;
	CString ConnectionValue = ParsedHeaders.FindHeader("Connection");
	ConnectionValue.MakeLower();

	if (ConnectionValue == "keep-alive")
		m_KeepAlive = true;
	else if (ConnectionValue == "close")
		m_KeepAlive = false;


	// If this is a partial, client must send Range:
	if (m_IsPartial && !HeadRequest)
		if (ParsedHeaders.FindHeader("Range") == "")
		{
			m_Socket->Send_HttpNotFound();
			return;
		}


	// Parse all other headers
	for (i = 0; i < ParsedHeaders.m_Headers.size(); i++)
	{
		//For each header
		CString HeaderName  = ParsedHeaders.m_Headers[i].Name;
		CString HeaderValue = ParsedHeaders.m_Headers[i].Value;
		HeaderName.MakeLower();


		// Range Header
		if (HeaderName == "range")
		{
			if( !ParseRangeHeader(HeaderValue) )
			{
				m_Socket->Send_HttpBadRequest();
				return;
			}
		}


		// User-Agent header
		else if (HeaderName == "user-agent")
		{
			m_RemoteClient = HeaderValue;

			HeaderValue.MakeLower();

			// Block normal browsers
			if(HeaderValue.Find("mozilla") != -1)
			{
				m_Socket->Send_BrowserBlock();
				return;
			}
		}

		else if (HeaderName == "server")
		{
			m_RemoteClient = HeaderValue;
		}

		// Listen-IP header
		else if(HeaderName == "listen-ip")
		{
			m_Socket->m_ListenIP = HeaderValue;
		}


		// Alt-Location header
		else if (HeaderName.Right(12) == "alt-location" || HeaderName.Right(18) == "alternate-location")
		{
			// dont add to local alt list, only hosts uploaded to are added
		}

		else if (HeaderName  == "x-alt")
		{
			// dont add to local alt list, only hosts uploaded to are added
		}

		else if (HeaderName  == "x-nalt")
		{
			m_pShare->RemoveShareAltLocation( m_Sha1Hash, AltLoctoAddress(HeaderValue));
		}

		// X-Queue header (signals that client wants to be queued)
		else if (HeaderName == "x-queue")
		{
			//Dont care about HeaderValue. Later versions are supposed to be compatible.
			m_QueueRequest = true;
		}
	}


	// If TigerTree request
	if( m_TigerTreeRequest )
	{
		m_Socket->Send_TigerTree();
		return;
	}


	// If there's no upload max or this is a head request, accept it
	if(m_pPrefs->m_MaxUploads == 0 || HeadRequest)
	{
		m_Socket->Send_HttpOK();
		return;
	}


	// Check upload queue to see if download can continue
	if( m_pTrans->m_UploadQueue.CheckReady(this) )
		m_Socket->Send_HttpOK();
	else
		m_Socket->Send_HttpBusy();



	m_pTrans->UploadUpdate(m_UploadID);
}

void CGnuUploadShell::PushFile()
{
	if(m_Socket)
		delete m_Socket;

	m_Socket = new CGnuUpload(this);

	// if udp push
	if(m_Index == F2F_PUSH_INDEX)
	{
		m_Socket->m_pSocket->RudpConnect(IPv4(m_Host, m_Port), m_pNet, false);
	}
	
	// else tcp push
	else
	{
		if(!m_Socket->m_pSocket->Create())
		{
			delete m_Socket;
			m_Socket = NULL;
			return;
		}

		if(!m_Socket->m_pSocket->Connect(IPtoStr(m_Host), m_Port))
			if(m_Socket->m_pSocket->GetLastError() != WSAEWOULDBLOCK)
			{
				m_Error = "Unable to Connect";
				StatusUpdate(TRANSFER_CLOSED);
				
				delete m_Socket;
				m_Socket = NULL;
				return;
			}
	}

	StatusUpdate(TRANSFER_PUSH);
}

void CGnuUploadShell::StatusUpdate(DWORD Status)
{
	m_nSecsDead = 0;

	m_Status = Status;
	m_ChangeTime = CTime::GetCurrentTime();
		
	m_pTrans->UploadUpdate(m_UploadID);
}

int CGnuUploadShell::GetStatus()
{
	if(m_CurrentPos == m_StopPos && m_StopPos != 0)
		return TRANSFER_COMPLETED;

	return m_Status;
}

int CGnuUploadShell::GetBytesPerSec()
{
	return m_AvgSentBytes.GetAverage();

	return 0;
}

int CGnuUploadShell::GetETD()
{
	if(GetBytesPerSec())
		return (m_StopPos - m_CurrentPos) / GetBytesPerSec();

	return 0;
}

CString CGnuUploadShell::GetFilePath()
{
	return m_pShare->GetFilePath(m_Index);
}

void CGnuUploadShell::RunFile()
{
	CString Path = GetFilePath();
				
	if(Path != "")
		ShellExecute(NULL, "open", Path, NULL, NULL, SW_SHOWNORMAL);
}

void CGnuUploadShell::Timer()
{
	if(m_Socket)
		m_Socket->Timer();

	if(m_UpdatedInSecond)
	{
		m_pTrans->UploadUpdate(m_UploadID);
		m_UpdatedInSecond = false;
	}

	
	// Check for incoming request
	if(TRANSFER_CONNECTING == m_Status ||
	   TRANSFER_CONNECTED  == m_Status ||
	   TRANSFER_QUEUED	   == m_Status)
	{
		if(m_RequsetPending && m_Socket && !m_Socket->m_ThreadRunning )
		{
			ParseRequest(m_Socket->m_GetRequest); 
			
			m_RequsetPending = false;
			m_nSecsDead = 0;
		}
	}


	// Check if connection is alive in each state
	if(TRANSFER_CONNECTING == m_Status ||
	   TRANSFER_CONNECTED  == m_Status)
	{
		m_nSecsDead++;

		if(m_nSecsDead > 15)
		{
			m_Error     = "No Response Connecting";
			m_KeepAlive = false;

			StatusUpdate(TRANSFER_CLOSED);

			if(m_Socket)
				m_Socket->Close();
		}
	}

	else if(TRANSFER_PUSH == m_Status)
	{
		// why is this here? seems recursive
		//if(	m_Name == "Unknown" && !m_FileLength)
		//	PushFile();

		m_nSecsDead++;

		if(m_nSecsDead > 15)
		{
			m_Error = "Host Unreachable";
			StatusUpdate(TRANSFER_CLOSED);
			
			if(m_Socket)
				m_Socket->Close();
		}
	}

	else if(TRANSFER_QUEUED == m_Status)
	{
		m_nSecsDead++;

		if(m_nSecsDead > MAX_POLL)
		{
			m_Error     = "No Response Queued";
			m_KeepAlive = false;

			if(m_Socket)
				m_Socket->Close();
		}

	}

	else if(TRANSFER_SENDING == m_Status)
	{
		if(m_Socket)
			if(m_AllocBytes || m_pPrefs->m_BandwidthUp == 0)
				m_Socket->m_MoreBytes.SetEvent();


		// Check for dead transfer
		if(m_AvgSentBytes.m_SecondSum == 0)
		{
			m_nSecsDead++;

			if(m_nSecsDead > 30)
			{
				m_Error     = "No Response Sending";
				m_KeepAlive = false;

				if(m_Socket)
					m_Socket->Close();
			}
		}
		else
			m_nSecsDead = 0;

		if(m_pPrefs->m_MinUpSpeed)
		{
			// Check if its under the bandwidth limit
			if((float)m_AvgSentBytes.m_SecondSum / (float)1024 < m_pPrefs->m_MinUpSpeed)
				m_nSecsUnderLimit++;
			else
				m_nSecsUnderLimit = 0;

			if(m_nSecsUnderLimit > 15)
			{	
				m_Error     = "Below Minimum Speed";
				m_KeepAlive = false;

				if(m_Socket)
					m_Socket->Close();
			}
		}

		// If entire file sent
		if(m_CurrentPos == m_StopPos)
		{
			if(!m_KeepAlive && m_Socket)
				m_Socket->Close();
		}
	}

	// Check for completion
	if(m_StopPos && m_CurrentPos == m_StopPos)
		m_pNet->m_HaveUploaded = true;


	// bandwidth average
	m_AvgSentBytes.Next();


	// Clean up Socket
	if(m_Status == TRANSFER_CLOSED)
	{
		m_SecsClosed++;

		if(m_Socket)
		{
			delete m_Socket;
			m_Socket = NULL;
		}
		
		if(m_TigerTree)
		{
			delete [] m_TigerTree;
			m_TigerTree = NULL;
		}

		if(m_File.m_hFile != CFile::hFileNull)
			m_File.Abort();
	}

	if(m_Status != TRANSFER_CLOSED)
		m_SecsClosed = 0;
}

uint64 CGnuUploadShell::GetFileLength()
{
	if (!m_IsPartial)
	{
		try
		{
			return m_File.GetLength();
		}
		catch(...)
		{
			m_Socket->Send_HttpInternalError();
		}
	}
	
	// Else it is a Partial Download
	else
	{
		std::map<int, CGnuDownloadShell*>::iterator itPart = m_pTrans->m_DownloadMap.find(m_PartialID);

		if(itPart != m_pTrans->m_DownloadMap.end())
			return itPart->second->m_FileLength;
	}

	return 0;
}

bool CGnuUploadShell::ParseRangeHeader(CString Value)
{
	// Example HeaderValue: "bytes=100-200"
	Value.MakeLower();
	
	Value.Replace("bytes ", "bytes=");	//Some wrongly send bytes x-y in Range header
	Value.Remove(' ');

	if (Value.Left(6) != "bytes=")
	{
		m_Socket->Send_HttpBadRequest();
		return false;
	}

	int dashpos = Value.Find("-");
	if (dashpos == -1)
	{
		m_Socket->Send_HttpBadRequest();
		return false;
	}

	// bytes=FirstValue-LastValue
	CString FirstValue = Value.Mid(6, dashpos-6);
	CString LastValue  = Value.Mid(dashpos+1);

	// Full range request
	if (!FirstValue.IsEmpty() && !LastValue.IsEmpty())
	{	
		m_StartPos = _atoi64(FirstValue);
		m_StopPos  = _atoi64(LastValue) + 1;
	}

	// Only start byte supplied
	else if (!FirstValue.IsEmpty())
	{	
		m_StartPos  = _atoi64(FirstValue);
	}

	// Negative request. Get the last <LastValue> bytes of file.
	else if (!LastValue.IsEmpty())
	{
		uint64 suffixlen = _atoi64(LastValue);

		if (suffixlen < m_FileLength)
			m_StartPos = m_FileLength - suffixlen;
		else
			m_StartPos = 0;	//Get full file
	}
	
	// Bad request
	else
	{
		m_Socket->Send_HttpBadRequest();
		return false;
	}

	if(m_StartPos >= m_StopPos || m_StopPos > m_FileLength)
	{
		m_Socket->Send_HttpBadRequest();
		return false;
	}


	m_CurrentPos = m_StartPos;


	// If not an upload of a partial
	if (m_IsPartial)
	{
		CGnuDownloadShell* p = NULL;		
		std::map<int, CGnuDownloadShell*>::iterator itPart = m_pTrans->m_DownloadMap.find(m_PartialID);

		if(itPart != m_pTrans->m_DownloadMap.end())
			p = itPart->second;
		else
		{
			m_Socket->Send_HttpRangeNotAvailable();
			return false;
		}
		
		bool RangeOk = false;
		bool chain   = false;
		uint64  StartByte  = m_StartPos;
		uint64  EndPos     = m_StopPos;

		for(int i = 0; i < p->m_PartList.size(); i++)
			if( p->m_PartList[i].Verified )
			{
				if( StartByte >= p->m_PartList[i].StartByte && StartByte <= p->m_PartList[i].EndByte)
				{
					if((StartByte >= p->m_PartList[i].StartByte && m_StopPos - 1 <= p->m_PartList[i].EndByte) ||
						(chain && m_StopPos - 1 <= p->m_PartList[i].EndByte))
					{
						RangeOk = true;
						break;
					}
					else 
						chain = true;
				}
			}
			else 
				chain = false;


		if (!RangeOk)
		{
			m_Socket->Send_HttpRangeNotAvailable();
			return false;
		}
	}

	return true;
}

bool CGnuUploadShell::LoadTigerTree()
{
	m_pShare->m_FilesAccess.Lock();

		std::vector<SharedFile>::iterator itFile;
		for (itFile = m_pShare->m_SharedFiles.begin(); itFile != m_pShare->m_SharedFiles.end(); itFile++)
			if(m_Index == (*itFile).Index) 
			{
					
				if((*itFile).TigerTree == NULL || (*itFile).TreeSize == 0)
				{
					m_pShare->m_FilesAccess.Unlock();
					return false;
				}

				if(m_TigerTree)
					delete [] m_TigerTree;

				// if thex 
				if( m_TigerThexRequest )
				{
					int TigerBuffSize = (*itFile).TreeSize + 4096;

					m_TigerTree = new byte[TigerBuffSize]; // extra space for dime
					
					// write xml record
					CString XmlRecordType = "text/xml";
					CString XmlRecordData;
					XmlRecordData += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
					XmlRecordData += "<!DOCTYPE hashtree SYSTEM \"http://open-content.net/spec/thex/thex.dtd\">";
					XmlRecordData += "<hashtree>";
					XmlRecordData += "<file size='" + NumtoStr((*itFile).Size) + "' segmentsize='1024'/>";
					XmlRecordData += "<digest algorithm='http://open-content.net/spec/digest/tiger' outputsize='24'/>";
					XmlRecordData += "<serializedtree depth='" + NumtoStr((*itFile).TreeDepth) + "' type='http://open-content.net/spec/thex/breadthfirst' uri='uuid:29242d8b-0e8e-41f2-a970-aea8e92231d9'/>";
			
					DIME TigerDime(m_TigerTree, TigerBuffSize);
					m_TreeSize = TigerDime.WriteRecord(0x0C, 0x10, "", "text/xml", (LPCSTR) XmlRecordData, XmlRecordData.GetLength());
				
					// write binary record
					CString TigerRecordType = "http://open-content.net/spec/thex/breadthfirst";
					CString TigerRecordID   = "uuid:29242d8b-0e8e-41f2-a970-aea8e92231d9";
				
					m_TreeSize += TigerDime.WriteRecord(0x0A, 0x20, TigerRecordID, TigerRecordType, (*itFile).TigerTree, (*itFile).TreeSize);
				}	

				else
				{
					m_TigerTree = new byte[(*itFile).TreeSize];
					m_TreeSize  = (*itFile).TreeSize;
					memcpy(m_TigerTree, (*itFile).TigerTree, m_TreeSize);
				}

				m_StopPos    = m_TreeSize;
				m_FileLength = m_TreeSize;

				m_pShare->m_FilesAccess.Unlock();

				return true;
			}

	m_pShare->m_FilesAccess.Unlock();

	return false;
}

bool operator > (const CGnuUploadShell &Transfer1, const CGnuUploadShell &Transfer2)
{
	if(Transfer1.m_ChangeTime > Transfer2.m_ChangeTime)
		return true;
	else
		return false;
}

