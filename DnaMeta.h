#pragma once


class CDnaCore;
class CGnuMeta;

class GNUC_API CDnaMeta 
{
public:
	CDnaMeta();
	 ~CDnaMeta();

	void InitClass(CDnaCore* dnaCore);

	CDnaCore* m_dnaCore;
	CGnuMeta* m_gnuMeta;
	
	void LoadSchemaDir(LPCTSTR DirPath);
	std::vector<int> GetMetaIDs(void);
	CString GetMetaName(LONG MetaID);
	std::vector<int> GetAttributeIDs(LONG MetaID);
	CString GetAttributeName(LONG MetaID, LONG AttributeID);
	BOOL GetAttributeReadOnly(LONG MetaID, LONG AttributeID);
	CString GetAttributeType(LONG MetaID, LONG AttributeID);
	std::vector<CString> GetAttributeEnums(LONG MetaID, LONG AttributeID);
	std::vector<CString> GetMetaExtensions(LONG MetaID);
};


