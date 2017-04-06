/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    ManagerService.h

Abstract:

	Class for work with services. Allows adding, deleting, receiving status, etc.

Environment:

    User mode

--*/

#pragma once

#include <windows.h>
#include <Winsvc.h>
#pragma comment(lib, "Advapi32.lib")

class CManagerService
{
private:
	SC_HANDLE	m_scm;
public:
	CManagerService(void);
	CManagerService(DWORD dwDesiredAccess);
	~CManagerService(void);

	bool	IsServiceCreated (LPCTSTR szServiceName);
	bool	DeleteService (LPCTSTR szServiceName);
	bool	CreateService (LPCTSTR szServiceName,
								LPCTSTR szDisplayName = NULL,
								DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS,
								DWORD dwStartType = SERVICE_DEMAND_START,
								LPCTSTR szBinaryPathName = NULL,
								LPCTSTR szLoadOrderGroup = NULL,
								LPCTSTR szDependencies = NULL,
								LPCTSTR szServiceStartName = NULL,
								LPCTSTR szPassword = NULL,
								DWORD dwErrorControl = SERVICE_ERROR_IGNORE
								);
	bool	IsServiceRunning (LPCTSTR szServiceName);
	DWORD	ServiceStartType (LPCTSTR szServiceName);
	bool	SetServiceStartType (LPCTSTR szServiceName, DWORD dwStartType);
	bool	StartService (LPCTSTR szServiceName);
	bool	StopService (LPCTSTR szServiceName);
};
