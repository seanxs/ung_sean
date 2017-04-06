/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    consts_helper.h

Environment:

    User mode

--*/
#pragma once

#include "consts.h"

namespace Consts
{
	// converts string command to enum value of CommandPipe or returns -1 if error
	int PipeCommandFromStr (LPCSTR str);

	// converts string to enum value of UNGNetSettings or returns -1 if error
	int SettingsCodeFromStr (LPCSTR str);

	// converts string to enum value of UNGErrorCodes or returns -1 if error
	int ErrorCodeFromStr (LPCSTR str);

	// converts string to enum value of UNGLicenseDataName or returns -1 if error
	int LicenseDataNameFromStr (LPCSTR str);

	// converts string to enum value of UNGLicenseType or returns -1 if error
	int LicenseTypeFromStr (LPCTSTR str);

	ELicenseClass LicenseClassFromStr (LPCTSTR str);

	// converts string to enum value of UNGLicenseAction or returns -1 if error
	int LicenseActionFromStr (LPCSTR str);

} // namespace Consts
