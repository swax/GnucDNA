#pragma once

#include "GnuSchema.h"

class CGnuSchemaContact : public CGnuSchema
{
public:
	CGnuSchemaContact(void);
	virtual ~CGnuSchemaContact(void);
	DECLARE_SCHEMA_CLASS()

	virtual void LoadData(SharedFile &File);
	virtual void SaveData(SharedFile &File);

	void LoadContact(SharedFile *pFile);
	void SaveContact(SharedFile *pFile);

	void ParseLine(SharedFile *pFile, CString &Line);
};
