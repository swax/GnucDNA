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

#include "GnuNetworks.h"
#include "GnuPrefs.h"
#include "GnuRouting.h"
#include "GnuControl.h"
#include "GnuProtocol.h"
#include "G2Control.h"
#include "G2Node.h"
#include "GnuTransfers.h"
#include "GnuDownload.h"
#include "GnuDownloadShell.h"
#include "GnuUpload.h"
#include "GnuUploadShell.h"
#include "GnuNode.h"
#include "GnuShare.h"
#include "GnuFileHash.h"

#include "GnuSock.h"


CGnuSock::CGnuSock(CGnuNetworks* pNet)
{	
	m_pNet   = pNet;
	m_pCore  = pNet->m_pCore;
	m_pShare = m_pCore->m_pShare;
	m_pPrefs = m_pCore->m_pPrefs;
	m_pTrans = m_pCore->m_pTrans;

	m_BuffLength  = 0;
	m_SecsAlive   = 0;
	m_bDestroy    = false;
}

CGnuSock::~CGnuSock()
{
	// Flush receive buffer
	byte pBuff[4096];
	while(Receive(pBuff, 4096) > 0)
		;

	if(m_hSocket != INVALID_SOCKET)
		AsyncSelect(0);
}


// Do not edit the following lines, which are needed by ClassWizard.
#if 0
BEGIN_MESSAGE_MAP(CGnuSock, CAsyncSocket)
	//{{AFX_MSG_MAP(CGnuSock)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
#endif	// 0



void CGnuSock::OnReceive(int nErrorCode) 
{
	if(nErrorCode)
	{
		Close();
		return;
	}

	// Firewall not up, unless this is over a LAN
	if(!m_pPrefs->m_BehindFirewall)
		m_pNet->m_TcpFirewall = false;


	// Get Host Check if blocked
	GetPeerName(m_RemoteHost, m_RemotePort);

	if(m_pPrefs->BlockedIP(StrtoIP(m_RemoteHost)) )
		return;


	// Receive Data
	m_BuffLength = Receive(m_pBuff, 4096);
	
	if(m_BuffLength <= 0)
	{
		Close();
		return;
	}

	m_Handshake += CString((char*) m_pBuff, m_BuffLength);


	if(m_Handshake.GetLength() > 4096)
	{
		Close();
		return;
	}

	// Parse if handshake finished
	if( m_Handshake.Find("\r\n\r\n") != -1 )
	{
		if(m_Handshake.Find(" CONNECT/") != -1 )
			ParseConnectRequest();

//#ifdef _DEBUG
		else if(m_Handshake.Left(10) == "GET / HTTP")
			ParseBrowseRequest();
//#endif

		else if(m_Handshake.Find("/gnutella/pushproxy?")  != -1 || 
			    m_Handshake.Find("/gnutella/push-proxy?") != -1 || 
			    m_Handshake.Find("/gnet/push-proxy?")     != -1)
			ParsePushProxyRequest();
		
		else if(m_Handshake.Left(4) == "GET " || m_Handshake.Left(5) == "HEAD ")
			ParseUploadRequest();

		else if(m_Handshake.Left(5) == "PUSH ")
			ParseDownloadRequest(NETWORK_G2);

		

		Close();
	}

	else if(m_Handshake.Find("GIV ") == 0 && m_Handshake.Find("\n\n") != -1 )
	{
		ParseDownloadRequest(NETWORK_GNUTELLA);
		Close();
	}

	CAsyncSocket::OnReceive(nErrorCode);
}


int CGnuSock::Send(const void* lpBuf, int nBufLen, int nFlags) 
{
	int Command = CAsyncSocket::Send(lpBuf, nBufLen, nFlags);

	return Command;
}

void CGnuSock::OnSend(int nErrorCode) 
{
	CAsyncSocket::OnSend(nErrorCode);
}

void CGnuSock::Close()
{
	if(m_hSocket != INVALID_SOCKET)
	{
		AsyncSelect(0);
		ShutDown(2);

		CAsyncSocket::Close();
	}

	m_bDestroy = true;
}


// Called every once a second
void CGnuSock::Timer()
{
	m_SecsAlive++;
}

void CGnuSock::ParseConnectRequest()
{
	// Check for duplicate or screened node
	if(!m_pPrefs->AllowedIP(StrtoIP(m_RemoteHost)))
	{
		Close();
		return;
	}


	CString lowHandshake = m_Handshake;
	lowHandshake.MakeLower();

	// G2 Connection
	if( m_pNet->m_pG2 && lowHandshake.Find("application/x-gnutella2") != -1 && m_pNet->m_pG2->FindNode(m_RemoteHost, 0) == NULL  )
	{
		CG2Node* G2NodeSock	= new CG2Node(m_pNet->m_pG2, m_RemoteHost, m_RemotePort);
		G2NodeSock->m_Inbound	= true;
	
		SOCKET FreeSock = Detach();
		G2NodeSock->Attach(FreeSock);

		m_pNet->m_pG2->m_G2NodeAccess.Lock();
		m_pNet->m_pG2->m_G2NodeList.push_back(G2NodeSock);
		m_pNet->m_pG2->m_G2NodeAccess.Unlock();

		m_pNet->m_pG2->G2NodeUpdate(G2NodeSock);
		
		G2NodeSock->ParseIncomingHandshake(m_Handshake, m_pBuff, m_BuffLength);
	}

	// Gnutella Connection
	else if( m_pNet->m_pGnu && m_pNet->m_pGnu->FindNode(m_RemoteHost, 0) == NULL )
	{
		CGnuNode* NodeSock	= new CGnuNode(m_pNet->m_pGnu, m_RemoteHost, m_RemotePort);
		NodeSock->m_Inbound	= true;

		SOCKET FreeSock = Detach();
		NodeSock->Attach(FreeSock);

		m_pNet->m_pGnu->m_NodeAccess.Lock();
		m_pNet->m_pGnu->m_NodeList.push_back(NodeSock);
		m_pNet->m_pGnu->m_NodeAccess.Unlock();
	
		m_pNet->m_pGnu->NodeUpdate(NodeSock);		

		NodeSock->ParseIncomingHandshake06(m_Handshake, m_pBuff, m_BuffLength);
	}

	else
		Close();
}

void CGnuSock::ParseBrowseRequest()
{
	// Check if from gnutella client wanting to browse files
	if(m_pNet->m_pGnu && m_Handshake.Find("application/x-gnutella-packets") != -1)
	{
		CGnuNode* BrowseSock  = new CGnuNode(m_pNet->m_pGnu, m_RemoteHost, m_RemotePort);
		BrowseSock->m_Inbound = true;

		SOCKET FreeSock = Detach();
		BrowseSock->Attach(FreeSock);


		m_pNet->m_pGnu->m_NodesBrowsing.push_back(BrowseSock);
		BrowseSock->ParseBrowseHandshakeRequest(m_Handshake);
	}

	// Else its a web browser
	else	
		SendGetGnucleus();	
}

void CGnuSock::ParseUploadRequest()
{
	CString FirstLine = m_Handshake.Left(m_Handshake.Find("\r\n") );
	FirstLine.MakeLower();
	CString RequestURI;

	// Get file request from first line
	int startpos = FirstLine.Find(" ") + 1;
	int endpos = FirstLine.ReverseFind(' ');

	if(endpos <= startpos)
		RequestURI = FirstLine.Mid(startpos + 1);
	else
		RequestURI = FirstLine.Mid(startpos, endpos - startpos);


	RequestURI = DecodeURL(RequestURI);


	CGnuUploadShell* UploadSock = NULL;

	// Try to find an upload from the same host with the same request
	for(int i = 0; i < m_pTrans->m_UploadList.size(); i++)
	{
		CGnuUploadShell* p = m_pTrans->m_UploadList[i];

		if(p->m_Host.S_addr == StrtoIP(m_RemoteHost).S_addr)
			if(p->m_RequestURI == RequestURI)
			{
				p->m_Attempts++;
				
				if(p->m_Socket)
				{
					Close();
					return;
				}
				else 
					UploadSock = p;
			}
	}

	
	// If new upload
	if(!UploadSock)
	{
		UploadSock = new CGnuUploadShell(m_pTrans);

		m_pTrans->m_UploadAccess.Lock();
		m_pTrans->m_UploadList.push_back(UploadSock);
		m_pTrans->m_UploadAccess.Unlock();
	}
	

	// Set Variables
	UploadSock->m_Host       = StrtoIP(m_RemoteHost);
	UploadSock->m_Port       = m_RemotePort;
	UploadSock->m_Handshake  = m_Handshake;
	
	UploadSock->m_Status     = TRANSFER_CONNECTED;


	// Attach socket to upload
	UploadSock->m_Socket = new CGnuUpload(UploadSock);
	UploadSock->m_Socket->m_GetRequest = m_Handshake;

	SOCKET FreeSock = Detach();
	UploadSock->m_Socket->Attach(FreeSock);
		
	UploadSock->m_RequsetPending = true;


	m_pTrans->UploadUpdate(UploadSock->m_UploadID);

	Close();
	return;
}

void CGnuSock::ParseDownloadRequest(int Network)
{
	CString RemoteGuid;

	if(Network == NETWORK_GNUTELLA)
	{
		// Get Index Number
		DWORD	Index;
		sscanf((LPCTSTR) m_Handshake, "GIV %ld:*/*\n\n", &Index);

		// Get Server ID of client giving us the file
		int Front = m_Handshake.Find(":") + 1;
		int End   = m_Handshake.Find("/");
		RemoteGuid  = m_Handshake.Mid(Front, End - Front);
		RemoteGuid.MakeUpper();

		// Get the name of the file
		Front = m_Handshake.Find("/") + 1,
		End   = m_Handshake.Find("\n\n");
		CString FileName  = m_Handshake.Mid(Front, End - Front);
		FileName.MakeLower();

		m_Handshake.Replace("\n\n", "\r\n\r\n");
	}

	if(Network == NETWORK_G2)
	{
		int Front = m_Handshake.Find("guid:") + 1;
		int End   = m_Handshake.Find("\r\n");
		RemoteGuid  = m_Handshake.Mid(Front, End - Front);
		RemoteGuid.MakeUpper();
	}


	// Find the download that requested the push
	for(int i = 0; i < m_pTrans->m_DownloadList.size(); i++)	
	{
		CGnuDownloadShell* p = m_pTrans->m_DownloadList[i];

		bool Found = false;

		for(int j = 0; j < p->m_Queue.size(); j++)
			if(p->m_Queue[j].PushSent)
				if(EncodeBase16((byte*) &p->m_Queue[j].PushID, 16) == RemoteGuid)
				{
					Found = true;
					break;
				}

		
		if(Found)
		{
			p->m_Queue[j].PushSent  = false;
			p->m_Queue[j].Error     = "";
			p->m_Queue[j].Handshake = "";
			p->m_Queue[j].Status = FileSource::eAlive;
		
			if(p->m_ShellStatus != CGnuDownloadShell::eDone)
				p->m_ShellStatus = CGnuDownloadShell::eActive;
			
			p->m_Cooling = 0;
			
			CGnuDownload* PushedFile = new CGnuDownload(p, p->m_Queue[j].SourceID);
			PushedFile->m_Push = true;
			
			SOCKET FreeSock = Detach();
			PushedFile->Attach(FreeSock);
			
			p->m_Sockets.push_back(PushedFile);

			PushedFile->HostInfo()->Handshake = m_Handshake;

			PushedFile->SendRequest();

			Close();
			break;
		}
	}

	return;
}

void CGnuSock::ParsePushProxyRequest()
{
	CString Response;

	CString lowHandshake = m_Handshake;
	lowHandshake.MakeLower();

	// Check Gnutella is running
	if(m_pNet->m_pGnu == NULL)
	{
		Response = "HTTP 410 Push Proxy: Servent not connected\r\n\r\n";
		Send(Response, Response.GetLength());
		return;
	}

	// Get Source Address from handshake
	IPv4 SourceAddress;
	int  pos = lowHandshake.Find("x-node");
	if(pos != -1)
	{
		CString strXNode = ParseString( lowHandshake.Mid(pos), '\n');
		strXNode.Replace("::", ":"); // fix limewire bug
		ParseString(strXNode, ':'); // header out of the way
		strXNode.Trim(' ');

		SourceAddress = StrtoIPv4(strXNode);
	}
	else
	{
		Response = "HTTP 400 Push Proxy: Bad Request\r\n\r\n";
		Send(Response, Response.GetLength());
		return;
	}

	
	// Get Client ID
	GUID ClientID;

	CString UrlString = ParseString(lowHandshake, '\n');
	pos = UrlString.Find("guid=");
	if(pos != -1)
		DecodeBase16( UrlString.Mid(pos + 5, 32), 32, (byte*) &ClientID);
		
	pos = UrlString.Find("serverid=");
	if(pos != -1)
		DecodeBase32( UrlString.Mid(pos + 9, 26), 26, (byte*) &ClientID);


	// Match with leaf node
	int HostID = 0;
	std::map<int, GUID>::iterator itHost = m_pNet->m_pGnu->m_PushProxyHosts.begin();
	for( ; itHost != m_pNet->m_pGnu->m_PushProxyHosts.end(); itHost++)
		if(ClientID == itHost->second)
			HostID = itHost->first;

	
	if(HostID == 0)
	{
		Response = "HTTP 410 Push Proxy: Servent not connected\r\n\r\n";
		Send(Response, Response.GetLength());
		return;
	}


	// Get File ID from handshake
	int FileID = 0;
	pos = UrlString.Find("file=");
	if(pos != -1)
		FileID = atoi( ParseString( UrlString.Mid(pos + 5), '&') );
		

	// Build and send push
	FileSource PushInfo;
	PushInfo.Distance	= 1;
	PushInfo.PushID		= ClientID;
	PushInfo.FileIndex	= FileID;
	PushInfo.GnuRouteID	= HostID;

	if(m_pNet->m_pGnu)
		m_pNet->m_pGnu->m_pProtocol->Send_Push(PushInfo, SourceAddress);

	Response = "HTTP 202 Push Proxy: Message Sent\r\n\r\n";
	Send(Response, Response.GetLength());
}

void CGnuSock::SendGetGnucleus()
{

	// Create File
	CString Http200;
	Http200 =  "HTTP/1.1 200 OK\r\n";
	Http200 += "Server: " + m_pCore->GetUserAgent() + "\r\n";
	Http200 += "Connection: close\r\n";
	Http200 += "Content-Type: text/html\r\n";
	Http200 += "\r\n";
	
	Http200 += "<html>\r\n";
	Http200 += "<head><title>Gnutella Network</title></head>\r\n";
	Http200 += "<body>\r\n";

	Http200 += "<br>\r\n";
	Http200 += "<font size=5>" + m_pCore->GetUserAgent() + "</font><br>\r\n";
	
	Http200 += "<b>Hardware:</b> ";
	if(m_pCore->m_IsKernalNT)
		Http200 += " WinNT, ";
	else
		Http200 += " Win9x, ";
	Http200 += NumtoStr(m_pCore->m_SysSpeed) + "MHz, ";
	Http200 += NumtoStr(m_pCore->m_SysMemory) + "MB Ram";
	Http200 += "<br><br>\r\n";

	if( m_pNet->m_pGnu )
	{
		// Get Stats
		int ConnectCount = 0;

		CString Mode = "Normal";
		if(m_pNet->m_pGnu->m_GnuClientMode == GNU_LEAF)
			Mode = "Leaf";
		else if(m_pNet->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER)
			Mode = "Ultrapeer";

		CTimeSpan Uptime(CTime::GetCurrentTime() - m_pNet->m_pGnu->m_ClientUptime);
		
		CString Host;

		Http200 += "<table width=95%><tr><td bgcolor=#DDDDDD>\r\n";
		Http200 += "<b><font size=3>Gnutella</font></b><br>\r\n";
		Http200 += "<i>Running in " + Mode + " mode</i><br>\r\n";
		
		Http200 += "<b>Uptime:</b> ";
		if(Uptime.GetDays())
			Http200 += NumtoStr(Uptime.GetDays()) + " Days, ";
		Http200 += NumtoStr(Uptime.GetHours()) + " Hours, " + NumtoStr(Uptime.GetMinutes()) + " Minutes<br>\r\n";
		Http200 += "<b>Routing:</b> " + InsertDecimal((m_pNet->m_pGnu->m_NetSecBytesUp) / 1024) + " KBs up / " + InsertDecimal((m_pNet->m_pGnu->m_NetSecBytesDown) / 1024) + " KBs down<br>\r\n";
		
		

		Http200 += "<br>\r\n";
		Http200 += "<b><u>Connections</u></b><br>\r\n";
		Http200 += "<table>\r\n";


		// Gnutella Connections
		for(int i = 0; i < m_pNet->m_pGnu->m_NodeList.size(); i++)
		{
			CGnuNode* pNode = m_pNet->m_pGnu->m_NodeList[i];

			if(m_pNet->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER)
				if(pNode->m_GnuNodeMode == GNU_LEAF)
					continue;

			if(pNode->m_Status != SOCK_CONNECTED)
				continue;

			// Host 1 hop away
			ConnectCount++;

			CString ClientMode = "Normal Node";
			
			Host = IPv4toStr(pNode->m_Address);
				
			if(pNode->m_GnuNodeMode == GNU_ULTRAPEER)
				ClientMode = "Ultrapeer";

			CTimeSpan Uptime(CTime::GetCurrentTime() - pNode->m_ConnectTime);

			Http200 += "\t <tr>\r\n";
			Http200 += "\t\t <td>\r\n";
			Http200 += "\t\t\t <a href=\"http://" + Host + "\">" + Host + "</a>\r\n";
			Http200 += "\t\t </td>\r\n";
			Http200 += "\t\t <td>\r\n";
			Http200 += "\t\t\t - " + Uptime.Format("%DD %HH %MM") + ", \r\n";
			Http200 += "\t\t </td>\r\n"; 
			Http200 += "\t\t <td>\r\n";
			Http200 += "\t\t\t " + pNode->m_RemoteAgent + "\r\n";
			Http200 += "\t\t </td>\r\n"; 
			Http200 += "\t </tr>\r\n";
		}

		// Add in connection number
		int pos = Http200.Find("Connections</u></b><br>");
		Http200.Insert(pos, NumtoStr(ConnectCount) + " ");
		ConnectCount = 0;

		Http200 += "</table>\r\n";
		Http200 += "<br>\r\n";

		// Gnutella Leaves
		if(m_pNet->m_pGnu->m_GnuClientMode == GNU_ULTRAPEER)
		{
			Http200 += "<b><u>Leaves</u></b><br>\r\n";
			Http200 += "<table>\r\n";

			for(i = 0; i < m_pNet->m_pGnu->m_NodeList.size(); i++)
			{	
				CGnuNode* pNode = m_pNet->m_pGnu->m_NodeList[i];

				if(pNode->m_GnuNodeMode != GNU_LEAF)
					continue;

				if(pNode->m_Status != SOCK_CONNECTED)
					continue;

				ConnectCount++;

				Host = IPv4toStr(pNode->m_Address);

				CTimeSpan Uptime(CTime::GetCurrentTime() - pNode->m_ConnectTime);

				Http200 += "\t <tr>\r\n";
				Http200 += "\t\t <td>\r\n";
				Http200 += "\t\t\t <a href=\"http://" + Host + "\">" + Host + "</a>\r\n";
				Http200 += "\t\t </td>\r\n";
				Http200 += "\t\t <td>\r\n";
				Http200 += "\t\t\t - " + Uptime.Format("%DD %HH %MM") + ", \r\n";
				Http200 += "\t\t </td>\r\n"; 
				Http200 += "\t\t <td>\r\n";
				Http200 += "\t\t\t " + pNode->m_RemoteAgent + "\r\n";
				Http200 += "\t\t </td>\r\n"; 
				Http200 += "\t </tr>\r\n";
			}
			

			int pos = Http200.Find("Leaves</u></b><br>");
			Http200.Insert(pos, NumtoStr(ConnectCount) + " ");
			ConnectCount = 0;

			Http200 += "</table>\r\n";
		}

		Http200 += "</td></tr></table>\r\n";
	}

	Http200 += "<br><br>\r\n";

	int EndG1Pos = Http200.GetLength();

	// G2
	if( m_pNet->m_pG2 )
	{
		// Get Stats
		int ConnectCount = 0;

		CString Mode = "Normal";
		if(m_pNet->m_pG2->m_ClientMode == G2_CHILD)
			Mode = "Child";
		else if(m_pNet->m_pG2->m_ClientMode == G2_HUB)
			Mode = "Hub";

		CTimeSpan Uptime(CTime::GetCurrentTime() - m_pNet->m_pG2->m_ClientUptime);
		
		CString Host;

		Http200 += "<table width=95%><tr><td bgcolor=#DDDDDD>\r\n";
		Http200 += "<b><font size=3>G2</font></b><br>\r\n";
		Http200 += "<i>Running in " + Mode + " mode</i><br>\r\n";
		
		Http200 += "<b>Uptime:</b> ";
		if(Uptime.GetDays())
			Http200 += NumtoStr(Uptime.GetDays()) + " Days, ";
		Http200 += NumtoStr(Uptime.GetHours()) + " Hours, " + NumtoStr(Uptime.GetMinutes()) + " Minutes<br>\r\n";
		Http200 += "<b>Routing:</b> " + InsertDecimal((m_pNet->m_pG2->m_NetSecBytesUp) / 1024) + " KBs up / " + InsertDecimal((m_pNet->m_pG2->m_NetSecBytesDown) / 1024) + " KBs down<br>\r\n";
		
		

		Http200 += "<br>\r\n";
		Http200 += "<b><u>Connections</u></b><br>\r\n";
		Http200 += "<table>\r\n";


		// Gnutella Connections
		for(int i = 0; i < m_pNet->m_pG2->m_G2NodeList.size(); i++)
		{
			CG2Node* pNode = m_pNet->m_pG2->m_G2NodeList[i];

			if(m_pNet->m_pG2->m_ClientMode == G2_HUB)
				if(pNode->m_NodeMode == G2_CHILD)
					continue;

			if(pNode->m_Status != SOCK_CONNECTED)
				continue;

			// Host 1 hop away
			ConnectCount++;

			CString ClientMode = "Normal Node";
			
			Host = IPv4toStr(pNode->m_Address);
				
			if(pNode->m_NodeMode == G2_HUB)
				ClientMode = "Hub";

			CTimeSpan Uptime(CTime::GetCurrentTime() - pNode->m_ConnectTime);

			Http200 += "\t <tr>\r\n";
			Http200 += "\t\t <td>\r\n";
			Http200 += "\t\t\t <a href=\"http://" + Host + "\">" + Host + "</a>\r\n";
			Http200 += "\t\t </td>\r\n";
			Http200 += "\t\t <td>\r\n";
			Http200 += "\t\t\t - " + Uptime.Format("%DD %HH %MM") + ", \r\n";
			Http200 += "\t\t </td>\r\n"; 
			Http200 += "\t\t <td>\r\n";
			Http200 += "\t\t\t " + pNode->m_RemoteAgent + "\r\n";
			Http200 += "\t\t </td>\r\n"; 
			Http200 += "\t </tr>\r\n";
		}

		// Add in connection number
		int pos = Http200.Find("Connections</u></b><br>", EndG1Pos);
		Http200.Insert(pos, NumtoStr(ConnectCount) + " ");
		ConnectCount = 0;

		Http200 += "</table>\r\n";
		Http200 += "<br>\r\n";

		// G2 Children
		if(m_pNet->m_pG2->m_ClientMode == G2_HUB)
		{
			Http200 += "<b><u>Children</u></b><br>\r\n";
			Http200 += "<table>\r\n";

			for(i = 0; i < m_pNet->m_pG2->m_G2NodeList.size(); i++)
			{	
				CG2Node* pNode = m_pNet->m_pG2->m_G2NodeList[i];

				if(pNode->m_NodeMode != G2_CHILD)
					continue;

				if(pNode->m_Status != SOCK_CONNECTED)
					continue;

				ConnectCount++;

				Host = IPv4toStr(pNode->m_Address);

				CTimeSpan Uptime(CTime::GetCurrentTime() - pNode->m_ConnectTime);

				Http200 += "\t <tr>\r\n";
				Http200 += "\t\t <td>\r\n";
				Http200 += "\t\t\t <a href=\"http://" + Host + "\">" + Host + "</a>\r\n";
				Http200 += "\t\t </td>\r\n";
				Http200 += "\t\t <td>\r\n";
				Http200 += "\t\t\t - " + Uptime.Format("%DD %HH %MM") + ", \r\n";
				Http200 += "\t\t </td>\r\n"; 
				Http200 += "\t\t <td>\r\n";
				Http200 += "\t\t\t " + pNode->m_RemoteAgent + "\r\n";
				Http200 += "\t\t </td>\r\n"; 
				Http200 += "\t </tr>\r\n";
			}
			

			int pos = Http200.Find("Children</u></b><br>");
			Http200.Insert(pos, NumtoStr(ConnectCount) + " ");
			ConnectCount = 0;

			Http200 += "</table>\r\n";
		}

		Http200 += "</td></tr></table>\r\n";
	}


	Http200 += "</body></html>";
	
	Send(Http200, Http200.GetLength());
	Close();
}

