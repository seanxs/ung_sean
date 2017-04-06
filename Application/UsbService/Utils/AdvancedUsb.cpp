/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    AdvancedUsb.cpp

Abstract:

	Class allowing to restart USB devices 

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "advancedusb.h"
#include "..\Usb\UsbServer.h"
#include <Cfgmgr32.h>
#include "..\Manager.h"


static DWORD dwVer = MAKEWORD (HIBYTE (LOWORD(GetVersion ())), LOBYTE (LOWORD(GetVersion ())));

CAdvancedUsb::CAdvancedUsb(void)
{
}

CAdvancedUsb::~CAdvancedUsb(void)
{
}

bool CAdvancedUsb::ControlDevices (HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo,  DWORD dwControl, bool *pbRestart)
{
    SP_PROPCHANGE_PARAMS pcp;
    SP_DEVINSTALL_PARAMS devParams;

	if (pbRestart)
	{
		*pbRestart = false;
	}

    switch (dwControl) 
	{
        case DICS_ENABLE:
		case DICS_START:
		case DICS_PROPCHANGE:
            //
            // enable both on global and config-specific profile
            // do global first and see if that succeeded in enabling the device
            // (global enable doesn't mark reboot required if device is still
            // disabled on current config whereas vice-versa isn't true)
            //
            pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            pcp.StateChange = dwControl;
            pcp.Scope = DICS_FLAG_GLOBAL;
            pcp.HwProfile = 0;
            //
            // don't worry if this fails, we'll get an error when we try config-
            // specific.
            if(SetupDiSetClassInstallParams(Devs, DevInfo, &pcp.ClassInstallHeader, sizeof(pcp))) 
			{
               SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, Devs, DevInfo);
            }
            //
            // now enable on config-specific
            //
            pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            pcp.StateChange = dwControl;
            pcp.Scope = DICS_FLAG_CONFIGSPECIFIC;
            pcp.HwProfile = 0;
            break;

        default:
            //
            // operate on config-specific profile
            //
            pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            pcp.StateChange = dwControl;
            pcp.Scope = DICS_FLAG_CONFIGSPECIFIC;
            pcp.HwProfile = 0;
            break;

    }

    if(!SetupDiSetClassInstallParams(Devs,DevInfo,&pcp.ClassInstallHeader,sizeof(pcp)) ||
       !SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,Devs,DevInfo)) 
	{
		return false;
    } 
	else 
	{
        //
        // see if device needs reboot
        //
        devParams.cbSize = sizeof(devParams);
        if(SetupDiGetDeviceInstallParams(Devs,DevInfo,&devParams) && (devParams.Flags & (DI_NEEDRESTART|DI_NEEDREBOOT))) 
		{
			if (pbRestart)
			{
				*pbRestart = TRUE;
			}
        } 
    }
    return true;

}

bool CAdvancedUsb::RestartHub (LPCTSTR szDriverName, GUID *Guid, CUsbServer *pServer)
{
	bool	bRestart = false;
	bool	bRet;
	bRet = RestartDevice (Guid, szDriverName, &bRestart, pServer);
	if (!bRet && (Guid != NULL))
	{
		bRet = RestartDevice (NULL, szDriverName, &bRestart, pServer);
	}
    return (bRet && !bRestart);
}

bool CAdvancedUsb::ControlOnInstanceId (LPCTSTR szInstanceId, DWORD dwControl, CUsbServer *pServer, bool *pbRestart)
{
	int		index = 0;

	HDEVINFO hDevInfo;
	hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	bool bRestart;
	bool bRet = false;

	if ( INVALID_HANDLE_VALUE != hDevInfo)
	{
		SP_DEVINFO_DATA  devInfoElem;
		DWORD			dwNeed = 0;
		DWORD			dwSize = 0;
		TCHAR			szService [200];
		devInfoElem.cbSize = sizeof(SP_DEVINFO_DATA);

		while ( SetupDiEnumDeviceInfo(hDevInfo, index++, &devInfoElem))
		{
			dwSize = 200;
			if (!SetupDiGetDeviceInstanceId (hDevInfo, 
										&devInfoElem, 
                                        szService,
										dwSize, 
										&dwNeed))
			{
				continue;
			}


			if (_tcsicmp (szService, szInstanceId) == 0)
			{
				bRet = true;
				if (dwControl & ADU_PROPCHANGE)
				{
					bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_PROPCHANGE, &bRestart);
				}

				if ((dwControl & ADU_DISABLE) || (dwControl & ADU_RESTART))
				{
					bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_STOP, &bRestart);
					if (pServer)
					{
						pServer->HubDeviceRelations ();
					}
					Sleep (500);

				}
				if (bRet && ((dwControl & ADU_ENABLE) || (dwControl & ADU_RESTART)))
				{
					bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_START, &bRestart);
				}

				if (pbRestart != NULL && bRestart)
				{
					*pbRestart = bRestart;
				}
				//bRet = true;
				break;
			}

		}

		int iError = GetLastError ();
		SetupDiDestroyDeviceInfoList(hDevInfo);
		SetLastError (iError);
	}

	return bRet;

}

bool CAdvancedUsb::RestartOnInstanceId (LPCTSTR szInstanceId, bool *pbRestart, CUsbServer *pServer)
{
//#ifdef DEBUG
//	CManager::AddLogFile (szInstanceId);
//#endif
	if (dwVer < 0x0602) // windows 7 and other
	{
		if (ControlOnInstanceId (szInstanceId, ADU_RESTART, pServer ,pbRestart))
		{
			return true;
		}
		else if (ControlOnInstanceId (szInstanceId, ADU_PROPCHANGE, pServer ,pbRestart))
		{
			return true;
		}

	}
	// window 8
	else if (ControlOnInstanceId (szInstanceId, ADU_DISABLE, pServer ,pbRestart))
	{
		CString str;
		if (pServer)
		{
			str = pServer->GetInstanceId ();
			if (str.GetLength ())
			{
				#ifdef DEBUG
				CManager::AddLogFile (str);
				#endif
				szInstanceId = str;
			}
		}
		if (ControlOnInstanceId (szInstanceId, ADU_ENABLE, pServer, pbRestart))
		{
			return true;
		}
	}
	else if (ControlOnInstanceId (szInstanceId, ADU_PROPCHANGE, pServer ,pbRestart))
	{
		return true;
	}

	if (GetLastError () == ERROR_NO_SUCH_DEVINST)
	{
		return true;
	}
	return false;
}

bool CAdvancedUsb::RestartDevice(GUID *Guid, LPCTSTR szDriverName, bool *pbRestart, CUsbServer *pServer)
{
	if (dwVer < 0x0602) // windows 7 and other
	{
		if (ControlDevicesIsDriverName (Guid, szDriverName, ADU_RESTART, pServer, pbRestart))
		{
			return true;
		}
		else if (ControlDevicesIsDriverName (Guid, szDriverName, ADU_PROPCHANGE, pServer, pbRestart))
		{
			return true;
		}
	}
	else if (ControlDevicesIsDriverName (Guid, szDriverName, ADU_DISABLE, pServer, pbRestart))
	{
		if (ControlDevicesIsDriverName (Guid, szDriverName, ADU_ENABLE, pServer, pbRestart))
		{
			return true;
		}
		else
		{
			CString str;
			if (pServer)
			{
				bool bRes;
				str = pServer->GetDevName (1, &bRes);
				if (str.GetLength ())
				{
					szDriverName = str;
				}

				if (ControlDevicesIsDriverName (Guid, szDriverName, ADU_ENABLE, pServer, pbRestart))
				{
					return true;
				}
			}
		}
	}
	else if (ControlDevicesIsDriverName (Guid, szDriverName, ADU_PROPCHANGE, pServer, pbRestart))
	{
		return true;
	}

	if (GetLastError () == ERROR_NO_SUCH_DEVINST)
	{
		return true;
	}
	return false;
}

bool CAdvancedUsb::ControlDevicesIsDriverName (GUID *Guid, LPCTSTR szDriverName, DWORD dwControl, CUsbServer *pServer, bool *pbRestart)
{
	int		index = 0;
	bool bRet = true;

	if ((dwControl & ADU_DISABLE) || (dwControl & ADU_RESTART))
	{
		bRet = EnableDevice (szDriverName, false);

		if (pServer)
		{
			pServer->HubDeviceRelations ();
		}
		Sleep (500);

	}
	if (bRet && ((dwControl & ADU_ENABLE) || (dwControl & ADU_RESTART)))
	{
		bRet = EnableDevice (szDriverName, true);
	}

	HDEVINFO hDevInfo;
	hDevInfo = SetupDiGetClassDevs(Guid, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
//	bool bRet = false;

	if ( INVALID_HANDLE_VALUE != hDevInfo)
	{
		SP_DEVINFO_DATA  devInfoElem;
		DWORD			dwType;
		DWORD			dwNeed;
		DWORD			dwSize = 0;
		TCHAR			szService [200];
		devInfoElem.cbSize = sizeof(SP_DEVINFO_DATA);

		while ( SetupDiEnumDeviceInfo(hDevInfo, index++, &devInfoElem))
		{
			dwSize = 200;
			if (!SetupDiGetDeviceRegistryProperty (hDevInfo, 
										&devInfoElem, 
										SPDRP_DRIVER,
										&dwType,
                                        (LPBYTE)szService,
										dwSize, 
										&dwNeed))
			{
				continue;
			}


			if (_tcsicmp (szService, szDriverName) == 0)
			{
				ATLTRACE2 (_T("usbhub\r\n"));
				bool bRestart;
				bRet = true;
				if (dwControl & ADU_PROPCHANGE)
				{
					bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_PROPCHANGE, &bRestart);
				}
				if ((dwControl & ADU_DISABLE) || (dwControl & ADU_RESTART))
				{
					//bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_DISABLE, &bRestart);
					bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_STOP, &bRestart);

					if (pServer)
					{
						pServer->HubDeviceRelations ();
					}
					Sleep (500);

				}
				if (bRet && ((dwControl & ADU_ENABLE) || (dwControl & ADU_RESTART)))
				{
					//bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_ENABLE, &bRestart);
					bRet = ControlDevices (hDevInfo, &devInfoElem, DICS_START, &bRestart);
				}

				if (pbRestart != NULL && bRestart)
				{
					*pbRestart = bRestart;
				}
				break;
				//return bRet;
			}

		}

		int iError = GetLastError ();
		SetupDiDestroyDeviceInfoList(hDevInfo);
		SetLastError (iError);
	}

	return bRet;
}


bool CAdvancedUsb::EnableDevice (LPCTSTR DriverName, bool bEnable)
{
	DEVINST     devInst;
	DEVINST     devInstNext;
	CONFIGRET   cr;
	ULONG       walkDone = 0;
	ULONG       len;
	TCHAR           buf[MAX_DEVICE_ID_LEN];  // (Dynamically size this instead?)
	// Get Root DevNode
	//
	cr = CM_Locate_DevNode(&devInst,
		NULL,
		0);

	if (cr != CR_SUCCESS)
	{
		return false;
	}

	// Do a depth first search for the DevNode with a matching
	// DriverName value
	//
	while (!walkDone)
	{
		// Get the DriverName value
		//
		len = sizeof(buf);
		cr = CM_Get_DevNode_Registry_Property(devInst,
			CM_DRP_DRIVER,
			NULL,
			buf,
			&len,
			0);

		// If the DriverName value matches, return the DeviceDescription
		//
		if (cr == CR_SUCCESS && _tcsicmp(DriverName, buf) == 0)
		{
			len = sizeof(buf);

			if (bEnable)
			{
				cr = CM_Enable_DevNode (devInst, 0);
			}
			else
			{
				cr = CM_Disable_DevNode (devInst, CM_DISABLE_UI_NOT_OK);
			}

			if (cr == CR_SUCCESS)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		// This DevNode didn't match, go down a level to the first child.
		//
		cr = CM_Get_Child(&devInstNext,
			devInst,
			0);

		if (cr == CR_SUCCESS)
		{
			devInst = devInstNext;
			continue;
		}

		// Can't go down any further, go across to the next sibling.  If
		// there are no more siblings, go back up until there is a sibling.
		// If we can't go up any further, we're back at the root and we're
		// done.
		//
		for (;;)
		{
			cr = CM_Get_Sibling(&devInstNext,
				devInst,
				0);

			if (cr == CR_SUCCESS)
			{
				devInst = devInstNext;
				break;
			}

			cr = CM_Get_Parent(&devInstNext,
				devInst,
				0);


			if (cr == CR_SUCCESS)
			{
				devInst = devInstNext;
			}
			else
			{
				walkDone = 1;
				break;
			}
		}
	}
	return false;
}
