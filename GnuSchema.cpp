/********************************************************************************

	GnucDNA - The Gnucleus Library
    Copyright (C) 2000-2004 John Marshall Group

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


	For support, questions, comments, etc...
	E-Mail: swabby@c0re.net

********************************************************************************/

#include "StdAfx.h"

#include "msxml.h"

#include "GnuCore.h"
#include "GnuMeta.h"
#include "GnuSchema.h"


CGnuSchema::CGnuSchema(void)
{
	m_MetaID = 0;

	m_NextAttributeID = 1;

	m_CurrentFile = NULL;

	m_pMeta = NULL;
}

CGnuSchema::~CGnuSchema(void)
{
}

bool CGnuSchema::LoadXMLFile(CString FilePath)
{
	// Make sure FilePath exists
	CStdioFile XMLFile;

	// Load nodes from file cache
	if ( XMLFile.Open(FilePath, CFile::modeRead) )
	{
		if(XMLFile.GetLength() == 0)
		{
			XMLFile.Abort();
			return false;
		}

		XMLFile.Abort();
	}
	else
		return false;


	if ( SUCCEEDED(CoInitialize(NULL)) )
	{
		IXMLDOMDocument* pDoc;
		if ( SUCCEEDED (CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, reinterpret_cast<void**>(&pDoc))))
		{
			// Tell the "doc" that we're not going to load asynchronously.
			if ( SUCCEEDED(pDoc->put_async(VARIANT_FALSE)) )
			{
				CComVariant  vFile(FilePath);
				VARIANT_BOOL vBool;

				try
				{
					pDoc->load(vFile, &vBool);
				}
				catch(...)
				{
					return false;
				}

				if ( vBool == VARIANT_TRUE )
				{
					IXMLDOMNode* pNode;
					if ( SUCCEEDED(pDoc->QueryInterface(IID_IXMLDOMNode, reinterpret_cast<void**>(&pNode))))
					{
						IterateChildNodes(pNode);

						pNode->Release();
						pNode = NULL;
					}
				}
			}

			pDoc->Release();
			pDoc = NULL;
		}

		CoUninitialize();
	}

	return true;
}

bool CGnuSchema::IterateChildNodes(IXMLDOMNode* pNode, CString ElementBranch)
{
	CString ElementName;

	if( pNode )
	{
		// Get Node Name
		BSTR bNodeName;
		pNode->get_nodeName(&bNodeName);
		CString NodeName(bNodeName);
		SysFreeString(bNodeName);

		// Get Node Type String
		BSTR bNodeType;
		pNode->get_nodeTypeString(&bNodeType);
		CString NodeType(bNodeType);
		SysFreeString(bNodeType);

		// Get Node Type
		DOMNodeType eEnum;
		pNode->get_nodeType(&eEnum);


		CString NodeValue;

		if ( eEnum == NODE_TEXT )
		{
			BSTR bValue;
			pNode->get_text(&bValue);
			NodeValue = bValue;
			SysFreeString(bValue);
		}

		else if ( eEnum == NODE_COMMENT )
		{
			VARIANT vValue;
			pNode->get_nodeValue(&vValue);
			
			if ( vValue.vt == VT_BSTR )
				NodeValue = V_BSTR(&vValue);
			else
				NodeValue = "Unknown comment type";

			VariantClear(&vValue);
		}

		else if ( eEnum == NODE_PROCESSING_INSTRUCTION )
		{
			// <xml> tag, continue
			NodeValue = NodeName;
		}

		else if ( eEnum == NODE_ELEMENT )
		{
			NodeValue   = NodeName;
			ElementName = ":" + NodeName;
		}

		else
		{
			// #document - document
			NodeValue = NodeName + " - " + NodeType;
		}

		IterateAttibutes(pNode, eEnum, ElementBranch + ElementName);
	}
	

	// Traverse child nodes
	IXMLDOMNode* pNext = NULL;
	IXMLDOMNode* pChild;
	pNode->get_firstChild(&pChild);

	while(pChild)
	{
		IterateChildNodes(pChild,  ElementBranch + ElementName);
		pChild->get_nextSibling(&pNext);
		pChild->Release();
		pChild = pNext;
	}


	return true;
}					

bool CGnuSchema::IterateAttibutes(IXMLDOMNode* pNode, DOMNodeType ParentType, CString ParentValue)
{
	IXMLDOMNamedNodeMap* pAttrs;

	if ( SUCCEEDED(pNode->get_attributes(&pAttrs)) && (pAttrs != NULL) )
	{
		IXMLDOMNode *pChild;
		pAttrs->nextNode(&pChild);
		while(pChild)
		{
			// Get Attribute Name
			BSTR bAttrName;
			pChild->get_nodeName(&bAttrName);
			CString AttrName(bAttrName);
			SysFreeString(bAttrName);
			
			VARIANT vAttrValue;
			pChild->get_nodeValue(&vAttrValue);
			
			CString AttrValue;
			switch ( vAttrValue.vt )
			{
			case VT_BSTR:
				AttrValue = V_BSTR(&vAttrValue);
				break;
			default:
				AttrValue = "Unsupport type";
				break;
			}
			
			VariantClear(&vAttrValue);
			
			// Filter out <all> no difference
			ParentValue.Replace(":all:", ":");

			// Schema element
			if(ParentType == NODE_ELEMENT && ParentValue.CompareNoCase(":schema") == 0)
			{
				if(AttrName.CompareNoCase("targetNamespace") == 0)
					m_Namespace = AttrValue;
			}
			
			// Schema:element element
			if(ParentType == NODE_ELEMENT && ParentValue.CompareNoCase(":schema:element") == 0)
			{
				if(AttrName.CompareNoCase("name") == 0)
					m_NamePlural = AttrValue;
			}

			// Schema:element:complexType:element element
			if(ParentType == NODE_ELEMENT && ParentValue.CompareNoCase(":schema:element:complexType:element") == 0)
			{
				if(AttrName.CompareNoCase("name") == 0)
				{
					m_Name = AttrValue;
				}

				if(AttrName.CompareNoCase("type") == 0)
					m_Type = AttrValue;
			}

			// Schema:complexType:all:attribute element
			if(ParentType == NODE_ELEMENT && (ParentValue.CompareNoCase(":schema:complexType:attribute") == 0 ||
											  ParentValue.CompareNoCase(":schema:complexType:element") == 0))
			{
				
				if(AttrName.CompareNoCase("name") == 0)
				{
					ElementAttribute NewAttrib;
					NewAttrib.Name = AttrValue;
					NewAttrib.AttributeID = m_NextAttributeID++;
					m_AttributeMap[NewAttrib.AttributeID] = m_Attributes.size();
					m_Attributes.push_back(NewAttrib);
				}
				
				int LastPos = m_Attributes.size() - 1;
				if(LastPos >= 0)
					if(AttrName.CompareNoCase("type") == 0)
						m_Attributes[LastPos].Type = AttrValue;

				
			}
			
			// Schema:complexType:all:attribute:simpleType element
			if(ParentType == NODE_ELEMENT && ParentValue.CompareNoCase(":schema:complexType:attribute:simpleType") == 0)
			{
				int LastPos = m_Attributes.size() - 1;
				if(LastPos >= 0)
					if(AttrName.CompareNoCase("base") == 0)
						m_Attributes[LastPos].Type = AttrValue;
			}

			// Schema:complexType:all:attribute:simpleType:enumeration element
			if(ParentType == NODE_ELEMENT && ParentValue.CompareNoCase(":schema:complexType:attribute:simpleType:enumeration") == 0)
			{
				int LastPos = m_Attributes.size() - 1;
				if(LastPos >= 0)
					if(AttrName.CompareNoCase("value") == 0)
					{	
						m_Attributes[LastPos].Type = "Enum";
						m_Attributes[LastPos].Enums.push_back(AttrValue);
					}
			}

			// SchemaDescriptor:typeFilter:type element
			if(ParentType == NODE_ELEMENT && ParentValue.CompareNoCase(":schemaDescriptor:typeFilter:type") == 0)
			{
				if(AttrName.CompareNoCase("extension") == 0)
					m_Extensions.push_back(AttrValue);
			}

			// NamePlural:Name element
			if(ParentType == NODE_ELEMENT && ParentValue.CompareNoCase(":" + m_NamePlural + ":" + m_Name) == 0)
			{
				if(m_CurrentFile)
					LoadFileAttribute(m_CurrentFile, AttrName, AttrValue);
			}

			pChild->Release();
			pAttrs->nextNode(&pChild);
		}

		pAttrs->Release();
	}

	return true;
}

bool CGnuSchema::MatchExtension(CString FileName)
{
	int DotPos = FileName.ReverseFind('.');

	if(DotPos != -1)
	{
		CString Ext = FileName.Mid(DotPos + 1);

		for(int i = 0; i < m_Extensions.size(); i++)
			if(m_Extensions[i].CompareNoCase(Ext) == 0)
				return true;
	}

	return false;
}

// Can be overridden by more specific classes
void CGnuSchema::LoadData(SharedFile &File)
{
	File.MetaID = m_MetaID;

	CString MetaFile = File.Dir.c_str();

	int SlashPos = MetaFile.ReverseFind('\\');

	if(SlashPos != -1)
	{
		MetaFile.Insert(SlashPos, "\\Meta");

		MetaFile += ".xml";
	
		// Generic load from external XML file
		m_CurrentFile = &File;

		LoadXMLFile(MetaFile);	
	}
	
	m_CurrentFile = NULL;
}

int CGnuSchema::VerifyAttribute(CString Name, CString Value)
{
	if(Name.IsEmpty())
		return 0;


	for(int i = 0; i < m_Attributes.size(); i++)
		if(m_Attributes[i].Name.CompareNoCase(Name) == 0)
		{
			// If type is enumerated, gotta match value with that
			if(!Value.IsEmpty() && m_Attributes[i].Type.CompareNoCase("enum") == 0)
			{
				for(int j = 0; j < m_Attributes[i].Enums.size(); j++)
					if(m_Attributes[i].Enums[j].CompareNoCase(Value) == 0)
						return m_Attributes[i].AttributeID;

				return 0;
			}
			
			// Else value can work
			return m_Attributes[i].AttributeID;
		}

	return 0;
}

void CGnuSchema::LoadFileAttribute(SharedFile* pFile, CString AttributeName, CString Value)
{
	Value.TrimRight();

	// Get attribute id
	int AttributeID = VerifyAttribute(AttributeName, Value);

	if(AttributeID == 0 || Value.IsEmpty())
		return;

	// Set value in files attribute map
	std::map<int, CString>::iterator itAttr = pFile->AttributeMap.find(AttributeID);

	if(itAttr != pFile->AttributeMap.end())
		itAttr->second = Value;
	else
		pFile->AttributeMap[AttributeID] = Value;
}

void CGnuSchema::SaveFileAttribute(SharedFile* pFile, int AttributeID, CString Value)
{
	//if(m_pMeta)
	//	m_pMeta->m_pCore->DebugLog("Saving Attributes");

	// Check if read only
	if(GetAttributeReadOnly(AttributeID))
		return;

	// Check attribute validity
	if( !VerifyAttribute(GetAttributeName(AttributeID), Value) )
		return;

	// Check if file has attribute
	std::map<int, CString>::iterator itAttr = pFile->AttributeMap.find(AttributeID);

	if(itAttr != pFile->AttributeMap.end())
		itAttr->second = Value;
	else
		pFile->AttributeMap[AttributeID] = Value;

	// Save meta-data, make sure shared files dont reload during this
	SaveData(*pFile);
}

CString CGnuSchema::GetAttributeName(int AttributeID)
{
	std::map<int, int>::iterator itAttr = m_AttributeMap.find(AttributeID);

	if(itAttr != m_AttributeMap.end())
		return m_Attributes[itAttr->second].Name;

	return "";
}

bool CGnuSchema::GetAttributeReadOnly(int AttributeID)
{
	std::map<int, int>::iterator itAttr = m_AttributeMap.find(AttributeID);

	if(itAttr != m_AttributeMap.end())
		return m_Attributes[itAttr->second].ReadOnly;

	return false;
}

void CGnuSchema::SaveData(SharedFile &File)
{
	//if(m_pMeta)
	//	m_pMeta->m_pCore->DebugLog("Writing File");

	CString FilePath = File.Dir.c_str();

	int SlashPos = FilePath.ReverseFind('\\');
	if(SlashPos == -1)
		return;


	CString Dirpath = FilePath.Left(SlashPos) + "\\Meta";
	CreateDirectory(Dirpath, NULL);
	
	FilePath.Insert(SlashPos, "\\Meta");
	FilePath += ".xml";

	SetFileAttributes(Dirpath, FILE_ATTRIBUTE_NORMAL);
	SetFileAttributes(FilePath, FILE_ATTRIBUTE_NORMAL);


	CStdioFile MetaFile;
	
	// Easier to write the xml than using the library
	if(MetaFile.Open(FilePath, CFile::modeCreate | CFile::modeWrite))
		MetaFile.WriteString( AttrMaptoFileXML(File.AttributeMap) );

	MetaFile.Abort();

	//if(m_pMeta)
	//	m_pMeta->m_pCore->DebugLog("Converting to XML");
}

CString CGnuSchema::AttrMaptoFileXML( std::map<int, CString> &AttrMap )
{
	//if(m_pMeta)
	//	m_pMeta->m_pCore->DebugLog("Converting to XML");

	CString FileXML = "<?xml version=\"1.0\"?>\n";

	FileXML += "<" + m_NamePlural + " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"" + m_Namespace + "\">\n";
	FileXML += "<" + m_Name;

	std::map<int, CString>::iterator itAttr = AttrMap.begin();
	for( ; itAttr != AttrMap.end(); itAttr++)
		if(VerifyAttribute(GetAttributeName(itAttr->first), itAttr->second) )
			FileXML += " " + GetAttributeName(itAttr->first) + "=\"" + itAttr->second + "\"";

	FileXML += "/>\n";
	FileXML += "</" + m_NamePlural + ">\n";

	return FileXML;
}

CString CGnuSchema::AttrMaptoNetXML( std::map<int, CString> &AttrMap, int Index )
{	
	CString NetXML = "<" + m_Name;
		
	if( Index )
		NetXML += " index=\"" + NumtoStr(Index) + "\"";

	// Add attributes
	std::map<int, CString>::iterator itAttr = AttrMap.begin();
	for( ; itAttr != AttrMap.end(); itAttr++)
		if( !itAttr->second.IsEmpty())
		{
			CString Value = itAttr->second;
			Value.Replace("'", "&apos;");
			NetXML += " " + GetAttributeName(itAttr->first) + "=\"" + itAttr->second + "\"";
		}

	NetXML += " />";

	return NetXML;
}

CString CGnuSchema::GetFileAttribute(SharedFile* pFile, CString Name)
{
	CString Value;

	int	AttributeID = VerifyAttribute(Name, "");

	std::map<int, CString>::iterator itAttr = pFile->AttributeMap.find(AttributeID);

	if(itAttr != pFile->AttributeMap.end())
		Value = itAttr->second;

	return Value;
}

void CGnuSchema::SetResultAttributes(std::map<int, CString> &AttributeMap, CString MetaLoad)
{
	int EqualPos = MetaLoad.Find("=\"");

	while(EqualPos != -1)
	{
		int SpaceFrontPos = MetaLoad.Left(EqualPos).ReverseFind(' ');

		int SpaceBackPos  = MetaLoad.Find("\" ", EqualPos + 2);

		if(SpaceFrontPos != -1 && SpaceBackPos != -1)
		{
			CString Name  = MetaLoad.Mid(SpaceFrontPos + 1, EqualPos - SpaceFrontPos - 1);
			CString Value = MetaLoad.Mid(EqualPos + 2, SpaceBackPos - EqualPos -2);

			Value.Replace("&apos;", "'");

			int AttrID = VerifyAttribute(Name, Value);

			if(AttrID)
				AttributeMap[AttrID] = Value;
		}
		else 
			break;

		EqualPos = MetaLoad.Find("=\"", SpaceBackPos + 2);
	}
}

void CGnuSchema::SetAttributeReadOnly(CString Name)
{
	for(int i = 0; i < m_Attributes.size(); i++)
		if(m_Attributes[i].Name.CompareNoCase(Name) == 0)
			m_Attributes[i].ReadOnly = true;
}

// Opening compound storage files
//LPSTORAGE pStorage = NULL;
//
//HRESULT myResult = StgOpenStorage(FilePath.AllocSysString(), NULL, STGM_SIMPLE | STGM_READ | STGM_SHARE_EXCLUSIVE , NULL, NULL, &pStorage);
//
//if(myResult != S_OK)
//	return;
//
//pStorage->Release();