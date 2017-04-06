/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    consts_helper.cpp

Environment:

    User mode

--*/
#include "stdafx.h"
#include "consts_helper.h"

namespace Consts 
{

	int PipeCommandFromStr (LPCSTR str)
	{
#define X(e,s) do { if (_stricmp (str, (s)) == 0) return (e); } while(0);
		PIPE_COMMANDS_LIST
#undef X
			
		return -1;
	}

	int SettingsCodeFromStr (LPCSTR str)
	{
		for (int i=0; i < UNG_NET_SETTINGS_MAX; ++i) 
		{
			if (_stricmp (str, szUngNetSettings [i]) == 0)
				return i;
		}
		return -1;
	}

	int ErrorCodeFromStr (LPCSTR str)
	{
		for (int i=0; i < UNG_ERROR_CODES_MAX; ++i) 
		{
			if (_stricmp (str, szUngErrorCodes [i]) == 0)
				return i;
		}
		return -1;
	}

	int LicenseDataNameFromStr (LPCSTR str)
	{
		for (int i=0; i < UNG_LICENSE_DATA_MAX; ++i) 
		{
			if (_stricmp (str, szLicenseDataName [i]) == 0)
				return i;
		}
		return -1;
	}

	int LicenseTypeFromStr (LPCTSTR str)
	{
		for (int i=0; i < UNG_LICENSE_TYPES_MAX; ++i) 
		{
			if (_tcsicmp (str, szLicenseType [i]) == 0)
				return i;
		}
		return -1;
	}

	ELicenseClass LicenseClassFromStr(LPCTSTR str)
	{
#define X(e,v,k,d)	do { if (_tcsicmp (str, _T(v)) == 0) return (k); } while(0);
		UNG_LICENSE_TYPES_LIST
#undef X
		
		return ELC_plain;
	}

	int LicenseActionFromStr (LPCSTR str)
	{
#define X(e,s) do { if (_stricmp (str, (s)) == 0) return (e); } while(0);
		UNG_LICENSE_ACTIONS
#undef X
			
		return -1;
	}

} // namespace Consts
