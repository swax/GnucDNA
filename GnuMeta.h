#pragma once

#include "GnuShare.h"

class CGnuCore;
class CGnuSchema;

class CGnuMeta
{
public:
	CGnuMeta(CGnuCore* pCore);
	~CGnuMeta(void);

	void LoadSchemaDir(CString DirPath);
	void LoadSchemaFile(CString FilePath);
	void LoadFileMeta(SharedFile &File);
	
	bool DecompressMeta(CString &MetaLoad, byte* MetaBuff, int MetaLength);
	void ParseMeta(CString MetaLoad, std::map<int, int> &MetaIDMap, std::map<int, CString> &MetaValueMap);
	int  MetaIDfromXml( CString MetaXml );

	CString GetMetaName(int MetaID);

	int m_NextMetaID;
	std::vector<CGnuSchema*>   m_MetaList;
	std::map<int, CGnuSchema*> m_MetaIDMap;

	CGnuCore* m_pCore;
};
