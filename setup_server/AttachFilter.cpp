/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    AttachFilter.cpp

Abstract:

	The class for connection/disconnection of filter driver to devices class

Environment:

    User mode

--*/
#include "StdAfx.h"
#include ".\attachfilter.h"
#include <atlbase.h>

const LPCTSTR szUpperFilter = _T("UPPERFILTERS");
const LPCTSTR szLowerFilter = _T("LOWERFILTERS");

bool CAttachFilter::RestartDevice(HDEVINFO DeviceInfoSet, 
								  PSP_DEVINFO_DATA DeviceInfoData, 
								  bool *pbRestart)
{
	if (pbRestart)
	{
		*pbRestart = false;
	}
	SP_PROPCHANGE_PARAMS params;
    SP_DEVINSTALL_PARAMS installParams;

    // for future compatibility; this will zero out the entire struct, rather
    // than just the fields which exist now
    memset(&params, 0, sizeof(SP_PROPCHANGE_PARAMS));

    // initialize the SP_CLASSINSTALL_HEADER struct at the beginning of the
    // SP_PROPCHANGE_PARAMS struct, so that SetupDiSetClassInstallParams will
    // work
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;

    // initialize SP_PROPCHANGE_PARAMS such that the device will be stopped.
    params.StateChange = DICS_STOP;
    params.Scope       = DICS_FLAG_CONFIGSPECIFIC;
    params.HwProfile   = 0; // current profile


	//Sleep (3000);
	// prepare for the call to SetupDiCallClassInstaller (to stop the device)
	if( !SetupDiSetClassInstallParams( DeviceInfoSet,
									   DeviceInfoData,
									   (PSP_CLASSINSTALL_HEADER) &params,
									   sizeof(SP_PROPCHANGE_PARAMS)
		) )
	{
		ATLTRACE("in RestartDevice(): couldn't set the installation parameters!");
		ATLTRACE(" error: %u\n", GetLastError());
		return false;
	}
	// stop the device
	if( !SetupDiCallClassInstaller( DIF_PROPERTYCHANGE,
									DeviceInfoSet,
									DeviceInfoData )
		)
	{
		ATLTRACE("in RestartDevice(): call to class installer (STOP) failed!");
		ATLTRACE(" error: %u\n", GetLastError() );
		return false;
	}

	installParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

	Sleep (500);
	// same as above, the call will succeed, but we still need to check status
	if( !SetupDiGetDeviceInstallParams( DeviceInfoSet,
										DeviceInfoData,
										&installParams )
		)
	{
		ATLTRACE("in RestartDevice(): couldn't get the device installation params!");
		ATLTRACE(" error: %u\n", GetLastError() );
		return false;
	}
	if( installParams.Flags & DI_NEEDREBOOT )
	{
		if (pbRestart)
		{
			*pbRestart = true;
		}
	}
    // restarting the device
    params.StateChange = DICS_START;

    // prepare for the call to SetupDiCallClassInstaller (to stop the device)
    if( !SetupDiSetClassInstallParams( DeviceInfoSet,
                                       DeviceInfoData,
                                       (PSP_CLASSINSTALL_HEADER) &params,
                                       sizeof(SP_PROPCHANGE_PARAMS)
        ) )
    {
        ATLTRACE("in RestartDevice(): couldn't set the installation parameters!");
        ATLTRACE(" error: %u\n", GetLastError());
        return false;
    }

    // restart the device
    if( !SetupDiCallClassInstaller( DIF_PROPERTYCHANGE,
                                    DeviceInfoSet,
                                    DeviceInfoData )
        )
    {
        ATLTRACE("in RestartDevice(): call to class installer (START) failed!");
        ATLTRACE(" error: %u\n", GetLastError());
        return false;
    }

    installParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

	Sleep (500);
    // same as above, the call will succeed, but we still need to check status
    if( !SetupDiGetDeviceInstallParams( DeviceInfoSet,
                                        DeviceInfoData,
                                        &installParams )
        )
    {
        ATLTRACE("in RestartDevice(): couldn't get the device installation params!");
        ATLTRACE(" error: %u\n", GetLastError() );
        return false;
    }

    // to see if the machine needs to be rebooted
    if( installParams.Flags & DI_NEEDREBOOT )
    {
		if (pbRestart)
		{
			*pbRestart = true;
		}
    }
	else
	{
		if ((DWORD)(LOBYTE(LOWORD(GetVersion ()))) > 5)
		{
			Sleep (4000);
			#ifdef DEBUG
				printf ("Sleep (4000)\n");
			#endif
		}
	}

    // if we got this far, then the device has been stopped and restarted
    return true;
}

bool CAttachFilter::RestartDevicesIsServiceName (GUID Guid, LPCTSTR szServiceName, bool *pbRestart)
{
	int		index = 0;

	HDEVINFO hDevInfo;
	hDevInfo = SetupDiGetClassDevs(&Guid, NULL, NULL, DIGCF_PRESENT);
	bool bRestart;

	if ( INVALID_HANDLE_VALUE != hDevInfo)
	{
		SP_DEVINFO_DATA  devInfoElem;
		DWORD			dwType;
		DWORD			dwNeed;
		DWORD			dwSize = 0;
		LPTSTR			szService = new TCHAR [0];
		devInfoElem.cbSize = sizeof(SP_DEVINFO_DATA);

		while ( SetupDiEnumDeviceInfo(hDevInfo, index++, &devInfoElem))
		{
			if (!SetupDiGetDeviceRegistryProperty (hDevInfo, 
										&devInfoElem, 
										SPDRP_SERVICE,
										&dwType,
                                        NULL,
										0, 
										&dwNeed))
			{
				if (GetLastError () == ERROR_INSUFFICIENT_BUFFER)
				{
					if (dwSize < dwNeed)
					{
						delete [] szService ;
						szService = (LPTSTR) new BYTE [dwNeed];
						dwSize = dwNeed;
					}
				}
			}

			if (!SetupDiGetDeviceRegistryProperty (hDevInfo, 
										&devInfoElem, 
										SPDRP_SERVICE,
										&dwType,
                                        (LPBYTE)szService,
										dwSize, 
										&dwNeed))
			{
				continue;
			}


			if (_tcsicmp (szService, szServiceName) == 0)
			{
				ATLTRACE2 (_T("usbhub\r\n"));
				RestartDevice (hDevInfo, &devInfoElem, &bRestart);
				if (pbRestart != NULL && bRestart)
				{
					*pbRestart = bRestart;
				}
			}

		}

		SetupDiDestroyDeviceInfoList(hDevInfo);
        return true;
	}

	return false;
}

bool CAttachFilter::AttachFilterToClass (GUID Guid, LPCTSTR szFilter, bool bUpperLower)
{
	HKEY	hKey;
	bool	bRet = false;
	DWORD	dwType;
	DWORD	dwSize = 0, dwSizeNew = 0;
	LPTSTR	szFilters, szFilterTemp;

	hKey = SetupDiOpenClassRegKey (&Guid, KEY_READ | KEY_WRITE);
	if (hKey != INVALID_HANDLE_VALUE)
	{
		// Get old filters
		RegQueryValueEx (hKey, bUpperLower?szUpperFilter:szLowerFilter, 0, &dwType, NULL, &dwSize);
		dwSizeNew = (dwSize?dwSize:sizeof (TCHAR)) + (_tcslen (szFilter) + 1)*sizeof (TCHAR);
        szFilters = (LPTSTR)new TCHAR [dwSizeNew]; 
		RtlZeroMemory (szFilters, dwSizeNew);

		RegQueryValueEx (hKey, bUpperLower?szUpperFilter:szLowerFilter, 0, &dwType, (LPBYTE)szFilters, &dwSize);

		// verify exist our service filter
		dwType = REG_MULTI_SZ;
		szFilterTemp = szFilters;
		while (_tcslen (szFilterTemp))
		{
			if (_tcsicmp (szFilterTemp, szFilter) == 0)
			{
				bRet = true;
			}
			szFilterTemp += _tcslen (szFilterTemp) + 1;
		}

		// add our filter to filters
		if (!bRet)
		{
			_tcscpy (szFilterTemp, szFilter);
			szFilterTemp += _tcslen (szFilterTemp) + 1;
			_tcscpy (szFilterTemp, _T(""));
			bRet = RegSetValueEx (hKey, bUpperLower?szUpperFilter:szLowerFilter, 0, dwType, (LPBYTE)szFilters, dwSizeNew)?false:true;
		}

		delete [] szFilters;
		RegCloseKey (hKey);
	}
	return bRet;
}

bool CAttachFilter::DetachFilterToClass (GUID Guid, LPCTSTR szFilter, bool bUpperLower)
{
	HKEY	hKey;
	bool	bRet = false;
	DWORD	dwType;
	DWORD	dwSize = 0;
	LPTSTR	szFilters, szFilterTemp;

	hKey = SetupDiOpenClassRegKey (&Guid, KEY_READ | KEY_WRITE);
	if (hKey != INVALID_HANDLE_VALUE)
	{
		// Get old filters
		RegQueryValueEx (hKey, bUpperLower?szUpperFilter:szLowerFilter, 0, &dwType, NULL, &dwSize);
        szFilters = (LPTSTR)new TCHAR [dwSize + 1]; 

		RtlZeroMemory (szFilters, dwSize + 1);

		if (ERROR_SUCCESS == RegQueryValueEx (hKey, bUpperLower?szUpperFilter:szLowerFilter, 0, &dwType, (LPBYTE)szFilters, &dwSize))
		{
			// verify exist our service filter
			szFilterTemp = szFilters;
			int iPos = 0;
			while (_tcslen (szFilterTemp))
			{
				if (_tcsicmp (szFilterTemp, szFilter) == 0)
				{
					DWORD dwTemp = (DWORD)_tcslen (szFilterTemp) + 1;
					CopyMemory (szFilterTemp, szFilterTemp + dwTemp, dwSize - (iPos + dwTemp)*sizeof (TCHAR));
					dwSize -= dwTemp*sizeof (TCHAR);

					bRet = RegSetValueEx (hKey, bUpperLower?szUpperFilter:szLowerFilter, 0, dwType, (LPBYTE)szFilters, dwSize)?false:true;
					break;
				}
				iPos += (int)_tcslen (szFilterTemp) + 1;
				szFilterTemp += _tcslen (szFilterTemp) + 1;
			}
		}
		delete [] szFilters;
		RegFlushKey (hKey);
		RegCloseKey (hKey);
	}
	return bRet;
}
