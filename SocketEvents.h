#pragma once

class CSocketEvents
{
public:
	CSocketEvents()  {};
	~CSocketEvents() {};

	virtual void OnAccept(int nErrorCode) {};
	virtual void OnConnect(int nErrorCode) {};
	virtual void OnReceive(int nErrorCode) {};
	virtual void OnSend(int nErrorCode)    {};
	virtual void OnClose(int nErrorCode)   {};
};