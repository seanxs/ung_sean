/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   utils.cpp

Environment:

    Kernel mode

--*/

#include "utils.h"

ULONG GetOsVersion ()
{
	static ULONG	   uVersion = 0;

	if (uVersion == 0)
	{
		ULONG	   uMinor;
		PsGetVersion (&uVersion, &uMinor, NULL, NULL);
		uVersion *= 100;
		uVersion += uMinor;
	}

	return uVersion;
}