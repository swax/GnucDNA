
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