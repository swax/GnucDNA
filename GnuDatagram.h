#pragma once


class CGnuControl;


class CGnuDatagram : public CAsyncSocket
{
public:
	CGnuDatagram(CGnuControl* pComm);
	virtual ~CGnuDatagram();

	void Init();

	void Timer();

	// Receiving
	virtual void OnReceive(int nErrorCode);

	CGnuControl* m_pComm;
};