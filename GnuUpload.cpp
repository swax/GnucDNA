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
#include "GnuCore.h"
#include "DnaCore.h"
#include "DnaEvents.h"

#include "GnuTransfers.h"
#include "GnuShare.h"
#include "GnuNetworks.h"
#include "GnuControl.h"
#include "GnuDownloadShell.h"
#include "GnuPrefs.h"
#include "GnuFileHash.h"

#include "GnuUploadShell.h"
#include "GnuUpload.h"


UINT UploadWorker(LPVOID pVoidUpload);


CGnuUpload::CGnuUpload(CGnuUploadShell* pShell)
{
	m_pShell = pShell;
	m_pTrans = pShell->m_pTrans;
	m_pNet   = pShell->m_pNet;
	m_pShare = pShell->m_pShare;
	m_pPrefs = pShell->m_pPrefs;

	// Clean Up	
	m_pUploadThread = NULL;
	m_ThreadRunning = false;

	m_Authorized = false;

	m_Push = false;
}

CGnuUpload::~CGnuUpload()
{
	m_pShell->m_Status = TRANSFER_CLOSED;

	// Flush receive buffer
	byte pBuff[4096];
	while(Receive(pBuff, 4096) > 0)
		;

	if(m_hSocket != INVALID_SOCKET)
		AsyncSelect(0);

	m_CanWrite.SetEvent();
	m_MoreBytes.SetEvent();

	// let thread die
	GnuEndThread(m_pUploadThread);

	
	if(m_pShell->m_File.m_hFile != CFile::hFileNull)
		m_pShell->m_File.Abort();
}


// Do not edit the following lines, which are needed by ClassWizard.
#if 0
BEGIN_MESSAGE_MAP(CGnuUpload, CAsyncSocket)
	//{{AFX_MSG_MAP(CGnuUpload)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
#endif	// 0

/////////////////////////////////////////////////////////////////////////////
// CGnuUpload member functions

void CGnuUpload::OnConnect(int nErrorCode) 
{
	if(nErrorCode)
	{
		return;
	}

	m_Push = true;
	
	CString HttpGiv;
	
	if( m_pShell->m_Network == NETWORK_GNUTELLA )
		// GIV FileID:ClientID/FileName\n\n
		HttpGiv = "GIV " + NumtoStr(m_pShell->m_Index) + ":" + EncodeBase16((byte*) &m_pPrefs->m_ClientID, 16) + "/" + m_pShell->m_Name + "\n\n";

	if( m_pShell->m_Network == NETWORK_G2 )
		// PUSH guid:ClientID\r\n\r\n
		HttpGiv = "PUSH guid:" + EncodeBase16((byte*) &m_pPrefs->m_ClientID, 16) + "\r\n\r\n";


	Send(HttpGiv, HttpGiv.GetLength());
	m_pShell->StatusUpdate(TRANSFER_CONNECTED);

	m_pShell->m_Handshake  = "";
	m_pShell->m_Handshake += HttpGiv;
	
	CAsyncSocket::OnConnect(nErrorCode);
}

void CGnuUpload::OnReceive(int nErrorCode) 
{
	byte pBuff[6000];

	int dwBuffLength = Receive(pBuff, 4096);

	switch (dwBuffLength)
	{
	case 0:
		m_pShell->m_Error = "Bad Push";
		Close();
		return;
		break;
	case SOCKET_ERROR:
		m_pShell->m_Error = "Bad Push";
		Close();
		return;
		break;
	}

	pBuff[dwBuffLength] = 0;
	CString Header(pBuff);


	// Clear old GetRequest when new one comes in
	if(m_GetRequest.Find("\r\n\r\n") != -1)
	{
		m_GetRequest = "";
		m_pShell->m_Handshake  = "";
	}

	m_pShell->m_Handshake += Header;
	m_GetRequest += Header;


	// New Upload
	if(m_GetRequest.Find("\r\n\r\n") != -1)
	{
		if(m_GetRequest.Left(4) == "GET " || m_GetRequest.Left(5) == "HEAD ")
		{	
			// Get Node info
			CString Host;
			UINT    nPort;
			GetPeerName(Host, nPort);
			

			// Check if it's a blocked Host 
			if (m_pPrefs->BlockedIP(StrtoIP(Host)) == true) 
			{ 
				m_pShell->m_Error = "Blocked"; 
				Send_ClientBlock(Host); 
				Close(); 
			} 
			else 
			{ 
				// Set Variables 
				m_pShell->m_Host = StrtoIP(Host); 
				m_pShell->m_Port = 0; 
				m_pShell->m_RequsetPending = true;
			}

		}
		else if( m_GetRequest.Left(4) == "AUTH" && !m_pShell->m_Challenge.IsEmpty() )
		{		
			CParsedHeaders ParsedHeaders(m_pShell->m_Handshake);
			
			if(m_pShell->m_ChallengeAnswer.Compare( ParsedHeaders.FindHeader("X-Auth-Response") ) == 0 )
			{
				m_Authorized = true;
				StartUpload();
			}
			else
			{
				m_pShell->m_Error = "Authentication Failed";
				Close();
			}
		}

		else
		{
			m_pShell->m_Error = "Bad Push";
			Close();
		}
	}

	CAsyncSocket::OnReceive(nErrorCode);
}

void CGnuUpload::Send_HttpOK()
{	
	if((CGnuUpload*) m_pShell->m_Socket != this)
		return;


	CString HttpOK;

	//HTTP Version
	if (m_pShell->m_HTTPVersion == "HTTP/1.1")
		HttpOK =  "HTTP/1.1 ";
	else
		HttpOK =  "HTTP ";

	//200 or 206
	if (m_pShell->m_StartPos == 0 && m_pShell->m_StopPos == m_pShell->m_FileLength)
		HttpOK +=  "200 OK\r\n";
	else
		HttpOK +=  "206 Partial Content\r\n";

	// Server
	HttpOK += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";
	
	// Content-Type
	HttpOK += "Content-type: application/octet-stream\r\n";	//I think octet-stream is more correct than binary
	//HttpOK += "Content-type: application/zip\r\n";

	// Accept-Ranges
	HttpOK += "Accept-Ranges: bytes\r\n";

	// Conent-Range, it is "bytes x-y/z" not "bytes=x-y/z"
	HttpOK += "Content-Range: bytes " + NumtoStr(m_pShell->m_StartPos) + "-" + NumtoStr(m_pShell->m_StopPos - 1) + "/" + NumtoStr(m_pShell->m_FileLength) + "\r\n";
	
	// Content-Length
	HttpOK += "Content-Length: " + NumtoStr(m_pShell->m_StopPos - m_pShell->m_StartPos) + "\r\n";

	// Connection
	if (m_pShell->m_KeepAlive)
		HttpOK += "Connection: Keep-Alive\r\n";	//Even on 1.1 where keep-alive is assumed
	else if (m_pShell->m_HTTPVersion == "HTTP/1.1")
		HttpOK += "Connection: Close\r\n";		//Only in 1.1. Close is assumed in 1.0


	std::map<int, CGnuDownloadShell*>::iterator itPart;

	// X-Available-Ranges
	if (m_pShell->m_IsPartial)
	{	
		itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

		if(itPart != m_pTrans->m_DownloadMap.end())
			HttpOK += "X-Available-Ranges: bytes " + itPart->second->AvailableRangesCommaSeparated() + "\r\n";
	}

	// X-Gnutella-Content-URN
	if (!m_pShell->m_Sha1Hash.IsEmpty())
	{
		if( m_pShell->m_Network == NETWORK_GNUTELLA )
			HttpOK += "X-Gnutella-Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		else if( m_pShell->m_Network == NETWORK_G2 )
			HttpOK += "Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";

		// X-TigerTree-Path
		if (!m_pShell->m_TigerHash.IsEmpty())
		{
			CString Client = m_pShell->m_RemoteClient;
			Client.MakeLower();

			// handle a million implementations
			if( Client.Find("dna") != -1)
				HttpOK += "X-TigerTree-Path: /gnutella/tigertree/v3?urn:tree:tiger/:" + m_pShell->m_TigerHash + "\r\n";
			else if( Client.Find("bearshare") != -1 || Client.Find("shareaza") != -1)
				HttpOK += "X-Thex-URI: /gnutella/thex/v1?urn:tree:tiger/:" + m_pShell->m_TigerHash + "\r\n";
			else
				HttpOK += "X-Thex-URI: /uri-res/N2X?urn:sha1:" + m_pShell->m_Sha1Hash + ";" + m_pShell->m_TigerHash + "\r\n";
		}

		// X-Alt
		if (!m_pShell->m_IsPartial)
			HttpOK += m_pShare->GetShareAltLocHeader(m_pShell->m_Sha1Hash, m_pShell->m_Host);
		else
		{
			itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

			if(itPart != m_pTrans->m_DownloadMap.end())
				HttpOK += itPart->second->GetAltLocHeader( m_pShell->m_Host );
		}
	}

	// X-Push-Proxy
	if(m_Push && m_pShell->m_Network == NETWORK_GNUTELLA && m_pNet->m_pGnu)
		HttpOK += m_pNet->m_pGnu->GetPushProxyHeader();

	// X-Filename
	if(m_pShell->m_RequestURI.Left(9) == "/uri-res/")
		HttpOK += "X-Filename: " + m_pShell->m_Name + "\r\n";

	// Authentication
	if( !m_Authorized )
	{
		if(m_pNet->m_pCore->m_dnaCore->m_dnaEvents)
			m_pNet->m_pCore->m_dnaCore->m_dnaEvents->UploadAuthenticate(m_pShell->m_UploadID);
 
		if(m_pShell->m_Challenge == "ERROR")
		{
			Close();
			return;
		}

		if(!m_pShell->m_Challenge.IsEmpty() && !m_pShell->m_ChallengeAnswer.IsEmpty())
			HttpOK += "X-Auth-Challenge: " + m_pShell->m_Challenge + "\r\n";
		else
			m_Authorized = true;
	}

	// End header
	HttpOK += "\r\n";

	// Send header
	Send(HttpOK, HttpOK.GetLength());

	m_pShell->m_Handshake += HttpOK;


	if(m_Authorized)
		StartUpload();
}

void CGnuUpload::StartUpload()
{
	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		m_pShell->StatusUpdate(TRANSFER_SENDING);

		
		if(!m_ListenIP.IsEmpty() && !m_pShell->m_Sha1Hash.IsEmpty())
			m_pShare->AddShareAltLocation(m_pShell->m_Sha1Hash, m_ListenIP);
		
		// Start upload thread
		GnuEndThread(m_pUploadThread);

		m_ThreadRunning = true;
		GnuStartThread(m_pUploadThread, UploadWorker, this);
	}

	// Else HEAD request
	else
	{
		if(m_pShell->m_KeepAlive)
			m_pShell->StatusUpdate(TRANSFER_CONNECTED);
		else
			m_pShell->StatusUpdate(TRANSFER_CLOSED);
	}
}	

void CGnuUpload::Send_TigerTree()
{
	if((CGnuUpload*) m_pShell->m_Socket != this)
		return;


	CString HttpTree;

	//HTTP Version
	if (m_pShell->m_HTTPVersion == "HTTP/1.1")
		HttpTree =  "HTTP/1.1 ";
	else
		HttpTree =  "HTTP ";

	//200 or 206
	if (m_pShell->m_StartPos == 0)
		HttpTree +=  "200 OK\r\n";
	else
		HttpTree +=  "206 Partial Content\r\n";

	// Server
	HttpTree += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";
	
	// Content-Type
	if(m_pShell->m_TigerThexRequest)
		HttpTree += "Content-type: application/dime\r\n";
	else
		HttpTree += "Content-type: application/tigertree-breadthfirst\r\n";	

	// Accept-Ranges
	HttpTree += "Accept-Ranges: bytes\r\n";

	// Conent-Range, it is "bytes x-y/z" not "bytes=x-y/z"
	HttpTree += "Content-Range: bytes " + NumtoStr(m_pShell->m_StartPos) + "-" + NumtoStr(m_pShell->m_StopPos - 1) + "/" + NumtoStr(m_pShell->m_FileLength) + "\r\n";
	
	// Content-Length
	HttpTree += "Content-Length: " + NumtoStr(m_pShell->m_StopPos - m_pShell->m_StartPos) + "\r\n";

	// Connection
	if (m_pShell->m_KeepAlive)
		HttpTree += "Connection: Keep-Alive\r\n";	//Even on 1.1 where keep-alive is assumed
	else if (m_pShell->m_HTTPVersion == "HTTP/1.1")
		HttpTree += "Connection: Close\r\n";		//Only in 1.1. Close is assumed in 1.0

	
	// End header
	HttpTree += "\r\n";

	// Send header
	Send(HttpTree, HttpTree.GetLength());

	m_pShell->m_Handshake += HttpTree;


	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		m_pShell->StatusUpdate(TRANSFER_SENDING);


		// Start upload thread
		GnuEndThread(m_pUploadThread);

		m_ThreadRunning = true;
		GnuStartThread(m_pUploadThread, UploadWorker, this);
	}

	// Else HEAD request
	else
	{
		if(m_pShell->m_KeepAlive)
			m_pShell->StatusUpdate(TRANSFER_CONNECTED);
		else
			m_pShell->StatusUpdate(TRANSFER_CLOSED);
	}
}

//This server's upload max has been met
void CGnuUpload::Send_HttpBusy()
{
	CString Http503;
	
	Http503 +=  "HTTP 503 Upload Slots Full\r\n";

	// Server
	Http503 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content Type
	Http503 += "Content-type:text/html\r\n";

	
	std::map<int, CGnuDownloadShell*>::iterator itPart;

	//Send X-Available-Ranges header
	if (m_pShell->m_IsPartial)
	{	
		itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

		if(itPart != m_pTrans->m_DownloadMap.end())	
			Http503 += "X-Available-Ranges: bytes " + itPart->second->AvailableRangesCommaSeparated() + "\r\n";
	}


	if (m_pShell->m_QueueRequest)
	{
		m_pShell->m_QueuePos = m_pTrans->m_UploadQueue.GetHostPos(m_pShell);
		
		if(m_pShell->m_QueuePos)
		{
			//Queued
			int PollMin = MAX_POLL / 2;
			Http503 += "X-Queue: position=" + NumtoStr(m_pShell->m_QueuePos) + ",length=" + NumtoStr(m_pTrans->m_UploadQueue.m_Queue.size()) + ",limit=" + NumtoStr(m_pTrans->m_UploadQueue.m_SameHostLimit) + ",pollMin=" + NumtoStr(PollMin) + ",pollMax=" + NumtoStr(MAX_POLL) + "\r\n";
			Http503 += "Connection: Keep-Alive\r\n";
		}
		else
		{
			//Queue is full - also 5 min retry for same host
			//TODO: 10 minutes pollMin when queue is full, ok?
			Http503 += "X-Queue: position=full,length=" + NumtoStr(m_pTrans->m_UploadQueue.m_Queue.size()) + ",limit=" + NumtoStr(m_pTrans->m_UploadQueue.m_SameHostLimit) + ",pollMin=" + NumtoStr(FULL_POLL) + "\r\n";
			Http503 += "Connection: Close\r\n";
		}
	}


	// X-Gnutella-Content-URN
	if (!m_pShell->m_Sha1Hash.IsEmpty())
	{
		if( m_pShell->m_Network == NETWORK_GNUTELLA )
			Http503 += "X-Gnutella-Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		else if( m_pShell->m_Network == NETWORK_G2 )
			Http503 += "Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";

		// X-Alt
		if (!m_pShell->m_IsPartial)
			Http503 += m_pShare->GetShareAltLocHeader(m_pShell->m_Sha1Hash, m_pShell->m_Host);
		else
		{
			itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

			if(itPart != m_pTrans->m_DownloadMap.end())	
				Http503 += itPart->second->GetAltLocHeader( m_pShell->m_Host );
		}
	}

	// X-Push-Proxy
	if(m_Push && m_pShell->m_Network == NETWORK_GNUTELLA && m_pNet->m_pGnu)
		Http503 += m_pNet->m_pGnu->GetPushProxyHeader();

	// Truncate header
	Http503 = LimitHeadersLength(Http503);	//Truncate to max 4090 bytes

	// End header
	Http503 += "\r\n";

	//Send describing info if GET req, and not Queue req
	//Do not send on Queue, since there is no Content-Length header.
	//Besides, this is definately an automatic download agent, who wont need any info webpage
	if (m_pShell->m_HTTPMethod == "GET"	&& !m_pShell->m_QueueRequest)
	{
		Http503 += "<HTML>\r\n";
		Http503 += "<HEAD><TITLE>503 Server Busy</TITLE></HEAD>\r\n";
		Http503 += "<BODY>\r\n";
		Http503 += "<H1>Server Busy</H1>\r\n";
		Http503 += "This server's upload max has been met, try again later.\r\n";
		Http503 += "</BODY>\r\n";
		Http503 += "</HTML>\r\n";
	}
			
	// Send header
	Send(Http503, Http503.GetLength());

	m_pShell->m_Handshake += Http503;


	// Show if upload is queued
	if (m_pShell->m_QueueRequest)
	{
		if (m_pShell->m_QueuePos)
		{
			//Queued
			//TODO: Multiple uplaods with show the same queue pos. Perhaps just say "(Queued)"
			m_pShell->m_Error = "";
			m_pShell->StatusUpdate(TRANSFER_QUEUED);
		}
		else
		{
			//Queue is full
			m_pShell->m_Error = "Queue is Full";
			Close();
		}
	}
	else
	{
		m_pShell->m_Error = "No Upload Slots";
		Close();
	}

	

}

//Remote host allready downloading from this host
void CGnuUpload::Send_HttpFailed()
{
	CString Http503;
	
	Http503 +=  "HTTP 503 Limit reached\r\n";
	
	// Server
	Http503 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content-Type
	Http503 += "Content-type:text/html\r\n";

	std::map<int, CGnuDownloadShell*>::iterator itPart;

	//X-Available-Ranges
	if (m_pShell->m_IsPartial)
	{
		itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

		if(itPart != m_pTrans->m_DownloadMap.end())	
			Http503 += "X-Available-Ranges: bytes " + itPart->second->AvailableRangesCommaSeparated() + "\r\n";
	}

	// X-Gnutella-Content-URN
	if (!m_pShell->m_Sha1Hash.IsEmpty())
	{
		if( m_pShell->m_Network == NETWORK_GNUTELLA )
			Http503 += "X-Gnutella-Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		else if( m_pShell->m_Network == NETWORK_G2 )
			Http503 += "Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		
		// X-Alt
		if (!m_pShell->m_IsPartial)
			Http503 += m_pShare->GetShareAltLocHeader(m_pShell->m_Sha1Hash, m_pShell->m_Host);
		else
		{
			itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

			if(itPart != m_pTrans->m_DownloadMap.end())	
				Http503 += itPart->second->GetAltLocHeader( m_pShell->m_Host );
		}
	}

	// X-Push-Proxy
	if(m_Push && m_pShell->m_Network == NETWORK_GNUTELLA && m_pNet->m_pGnu)
		Http503 += m_pNet->m_pGnu->GetPushProxyHeader();

	// Truncate header
	Http503 = LimitHeadersLength(Http503);	//Truncate to max 4090 bytes

	// End header
	Http503 += "\r\n";

	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		Http503 += "<HTML>\r\n";
		Http503 += "<HEAD><TITLE>503 Limit reached</TITLE></HEAD>\r\n";
		Http503 += "<BODY>\r\n";
		Http503 += "<H1>Already Downloading File</H1>\r\n";
		Http503 += "You may only download one file at a time from this server.\r\n";
		Http503 += "</BODY>\r\n";
		Http503 += "</HTML>\r\n";
	}

	// Send header
	Send(Http503, Http503.GetLength());

	m_pShell->m_Error = "Host Already Downloading";
	m_pShell->m_Handshake += Http503;

	Close();
}


void CGnuUpload::Send_HttpNotFound()
{
	CString Http404;
	
	Http404 +=  "HTTP 404 Not Found\r\n";

	// Server
	Http404 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content-Type
	Http404 += "Content-type:text/html\r\n";
		
	// End header
	Http404 += "\r\n";
	
	// If GET reqeust
	if (m_pShell->m_HTTPMethod == "GET")
	{
		Http404 += "<HTML>\r\n";
		Http404 += "<HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n";
		Http404 += "<BODY>\r\n";
		Http404 += "<H1>Not Found</H1>\r\n";
		Http404 += "The requested file " + m_pShell->m_RequestURI + " was not found on this server.\r\n";
		Http404 += "</BODY>\r\n";
		Http404 += "</HTML>\r\n";
	}

	// Send header
	Send(Http404, Http404.GetLength());

	m_pShell->m_Error = "File Not Found";
	m_pShell->m_Handshake += Http404;

	Close();
}

void CGnuUpload::Send_HttpBadRequest()
{
	CString Http400;
	
	Http400 +=  "HTTP 400 Bad Request\r\n";

	// Server
	Http400 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content-Type
	Http400 += "Content-type:text/html\r\n";
		
	// End header
	Http400 += "\r\n";

	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		Http400 += "<HTML>\r\n";
		Http400 += "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n";
		Http400 += "<BODY>\r\n";
		Http400 += "<H1>Bad Request</H1>\r\n";
		Http400 += "The request could not be understood by the server due to malformed syntax.\r\n";
		Http400 += "</BODY>\r\n";
		Http400 += "</HTML>\r\n";
	}

	// Send header
	Send(Http400, Http400.GetLength());

	m_pShell->m_Error = "Bad Request";
	m_pShell->m_Handshake += Http400;

	Close();
}

void CGnuUpload::Send_HttpInternalError()
{
	CString Http500;
	
	Http500 +=  "HTTP 500 Internal Server Error\r\n";
	
	// Server
	Http500 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content-Type
	Http500 += "Content-type:text/html\r\n";
	
	// X-Gnutella-Content-URN
	if (!m_pShell->m_Sha1Hash.IsEmpty())
	{
		if( m_pShell->m_Network == NETWORK_GNUTELLA )
			Http500 += "X-Gnutella-Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		else if( m_pShell->m_Network == NETWORK_G2 )
			Http500 += "Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		
		// X-Alt
		if (!m_pShell->m_IsPartial)
			Http500 += m_pShare->GetShareAltLocHeader(m_pShell->m_Sha1Hash, m_pShell->m_Host);
		else
		{
			std::map<int, CGnuDownloadShell*>::iterator itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

			if(itPart != m_pTrans->m_DownloadMap.end())
				Http500 += itPart->second->GetAltLocHeader( m_pShell->m_Host );
		}
	}

	// X-Push-Proxy
	if(m_Push && m_pShell->m_Network == NETWORK_GNUTELLA && m_pNet->m_pGnu)
		Http500 += m_pNet->m_pGnu->GetPushProxyHeader();

	// Truncate header
	Http500 = LimitHeadersLength(Http500);	//Truncate to max 4090 bytes

	// End header
	Http500 += "\r\n";

	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		Http500 += "<HTML>\r\n";
		Http500 += "<HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n";
		Http500 += "<BODY>\r\n";
		Http500 += "<H1>Internal Server Error</H1>\r\n";
		Http500 += "The server encountered an unexpected condition which prevented it from fulfilling the request.\r\n";
		Http500 += "</BODY>\r\n";
		Http500 += "</HTML>\r\n";
	}
	
	// Send header
	Send(Http500, Http500.GetLength());

	m_pShell->m_Error = "Internal Server Error";
	m_pShell->m_Handshake += Http500;

	Close();
}

void CGnuUpload::Send_BrowserBlock()
{
	CString Http403;
	
	Http403 +=  "HTTP 403 Browser access blocked\r\n";

	// Server
	Http403 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content-Type
	Http403 += "Content-type:text/html\r\n";
		
	// End header
	Http403 += "\r\n";

	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		Http403 += "<HTML>\r\n";
		Http403 += "<HEAD><TITLE>Download Gnucleus to get this file!</TITLE></HEAD>\r\n";
		Http403 += "<BODY>\r\n";
		Http403 += "<H1>Download Gnucleus to get this file</H1>\r\n";
		Http403 += "This file is available on the Gnutella filesharing network.\r\n";
		Http403 += "To get this file you have to get a Gnutella client that lets you \r\n";
		Http403 += "share back to the network.<br><br>\r\n";
		Http403 += "Download Gnucleus at <a href=\"http://www.gnucleus.com\">http://www.gnucleus.com</a>\r\n";
		Http403 += "</BODY>\r\n";
		Http403 += "</HTML>\r\n";
	}

	// Send header
	Send(Http403, Http403.GetLength());

	m_pShell->m_Error = "Brower Blocked";
	m_pShell->m_Handshake += Http403;

	Close();
}

void CGnuUpload::Send_ClientBlock(CString ClientName)
{
	CString Http403;
	
	Http403 +=  "HTTP 403 Access blocked\r\n";

	// Server
	Http403 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content-Type
	Http403 += "Content-type:text/html\r\n";

	// End header
	Http403 += "\r\n";

	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		Http403 += "<HTML>\r\n";
		Http403 += "<HEAD><TITLE>Download Gnucleus to get this file!</TITLE></HEAD>\r\n";
		Http403 += "<BODY>\r\n";
		Http403 += "<H1>Download Gnucleus to get this file</H1>\r\n";
		Http403 += "This file is available on the Gnutella filesharing network.\r\n";
		Http403 += "To get this file you have to get a Gnutella client that lets you \r\n";
		Http403 += "share back to the network.<br><br>\r\n";
		Http403 += "Download Gnucleus at <a href=\"http://www.gnucleus.com\">http://www.gnucleus.com</a>\r\n";
		Http403 += "</BODY>\r\n";
		Http403 += "</HTML>\r\n";
	}

	// Send header
	Send(Http403, Http403.GetLength());

	m_pShell->m_Error = "Client " + ClientName + " Blocked";
	m_pShell->m_Handshake += Http403;

	Close();
}

//Remote host requested a range for a partial file that is not yet available
void CGnuUpload::Send_HttpRangeNotAvailable()
{
	CString Http503;
	
	Http503 +=  "HTTP 503 Requested Range Not Available\r\n";

	// Server
	Http503 += "Server: " + m_pNet->m_pCore->GetUserAgent() + "\r\n";

	// Content-Type
	Http503 += "Content-type:text/html\r\n";
	
	// X-Available-Ranges
	std::map<int, CGnuDownloadShell*>::iterator itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

	if(itPart != m_pTrans->m_DownloadMap.end())
		Http503 += "X-Available-Ranges: bytes " + itPart->second->AvailableRangesCommaSeparated() + "\r\n";

	// X-Gnutella-Content-URN
	if (!m_pShell->m_Sha1Hash.IsEmpty())
	{
		if( m_pShell->m_Network == NETWORK_GNUTELLA )
			Http503 += "X-Gnutella-Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		else if( m_pShell->m_Network == NETWORK_G2 )
			Http503 += "Content-URN: urn:sha1:" + m_pShell->m_Sha1Hash + "\r\n";
		
		// X-Alt
		if (!m_pShell->m_IsPartial)
			Http503 += m_pShare->GetShareAltLocHeader(m_pShell->m_Sha1Hash, m_pShell->m_Host);
		else
		{
			itPart = m_pTrans->m_DownloadMap.find(m_pShell->m_PartialID);

			if(itPart != m_pTrans->m_DownloadMap.end())
				Http503 += itPart->second->GetAltLocHeader( m_pShell->m_Host );
		}
	}

	// X-Push-Proxy
	if(m_Push && m_pShell->m_Network == NETWORK_GNUTELLA && m_pNet->m_pGnu)
		Http503 += m_pNet->m_pGnu->GetPushProxyHeader();

	// Truncate header
	Http503 = LimitHeadersLength(Http503);	//Truncate to max 4090 bytes

	// End header
	Http503 += "\r\n";

	// If GET request
	if (m_pShell->m_HTTPMethod == "GET")
	{
		Http503 += "<HTML>\r\n";
		Http503 += "<HEAD><TITLE>HTTP 503 Requested Range Not Available\r\n</TITLE></HEAD>\r\n";
		Http503 += "<BODY>\r\n";
		Http503 += "<H1>Requested Range Not Available\r\n</H1>\r\n";
		Http503 += "Only parts of the requested file are available. No part of the requested range is available\r\n";
		Http503 += "</BODY>\r\n";
		Http503 += "</HTML>\r\n";
	}

	// Send header
	Send(Http503, Http503.GetLength());

	m_pShell->m_Error = "Host Already Downloading";
	m_pShell->m_Handshake += Http503;

	Close();
}


int CGnuUpload::Send(const void* lpBuf, int nBufLen, int nFlags) 
{
	CAutoLock SendLock(&m_SendSection);

	return CAsyncSocket::Send(lpBuf, nBufLen, nFlags);
}


void CGnuUpload::OnSend(int nErrorCode) 
// keep buffer full
{	
	m_CanWrite.SetEvent();

	CAsyncSocket::OnSend(nErrorCode);
}

void CGnuUpload::OnClose(int nErrorCode) 
{
	if(m_pShell->m_Error == "")
		m_pShell->m_Error = "Remotely Canceled";

	Close();
		
	CAsyncSocket::OnClose(nErrorCode);
}

void CGnuUpload::Close()
{
	if(m_hSocket != INVALID_SOCKET)
	{
		AsyncSelect(0);
		ShutDown(2);

		CAsyncSocket::Close();
	}


	// Close file
	if(m_pShell->m_File.m_hFile != CFile::hFileNull)
		m_pShell->m_File.Abort();

	m_pShell->StatusUpdate(TRANSFER_CLOSED);

	m_CanWrite.SetEvent();
	m_MoreBytes.SetEvent();
}

void CGnuUpload::Timer()
{
	// Sometimes OnSend doesnt get called
	m_CanWrite.SetEvent();
}

UINT UploadWorker(LPVOID pVoidUpload)
{
	//AfxMessageBox("Worker Thread Started");
	//TRACE0("*** Upload Thread Started\n");

	CGnuUpload*      pSock  = (CGnuUpload*) pVoidUpload;
	CGnuUploadShell* pShell = pSock->m_pShell;
	CGnuNetworks*    pNet   = pShell->m_pNet;
	CGnuPrefs*		 pPrefs = pShell->m_pPrefs;
	CGnuTransfers*   pTrans = pShell->m_pTrans;

	int BytesRead = 0;
	int BytesSent = 0;

	byte pBuff[SEND_BUFF];


	// Stream Test
	/*byte chkBuff[SEND_BUFF];
	if(pShell->m_Name == "stream.mp3")
	{
		memset(pBuff, 0, SEND_BUFF);
		memset(chkBuff, 0, SEND_BUFF);
	}*/

	// Transport buffer mod test
	//int sendsize = SEND_BUFF;
	//int varlen = 4;
	//pSock->SetSockOpt(SO_SNDBUF, &sendsize, varlen);


	while(pShell->m_Status == TRANSFER_SENDING)
		// If bytes still need to be sent
		if(pShell->m_CurrentPos < pShell->m_StopPos)
		{
			// Send chunk of bytes read from file
			if(BytesSent < BytesRead)
			{
				// Stream Test
				//if(pShell->m_Name == "stream.mp3")
				//	if( memcmp(pBuff, chkBuff, SEND_BUFF) != 0 )
				//		pShell->m_pNet->m_pCore->DebugLog("Upload Check Failed at " + CommaIze(NumtoStr(pShell->m_BytesSent)));


				int AttemptSend = pSock->Send(pBuff + BytesSent, BytesRead - BytesSent);
					

				if(AttemptSend == SOCKET_ERROR)
				{
					int code = pSock->GetLastError();
					if(code != WSAEWOULDBLOCK)
					{
						pShell->m_Error  = "Remotely Canceled";  // Make more descriptive
						pShell->m_Status = TRANSFER_CLOSED;	
					}
					else
					{
						// Send Logging
						//if( pShell->m_Sha1Hash.Left(4) == "F2K5" )
						//	pShell->m_pNet->m_pCore->DebugLog("Block: " + pShell->m_Name + " -- CurrentPos=" + NumtoStr(pShell->m_CurrentPos));

						WaitForSingleObject((HANDLE) pSock->m_CanWrite, INFINITE);
						pSock->m_CanWrite.ResetEvent();
						
						if(pShell->m_Status != TRANSFER_SENDING)
							break;
					}
				}
				else
				{
					// Upload post-send integrity check
					/*if(pShell->m_MirrorFile.m_hFile != CFile::hFileNull)
						pShell->m_MirrorFile.Write(pBuff + BytesSent, AttemptSend);

					if(pShell->m_CheckFile.m_hFile != CFile::hFileNull)
					{
						byte* CheckBytes = new byte[AttemptSend];
						
						pShell->m_CheckFile.Seek(pShell->m_CurrentPos, CFile::begin);
						pShell->m_CheckFile.Read(CheckBytes, AttemptSend);

						if( memcmp(pBuff + BytesSent, CheckBytes, AttemptSend) != 0 )
						{
							CString Problem = "Read Check Failed: " + pShell->m_Name + " -- ReadPos=" + NumtoStr(pShell->m_CurrentPos) + ", BytesSent=" + NumtoStr(BytesSent) + ", BytesSent=" + NumtoStr(AttemptSend);
							pShell->m_pTrans->m_pCore->DebugLog(Problem);
						}
						
						delete [] CheckBytes;
					}*/

					// Send Logging
					//if( pShell->m_Sha1Hash.Left(4) == "F2K5" )
					//	pShell->m_pNet->m_pCore->DebugLog("Sent: " + pShell->m_Name + " -- CurrentPos=" + NumtoStr(pShell->m_CurrentPos) + ", SentBytes=" + NumtoStr(AttemptSend));

					BytesSent   += AttemptSend;
					pShell->m_CurrentPos += AttemptSend;
					pShell->m_dwSecBytes += AttemptSend;
					pShell->m_BytesSent  += AttemptSend;
				}

				if(!pShell->m_UpdatedInSecond)
					pShell->m_UpdatedInSecond = true;
			}

			// Get next chunk of bytes from file
			else
			{
				BytesSent = 0;
				BytesRead = 0;

				int ReadSize = pShell->m_StopPos - pShell->m_CurrentPos;


				// If theres a bandwidth limit
				if(pPrefs->m_BandwidthUp)
				{
					if(pShell->m_AllocBytes <= 0)
					{
						pShell->m_AllocBytes = 0;

						pSock->m_MoreBytes.ResetEvent();
						WaitForSingleObject((HANDLE) pSock->m_MoreBytes, INFINITE);

						if(pShell->m_Status != TRANSFER_SENDING)
							break;
					}


					if(pShell->m_AllocBytes < ReadSize)
						ReadSize = pShell->m_AllocBytes;
				}
	

				if(SEND_BUFF < ReadSize)
					ReadSize = SEND_BUFF;


				// If file being uploaded is a partial
				if(pShell->m_IsPartial)
				{

					std::map<int, CGnuDownloadShell*>::iterator itPart = pTrans->m_DownloadMap.find(pShell->m_PartialID);

					if(itPart != pTrans->m_DownloadMap.end())
						BytesRead = itPart->second->GetRange(pShell->m_CurrentPos , pBuff, ReadSize);
				}
			
				// If tiger tree being sent
				else if(pShell->m_TigerTreeRequest)
				{
					memcpy(pBuff, pShell->m_TigerTree + pShell->m_CurrentPos, ReadSize);
					BytesRead = ReadSize;
				}

				// Normal file being uploaded
				else
				{
					try
					{
						// Stream Test
						/*if(pShell->m_Name == "stream.mp3")
						{
							BytesRead = ReadSize;
						}
						else
						{*/
							pShell->m_File.Seek(pShell->m_CurrentPos, CFile::begin);
							BytesRead = pShell->m_File.Read(pBuff, ReadSize);
						//}

						// Send Logging
						//if( pShell->m_Sha1Hash.Left(4) == "F2K5" )
						//	pShell->m_pNet->m_pCore->DebugLog("Read: " + pShell->m_Name + " -- CurrentPos=" + NumtoStr(pShell->m_CurrentPos) + ", ReadBytes=" + NumtoStr(BytesRead));
						
					}
					catch(...)
					{
						pShell->m_Error  = "Error Reading File";
						pShell->m_Status = TRANSFER_CLOSED;
					}
				}
				
				

				
				if(pPrefs->m_BandwidthUp)
					pShell->m_AllocBytes -= BytesRead;


				if(BytesRead == 0)
				{
					pShell->m_Error  = "No Bytes Read from File";
					pShell->m_Status = TRANSFER_CLOSED;
				}
			}
		}

		// Else all bytes sent
		else
		{
			if(pShell->m_KeepAlive)
				pShell->m_Status = TRANSFER_CONNECTED;
			else
				pShell->m_Status = TRANSFER_CLOSED;	
		}


	// Make sure shell still exists
	for(int i = 0; i < pTrans->m_UploadList.size(); i++)
		if(pTrans->m_UploadList[i] == pShell)
		{
			pShell->m_UpdatedInSecond = true;

			if(pShell->m_Socket)
				pSock->m_ThreadRunning = false;
		}


	//TRACE0("*** Upload Thread Ended\n");

	ExitThread(0);
}




