#pragma once

#include "GnuSchema.h"



// list of genres
static char* g_arrMP3Genre[] = {
	"Blues","Classic Rock","Country","Dance","Disco","Funk","Grunge","Hip-Hop",
	"Jazz","Metal","New Age","Oldies","Other","Pop","R&B","Rap","Reggae","Rock",
	"Techno","Industrial","Alternative","Ska","Death Metal","Pranks","Soundtrack",
	"Euro-Techno","Ambient","Trip Hop","Vocal","Jazz Funk","Fusion","Trance",
	"Classical","Instrumental","Acid","House","Game","Sound Clip","Gospel","Noise",
	"Alternative Rock","Bass","Soul","Punk","Space","Meditative","Instrumental Pop",
	"Instrumental Rock","Ethnic","Gothic","Darkwave","Techno-Industrial","Electronic",
	"Pop Folk","Eurodance","Dream","Southern Rock","Comedy","Cult","Gangsta","Top 40",
	"Christian Rap","Pop Funk","Jungle","Native American","Cabaret","New Wave",
	"Psychadelic","Rave","ShowTunes","Trailer","Lo-Fi","Tribal","Acid Punk","Acid Jazz",
	"Polka","Retro","Musical","Rock & Roll","Hard Rock","Folk","Folk Rock",
	"National Folk","Swing","Fast Fusion","Bebob","Latin","Revival","Celtic",
	"Bluegrass","Avantgarde","Gothic Rock","Progressive Rock","Psychedelic Rock",
	"Symphonic Rock","Slow Rock","Big Band","Chorus","Easy Listening","Acoustic",
	"Humour","Speech","Chanson","Opera","Chamber Music","Sonata","Symphony","Booty Bass",
	"Primus","Porn Groove","Satire","Slow Jam","Club","Tango","Samba","Folklore","Ballad",
	"Power Ballad","Rhytmic Soul","Freestyle","Duet","Punk Rock","Drum Solo","A Capella",
	"Euro House","Dance Hall","Goa","Drum & Bass","Club House","Hardcore","Terror",
	"Indie","BritPop","Negerpunk","Polsk Punk","Beat","Christian Gangsta Rap",
	"Heavy Metal","Black Metal","Crossover","Contemporary Christian","Christian Rock",
	"Merengue","Salsa","Trash Metal","Anime","JPop","SynthPop"
};

static int g_nMP3GenreCount = 148;

// matrix of bitrates [based on MPEG version data][bitrate index]
static int g_nMP3BitRate[6][16] = {
	{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,-1},
	{0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,-1},
	{0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,-1},
	{0,32,48,56, 64, 80, 96,112,128,144,160,176,192,224,256,-1},
	{0, 8,16,24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,-1},
	{0, 8,16,24, 32, 64, 80, 56, 64,128,160,112,128,256,320,-1},
};

// MP3FRAMEHEADER structure
struct MP3FRAMEHEADER
{
	unsigned emphasis   : 2;		// M
	unsigned original   : 1;		// L
	unsigned copyright  : 1;		// K
	unsigned modeext    : 2;		// J
	unsigned chanmode   : 2;		// I
	unsigned privbit    : 1;		// H
	unsigned padding    : 1;		// G
	unsigned samplerate : 2;		// F
	unsigned bitrate    : 4;		// E
	unsigned hascrc     : 1;		// D
	unsigned mpeglayer  : 2;		// C
	unsigned mpegver    : 2;		// B
	unsigned framesync  : 11;		// A
};

// MP3ID3V1TAG structure
struct MP3ID3V1TAG
{
	char ident[3]; // TAG
	char title[30];
	char artist[30];
	char album[30];
	char year[4];
	char comment[28];
	BYTE reserved;
	BYTE tracknum;
	BYTE genre;
};

// ENMPEGVER enumeration
enum ENMPEGVER
{
	MPEGVER_NA,		// reserved, N/A
	MPEGVER_25,		// 2.5
	MPEGVER_2,		// 2.0 (ISO/IEC 13818-3)
	MPEGVER_1		// 1.0 (ISO/IEC 11172-3)
};

// ENCHANNELMODE enumeration
enum ENCHANNELMODE
{
	MP3CM_STEREO,
	MP3CM_JOINT_STEREO,
	MP3CM_DUAL_CHANNEL,
	MP3CM_SINGLE_CHANNEL
};

// ENEMPHASIS enumeration
enum ENEMPHASIS
{
	MP3EM_NONE,
	MP3EM_50_15_MS,		// 50/15 ms
	MP3EM_RESERVED,
	MP3EM_CCIT_J17		// CCIT J.17
};

// bitmask of validity of files
#define MP3VLD_INVALID			0x0000
#define MP3VLD_ID3V1_VALID		0x0001
#define MP3VLD_DATA_VALID		0x0002



class CGnuSchemaAudio : public CGnuSchema
{
public:
	DECLARE_SCHEMA_CLASS()
	
	CGnuSchemaAudio();
	virtual ~CGnuSchemaAudio(void);


	virtual void LoadData(SharedFile &File);
	virtual void SaveData(SharedFile &File);


	// Audio Specific
	bool LoadID3v1(SharedFile *pFile);
	bool SaveID3v1(SharedFile *pFile); // defaults to current file
	void Clear();

	static CString GetGenreString(int nIndex);
	static CString GetLengthString(int nSeconds);

	// helper functions
	bool GetNextFrameHeader(HANDLE hFile, MP3FRAMEHEADER* pHeader, int nPassBytes = 0);
	void ChangeEndian(void* pBuffer, int nBufSize);


	CString m_strFile;
	DWORD	m_dwValidity;

	// ID3V1 information
	//CString  m_strArtist;
	//CString  m_strTitle;
	//CString  m_strAlbum;
	//CString  m_strComment;
	//CString  m_strYear;
	//int		 m_nTrack;
	//int		 m_nGenre;

	// MP3 frame information
	UINT		m_nFrames;
	//UINT		m_nLength;					// in seconds
	ENMPEGVER	m_enMPEGVersion;
	int			m_nMPEGLayer;
	bool		m_bHasCRC;
	//int			m_nBitRate;					// average if VBR, 0 if "free"
	//int			m_nSampleRate;
	ENCHANNELMODE m_enChannelMode;
	ENEMPHASIS	m_enEmphasis;
	bool		m_bCopyrighted;
	bool		m_bOriginal;
};
