#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#endif

//#define _UNICODE

#pragma warning(disable:4786)
#pragma warning(disable:4018)  // Disable un-signed/signed comparison warnings
#pragma warning(disable:4244)  // Disable type conversion warnings
#pragma warning(disable:4800)  // Disable bool conversion warnings
#pragma warning(disable:4267)  // Disable int conversion warnings
#pragma warning(disable:4312)

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows 95 and Windows NT 4 or later.
#define WINVER 0x0400		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows NT 4 or later.
#define _WIN32_WINNT 0x0400	// Change this to the appropriate value to target Windows 2000 or later.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 4.0 or later.
#define _WIN32_IE 0x0400	// Change this to the appropriate value to target IE 5.0 or later.
#endif


#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit


#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions


//#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE classes
#include <afxodlgs.h>       // MFC OLE dialog classes
#include <afxdisp.h>        // MFC Automation classes
//#endif						// _AFX_NO_OLE_SUPPORT

#include <comdef.h>		
#include <comutil.h>

#include <afxdtctl.h>			// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>				// MFC support for Windows Common Controls
#endif							// _AFX_NO_AFXCMN_SUPPORT


#include <afxsock.h>		// MFC socket extensions
#include <afxinet.h>		// MFC INet extensions
//#include <msxml2.h>


// STL Lib
#include <memory>
#include <vector>
#include <string>
#include <list>
#include <queue>
#include <deque>
#include <map>

// Zlib
#include "zlib/zlib.h"


// DNA Build Options, only define one
#define DNA_VERSION "1.1.0.1"

#define BUILD_STATIC           // Compiling or using DNA as library
//#define BUILD_DYNAMIC_DNA    // Compiling DNA as DLL
//#define BUILD_DYNAMIC_CLIENT // Client using DNA as DLL


#ifdef BUILD_STATIC
 #define GNUC_API
#endif

#ifdef BUILD_DYNAMIC_DNA
  #define GNUC_API __declspec(dllexport)
#endif

#ifdef BUILD_DYNAMIC_CLIENT
 #define GNUC_API __declspec(dllimport)
#endif


// Debugging
#define new DEBUG_NEW

// Networking
#define UDP_PORT				5467
#define VERSION_4_CONNECT		"GNUTELLA CONNECT/0.4\n\n"
#define VERSION_4_CONNECT_OK	"GNUTELLA OK\n\n"


// Threading
#define CPU_0	0x0001
#define CPU_1	0x0002
#define CPU_2	0x0004 
#define CPU_3	0x0008


// Prog defines
#define CONNECT_TIMEOUT		5
#define TRANSFER_TIMEOUT    10

// Hardcoded Enums
#define NETWORK_GNUTELLA	1
#define NETWORK_G2			2
#define NETWORK_WEB			3

#define CLIENT_UNKNOWN		 1
#define CLIENT_GNU_ULTRAPEER 2
#define CLIENT_GNU_LEAF		 3
#define CLIENT_G2_HUB		 4
#define CLIENT_G2_CHILD		 5

#define UPDATE_RELEASE		1
#define UPDATE_BETA			2
#define UPDATE_NONE			3

#define SOCK_UNKNOWN		0
#define SOCK_CONNECTING		1
#define SOCK_CONNECTED		2
#define SOCK_CLOSED			3

// Priority of each packet
#define PACKET_PING			5
#define PACKET_PONG			4
#define PACKET_QUERY		3
#define PACKET_QUERYHIT		2
#define PACKET_PUSH			1
#define PACKET_BYE			1
#define PACKET_VENDMSG		1

#define ERROR_NONE			0
#define ERROR_HOPS			1
#define ERROR_LOOPBACK		2
#define ERROR_TTL			3
#define ERROR_DUPLICATE		4
#define ERROR_ROUTING		5
#define ERROR_UNKNOWN		6

#define TRANSFER_NOSOURCES  1
#define TRANSFER_PENDING	2
#define TRANSFER_CONNECTING	3
#define TRANSFER_CONNECTED	4
#define TRANSFER_SENDING	5
#define TRANSFER_RECEIVING  6
#define TRANSFER_PUSH		7
#define TRANSFER_CLOSED		8
#define TRANSFER_COMPLETED	9
#define TRANSFER_COOLDOWN	10
#define TRANSFER_QUEUED		11

#define RESULT_INACTIVE		1
#define RESULT_TRYING		2
#define RESULT_DOWNLOADING	3
#define RESULT_COMPLETED	4
#define RESULT_NOSOURCES	5
#define RESULT_SHARED		6

#define HASH_TYPES			5
#define HASH_SHA1			0
#define HASH_MD5			1
#define HASH_MD4_ED2K		2
#define HASH_TIGERTREE		3
#define HASH_BITPRINT		4
#define HASH_UNKNOWN		-1

#define ALLOW				1
#define DENY				0

#define LIMIT_NONE			0
#define LIMIT_LESS			1
#define LIMIT_EXACTLY		2
#define LIMIT_MORE			3


typedef unsigned char    word8;
typedef unsigned short   word16;
typedef unsigned long    word32;
typedef unsigned __int64 word64;

typedef char    int8;
typedef short   int16;
typedef long    int32;
typedef __int64 int64;

typedef unsigned char    uint8;
typedef unsigned short   uint16;
typedef unsigned int     uint32;
typedef unsigned __int64 uint64;


namespace gdna { };
using namespace gdna;

#include "./Conversions.h"
#include "./Packet.h"
#include "./Common.h"
#include "./FileLock.h"
#include "./Headers.h"

