#pragma once

#include <afxstr.h>
#include <atlimage.h>
#include "GnuSchema.h"


class CGnuSchemaImage : public CGnuSchema
{
public:
	CGnuSchemaImage(void);
	virtual ~CGnuSchemaImage(void);
	DECLARE_SCHEMA_CLASS()

	virtual void LoadData(SharedFile &File);

	//CImage m_ImageFile;
};
