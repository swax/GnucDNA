#pragma once

#include "Gnushare.h"

struct ElementAttribute
{
	int AttributeID;

	CString Name;
	CString Type;

	bool ReadOnly;
	
	std::vector<CString> Enums;

	ElementAttribute::ElementAttribute()
	{
		ReadOnly = false;
	};
};

class CGnuMeta;

class CGnuSchema
{
public:
	CGnuSchema(void);
	virtual ~CGnuSchema(void);

	bool LoadXMLFile(CString FilePath);

	bool IterateChildNodes(IXMLDOMNode* pNode, CString ElementBranch=_T(""));
	bool IterateAttibutes(IXMLDOMNode* pNode, DOMNodeType ParentType, CString ParentValue);

	bool MatchExtension(CString FileName);


	bool	GetAttributeReadOnly(int AttributeID);
	void    SetAttributeReadOnly(CString Name);

	CString GetAttributeName(int AttributeID);

	CString AttrMaptoFileXML( std::map<int, CString> &AttrMap );
	CString AttrMaptoNetXML( std::map<int, CString> &AttrMap, int Index = -1 );
	

	CGnuMeta* m_pMeta;


	// GnuShare helpers
	int		VerifyAttribute(CString Name, CString Value);
	void	LoadFileAttribute(SharedFile* pFile, CString AttributeName, CString Value);
	void	SaveFileAttribute(SharedFile* pFile, int AttributeID, CString Value);
	CString GetFileAttribute(SharedFile* pFile, CString Name);
	

	// GnuSearch helpers
	void    SetResultAttributes(std::map<int, CString> &AttributeMap, CString MetaLoad);


	// Overridable functions
	virtual void LoadData(SharedFile &File);
	virtual void SaveData(SharedFile &File);


	
	// Schema values
	int		m_MetaID;
	CString m_Namespace;

	CString m_Name;
	CString m_NamePlural;
	CString m_Type;

	int m_NextAttributeID;
	std::vector<ElementAttribute> m_Attributes;
	std::map<int, int> m_AttributeMap;
	
	std::vector<CString> m_Extensions;

	// Temp
	SharedFile* m_CurrentFile;
};
