#pragma once

#define GNU_RECV_BUFF    2048

class CGnuNetworks;

class CUdpListener : public CAsyncSocket
{
public:
	CUdpListener(CGnuNetworks* pNet);
	virtual ~CUdpListener(void);

	// Receiving
	virtual void OnReceive(int nErrorCode);

	byte m_pRecvBuff[GNU_RECV_BUFF];

	CGnuNetworks*  m_pNet;
};
