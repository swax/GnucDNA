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
#include "meta/image.h"
#include "GnuSchemaImage.h"

REGISTER_SCHEMA_CLASS(CGnuSchemaImage, "image.xsd")

CGnuSchemaImage::CGnuSchemaImage(void)
{
	SetAttributeReadOnly("colors");
	SetAttributeReadOnly("width");
	SetAttributeReadOnly("height");
}

CGnuSchemaImage::~CGnuSchemaImage(void)
{
	//m_ImageFile.Destroy();
}

void CGnuSchemaImage::LoadData(SharedFile &File)
{
	// Load generic external meta file
	CGnuSchema::LoadData(File);

	Attribute* imgattr = image_file_analyze(File.Dir.c_str());
	
	if(imgattr)
	{
		LoadFileAttribute(&File, "width",  imgattr[0].value );
		LoadFileAttribute(&File, "height", imgattr[1].value );
		LoadFileAttribute(&File, "colors", imgattr[2].value );

		image_free_attributes(imgattr);
	}

}