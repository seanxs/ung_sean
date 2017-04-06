/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

	UsbDevInfo.cpp

Abstract:

	Class that builds USB devices tree 

Environment:

	Kernel mode

--*/

#include "StdAfx.h"
#include <setupapi.h>
#include <boost\scoped_array.hpp>
#include "consts.h"
#include "Cfgmgr32.h"
#include "System.h"
#include "UsbDevInfo.h"


#define UNDEFINED_TCP_PORT (0)

PCHAR GetVendorString (USHORT     idVendor);
std::map <ULONG, CString> CUsbDevInfo::m_mapName;

CUsbDevInfo::CUsbDevInfo(void)
{
	Init ();
}

CUsbDevInfo::~CUsbDevInfo(void)
{
}

CUsbDevInfo::CUsbDevInfo(bool bHub, LPCTSTR szName, LPCTSTR szGiud, bool bConnected)
{
	Init ();

	m_bHub = bHub;
	m_strName = szName;
	m_strGuid = szGiud;
	m_bConnected = bConnected;
}

void CUsbDevInfo::Init ()
{
	m_bConnected = false;
	m_bHub = false;
	m_iPort = 0;

	m_bShare = false;
	m_bAuth = false;
	m_bCrypt = false;
	m_bCompress = false;
	m_bAllowRDisconn = false;
	m_iCompressType = 0;

	// net settings
	m_iTcpPort = UNDEFINED_TCP_PORT;

	// usb descriptor data
	m_bBaseClass	= 0;
	m_bSubClass		= 0;
	m_bProtocol		= 0;
	m_bcdUSB		= 0;
	m_idVendor		= 0;
	m_idProduct		= 0;
	m_bcdDevice		= 0;

	// connection status (valid for shared dev only)
	m_iStatus = 0;
	m_bRdp = false;
}

void CUsbDevInfo::EnumerateUsbTree (CUsbDevInfo& devInfo)
{
	devInfo.Clear ();
	CUsbDevInfo::EnumerateHostControllers (devInfo);
}

void CUsbDevInfo::EnumerateHostControllers (CUsbDevInfo& devInfo)
{
	TCHAR       HCName[16];
	int         HCNum;
	HANDLE      hHCDev;
	LPCTSTR     leafName;

	CString str;
	int     iHubIndex = 1;
	CString strUsbLoc;		// USB port location as N:M:X:Y:Z

	// Now iterate over host controllers using the new GUID based interface
	// find setupapi
	HDEVINFO                         deviceInfo;
	SP_DEVICE_INTERFACE_DATA         deviceInfoData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetailData;
	ULONG                            index;
	ULONG                            requiredLength;
	GUID    InterfaceGUID = {0x3abf6f2d, 0x71c4, 0x462a, 0x8a, 0x92, 0x1e, 0x68, 0x61, 0xe6, 0xaf, 0x27};

	deviceInfo = SetupDiGetClassDevs((LPGUID)&InterfaceGUID,
		NULL,
		NULL,
		(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

	deviceInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	for (index=0;
		 SetupDiEnumDeviceInterfaces(deviceInfo,0,(LPGUID)&InterfaceGUID,index,&deviceInfoData);
		 ++index)
	{
		SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&deviceInfoData,
			NULL,
			0,
			&requiredLength,
			NULL);

		deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)GlobalAlloc(GPTR, requiredLength);

		deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&deviceInfoData,
			deviceDetailData,
			requiredLength,
			&requiredLength,
			NULL);

		hHCDev = CreateFile(deviceDetailData->DevicePath,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		// If the handle is valid, then we've successfully opened a Host
		// Controller.  Display some info about the Host Controller itself,
		// then enumerate the Root Hub attached to the Host Controller.
		//
		if (hHCDev != INVALID_HANDLE_VALUE)
		{
			leafName = deviceDetailData->DevicePath;

			str.Format (_T("RootHub#%d"), iHubIndex);
			strUsbLoc.Format (_T("%d"), iHubIndex);

			if (CUsbDevInfo::EnumerateHostController (devInfo, hHCDev, str, strUsbLoc))
			{
				++iHubIndex;
			}

			CloseHandle(hHCDev);
		}

		GlobalFree(deviceDetailData);
	}

	SetupDiDestroyDeviceInfoList(deviceInfo);

	for (HCNum = 0; HCNum < 10; HCNum++)
	{
		wsprintf(HCName, _T("\\\\.\\HCD%d"), HCNum);

		hHCDev = CreateFile(HCName,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		// If the handle is valid, then we've successfully opened a Host
		// Controller.  Display some info about the Host Controller itself,
		// then enumerate the Root Hub attached to the Host Controller.
		//
		if (hHCDev != INVALID_HANDLE_VALUE)
		{
			leafName = HCName + sizeof(_T("\\\\.\\")) - sizeof(_T(""));

			str.Format (_T("RootHub#%d"), iHubIndex);
			strUsbLoc.Format (_T("%d"), iHubIndex);

			if (CUsbDevInfo::EnumerateHostController (devInfo, hHCDev, str, strUsbLoc))
			{
				++iHubIndex;
			}

			CloseHandle(hHCDev);
		}
	}
}

bool CUsbDevInfo::EnumerateHostController (CUsbDevInfo& devInfo, HANDLE hHCDev, LPCTSTR szName, LPCTSTR szUsbLoc)
{
	CString strHubName, strHubId, strHcdDevice;

	strHubName = GetRootHubName(hHCDev);
	strHcdDevice  = GetRootHubName(hHCDev, IOCTL_GET_HCD_DRIVERKEY_NAME);
	if (strHubName.GetLength ())
	{
	    if (strHubName.GetLength () && (strHubName.CollateNoCase (_T("VHubStub")) != 0))
		{
			strHubId = GetRootHubDriverKeyName (strHubName, strHcdDevice);
			if (strHubId.GetLength () && strHubName.GetLength ())
			{
				if (!devInfo.IsPresent (strHubId))
				{
					CUsbDevInfo::EnumerateHub (devInfo, strHubName, strHubId, szName, szUsbLoc);
					return true;
				}
			}
		}
	}

	return false;
}

CString CUsbDevInfo::GetRootHubName ( HANDLE HostController, DWORD dwIoControlCode )
{
	BOOL                success;
	ULONG               nBytes;
	USB_ROOT_HUB_NAME   rootHubName;
	PUSB_ROOT_HUB_NAME  rootHubNameW;
	CString             ret;


	// Get the length of the name of the Root Hub attached to the
	// Host Controller
	//
	success = DeviceIoControl(HostController,
		dwIoControlCode,
		0,
		0,
		&rootHubName,
		sizeof(rootHubName),
		&nBytes,
		NULL);

	if (!success)
	{
		return ret;
	}

	// Allocate space to hold the Root Hub name
	//
	rootHubNameW = (PUSB_ROOT_HUB_NAME)new BYTE [rootHubName.ActualLength];

	if (rootHubNameW == NULL)
	{
		return ret;
	}

	// Get the name of the Root Hub attached to the Host Controller
	//
	success = DeviceIoControl(HostController,
		dwIoControlCode,
		NULL,
		0,
		rootHubNameW,
		rootHubName.ActualLength,
		&nBytes,
		NULL);

	if (success)
	{
		ret = rootHubNameW->RootHubName;
	}

	delete [] rootHubNameW;

	return ret;
}

CString CUsbDevInfo::GetRootHubDriverKeyName ( LPCTSTR HubName, LPCTSTR szHcdDevice )
{
	CString             strHubName = HubName;
	CString         strTemp;
	CString         strKey = _T("SYSTEM\\CurrentControlSet\\Enum\\");;
	int                     iPos = 0;
	HKEY            hReg;
	DWORD           dwType, dwSize = 0;


	do
	{
		strTemp = strHubName.Tokenize (_T("#"), iPos);
		if (iPos == -1)
		{
			break;
		}
		strKey += strTemp + _T("\\");
		strTemp = strHubName.Tokenize (_T("#"), iPos);
		if (iPos == -1)
		{
			break;
		}
		strKey += strTemp + _T("\\");
		strTemp = strHubName.Tokenize (_T("#"), iPos);
		if (iPos == -1)
		{
			break;
		}
		strKey += strTemp;

		strTemp = _T("");

		if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, strKey, 0, KEY_READ, &hReg) == ERROR_SUCCESS)
		{
			RegQueryValueEx (hReg, _T("Driver"), 0, &dwType, NULL, &dwSize);
			RegQueryValueEx (hReg, _T("Driver"), 0, &dwType, (LPBYTE)strTemp.GetBufferSetLength (dwSize), &dwSize);
			RegCloseKey (hReg);
		}
	}while (false);

	if (strTemp.IsEmpty () && szHcdDevice)
	{
	    TCHAR		pBuff [250];

		HDEVINFO                         deviceInfo;
		SP_DEVICE_INTERFACE_DATA         deviceInfoData;
		PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetailData;
		SP_DEVINFO_DATA					 InfoData;

		DWORD dwType;
		DWORD dwSize;

		ULONG                            index;
		ULONG                            requiredLength;
        GUID hubInterface = {0xf18a0e88, 0xc30c, 0x11d0, 0x88, 0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8};


		CString str;
		int             iHubIndex = 1;

		deviceInfo = SetupDiGetClassDevs((LPGUID)&hubInterface,
			NULL,
			NULL,
			(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

		deviceInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		for (index=0;
			SetupDiEnumDeviceInterfaces(deviceInfo,
			0,
			(LPGUID)&hubInterface,
			index,
			&deviceInfoData);
				index++)
		{
			SetupDiGetDeviceInterfaceDetail(deviceInfo,
				&deviceInfoData,
				NULL,
				0,
				&requiredLength,
				NULL);

			deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)GlobalAlloc(GPTR, requiredLength);

			deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			ZeroMemory (&InfoData, sizeof (InfoData));
			InfoData.cbSize  = sizeof(SP_DEVINFO_DATA);
			SetupDiGetDeviceInterfaceDetail(deviceInfo,
				&deviceInfoData,
				deviceDetailData,
				requiredLength,
				&requiredLength,
				&InfoData);

			dwSize = sizeof (pBuff);
			if (!SetupDiGetDeviceRegistryProperty (  deviceInfo,
													&InfoData,
													SPDRP_DRIVER, 
													&dwType, 
													(BYTE*)pBuff, 
													dwSize,
													&dwSize ))
			{
				GlobalFree(deviceDetailData);
				continue;
			}

			CString strHdc = DriverNameToParam (pBuff, ParentDevice);
			if (strHdc.CompareNoCase (szHcdDevice) == 0)
			{
				strTemp = pBuff;
				GlobalFree(deviceDetailData);
				break;
			}
			GlobalFree(deviceDetailData);
		}

		SetupDiDestroyDeviceInfoList(deviceInfo);
	}

	return strTemp;
}

void CUsbDevInfo::EnumerateHub (CUsbDevInfo& devInfo, LPCTSTR HubName, LPCTSTR HubId, LPCTSTR Description, LPCTSTR UsbLoc)
{
	USB_NODE_INFORMATION    hubInfo;
	HANDLE                  hHubDevice;
	CString                 deviceName = _T("\\\\.\\");
	BOOL                    success;
	ULONG                   nBytes;

	// add node
	devInfo.m_arrDevs.push_back ( CUsbDevInfo (true, Description, HubId) );
	devInfo.m_arrDevs.back ().m_bBaseClass = 0x09;
	devInfo.m_arrDevs.back ().m_bSubClass = 0;
	devInfo.m_arrDevs.back ().m_bProtocol = 0;
	devInfo.m_arrDevs.back ().m_strUsbLoc = UsbLoc;

	// Initialize locals to not allocated state so the error cleanup routine
	// only tries to cleanup things that were successfully allocated.
	//
	hHubDevice  = INVALID_HANDLE_VALUE;

	// Allocate a temp buffer for the full hub device name.
	//
	deviceName += HubName;

	// Try to hub the open device
	//
	hHubDevice = CreateFile(deviceName,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hHubDevice == INVALID_HANDLE_VALUE)
	{
		return;
	}

	//
	// Now query USBHUB for the USB_NODE_INFORMATION structure for this hub.
	// This will tell us the number of downstream ports to enumerate, among
	// other things.
	//
	ZeroMemory (&hubInfo, sizeof(USB_NODE_INFORMATION));
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_NODE_INFORMATION,
		&hubInfo,
		sizeof(USB_NODE_INFORMATION),
		&hubInfo,
		sizeof(USB_NODE_INFORMATION),
		&nBytes,
		NULL);

	if (!success)
	{
		CloseHandle (hHubDevice);
		return;
	}

	// Now add an item to the TreeView with the PUSBDEVICEINFO pointer info
	// as the LPARAM reference value containing everything we know about the
	// hub.
	//
	//
	devInfo.m_arrDevs.back ().m_strHubDev = HubName;
	CUsbDevInfo::EnumerateHubPorts (
		devInfo.m_arrDevs.back (), 
		hHubDevice, 
		hubInfo.u.HubInformation.HubDescriptor.bNumberOfPorts, 
		HubId,
		UsbLoc);

	CloseHandle(hHubDevice);
	return;
}

ULONG CUsbDevInfo::BuildIdFromDescription (PUSB_DEVICE_DESCRIPTOR pDesc)
{
	return (pDesc->idVendor << 16) + pDesc->idProduct;
	//ULONG lRet = 0;
	//ULONG *pBuff = (ULONG *)&pDesc->bcdUSB;

	//for (int a = 0; a < 4; a++)
	//{
	//	lRet |= *pBuff;
	//	++pBuff;
	//	lRet = (lRet << 2);
	//}

	//return lRet;
}

void CUsbDevInfo::EnumerateHubPorts (CUsbDevInfo& devInfo, HANDLE hHubDevice, ULONG NumPorts, LPCTSTR HubId, LPCTSTR UsbLoc)
{
	ULONG       index;
	BOOL        success;
	bool        bPresent;

	PUSB_NODE_CONNECTION_INFORMATION_EX connectionInfoEx;
	CString driverKeyName;
	CString strUsbLoc;			// USB port location as N:M:X:Y:Z

	// Loop over all ports of the hub.
	//
	// Port indices are 1 based, not 0 based.
	//
	for (index=1; index <= NumPorts; index++)
	{
		bPresent = false;
		driverKeyName = _T("");
		ULONG nBytesEx;
		strUsbLoc.Format (_T("%s:%d"), UsbLoc, index);

		// Allocate space to hold the connection info for this port.
		// For now, allocate it big enough to hold info for 30 pipes.
		//
		// Endpoint numbers are 0-15.  Endpoint number 0 is the standard
		// control endpoint which is not explicitly listed in the Configuration
		// Descriptor.  There can be an IN endpoint and an OUT endpoint at
		// endpoint numbers 1-15 so there can be a maximum of 30 endpoints
		// per device configuration.
		//
		// Should probably size this dynamically at some point.
		//
		nBytesEx = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) +
			sizeof(USB_PIPE_INFO) * 30;

		connectionInfoEx = (PUSB_NODE_CONNECTION_INFORMATION_EX)new BYTE[nBytesEx];

		if (connectionInfoEx == NULL)
		{
			break;
		}

		//
		// Now query USBHUB for the USB_NODE_CONNECTION_INFORMATION_EX structure
		// for this port.  This will tell us if a device is attached to this
		// port, among other things.
		//
		ZeroMemory (connectionInfoEx, nBytesEx);
		connectionInfoEx->ConnectionIndex = index;

		success = DeviceIoControl(hHubDevice,
			IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
			connectionInfoEx,
			nBytesEx,
			connectionInfoEx,
			nBytesEx,
			&nBytesEx,
			NULL);

		if (!success)
		{
			PUSB_NODE_CONNECTION_INFORMATION    connectionInfo;
			ULONG                               nBytes;

			// Try using IOCTL_USB_GET_NODE_CONNECTION_INFORMATION
			// instead of IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX
			//
			nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION) +
				sizeof(USB_PIPE_INFO) * 30;

			connectionInfo = (PUSB_NODE_CONNECTION_INFORMATION)new BYTE [nBytes];
			ZeroMemory (connectionInfo, nBytes);
			connectionInfo->ConnectionIndex = index;

			success = DeviceIoControl(hHubDevice,
				IOCTL_USB_GET_NODE_CONNECTION_INFORMATION,
				connectionInfo,
				nBytes,
				connectionInfo,
				nBytes,
				&nBytes,
				NULL);

			if (!success)
			{
				delete [] (BYTE*)connectionInfo;
				delete [] (BYTE*)connectionInfoEx;
				continue;
			}

			// Copy IOCTL_USB_GET_NODE_CONNECTION_INFORMATION into
			// IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX structure.
			//
			connectionInfoEx->DeviceDescriptor = connectionInfo->DeviceDescriptor;

			connectionInfoEx->ConnectionIndex =
				connectionInfo->ConnectionIndex;

			connectionInfoEx->DeviceDescriptor =
				connectionInfo->DeviceDescriptor;

			connectionInfoEx->DeviceIsHub =
				connectionInfo->DeviceIsHub;

			connectionInfoEx->DeviceAddress =
				connectionInfo->DeviceAddress;

			connectionInfoEx->ConnectionStatus =
				connectionInfo->ConnectionStatus;

			delete [] connectionInfo;
		}

		// If there is a device connected, get the Device Description
		//
		if (connectionInfoEx->ConnectionStatus != NoDeviceConnected)
		{
			bPresent = true;
			driverKeyName = GetDriverKeyName (hHubDevice, index);
		}

		// If the device connected to the port is an external hub, get the
		// name of the external hub and recursively enumerate it.
		//
		CString deviceDesc;

		if (bPresent)
		{
			const ULONG lId = BuildIdFromDescription (&connectionInfoEx->DeviceDescriptor);
			auto item = m_mapName.find (lId);
			if (item == m_mapName.end ())
			{
				if (connectionInfoEx->DeviceDescriptor.iProduct)
				{
					deviceDesc = GetStringDescriptor (hHubDevice, index, connectionInfoEx->DeviceDescriptor.iProduct, 0x0409);
					if (!deviceDesc.IsEmpty ())
					{
						m_mapName [lId] = deviceDesc;
					}
				}
			}
			else
			{
				deviceDesc = item->second;
			}

			if (deviceDesc.IsEmpty () && connectionInfoEx->DeviceDescriptor.idVendor)
			{
				deviceDesc = GetVendorString(connectionInfoEx->DeviceDescriptor.idVendor);
			}
		}

		if (driverKeyName.IsEmpty ())
		{
			if (bPresent)
			{
				if (deviceDesc.IsEmpty ())
				{
					deviceDesc = _T("Unknown device");
				}
			}
			else
			{
				deviceDesc.Format (_T("Port%d: Free"), index);
			}
		}
		else
		{
			if (deviceDesc.IsEmpty ())
			{
				deviceDesc = DriverNameToParam (driverKeyName, DeviceDesc);
				if (deviceDesc.Find (_T("Eltima")) != -1)
				{
					deviceDesc = _T("");
				}
			}
		}

		if (!connectionInfoEx->DeviceIsHub)
		{
			// plain device / empty port
			devInfo.m_arrDevs.push_back ( CUsbDevInfo (false, deviceDesc, driverKeyName, bPresent) );
			devInfo.m_arrDevs.back ().m_strHubDev = HubId;
			devInfo.m_arrDevs.back ().m_iPort = index;
			devInfo.m_arrDevs.back ().m_strUsbLoc = strUsbLoc;

			if (bPresent)
			{ 
				InitDevClass (hHubDevice, connectionInfoEx);
				devInfo.m_arrDevs.back ().StoreDeviceDescriptorData (hHubDevice, connectionInfoEx);
			}
		}
		else // if (!connectionInfoEx->DeviceIsHub)
		{
			// USB hub connected
			const CString extHubName = GetExternalHubName(hHubDevice, index);
			if (!extHubName.IsEmpty ())
			{
				CUsbDevInfo::EnumerateHub (devInfo, extHubName, driverKeyName, deviceDesc, strUsbLoc);
			}
		}
		delete [] (BYTE*)connectionInfoEx;
	}
}

CString CUsbDevInfo::GetDriverKeyName (HANDLE  Hub, ULONG   ConnectionIndex)
{
	CString                             ret;
	BOOL                                success;
	ULONG                               nBytes;
	USB_NODE_CONNECTION_DRIVERKEY_NAME  driverKeyName;
	PUSB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyNameW;

	driverKeyNameW = NULL;

	// Get the length of the name of the driver key of the device attached to
	// the specified port.
	//
	driverKeyName.ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		&driverKeyName,
		sizeof(driverKeyName),
		&driverKeyName,
		sizeof(driverKeyName),
		&nBytes,
		NULL);

	if (!success)
	{
		return ret;
	}

	// Allocate space to hold the driver key name
	//
	nBytes = driverKeyName.ActualLength;

	if (nBytes <= sizeof(driverKeyName))
	{
		return ret;    
	}

	driverKeyNameW = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)malloc(nBytes);

	if (driverKeyNameW == NULL)
	{
		return ret; 
	}

	// Get the name of the driver key of the device attached to
	// the specified port.
	//
	driverKeyNameW->ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		driverKeyNameW,
		nBytes,
		driverKeyNameW,
		nBytes,
		&nBytes,
		NULL);

	if (success)
	{
		ret = driverKeyNameW->DriverKeyName;
	}

	// Convert the driver key name
	//

	// All done, free the uncoverted driver key name and return the
	// converted driver key name
	//
	free(driverKeyNameW);

	return ret;
}

CString CUsbDevInfo::GetStringDescriptor (
	HANDLE  hHubDevice,
	ULONG   ConnectionIndex,
	UCHAR   DescriptorIndex,
	USHORT  LanguageID
	)
{
	BOOL    success;
	ULONG   nBytes;
	ULONG   nBytesReturned;

	UCHAR   stringDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
		MAXIMUM_USB_STRING_LENGTH];

	PUSB_DESCRIPTOR_REQUEST stringDescReq;
	PUSB_STRING_DESCRIPTOR  stringDesc;

	nBytes = sizeof(stringDescReqBuf);

	stringDescReq = (PUSB_DESCRIPTOR_REQUEST)stringDescReqBuf;
	stringDesc = (PUSB_STRING_DESCRIPTOR)(stringDescReq+1);

	// Zero fill the entire request structure
	//
	memset(stringDescReq, 0, nBytes);

	// Indicate the port from which the descriptor will be requested
	//
	stringDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	stringDescReq->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8)
		| DescriptorIndex;

	stringDescReq->SetupPacket.wIndex = LanguageID;

	stringDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		stringDescReq,
		nBytes,
		stringDescReq,
		nBytes,
		&nBytesReturned,
		NULL);

	//
	// Do some sanity checks on the return from the get descriptor request.
	//

	if (!success)
	{
		return CString ();
	}

	if (nBytesReturned < 2)
	{
		return CString ();
	}

	if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE)
	{
		return CString ();
	}

	if (stringDesc->bLength != nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST))
	{
		return CString ();
	}

	if (stringDesc->bLength % 2 != 0)
	{
		return CString ();
	}

	//
	// Looks good, allocate some (zero filled) space for the string descriptor
	// node and copy the string descriptor to it.
	//

	if (stringDesc->bLength)
	{
		return CString(stringDesc->bString);
	}

	return CString ();
}

CString CUsbDevInfo::GetExternalHubName (HANDLE  Hub,ULONG   ConnectionIndex)
{
	CString                                         ret;
	BOOL                        success;
	ULONG                       nBytes;
	USB_NODE_CONNECTION_NAME    extHubName;
	PUSB_NODE_CONNECTION_NAME   extHubNameW;
	PCHAR                       extHubNameA;

	extHubNameW = NULL;
	extHubNameA = NULL;

	// Get the length of the name of the external hub attached to the
	// specified port.
	//
	extHubName.ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		&extHubName,
		sizeof(extHubName),
		&extHubName,
		sizeof(extHubName),
		&nBytes,
		NULL);

	if (!success)
	{
		return ret;
	}

	// Allocate space to hold the external hub name
	//
	nBytes = extHubName.ActualLength;

	if (nBytes <= sizeof(extHubName))
	{
		return ret;
	}

	extHubNameW = (PUSB_NODE_CONNECTION_NAME)malloc(nBytes);

	if (extHubNameW == NULL)
	{
		return ret;
	}

	// Get the name of the external hub attached to the specified port
	//
	extHubNameW->ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		extHubNameW,
		nBytes,
		extHubNameW,
		nBytes,
		&nBytes,
		NULL);

	if (success)
	{
		ret = extHubNameW->NodeName;
	}

	free(extHubNameW);

	return ret;
}

CString CUsbDevInfo::DriverNameToParam (LPCTSTR DriverName, ToParam param)
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
		return CString ();
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

			switch (param)
			{
			case DeviceId:
				cr = CM_Get_Device_ID(devInst,
					buf,
					len,
					0);
				break;
			case DeviceDesc:
				cr = CM_Get_DevNode_Registry_Property(devInst,
					CM_DRP_DEVICEDESC,
					NULL,
					buf,
					&len,
					0);
				break;
			case DeviceClass:
				cr = CM_Get_DevNode_Registry_Property(devInst,
					CM_DRP_CLASS,
					NULL,
					buf,
					&len,
					0);
				break;
			case ParentDevice:
				{
					DEVINST     devInstParent;
					cr = CM_Get_Parent (&devInstParent, devInst, 0);
					if (cr == CR_SUCCESS)
					{
						len = sizeof(buf);
						cr = CM_Get_DevNode_Registry_Property(devInstParent,
							CM_DRP_DRIVER,
							NULL,
							buf,
							&len,
							0);
					}
					break;
				}
			case DeviceDisable:
				{
					break;
				}
			}

			if (cr == CR_SUCCESS)
			{
				return buf;
			}
			else
			{
				return CString ();
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
	return CString ();
}

const CUsbDevInfo* CUsbDevInfo::FindDevice (LPCTSTR szHub, int iPort) const
{
	for (auto item = m_arrDevs.begin (); item != m_arrDevs.end (); item++)
	{
		if ((item->m_iPort == iPort) && (_tcsicmp (szHub, m_strGuid) == 0))
		{
			return &(*item);
		}
		if (item->m_bHub)
		{
			const CUsbDevInfo *pDev = item->FindDevice (szHub, iPort);
			if (pDev)
			{
				return pDev;
			}
		}
	}
	return NULL;
}

bool CUsbDevInfo::IsPresent (LPCTSTR szGuid)
{
	std::vector <CUsbDevInfo>::iterator item;

	for (item = m_arrDevs.begin (); item != m_arrDevs.end (); item++)
	{
		if (item->m_strGuid == szGuid)
		{
			return true;
		}
	}

	return false;
}

const CUsbDevInfo* CUsbDevInfo::FindHub (LPCTSTR szHub) const
{
	std::vector <CUsbDevInfo>::const_iterator item;
	CString                             strTemp;
	const CUsbDevInfo                         *pDev;

	for (item = m_arrDevs.begin (); item != m_arrDevs.end (); item++)
	{
		if (item->m_bHub)
		{
			//AtlTrace (_T("%s\n"), item->m_strGuid);
			if (item->m_strGuid.CompareNoCase (szHub) == 0)
			{
				return &*item;
			}
			pDev = item->FindHub (szHub);
			if (pDev)
			{
				return pDev;
			}
		}
	}
	return NULL;
}

void CUsbDevInfo::Clear ()
{
	m_arrDevs.clear ();
	m_mapDevs.clear ();
}

bool CUsbDevInfo::GetConfigDescriptor (HANDLE  hHubDevice, ULONG ConnectionIndex, UCHAR   DescriptorIndex, PUSB_DESCRIPTOR_REQUEST pDescriptor, ULONG &iSize)
{
    BOOL    success;
    ULONG   nBytes;
    
    PUSB_DESCRIPTOR_REQUEST         configDescReq;
    PUSB_CONFIGURATION_DESCRIPTOR   configDesc;

    // Request the Configuration Descriptor the first time using our
    // local buffer, which is just big enough for the Cofiguration
    // Descriptor itself.
    //
    nBytes = iSize;

    configDescReq = (PUSB_DESCRIPTOR_REQUEST)pDescriptor;
    configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(configDescReq+1);

    // Zero fill the entire request structure
    //
    memset(configDescReq, 0, nBytes);

    // Indicate the port from which the descriptor will be requested
    //
    configDescReq->ConnectionIndex = ConnectionIndex;
    configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
                                        | DescriptorIndex;
    configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));


    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must inititialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
                                        | DescriptorIndex;

    configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              configDescReq,
                              nBytes,
                              configDescReq,
                              nBytes,
							  &iSize,
                              NULL);

    if (!success)
    {
        return false;
    }

    if (configDesc->wTotalLength != (iSize - sizeof(USB_DESCRIPTOR_REQUEST)))
    {
        return false;
    }

    //return configDescReq;
	return true;
}

bool CUsbDevInfo::InitDevClass (HANDLE  hHubDevice, PUSB_NODE_CONNECTION_INFORMATION_EX pConnInfo)
{
	if (	pConnInfo->DeviceDescriptor.bDeviceClass 
		||	pConnInfo->DeviceDescriptor.bDeviceSubClass
		||	pConnInfo->DeviceDescriptor.bDeviceProtocol)
	{
		return true;
	}

	ULONG uSize = 1024;
	boost::scoped_array <BYTE>			pBuff (new BYTE [1024]);
	PUSB_DESCRIPTOR_REQUEST				pDescriptor = (PUSB_DESCRIPTOR_REQUEST)pBuff.get ();
	PUSB_CONFIGURATION_DESCRIPTOR		configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(pDescriptor+1);

	if (GetConfigDescriptor (hHubDevice, pConnInfo->ConnectionIndex, 0, pDescriptor, uSize))
	{
		if (configDesc->bNumInterfaces > 0)
		{
			PUSB_COMMON_DESCRIPTOR  commonDesc;
		    PUCHAR                  descEnd;

			descEnd = (PUCHAR)configDesc + configDesc->wTotalLength;
			commonDesc = (PUSB_COMMON_DESCRIPTOR)configDesc;

			while ((PUCHAR)commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd &&
				   (PUCHAR)commonDesc + commonDesc->bLength <= descEnd)
			{
				switch (commonDesc->bDescriptorType)
				{
					case USB_INTERFACE_DESCRIPTOR_TYPE:
						if (commonDesc->bLength < sizeof(USB_INTERFACE_DESCRIPTOR))
						{
							break;
						}
						//if (((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface)
						{
							PUSB_INTERFACE_DESCRIPTOR pInterface = (PUSB_INTERFACE_DESCRIPTOR)commonDesc;

							pConnInfo->DeviceDescriptor.bDeviceClass = pInterface->bInterfaceClass; 
							pConnInfo->DeviceDescriptor.bDeviceSubClass = pInterface->bInterfaceSubClass;
							pConnInfo->DeviceDescriptor.bDeviceProtocol = pInterface->bInterfaceProtocol;

							return true;
						}
						//commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc += commonDesc->bLength);
						//continue;

					default:
						commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
						continue;
				}
				break;
			}
		}
	}

	return false;
}

void CUsbDevInfo::StoreDeviceDescriptorData (HANDLE  hHubDevice, PUSB_NODE_CONNECTION_INFORMATION_EX pConnInfo)
{
	if (pConnInfo->ConnectionStatus == NoDeviceConnected)
		return;
	
	this->m_bBaseClass	= pConnInfo->DeviceDescriptor.bDeviceClass;
	this->m_bSubClass	= pConnInfo->DeviceDescriptor.bDeviceSubClass;
	this->m_bProtocol	= pConnInfo->DeviceDescriptor.bDeviceProtocol;

	this->m_bcdUSB		= pConnInfo->DeviceDescriptor.bcdUSB;

	this->m_idVendor	= pConnInfo->DeviceDescriptor.idVendor;
	this->m_idProduct	= pConnInfo->DeviceDescriptor.idProduct;
	this->m_bcdDevice	= pConnInfo->DeviceDescriptor.bcdDevice;

	CString strUniqueId;

	strUniqueId.Format (L"USB\\VID_%04x&PID_%04x&REV_%04x", 
	                    this->m_idVendor, this->m_idProduct, this->m_bcdDevice);

	if (pConnInfo->DeviceDescriptor.iSerialNumber)
	{
		CString strSerial = GetStringDescriptor (hHubDevice, pConnInfo->ConnectionIndex, pConnInfo->DeviceDescriptor.iSerialNumber, 0x0409);
		if (!strSerial.IsEmpty ())
		{
			this->m_strSerialNumber = strSerial;
			strUniqueId.AppendFormat (L"\\%s", strSerial);
		}
	}

	this->m_strDevUnicId = strUniqueId;
}

CString CUsbDevInfo::GetNetSettings () const
{
	if (m_iTcpPort <= UNDEFINED_TCP_PORT)
	{
		// no valid net settings
		return _T("");
	}

	if (m_strServer.IsEmpty ())
	{
		// only tcp port
		CString strNet;
		strNet.Format (_T("%d"), m_iTcpPort);
		return strNet;
	}
	else
	{
		// host:port
		CString strNet;
		strNet.Format (_T("%s:%d"), m_strServer, m_iTcpPort);
		return strNet;
	}
}

void CUsbDevInfo::SetNetSettings (const CString &strNetSettings)
{
	CString strServer;
	int iTcpPort;

	CSystem::ParseNetSettings (strNetSettings, strServer, iTcpPort);
	SetNetSettings (strServer, iTcpPort);
}

void CUsbDevInfo::SetNetSettings (const CString &strServer, int iTcpPort)
{
	if (iTcpPort <= 0)
	{
		iTcpPort = UNDEFINED_TCP_PORT;
	}

	m_strServer = strServer;
	m_iTcpPort = iTcpPort;
}

int CUsbDevInfo::GetCompressType () const
{
	if (m_bCompress)
	{
		return (m_iCompressType > 0) ? m_iCompressType : Consts::UCT_COMPRESSION_DEFAULT;
	}
	else
	{
		return Consts::UCT_COMPRESSION_NONE;
	}
}

CStringA CUsbDevInfo::UsbClassAsHexStrA () const
{
	return CUsbDevInfo::UsbClassAsHexStrA (m_bBaseClass, m_bSubClass, m_bProtocol);
}

/*static*/
CStringA CUsbDevInfo::UsbClassAsHexStrA (BYTE uBaseClass, BYTE uSubClass, BYTE uProtocol)
{
	CStringA strA;
	strA.Format ("%02X%02X%02X", (int)uBaseClass, (int)uSubClass, (int)uProtocol);
	return strA;
}

/*static*/
bool CUsbDevInfo::UsbClassFromHexStrA (const CStringA &strUsbClassHex, BYTE &uBaseClass, BYTE &uSubClass, BYTE &uProtocol)
{
	enum {HEX_LEN = 6 /*3 bytes * 2 characters*/};

	if (strUsbClassHex.GetLength () != HEX_LEN)
	{
		return false;
	}
	else
	{
		ULONG value = 0;
		for (int i=0; i<HEX_LEN; ++i)
		{
			value *= 0x10;
			const char x = strUsbClassHex [i];
			if (x >= '0' && x <= '9')
			{
				value += x - '0';
			}
			else if (x >= 'a' && x <= 'f')
			{
				value += x - 'a' + 10;
			}
			else if (x >= 'A' && x <= 'F')
			{
				value += x - 'A' + 10;
			}
			else
			{
				// wrong character
				return false;
			}
		}
		// set corresponding variables
		uProtocol = value & 0x0FF; value /= 256;
		uSubClass = value & 0x0FF; value /= 256;
		uBaseClass = value & 0x0FF;
		return true;
	}
}
