#pragma once

#include "GnuSchema.h"


struct LANGANDCODEPAGE 
{
	WORD wLanguage;
	WORD wCodePage;
};


class CGnuSchemaApp : public CGnuSchema
{
public:
	CGnuSchemaApp(void);
	virtual ~CGnuSchemaApp(void);

	virtual void LoadData(SharedFile &File);

	void ReadVersionBlock(SharedFile &File);
	CString ExtractString(CString VersQuery);
	CString GetOS();


	CString			 m_strFilename;
	LPBYTE			 m_pVersionInfo;
	DWORD			 m_dwTransInfo;
	VS_FIXEDFILEINFO m_fixedFileInfo;
	LANGANDCODEPAGE* m_pTranslate;
};
