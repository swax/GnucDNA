#pragma once

class CGnuControl;
class CGnuProtocol;

class CGnuDatagram
{
public:
	CGnuDatagram(CGnuControl* pComm);
	virtual ~CGnuDatagram();

	void Timer();

	// Receiving
	virtual void OnReceive(IPv4 Address, byte* pRecvBuff, int RecvLength);

	// Sending
	void SendPacket(IPv4 Address, byte* packet, uint32 length);

	// Bandwidth
	CMovingAvg m_AvgUdpDown;
	CMovingAvg m_AvgUdpUp;

	CGnuControl*  m_pComm;
	CGnuProtocol* m_pProtocol;
};