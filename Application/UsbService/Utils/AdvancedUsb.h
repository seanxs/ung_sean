/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    AdvancedUsb.h

Abstract:

	Class allowing to restart USB devices 

Environment:

    User mode

--*/

#pragma once

#include <setupapi.h>

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

enum
{
	ADU_ENABLE = 1,
	ADU_DISABLE = 2,
	ADU_RESTART = 4,
	ADU_PROPCHANGE = 8,
};

#pragma comment( lib, "setupapi.lib" )
class CUsbServer;

class CAdvancedUsb
{
private:
	CAdvancedUsb(void);
	~CAdvancedUsb(void);

	static bool ControlDevices (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD dwControl, bool *pbRestart);
	static bool ControlDevicesIsDriverName (GUID *Guid, LPCTSTR szDriverName, DWORD dwControl, CUsbServer *pServer, bool *pbRestart);
	static bool RestartDevice(GUID *Guid, LPCTSTR szDriverName, bool *pbRestart, CUsbServer *pServer);

	static bool EnableDevice (LPCTSTR DriverName, bool bEnable);

public:
	static bool RestartHub (LPCTSTR szDriverName, GUID *Guid, CUsbServer *pServer);

	static bool RestartOnInstanceId (LPCTSTR szInstanceId, bool *pbRestart, CUsbServer *pServer);
	static bool ControlOnInstanceId (LPCTSTR szInstanceId, DWORD dwControl, CUsbServer *pServer, bool *pbRestart);
};
