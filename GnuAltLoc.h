#pragma once


class AltLocation;
class CGnuShare;
class CGnuPrefs;
class CGnuCore;

class CGnuAltLoc  
{
public:
	CGnuAltLoc(CGnuShare*);
	virtual ~CGnuAltLoc();

	void AddAltLocation(CString& locStr, CString& Sha1Hash);
	CString GetAltLocationHeader(CString Sha1Hash, CString Host="", int HostCount=5);

	CGnuCore*	m_pCore;
	CGnuShare*	m_pShare;
	CGnuPrefs*  m_pPrefs;
};

