
#pragma once


namespace gdna 
{

	
struct DimeRecord
{
	bool First;
	bool Last;
	bool Chunked;

	byte tType;

	CString Options;
	uint16  OptionsLength;

	CString ID;
	uint16  IDLength;

	CString Type;
	uint16  TypeLength;

	byte* Data;
	int   DataLength;

	DimeRecord()
	{
		First   = false;
		Last    = false;
		Chunked = false;

		OptionsLength = 0;
		IDLength      = 0;
		TypeLength    = 0;

		Data   = NULL;
		DataLength = 0;
	}
};


class DIME
{
public:
	DIME(byte* pData, int length);

	enum ReadResult { READ_GOOD, READ_INCOMPLETE, READ_ERROR };
	enum RecordType { UNCHANGED, MEDIA_TYPE, ABSOLUTE_URI, UNKNOWN, NONE};

	ReadResult ReadNextRecord(DimeRecord &Record);
	ReadResult ReadSetting(CString &Setting, int Length);

	int WriteRecord(byte Flags, byte tType, CString ID, CString Type, const void* Data, int DataLength);
	int WriteField(const void* Data, int DataLength);

	byte* m_pData;
	int   m_Length;

	byte* m_pNextPos;
	int   m_BytesLeft;
};


} // end dna namespace