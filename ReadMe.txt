
Welcome to GnucDNA.

This is a Gnutella COM component for use in a variety of applications
and services.  Any changes made please add them to the ChangeLog file 
with your name and contact information


Version changing notes:
	.idl file
	version res file
	stdafx.h file
	gnucdna.cpp file
	
	
Before Release
	Make sure version set in resource file
	Make sure debug log off

	
C++ Implementation notes:
	#include "afxctl.h" needed for AfxConnectionAdvise (events)
	#include <comdef.h> needed for _variant_t and helper classes
	
Lesson
	Use most popular of data set to prevent fakes
	6025 - pure virtual function call
	  Occured while deleting a corrupt class, array was being assigned values out of bounds
	  
-----------------------------------------------------------------------------
ReSearching Transfers

Start()
    Set the research on start
    Set next, 15 min from now

ReSearch()
    Reset the research on research
    Set next research double of previous
    Max at 8 hours, 15m, 30m, 1h, 2h, 4h, 8h...

Minute Timer in Transfer Control
    Dont research pending or done
    Find the host with the lowest research value
       If greater than current time
       If 5 minutes past since last research
          Do research()

Math worst case
	200 Nodes, 1 ReSearch Query ever 5 mins
	G1 Ultrapeer 40 queries / min all dynamic
	G2 whole network...
	


Connecting
	Trust 5 alt hosts from remote node
	Pick random node from top 15 to try next
	If cache item is added that has been tried in last minute put at bottom of list

Downloading
	only hosts that transferred put on alt
	hosts that didnt work put on nalt
	nalt is used to notify upload side

Subnets
	Only use 3 hosts per class C subnet to prevent flooding
	Doesnt apply to private IPs, limewire sends them like crazy

upload
	only hosts transferred data to put on alt
	take hosts off if received nalt for it // no rouge is going to know everyone we're transferring to
	servers dont send or keep nalt list

