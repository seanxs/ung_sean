/*++

Copyright (c) 2000-2014 ELTIMA Software All Rights Reserved

Module Name:

    setup_server.cpp

Abstract:

	The main module of installation and deinstallation of U2EC

Environment:

   user mode

--*/

#include "stdafx.h"
#include "ManagerService.h"
#include "..\Application\Consts\consts.h"
#include "attachfilter.h"
#include "setup_server.h"
#include "atlstr.h"
#include "newdev.h"
#include "Winsvc.h"
#include "..\Drivers\common\public.h"
#include <winioctl.h>
#include "..\Application\Consts\UsbDevInfo.h"
#include <boost\scoped_array.hpp>
#include <vector>
//#include "usbioctl.h"
#include "FileVersionInfo.h"
#include "exename.h"
#include "reg_token_array.h"
#include "reg_token_cfg.h"

#pragma comment( lib, "newdev.lib" )
#pragma comment( lib, "newdev64.lib" )

bool UsbHubStopAllDev (CUsbDevInfo *dev, int iType);

//PCHAR GetVendorString (USHORT     idVendor);

typedef struct CommandLineArgs
{
	std::string cmd; //!< command to execute: `install`, `uninstall`
	bool force;      //!< force execute command
} CommandLineArgs;
CommandLineArgs ParseCommandLine (int argc, _TCHAR* argv[]);


int _tmain(int argc, _TCHAR* argv[])
{
	CommandLineArgs args = ParseCommandLine (argc, argv);

	//MessageBox (NULL, _T(""), _T(""), MB_OK);
	if (args.cmd == "install")
	{
		// Install drivers
		std::string token;
		if (!GetTokenFromExeNameAsStr (token) || token == "" || token == "server")
		{
#if 0
			MessageBox (NULL, _T("Setup server executable is corrupted\n(Error 42)"), 
				_T("Error"), MB_OK | MB_ICONERROR);
			return -1;
#endif
		}

		if (!args.force) 
		{
			const bool need_update = CheckUsbHubFilterNeedUpdate();
			//printf ("need_update: %s\n", need_update ? "true" : "false");
			args.force = need_update;
		}

		if (!args.force) 
		{
			CManagerService manager;
			args.force = manager.IsServiceCreated (Consts::szDeviceBusFilterName)?false:true;
		}

		return MainInstallDrivers(token, args.force);
    }
    else if (args.cmd == "uninstall")
    {
		std::string token;
		GetTokenFromExeNameAsStr (token);
		return MainUninstallDrivers(token, args.force);
	}

	// [bialix 2014/10/02] There was code to install/enable/disable/uninstall UNG's UsbService.
	// It's now deleted because we want this application to support both UNG and FlexiHub.
	// If you need those commands back, please see svn history r785 or earlier.

    return 0;
}

CommandLineArgs ParseCommandLine (int argc, _TCHAR* argv[])
{
	CommandLineArgs a;
	a.cmd = "install";
	a.force = false;

	if (argc == 1)
	{
		a.cmd = "install";
		return a;
	}

	for (int i=1; i<argc; ++i)
	{
		TCHAR *pArg = argv [i];

		if (_tcsicmp (pArg, _T("/F")) == 0)
			a.force = true;
		else if (_tcsicmp (pArg, _T("/U")) == 0)
			a.cmd = "uninstall";
		else if (pArg [0] != _T('/'))
			break;
	}

	return a;
}

typedef BOOL (APIENTRY *LPFN_ISWOW64PROCESS) (HANDLE hProcess,PBOOL Wow64Process);
static LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(_T("kernel32")),"IsWow64Process");

BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;

    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            // handle error
        }
    }
    return bIsWow64;
}

// *********************************************************************
//                                      Execute command-line tasks
// *********************************************************************

// Install drivers and save token to registry
// Return exit code for main.
int MainInstallDrivers(const std::string &token, const bool force)
{
	bool bRestart = false;
	bool bRestartGlobal = false;
	bool bRet = false;

	do
	{
#ifdef DEBUG
		//MessageBox (NULL, L"1", L"1", MB_OK);
#endif

		if (force)
		{
			bRet = InstallUsbStub (bRestart);
			if (!bRet)
			{
				printf ("Error: USB Stub installation failed.\n");
				//break;
			}
			printf ("USB Stub installation succeed.\n");
			bRestartGlobal |= bRestart;

			bRet = InstallVirtualUsbHub (bRestart);
			if (!bRet)
			{
				printf ("Error: Virtual Usb Hub installation failed.\n");
				//break;
			}
			printf ("Virtual Usb Hub installation succeed.\n");
			bRestartGlobal |= bRestart;
#ifdef WIN64
			Sleep (6000);
#else
			Sleep (3000);
#endif
			//modified by sean
			bRet = InstallUsbHubFilter (bRestart, force);
			if (!bRet)
			{
				printf ("Error: USB Hub Filter installation failed.\n");
				//break;
			}
			//modified by sean

			printf ("USB Hub Filter installation succeed.\n");
			bRestartGlobal |= bRestart;
		}

		// save token
		const size_t n = AddToken (ROOT_KEY, SUB_KEY, VALUE_NAME, token);
		if (n == 0)
		{
			printf ("Add token failed.\n");
		}

		if (bRestartGlobal)
		{
			CRegKey reg;
			if (reg.Create (HKEY_LOCAL_MACHINE, _T("Software\\ELTIMA Software\\UsbToEthernetConnector")) == ERROR_SUCCESS)
			{
				reg.SetDWORDValue (_T("needrestart"), 1);
				printf ("You must restart computer.\n");
			}
		}
	}
	while (false);

	return bRet;
}

// Delete token from registry and maybe uninstall drivers (if no tokens left).
// Return exit code for main.
int MainUninstallDrivers(const std::string &token, const bool force)
{
	// remove token
	const size_t n = DeleteToken (ROOT_KEY, SUB_KEY, VALUE_NAME, token);
	if (n != 0) {
		if (!force)
		{
			printf ("Token `%s` deleted but there are more tokens left. Don't uninstall drivers\n", token.c_str());
			return 0;
		}
		else
			printf ("Forced drivers uninstall\n");
	}

	bool bRestart = false;
	bool bRet = false;

	bRet = UnInstallUsbHubFilter (bRestart);
	printf ("USB Hub Filter uninstallation succeed\n");

	bRet = UnInstallUsbStub (bRestart);
	printf ("USB Stub uninstallation succeed\n");

	bRet = UnInstallVirtualUsbHub (bRestart);
	printf ("Virtual USB Hub uninstallation succeed\n");

	return bRet;
}

// *********************************************************************
//                                      Install/Uninstall USB hub filter
// *********************************************************************
bool AttachToUsbHub (CUsbDevInfo *dev)
{
	CUsbDevInfo devs;
	std::vector <CUsbDevInfo>::iterator itemRootHub;
	bool bRestart = false;

	for (itemRootHub = dev->m_arrDevs.begin (); itemRootHub != dev->m_arrDevs.end (); itemRootHub++)
	{
		if (itemRootHub->m_bHub)
		{
			bRestart |= AttachToUsbHub (&(*itemRootHub));
		}
	}
	if (dev->m_bHub)
	{
		bRestart |= AttachToUsbHub (dev->m_strGuid);
	}
	return bRestart;
}

bool AttachToUsbHubs ()
{
	CUsbDevInfo devs;
	CUsbDevInfo::EnumerateUsbTree (devs);
	std::vector <CUsbDevInfo>::iterator itemRootHub;
	std::vector <CUsbDevInfo>::iterator item;
	bool bRestart = false;

	for (itemRootHub = devs.m_arrDevs.begin (); itemRootHub != devs.m_arrDevs.end (); itemRootHub++)
	{
		bRestart |= !AttachToUsbHub (&(*itemRootHub));
	}
	return bRestart;
}

#define IOCTL_USBHUB_ATTACH_TO_HUB			USBHUB_FILTER_IOCTL (0x12)
bool AttachToUsbHub (LPCTSTR HardwareId)
{
	HANDLE m_hDevice;
	DWORD		Returned;
	OVERLAPPED	Overlapped;
	bool		bRet = true;


	m_hDevice = CreateFile (_T("\\\\.\\UHFControl"), 0, 0, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	if (m_hDevice == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); 

	USB_DEV_SHARE usbDev;
	usbDev.DeviceAddress = 0;
	usbDev.ServerIpPort = 0;
	wcscpy_s (usbDev.HubDriverKeyName, 200, CStringW (HardwareId));

	if (!DeviceIoControl(m_hDevice,
						IOCTL_USBHUB_ATTACH_TO_HUB,
						&usbDev,
						sizeof (USB_DEV_SHARE),
						NULL,
						0,
						&Returned,
						&Overlapped)) 
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			bRet = GetOverlappedResult(m_hDevice, &Overlapped, &Returned, true)?true:false;
		}
	}

	return bRet;
}


bool InstallUsbHubFilter(bool &bRestart, const bool force)
{
    CManagerService manager;
	bool bCreated = false;

	if (manager.IsServiceCreated (Consts::szDeviceBusFilterName))
    {
		if (!force)
			return true;
		else
		{
			printf ("Update filter driver...\n");
			bCreated = true;
		}
    }

	// Copy sys file to System32\drivers folder
	CString source;
	if (!GetExeDir (&source))
	{
		_tprintf (_T("Install filter: failed to get source location\n"));
		return false;
	}
	source = JoinPath (source, CString (Consts::szDeviceBusFilterFileName));

	CString dest;
	if (!GetSystem32Dir (&dest))
	{
		_tprintf (_T("Install filter: failed to get destination location\n"));
		return false;
	}
	dest = JoinPath (dest, 
					 CString (_T("drivers\\")) + CString (Consts::szDeviceBusFilterFileName));

	if (!CopyFile (source, dest, FALSE))
	{
		_tprintf (_T("Install filter: copying failed, err:%d\n"),GetLastError());
		return false;
	}

	if (bCreated)
	{
		bRestart = true;
		printf ("Computer must be restarted\n");
		return true;
	}

	dest = CString (_T("system32\\drivers\\")) + CString (Consts::szDeviceBusFilterFileName);

    if (manager.CreateService (Consts::szDeviceBusFilterName, 
        Consts::szDeviceBusFilterDisplay,
        SERVICE_KERNEL_DRIVER, 
        SERVICE_DEMAND_START,
        dest))
    {

#ifndef CM_SERVICE_USB_DISK_BOOT_LOAD
#define CM_SERVICE_USB_DISK_BOOT_LOAD 0x4 
#endif

#ifndef CM_SERVICE_USB3_DISK_BOOT_LOAD
#define CM_SERVICE_USB3_DISK_BOOT_LOAD 0x10
#endif

		manager.SetBootFlags (Consts::szDeviceBusFilterName, CM_SERVICE_USB_DISK_BOOT_LOAD | CM_SERVICE_USB3_DISK_BOOT_LOAD);
        manager.StartService (Consts::szDeviceBusFilterName);
        GUID    ClassGUID = {0x36fc9e60L, 0xc465, 0x11cf, 0x80, 0x56, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
        CAttachFilter::AttachFilterToClass (ClassGUID, Consts::szDeviceBusFilterName, true);
		bRestart = AttachToUsbHubs ();
        if (bRestart)
        {
            printf ("Computer must be restarted\n");
        }

        return true;
    }
    return false;
}

bool UnInstallUsbHubFilter (bool &bRestart)
{
    CManagerService manager;
    bool bRet = false;

    GUID    ClassGUID = {0x36fc9e60L, 0xc465, 0x11cf, 0x80, 0x56, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
    bRet = CAttachFilter::DetachFilterToClass (ClassGUID, Consts::szDeviceBusFilterName, true);

    if (manager.IsServiceCreated (Consts::szDeviceBusFilterName))
    {
        bRet &= manager.DeleteService (Consts::szDeviceBusFilterName);
        return true;
    }
    return false;
}

bool CheckUsbHubFilterNeedUpdate ()
{
	// Compare version info of local file in setup_server.exe directory
	// with version of file from system32\drivers directory.
	// If local file newer than system file - returns true.

	CString systemFile;
	if (!GetSystem32Dir (&systemFile))
		return false;
	systemFile = JoinPath (systemFile, 
		CString (_T("drivers\\")) + CString (Consts::szDeviceBusFilterFileName));

	std::string systemFileVersion;
	if (!GetVersionInfoFromFile (systemFile, systemFileVersion))
		// can't get version - maybe the file is not here
		return true;

	CString localFile;
	if (!GetExeDir (&localFile))
		return false;
	localFile = JoinPath (localFile, CString (Consts::szDeviceBusFilterFileName));

	std::string localFileVersion;
	if (!GetVersionInfoFromFile (localFile, localFileVersion))
		return false;

	const int compare_result = CompareVersions (localFileVersion, systemFileVersion);
	return compare_result > 0;
}

// *********************************************************************
//                                      Install/Uninstall USB stub
// *********************************************************************
bool InstallUsbStub (bool &bRestart)          
{
    TCHAR*  Buf;
    CString strInf;
	bool bRet = true;
	CRegKey	key;
	CRegKey	keyClass;
	boost::scoped_array <BYTE> pByte (new BYTE [1024]); 
	LPTSTR	szFilters = (LPTSTR)pByte.get ();
	ULONG	uSizeFilter = 1024;
	std::vector <CString>	arrEnum;

	// get full path inf
    Buf = strInf.GetBufferSetLength (1024)  ;
    GetModuleFileName (NULL, Buf, 1024);
    *(_tcsrchr (Buf, _T('\\'))) = 0;
    strInf.ReleaseBuffer ();
    //strInf += _T("\\UsbStub.inf");
	strInf += _T("\\eusbstub.inf");

	ZeroMemory (pByte.get (), 1024);

	// verify install usb mon
	if (keyClass.Open (HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Class", KEY_READ) == ERROR_SUCCESS)
	{
		if (key.Open (keyClass, L"{BA60D326-EE12-4957-B702-7F5EDBE9ECE5}", KEY_ALL_ACCESS) == ERROR_SUCCESS)
		{
			int iCount = 0;
			while (RegEnumValue (key, iCount, szFilters, &uSizeFilter, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			{
				ULONG	uSizeFilter = 1024;
				arrEnum.push_back (szFilters);
				iCount++;
			}

			ZeroMemory (pByte.get (), 1024);

			if (arrEnum.size () < 2)
			{
				key.QueryMultiStringValue (_T("UPPERFILTERS"), szFilters, &uSizeFilter);
			}

			key.Close ();

			if (arrEnum.size () < 2)
			{
				keyClass.DeleteSubKey (_T("{BA60D326-EE12-4957-B702-7F5EDBE9ECE5}"));
			}
		}
	}

    if (!SetupCopyOEMInf(strInf, NULL, SPOST_NONE, 0, NULL, 0, NULL, NULL)) 
    {
		//if (!SetupCopyOEMInf(strInf, NULL, SPOST_NONE, 0, NULL, 0, NULL, NULL)) 
		{
			bRet = false;
			_tprintf (_T("Cannot copy eusbstub.inf, error : %d!\n"), GetLastError());
			//return false; // Installation Failure
		}
    }

	if (wcslen (szFilters))
	{
		if (key.Create (keyClass, L"{BA60D326-EE12-4957-B702-7F5EDBE9ECE5}", NULL, 0, KEY_ALL_ACCESS) == ERROR_SUCCESS)
		{
			key.SetMultiStringValue (_T("UPPERFILTERS"), szFilters);
			key.Close ();
		}
	}


    if (!SetupDiInstallClass(NULL, strInf, DI_QUIETINSTALL, NULL)) 
    {
		bRet = false;
        _tprintf (_T("Cannot install USB stub class, error : %d!\n"), GetLastError());
        //return false; // Installation Failure
    }

    // Create service
    SC_HANDLE hSCManager, hService;
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) 
    {
        _tprintf (_T("Cannot open local Service Manager"));
        return false; // Installation Failure
    }

	// [bialix] we don't need to manually copy the sys file to system32/drivers dir,
	// because that should be done indirectly via inf file installation (above).
    //TCHAR   szPath [500];
    //GetSystemDirectory (szPath, 500);
    //_tcscat_s (szPath, 500, _T("\\DRIVERS\\eusbstub.sys"));
    //CopyFile (_T("eusbstub.sys"), szPath, false);

    hService = OpenService(hSCManager, _T("eustub"), DELETE);
    if (hService) 
    {
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
        return true;
    }
    Sleep(300);

    hService = CreateService(
        hSCManager,
        _T("eustub"),
        _T("Usb Stub (Eltima software)"),
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        _T("System32\\DRIVERS\\eusbstub.sys"),
        _T("Extended Base"),
        NULL,
        NULL,
        NULL,
        NULL);

    if (!hService) 
    {
        CloseServiceHandle(hSCManager);
        _tprintf ((_T("Cannot create \"ELTIMA Usb Stub\" service")));
        return false; // Installation Failure
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

	return bRet;
}

bool UnInstallUsbStub (bool &bRestart)
{
	CManagerService manager;

	// Try just to delete the service
	if (manager.IsServiceCreated (_T("eustub")))
	{
		//_tprintf (_T("eustub was created, try to delete it...\n"));
		const bool bDeleted = manager.DeleteService (_T("eustub"));
		if (!bDeleted) 
		{
			//_tprintf (_T("delete service failed\n"));
		}
		return true;
	}
	return false;
}


// *********************************************************************
//                                    Install/Uninstall Virtual USB hub
// *********************************************************************
bool InstallVirtualUsbHub (bool &bRestart)
{
    HDEVINFO            DeviceInfoSet = 0;
    SP_DEVINFO_DATA DeviceInfoData;
    GUID                        ClassGUID;
    TCHAR                       ClassName[32];
    DWORD                       err = NO_ERROR;
    LPCTSTR                 HardwareId = _T("vuhub\0");

    TCHAR*  Buf;
    CString InfFileName;

    Buf = InfFileName.GetBufferSetLength (1024)     ;
    GetModuleFileName (NULL, Buf, 1024);
    *(_tcsrchr (Buf, _T('\\'))) = 0;
    InfFileName.ReleaseBuffer ();
    //InfFileName += _T("\\vuh.inf");
	InfFileName += _T("\\vuhub.inf");

    //
    // Use the INF File to extract the Class GUID. 
    //
    if (!SetupDiGetINFClass(InfFileName, &ClassGUID, ClassName, sizeof(ClassName), 0))
    {
        _tprintf(_T("GetINFClass error : %d\n"), GetLastError());
        return false;
    }

    //
    // Create the container for the to-be-created Device Information Element.
    //
    DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID, 0);
    if(DeviceInfoSet == INVALID_HANDLE_VALUE) 
    {
        _tprintf(_T("CreateDeviceInfoList"));
        return false;
    }

    // 
    // Now create the element. 
    // Use the Class GUID and Name from the INF file.
    //
    if (!FindVirtualUsbHub (HardwareId))
    {
        do 
        {
            DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            if (!SetupDiCreateDeviceInfo(DeviceInfoSet,
                ClassName,
                &ClassGUID,
                NULL,
                0,
                DICD_GENERATE_ID,
                &DeviceInfoData))
            {
                _tprintf(_T("CreateDeviceInfo"));
                err = GetLastError();
                break;
            }

            //
            // Add the HardwareID to the Device's HardwareID property.
            //
            if(!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
                &DeviceInfoData,
                SPDRP_HARDWAREID,
                (LPBYTE)HardwareId,
                (lstrlen(HardwareId) + 1 + 1) * sizeof(TCHAR))) 
            {
                _tprintf(_T("SetDeviceRegistryProperty"));
                err = GetLastError();
                break;
            }

            //
            // Transform the registry element into an actual devnode 
            // in the PnP HW tree.
            //
            if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,
                DeviceInfoSet,
                &DeviceInfoData))
            {
                _tprintf(_T("CallClassInstaller(REGISTERDEVICE)"));
                err = GetLastError();
                break;
            }
        }while (false);
    }

    //
    // The element is now registered. We must explicitly remove the 
    // device using DIF_REMOVE, if we encounter any failure from now on.
    //

    if (err == NO_ERROR)
    {
        //
        // Install the Driver.
        //
        BOOL bRes;
        if (!UpdateDriverForPlugAndPlayDevices(0,
            HardwareId,
            InfFileName,
            0x00000001,
            &bRes))
        {
            DWORD err = GetLastError();
            _tprintf(_T("UpdateDriverForPlugAndPlayDevices"));

            if (!SetupDiCallClassInstaller(
                DIF_REMOVE,
                DeviceInfoSet,
                &DeviceInfoData))
            {
                _tprintf(_T("CallClassInstaller(REMOVE)"));
            }
            SetLastError(err);
        } 
        else 
        {
            SetLastError(NO_ERROR);
        }
        bRestart = bRes?true:false;
    }
    else
    {
        err = GetLastError();
        SetupDiDestroyDeviceInfoList(DeviceInfoSet);
        SetLastError(err);
    }

    return err == NO_ERROR;
}

bool FindVirtualUsbHub (LPCTSTR HardwareId)
{
    HDEVINFO DeviceInfoSet;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD i,err;
    BOOL Found;

    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(NULL, // All Classes
        0,
        0, 
        DIGCF_ALLCLASSES | DIGCF_PRESENT ); // All devices present on system

    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        _tprintf (_T("GetClassDevs(All Present Devices)"));        
        return false;
    }

    //
    //  Enumerate through all Devices.
    //
    Found = FALSE;
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i=0;SetupDiEnumDeviceInfo(DeviceInfoSet,i,&DeviceInfoData);i++)
    {
        DWORD DataT;
        LPTSTR p,buffer = NULL;
        DWORD buffersize = 0;

        //
        // We won't know the size of the HardwareID buffer until we call
        // this function. So call it with a null to begin with, and then 
        // use the required buffer size to Alloc the nessicary space.
        // Keep calling we have success or an unknown failure.
        //
        while (!SetupDiGetDeviceRegistryProperty(
            DeviceInfoSet,
            &DeviceInfoData,
            SPDRP_HARDWAREID,
            &DataT,
            (PBYTE)buffer,
            buffersize,
            &buffersize))
        {
            if (GetLastError() == ERROR_INVALID_DATA)
            {
                //
                // May be a Legacy Device with no HardwareID. Continue.
                //
                break;
            }
            else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                //
                // We need to change the buffer size.
                //
                if (buffer) 
                {
                    LocalFree(buffer);
                }
                buffer = (LPTSTR) LocalAlloc(LPTR,buffersize);
            }
            else
            {
                //
                // Unknown Failure.
                //
                _tprintf (_T("GetDeviceRegistryProperty"));
                break;
            }            
        }

        if (GetLastError() == ERROR_INVALID_DATA) 
        {
            continue;
        }

        //
        // Compare each entry in the buffer multi-sz list with our HardwareID.
        //
        for (p=buffer; *p && (p < &buffer[buffersize]); p += lstrlen(p) + 1)
        {
            if (!_tcscmp(HardwareId,p)) {
                SetupDiDestroyDeviceInfoList(DeviceInfoSet);
                if (buffer)
                {
                    LocalFree(buffer);
                }
                return true;
            }
        }

        if (buffer)
            LocalFree(buffer);
    }

    err = GetLastError();
    if (err != NO_ERROR && err != ERROR_NO_MORE_ITEMS) 
    {
        _tprintf (_T("EnumDeviceInfo"));
    }

    //
    //  Cleanup.
    //    
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

    return false;
}

bool UnInstallVirtualUsbHub (bool &bRestart)
{
    HDEVINFO DeviceInfoSet;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD i,err;
    LPCTSTR                 HardwareId = _T("vuhub");
    bool                    bRet = false;

    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(NULL, // All Classes
        0,
        0, 
        DIGCF_ALLCLASSES | DIGCF_PRESENT ); // All devices present on system

    if (DeviceInfoSet == INVALID_HANDLE_VALUE) 
    {
        _tprintf (_T("GetClassDevs(All Present Devices)"));        
        return bRet;
    }

    //
    //  Enumerate through all Devices.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i = 0; SetupDiEnumDeviceInfo(DeviceInfoSet, i, &DeviceInfoData); i++)
    {
        DWORD DataT;
        LPTSTR p,buffer = NULL;
        DWORD buffersize = 0;

        //
        // We won't know the size of the HardwareID buffer until we call
        // this function. So call it with a null to begin with, and then 
        // use the required buffer size to Alloc the necessary space.
        // Keep calling we have success or an unknown failure.
        //
        while (!SetupDiGetDeviceRegistryProperty(
            DeviceInfoSet,
            &DeviceInfoData,
            SPDRP_HARDWAREID,
            &DataT,
            (PBYTE)buffer,
            buffersize,
            &buffersize))
        {
            if (GetLastError() == ERROR_INVALID_DATA) 
            {
                //
                // May be a Legacy Device with no HardwareID. Continue.
                //
                break;
            } 
            else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
            {
                //
                // We need to change the buffer size.
                //
                if (buffer) 
                {
                    LocalFree(buffer);
                }
                buffer = (LPTSTR)LocalAlloc(LPTR, buffersize);
            } 
            else 
            {
                //
                // Unknown Failure.
                //
                if (buffer) 
                {
                    LocalFree(buffer);
                }
                SetupDiDestroyDeviceInfoList(DeviceInfoSet);
                return bRet;
            }            
        }

        if (GetLastError() == ERROR_INVALID_DATA) 
            continue;

        //
        // Compare each entry in the buffer multi-sz list with our HardwareID.
        //
        for (p = buffer; *p && (p < &buffer[buffersize]); p += _tcslen(p) + 1)
        {
            if (!_tcsicmp(HardwareId, p))
            {
                //
                // Worker function to remove device.
                //
                if (!SetupDiCallClassInstaller(DIF_REMOVE,
                    DeviceInfoSet,
                    &DeviceInfoData))
                {
                    _tprintf (_T("CallClassInstaller(REMOVE)"));
                }
                else
                {
                    bRet = true;
                }
                break;
            }
        }
        if (buffer) 
        {
            LocalFree(buffer);
        }
    }

    if ((GetLastError() != NO_ERROR) && (GetLastError() != ERROR_NO_MORE_ITEMS))
    {
        _tprintf(_T("EnumDeviceInfo"));
    } 
    else 
    {
        SetLastError(NO_ERROR);
    }

    //
    //  Cleanup.
    //    
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

	if (bRet)
	{
		// if drivers were removed successfully then try to remove service entry from registry
		CManagerService manager;
		if (manager.IsServiceCreated (_T("vuhub")))
		{
			(void)manager.DeleteService (_T("vuhub"));
		}
	}

    return bRet;
}

PCHAR GetVendorString (USHORT     idVendor)
{
    return NULL;
}
