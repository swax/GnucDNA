#pragma once

class CDnaCore;
class CGnuChat;

class GNUC_API CDnaChat  
{
public:
	CDnaChat();
	  ~CDnaChat();


	void InitClass(CDnaCore* dnaCore);

	CDnaCore*  m_dnaCore;
	CGnuChat*  m_gnuChat;

	void SendDirectMessage(LPCTSTR Address, LPCTSTR Message);

	 
};


