#pragma once


#define GNU_RECV_BUFF    2048


class CGnuControl;
class CGnuProtocol;


class CGnuDatagram : public CAsyncSocket
{
public:
	CGnuDatagram(CGnuControl* pComm);
	virtual ~CGnuDatagram();

	void Init();

	void Timer();

	// Receiving
	virtual void OnReceive(int nErrorCode);

	// Sending
	void SendPacket(IPv4 Address, byte* packet, uint32 length);

	byte m_pRecvBuff[GNU_RECV_BUFF];

	// Bandwidth
	CRangeAvg m_AvgUdpDown;
	CRangeAvg m_AvgUdpUp;

	int m_UdpSecBytesDown;
	int m_UdpSecBytesUp;

	CGnuControl*  m_pComm;
	CGnuProtocol* m_pProtocol;
};