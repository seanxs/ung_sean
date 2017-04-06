/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    ManagerService.cpp

Abstract:

	Work with services class. Creation, deletion, status, etc.

Environment:

    User mode

--*/
#include <tchar.h>
#include "managerservice.h"
#include <atlbase.h>
#include <atlstr.h>

CManagerService::CManagerService(void)
{
	m_scm = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);
}

CManagerService::CManagerService(DWORD dwDesiredAccess)
{
	m_scm = OpenSCManager (NULL, NULL, dwDesiredAccess);
}

CManagerService::~CManagerService(void)
{
	CloseServiceHandle (m_scm);
}

bool CManagerService::IsServiceCreated (LPCTSTR szServiceName)
{
	bool bRet = false;
	if (!m_scm)
	{
		return bRet;
	}
	SC_HANDLE hService;

	hService = ::OpenService (m_scm, szServiceName, SC_MANAGER_ALL_ACCESS);
	bRet = hService?true:false;
	CloseServiceHandle (hService);

	return bRet;
}

bool CManagerService::DeleteService (LPCTSTR szServiceName)
{
	bool bRet = false;

	if (!m_scm)
	{
		return bRet;
	}
	SC_HANDLE hService;

	StopService (szServiceName);
	if (hService = ::OpenService (m_scm, szServiceName, SC_MANAGER_ALL_ACCESS))
	{
		if (::DeleteService (hService))
		{
			bRet = true;
		}
		CloseServiceHandle (hService);
	}

	return bRet;
}

bool CManagerService::CreateService (LPCTSTR szServiceName,
							LPCTSTR szDisplayName,
							DWORD dwServiceType,
							DWORD dwStartType,
							LPCTSTR szBinaryPathName,
							LPCTSTR szLoadOrderGroup,
							LPCTSTR szDependencies,
							LPCTSTR szServiceStartName,
							LPCTSTR szPassword,
							DWORD dwErrorControl)
{
	TCHAR	szPath [MAX_PATH];
	bool bRet = false;

	if (!m_scm)
	{
		return bRet;
	}

	if (szBinaryPathName == NULL || !_tcslen (szBinaryPathName))
	{
		GetModuleFileName (NULL, szPath, MAX_PATH);
	}
	else
	{
		_tcsncpy (szPath, szBinaryPathName, MAX_PATH);
	}

	SC_HANDLE hService =  ::CreateService (m_scm, 
										szServiceName,
										szDisplayName, 
										SC_MANAGER_ALL_ACCESS, 
										dwServiceType,
                                        dwStartType,
										dwErrorControl, 
										szPath, 
										szLoadOrderGroup,
										NULL,
										szDependencies,
										szServiceStartName,
										szPassword);

	bRet = hService?true:false;
	int iErr = GetLastError ();
	CloseServiceHandle (hService);
	SetLastError (iErr);
	return bRet;
}

bool CManagerService::IsServiceRunning (LPCTSTR szServiceName)
{
	SERVICE_STATUS	status;
	bool bRet = false;
	if (!m_scm)
	{
		return bRet;
	}
	SC_HANDLE hService;

	if (hService = ::OpenService (m_scm, szServiceName, 0))
	{
		if (QueryServiceStatus (hService, &status))
		{
			bRet = (status.dwCurrentState == SERVICE_RUNNING);
		}
		int iErr = GetLastError ();
		CloseServiceHandle (hService);
		SetLastError (iErr);
	}

	return bRet;
}

DWORD CManagerService::ServiceStartType (LPCTSTR szServiceName)
{
	QUERY_SERVICE_CONFIG	config;
	DWORD dwRet = -1;
	DWORD dwNeed;
	if (!m_scm)
	{
		return dwRet;
	}
	SC_HANDLE hService;

	if (hService = ::OpenService (m_scm, szServiceName, 0))
	{
		if (QueryServiceConfig (hService, &config, sizeof (QUERY_SERVICE_CONFIG), &dwNeed))
		{
			dwRet = config.dwStartType;
		}
		else
		{
			if (GetLastError () == ERROR_INSUFFICIENT_BUFFER)
			{
				dwRet = config.dwStartType;
			}
		}
		int iErr = GetLastError ();
		CloseServiceHandle (hService);
		SetLastError (iErr);
	}

	return dwRet;
}
bool CManagerService::StartService (LPCTSTR szServiceName)
{
	if (!m_scm)
	{
		return false;
	}	

	SC_HANDLE hServ = OpenService(m_scm, szServiceName, SC_MANAGER_ALL_ACCESS);

	if (!hServ)
	{
		return false;
	}

	if (!::StartService(hServ, 0, NULL))
	{
		return false;
	}
	int iErr = GetLastError ();
	CloseServiceHandle (hServ);
	SetLastError (iErr);

	return true;
}

bool CManagerService::StopService (LPCTSTR szServiceName)
{
	if (!m_scm)
	{
		return false;
	}	

	SC_HANDLE hServ = OpenService(m_scm, szServiceName, SERVICE_STOP);

	if (!hServ) 
	{
		return false;
	}

	SERVICE_STATUS status;
	if (!ControlService(hServ, SERVICE_CONTROL_STOP, &status))
	{
		int err = GetLastError();
		if (err != ERROR_SERVICE_NOT_ACTIVE &&
			err != ERROR_SHUTDOWN_IN_PROGRESS)
		{
			return false;
		}
	}
	else
	{
		Sleep(status.dwWaitHint);
	}

	int iErr = GetLastError ();
	CloseServiceHandle (hServ);
	SetLastError (iErr);
	return true;
}


bool CManagerService::SetServiceStartType (LPCTSTR szServiceName, DWORD dwStartType)
{
	bool bRet = true;
	if (!m_scm)
	{
		return false;
	}	

	SC_HANDLE hServ = OpenService(m_scm, szServiceName, SC_MANAGER_ALL_ACCESS);

	if (!hServ) 
	{
		return false;
	}

	if (!ChangeServiceConfig(hServ,
					SERVICE_NO_CHANGE, 
					dwStartType, 
					SERVICE_NO_CHANGE, 
					NULL,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL))
	{
		int err = GetLastError();
		if (err != ERROR_SERVICE_NOT_ACTIVE &&
			err != ERROR_SHUTDOWN_IN_PROGRESS)
		{
			bRet = false;
		}
	}

	int iErr = GetLastError ();
	CloseServiceHandle (hServ);
	SetLastError (iErr);
	return bRet;
}

bool CManagerService::SetBootFlags (LPCTSTR szServiceName, DWORD dwFalgs)
{
	CRegKey reg;
	CString strKey = _T("SYSTEM\\CurrentControlSet\\services\\");

	strKey += szServiceName;

	if (reg.Open(HKEY_LOCAL_MACHINE, strKey) == ERROR_SUCCESS)
	{
		return (reg.SetDWORDValue(_T("BootFlags"), dwFalgs) == ERROR_SUCCESS);
	}

	return false;
}