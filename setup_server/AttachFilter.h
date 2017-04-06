/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    AttachFilter.h

Abstract:

	The class for connection/disconnection of filter driver to devices class

Environment:

    User mode

--*/
#pragma once

#include <setupapi.h>

#pragma comment( lib, "setupapi.lib" )

typedef bool (*AttatchFunction)(HDEVINFO , SP_DEVINFO_DATA *, LPCTSTR, bool, bool *);

class CAttachFilter
{
private:
	static bool RestartDevice(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, bool *pbRestart = NULL);

public:

    static GUID		GetInfClass (LPCTSTR szInfFile);
	static LPTSTR	GetInfClass (LPCTSTR szInfFile, LPTSTR zzClassName, int ClassNameSize);

	static bool		RestartDevicesIsServiceName (GUID Guid, LPCTSTR szServiceName, bool *pbRestart);
    static bool		AttachFilterToClass (GUID Guid, LPCTSTR szFilter, bool bUpperLower = true);
	static bool		DetachFilterToClass (GUID Guid, LPCTSTR szFilter, bool bUpperLower = true);
};
