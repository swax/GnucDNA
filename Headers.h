#pragma once


struct Header
{
	CString Name;
	CString Value;
};


class CParsedHeaders
{
public:

	CParsedHeaders (CString RawHeaderBlock);
	CString FindHeader (CString HeaderName);

	std::vector<Header> m_Headers;
	int m_Warning;
};


CString LimitHeadersLength(CString Headers);
bool ValidHttpHeader(CString Header, CString Version, int Code, CString Reason);