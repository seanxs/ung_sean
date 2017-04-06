/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbServer.cpp

Abstract:

	USB device server implementation.

Environment:

    User mode

--*/

#include "stdafx.h"
#include <atldef.h>
#include <boost/make_shared.hpp>
#include "Manager.h"
#include "public.h"
#include "Service.h"
#include "SettingsInRegistry.h"
#include "UsbServer.h"
#include "Consts\System.h"
#include "Consts\UsbDevInfo.h"
#include "Transport\Tcp.h"
#include "Utils\UsbHub.h"
#include "Utils\AdvancedUsb.h"
#include "Utils\Security.h"
#include "Utils\UserLog.h"
#include "..\Service.h"

PCHAR GetVendorString (USHORT     idVendor);

CCount	CUsbServer::m_cRestart;


TPtrUsbServer CUsbServer::Create(const CUngNetSettings &settings)
{
	return new CUsbServer (settings);
}

CUsbServer::CUsbServer (const CUngNetSettings &Settings)
	: CBaseUsb (Settings)
	, m_hWait (INVALID_HANDLE_VALUE)
	, m_devicePresent(false)
{
	ZeroMemory(&m_OverlappedWait, sizeof(m_OverlappedWait));
	m_OverlappedWait.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

bool CUsbServer::ConnectToDriver ()
{
#ifdef NO_DRIVER
	return true;
#endif

	m_hDriver = CreateFile (_T("\\\\.\\UHFControl"), 0, 0, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);

	return (m_hDriver != INVALID_HANDLE_VALUE);
}

bool CUsbServer::CloseDriver ()
{
	if (m_hDriver != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hDriver);
		return true;
	}
	if (m_OverlappedWait.hEvent != NULL)
	{
		CloseHandle(m_OverlappedWait.hEvent);
		m_OverlappedWait.hEvent = NULL;
	}
	return false;
}

void CUsbServer::FillDevName ()
{
	int iId = StrDevIdToInt (GetDeviceId ());
	CString str = CUsbDevInfo::GetNameDevById (iId);
	if (str.IsEmpty ())
	{
		bool bReset;
		str = GetDevName (0, &bReset);
	}

	if (!str.IsEmpty ())
	{
		m_strNameDev = str;
		m_mapDevName [iId] = str;
	}
	else if (DeviceIsPresent ())
	{
		TMapIntToString::const_iterator item = m_mapDevName.find (iId);
		if (iId && item != m_mapDevName.end ())
		{
			m_strNameDev = item->second;
		}
		else
		{
			m_strNameDev = Consts::szUnknowDevice;
		}
	}
	else
	{
		m_strNameDev = _T("");
	}
}

CString CUsbServer::GetHubLinkName (LPCTSTR HubName)
{
	return CUsbHub::GetUsbHubName (HubName);
}

CString CUsbServer::GetDriverKeyName (HANDLE  Hub, ULONG   ConnectionIndex)
{
	CString								ret;
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
	free(driverKeyNameW);

	return ret;
}

CString CUsbServer::GetDescriptionPort ( HANDLE hHubDevice, ULONG NumPorts, int iParam, bool *pReset )
{
	ULONG       index = NumPorts;
	BOOL        success;

	if (pReset)
	{
		*pReset = false;
	}


	PUSB_NODE_CONNECTION_INFORMATION_EX connectionInfoEx;
	CString driverKeyName;

	ULONG nBytesEx;

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
	ZeroMemory (connectionInfoEx, nBytesEx);

	if (connectionInfoEx == NULL)
	{
		return driverKeyName;
	}

	//
	// Now query USBHUB for the USB_NODE_CONNECTION_INFORMATION_EX structure
	// for this port.  This will tell us if a device is attached to this
	// port, among other things.
	//
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
			return driverKeyName;
		}

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
		if (pReset)
		{
			*pReset = true;
		}

		switch (iParam)
		{
		case 0:
			if (connectionInfoEx->DeviceDescriptor.iProduct)
			{
				driverKeyName = GetStringDescriptor (hHubDevice, index, connectionInfoEx->DeviceDescriptor.iProduct, 0x0409);
			}
			if (driverKeyName.IsEmpty () && connectionInfoEx->DeviceDescriptor.idVendor)
			{
				driverKeyName = GetVendorString(connectionInfoEx->DeviceDescriptor.idVendor);
			}
			break;

		case 1:
			driverKeyName = GetDriverKeyName (hHubDevice, index);
			break;

		case 2:
			driverKeyName.Format (_T("USB\\VID_%x&PID_%x"), connectionInfoEx->DeviceDescriptor.idVendor, connectionInfoEx->DeviceDescriptor.idProduct);
			break;
		}
	}

	delete [] (BYTE*)connectionInfoEx;
	return driverKeyName;
}

CString CUsbServer::GetStringDescriptor (
	HANDLE  hHubDevice,
	ULONG   ConnectionIndex,
	UCHAR   DescriptorIndex,
	USHORT  LanguageID
	)
{
	BOOL    success;
	ULONG   nBytes;
	ULONG   nBytesReturned;
	CString strRet;

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
		return strRet;
	}

	if (nBytesReturned < 2)
	{
		return strRet;
	}

	if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE)
	{
		return strRet;
	}

	if (stringDesc->bLength != nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST))
	{
		return strRet;
	}

	if (stringDesc->bLength % 2 != 0)
	{
		return strRet;
	}

	//
	// Looks good, allocate some (zero filled) space for the string descriptor
	// node and copy the string descriptor to it.
	//

	if (stringDesc->bLength)
	{
		return CString(stringDesc->bString);
	}

	return strRet;
}

CString CUsbServer::GetDevName ( int iParam, bool *pReset )
{
	HANDLE                  hHubDevice;
	CString					deviceName;
	CString					strRet;

	// Initialize locals to not allocated state so the error cleanup routine
	// only tries to cleanup things that were successfully allocated.
	//
	hHubDevice  = INVALID_HANDLE_VALUE;

	// Allocate a temp buffer for the full hub device name.
	//
	deviceName = GetHubLinkName ( GetHubName() );

	// Try to hub the open device
	//
	hHubDevice = CreateFile(_T("\\\\.\\") + deviceName,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hHubDevice == INVALID_HANDLE_VALUE)
	{
		return strRet;
	}

	strRet = GetDescriptionPort ( hHubDevice, (ULONG)GetUsbPort(), iParam, pReset);

	CloseHandle(hHubDevice);
	return strRet;
}

bool CUsbServer::HubDeviceRelations ()
{
	DWORD		Returned;
	bool		bRet = true;

	CAutoLock lock (&m_cs);

	USB_DEV_SHARE	usbDev;
	usbDev.DeviceAddress = GetUsbPort();
	usbDev.ServerIpPort = GetTcpPort();
	usbDev.Status = 0;
	wcscpy_s (usbDev.HubDriverKeyName, 200, GetHubName());

	if (!DeviceIoControl(m_hDriver,
		IOCTL_USBHUB_DEVICE_RELATIONS,
		&usbDev,
		sizeof (USB_DEV_SHARE),
		NULL,
		0,
		&Returned,
		&m_Overlapped)) 
	{

		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			bRet = GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true)?true:false;
		}
	}

	return bRet;
}

int CUsbServer::StrDevIdToInt (CStringW strId)
{
	int iVid = 0;
	int iPid = 0;

	strId.MakeLower ();

	swscanf_s (strId, L"usb\\vid_%x&pid_%x", &iVid, &iPid);

	return (iVid << 16) + iPid;
}

CStringW CUsbServer::GetDeviceId ()
{
#ifdef NO_DRIVER
	return L"test";
#endif
	//OVERLAPPED		Overlapped;
	DWORD			Returned;
	int				iBufferSize = 1024;
	BYTE			*Buffer = new BYTE [iBufferSize];
	CStringW		strRet;

	if (m_hDriver == INVALID_HANDLE_VALUE)
	{
		return strRet;
	}

	ZeroMemory (Buffer, iBufferSize);

	do
	{
		if (!DeviceIoControl(	m_hDriver,
								IOCTL_USBHUB_GET_DEV_ID,
								NULL,
								0,
								Buffer,
								iBufferSize,
								&Returned,
								&m_Overlapped)) 
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true);
			}
			switch (GetLastError ())
			{
			case ERROR_MORE_DATA:
				// small buffer 
				delete [] Buffer;
				iBufferSize *= 2;
				Buffer = new BYTE [iBufferSize];
				continue;
			default:
				break;
			}
		}
		strRet = (WCHAR*)Buffer;

	}while (false);

	return strRet;
}

bool CUsbServer::ShareUsbPort (bool &bManulRestart)
{
#ifdef NO_DRIVER
	m_hDriver = 0;
	return true;
#endif
	DWORD		Returned;
	bool		bRet = true;

	CAutoLock lock (&m_cs);

	USB_DEV_SHARE	usbDev;
	usbDev.DeviceAddress = GetUsbPort();
	usbDev.Status = 0;
	wcscpy_s (usbDev.HubDriverKeyName, 200, GetHubName());

	if (!DeviceIoControl(m_hDriver,
		IOCTL_USBHUB_ADD_DEV,
		&usbDev,
		sizeof (USB_DEV_SHARE),
		&usbDev,
		sizeof (USB_DEV_SHARE),
		&Returned,
		&m_Overlapped)) 
	{

		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			bRet = GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true)?true:false;
		}
	}

	bManulRestart = (usbDev.Status != 0);

	return bRet;
}

bool CUsbServer::UnshareUsbPort ()
{
	DWORD		Returned;
	bool		bRet = true;
	CString		str;

	str = GetDevName (1, NULL);

	CAutoLock lock (&m_cs);

	USB_DEV_SHARE	usbDev;
	usbDev.DeviceAddress = GetUsbPort();
	wcscpy_s (usbDev.HubDriverKeyName, 200, GetHubName());

	if (!DeviceIoControl(m_hDriver,
		IOCTL_USBHUB_REMOVE_DEV,
		&usbDev,
		sizeof (USB_DEV_SHARE),
		NULL,
		0,
		&Returned,
		&m_Overlapped)) 
	{

		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			bRet = GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true)?true:false;
		}
	}

	return bRet;
}

bool CUsbServer::CycleUsbPort ()
{
	DWORD		Returned;
	bool		bRet = true;

	CAutoLock lock (&m_cs);

	USB_DEV_SHARE	usbDev;
	usbDev.DeviceAddress = GetUsbPort();
	usbDev.ServerIpPort = GetTcpPort();
	usbDev.Status = 0;
	wcscpy_s (usbDev.HubDriverKeyName, 200, GetHubName());

	if (!DeviceIoControl(m_hDriver,
		IOCTL_USBHUB_DEVICE_RESTART,
		&usbDev,
		sizeof (USB_DEV_SHARE),
		NULL,
		0,
		&Returned,
		&m_Overlapped)) 
	{

		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			bRet = GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true)?true:false;
		}
	}

	return bRet;
}

CString CUsbServer::GetInstanceId ()
{
	DWORD		Returned;
	bool		bRet = true;
	CString		strRet;

	CAutoLock lock (&m_cs);

	USB_DEV_SHARE	usbDev;
	usbDev.DeviceAddress = GetUsbPort();
	wcscpy_s (usbDev.HubDriverKeyName, 200, GetHubName());

	if (!DeviceIoControl(m_hDriver,
		IOCTL_USBHUB_DEVICE_INSTANCE_ID,
		&usbDev,
		sizeof (USB_DEV_SHARE),
		&usbDev,
		sizeof (USB_DEV_SHARE),
		&Returned,
		&m_Overlapped)) 
	{

		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			bRet = GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true)?true:false;
		}
	}

	if (bRet)
	{
		strRet = usbDev.HubDriverKeyName;
	}

	{
		strRet.MakeLower ();
		int iPos = strRet.Find (_T("\\enum\\"));
		if (iPos != -1)
		{
			strRet = strRet.Mid (iPos + (int)_tcslen (_T("\\enum\\")));
		}
		strRet.Replace (_T("\\device parameters"), _T(""));
	}

	return strRet;
}

bool CUsbServer::RestartDevice ()
{
	CString str;
	bool bRes = false;

	{
		for (int a = 0; a < 3; a++)
		{
			CAutoLock aLock (&m_cs);

			if (!CycleUsbPort ())
			{
				// manual disable/enable device
				str = GetDevName (1, &bRes);
				if (!str.IsEmpty ())
				{
					if (CAdvancedUsb::RestartHub (str, NULL, this))
					{
						return true;
					}
				}
				else
				{
					str = GetInstanceId ();
					if (!str.IsEmpty ())
					{
						if (CAdvancedUsb::RestartOnInstanceId (str, NULL, this))
						{
							return true;
						}
					}
				}

				if (str.CompareNoCase (_T("device parameters")) != 0)
				{
					Sleep (100);
					break;
				}
			}
			else
			{
				return true;
			}
		}
	}

	return bRes;
}

bool CUsbServer::Connect ()
{
	bool	bManulRestart;

	do 
	{
		if (GetStatus() != Empty || m_hDriver != INVALID_HANDLE_VALUE)
		{
			break;
		}

		if (!ConnectToDriver())
		{
			break;
		}

		if (!CheckVersionDriver (m_hDriver, IOCTL_USBHUB_VERSION))
		{
			return false;
		}

		FillDevName ();

		if (!ShareUsbPort (bManulRestart))
		{
			Stop ();
			break;
		}

#ifndef NO_DRIVER
		if (!InitPoolList(m_hDriver, IOCTL_USBHUB_IRP_TO_3_RING_SET, &m_poolRead))
		{
			Disconnect ();
			return false;
		}
		if (!InitPoolList(m_hDriver, IOCTL_USBHUB_IRP_TO_0_RING_SET, &m_poolWrite))
		{
			Disconnect ();
			return false;
		}

		m_poolRead.Stop();
#endif

		SetStatus(Created);
		StartWaitDevice ();

		return true;
	} while (false);

	//exit_label:
	return false;
}

bool CUsbServer::Disconnect()
{
	CAutoLock lock (&m_cs);
	const bool isActive = IsConnectionActive ();
	const bool wasConnected = GetStatus () == CBaseUsb::Connected;
	SetStatus(Deleting);
	m_shptrDeviceTimer.reset();

	if (isActive)
	{
			Transport::TPtrTransport pTr = GetTrasport();
			if (pTr)
			{
				if (wasConnected)
					eltima::ung::UserLog::Server::Disconnected (this);
				pTr->ClearEvent();
				SetTrasport(NULL);
				pTr->Disconnect();
			}
	}
	Stop ();
	if (UnshareUsbPort())
	{
		RestartDevice();
	}
	return true;
}

bool CUsbServer::DeviceIsPresent ()
{
	if (IsConnectionActive ())
	{
			DWORD		Returned;
			bool		bRet = true;

			if (!DeviceIoControl(m_hDriver,
				IOCTL_USBHUB_DEVICE_PRESENT,
				NULL,
				0,
				NULL,
				0,
				&Returned,
				&m_Overlapped)) 
			{

				if (GetLastError() != ERROR_IO_PENDING)
				{
					bRet = false;
				}
				else 
				{
					bRet = GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true)?true:false;
				}
			}

			return bRet;
	}
	return false;
}

bool CUsbServer::WaitPluginDevice ()
{
#ifdef NO_DRIVER
	return true;
#endif

	DWORD		Returned;
	bool		bRet = true;

	ATLTRACE ("WaitDevice\n");

	if (!DeviceIoControl(m_hDriver,
		IOCTL_USBHUB_WAIT_INSERT_DEV,
		NULL,
		0,
		NULL,
		0,
		&Returned,
		&m_OverlappedWait))
	{

		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			ATLTRACE ("pending WaitDevice %d\n", bRet);
			bRet = GetOverlappedResult(m_hDriver, &m_OverlappedWait, &Returned, true) ? true : false;
		}
	}
	ATLTRACE ("exit WaitDevice %d\n", bRet);

	return bRet;
}

bool CUsbServer::InitUsbDesc ()
{
	DWORD		Returned;
	bool		bRet = true;

	CAutoLock lock (&m_cs);

	if (!DeviceIoControl(m_hDriver,
		IOCTL_USBHUB_INIT_USB_DESC,
		NULL,
		0,
		NULL,
		0,
		&Returned,
		&m_Overlapped)) 
	{

		if (GetLastError() != ERROR_IO_PENDING)
		{
			bRet = false;
		}
		else 
		{
			bRet = GetOverlappedResult(m_hDriver, &m_Overlapped, &Returned, true)?true:false;
		}
	}

	return bRet;
}

void CUsbServer::StartWaitDevice ()
{
	m_hWait = CreateThread (NULL, 0, stWaitDevice, this, 0, NULL);
}

DWORD WINAPI CUsbServer::stWaitDevice (LPVOID lpParam)
{
	TPtrUsbServer	pBase ((CUsbServer*)lpParam);

	CloseHandle (pBase->m_hWait);
	pBase->m_hWait = INVALID_HANDLE_VALUE;

	pBase->m_cs.Lock();
	pBase->SetStatus(Connecting);
	pBase->m_cs.UnLock();

	pBase->WaitDevice ();

	return 0;
}

void CUsbServer::WaitDevice ()
{
	CStringW			strId;
	bool				bRet = true;
	
	if (m_shptrDeviceTimer)
		m_shptrDeviceTimer->Cancel ();

	if (GetStatus() == Connecting)
	{
		ClearStatistics ();

		// waiting for the new device to be detected on USB hub
		if (DeviceIsPresent ())
		{
			RestartDevice ();
			Sleep (100);
			FillDevName ();	// XXX do we really need this call here, it's also called later again
		}
		else
		{
			// device is not present - erase cached data
			m_devicePresent = false;
			m_strNameDev.Empty ();
		}

		while (!WaitPluginDevice ())
		{
			if (GetStatus() != Connecting)
			{
				return;
			}
			Sleep (500);
		}

		m_poolRead.Resume();
		InitUsbDesc ();

		// Getting device name
		for (int a = 0; a < 10; a++)
		{
			strId = GetDeviceId ();
			if (strId.GetLength ())
			{
				break;
			}
			Sleep (50);
		}

		FillDevName ();

		m_devicePresent = true;

		eltima::ung::UserLog::Server::Waiting (this);
		
		// check status
		{
			CAutoLock lock (&m_cs);
			if (GetStatus() != Connecting)
			{
				return;
			}

			Transport::TPtrTcp conn = Transport::TPtrTcp (new Transport::CTcp (m_settings));
			SetTrasport(conn);
			if (conn->Connect (this))
			{
				// let's track if device still present in the USB port
				if (!m_shptrDeviceTimer) m_shptrDeviceTimer = eltima::utils::ITimeout::newTimeout ();
				m_shptrDeviceTimer->StartPeriodic (3*1000/*ms*/,
					[this]() {this->CheckDevicePresence();}
					);
			}
			else
			{
				// error to initiate connection
				eltima::ung::UserLog::Server::NotStarted (this);
			}
		}
	}
}

bool CUsbServer::InitSecurityConnection (Transport::TPtrTransport pTr)
{
	if (pTr->AuthSupport() || pTr->CryptSupport())	
	{
		SetStatus (CBaseUsb::Handshake);

		USHORT		uHandshake = 0;
		CString  str;

		if (m_security.GetIsAuth ())
		{
			uHandshake |= Transport::HANDSHAKE_AUTH;
		}
		if (m_security.GetIsCrypt ())
		{
			uHandshake |= Transport::HANDSHAKE_ENCR;
		}
		if (IsCompressionEnabled ())
		{
			switch (GetCompressionType ())
			{
			case Consts::UCT_COMPRESSION_LZ4:
				uHandshake |= Transport::HANDSHAKE_COMPR_LZ4;
				break;

			case Consts::UCT_COMPRESSION_ZLIB:
			default:
				uHandshake |= Transport::HANDSHAKE_COMPR_ZLIB;
				break;
			}
		}
		if (pTr->Write ((char*)&uHandshake, 2) == 2)
		{
			if (IsAuthEnable ())
			{
				if (!Auth (pTr))
				{
					eltima::ung::UserLog::Server::ConnectAuthError (this, pTr);
					return false;
				}
			}
			if (IsCryptEnable ())
			{
				m_security.ClearCrypt ();
				m_security.SetCrypt ();
				if (!Crypt (pTr))
				{
					eltima::ung::UserLog::Server::ConnectCryptError (this, pTr);
					return false;
				}
			}
			return true;
		}
		else
		{
			return true;
		}
	}

	return true;
}

void CUsbServer::SetDescription (CString str)
{
	m_settings.DeleteParam (Consts::szUngNetSettings [Consts::UNS_DESK]);
	m_settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], str);
	if (m_State == Connected)
		SendDeviceId ();
}

bool CUsbServer::EnableCompression (const CUngNetSettings &settings)
{
	AptrCompression Compress;
	if (settings.ExistParam(Consts::szUngNetSettings[Consts::UNS_COMPRESS]))
	{
		int iComprType = settings.GetParamInt(Consts::szUngNetSettings[Consts::UNS_COMPRESS_TYPE],
			Consts::UCT_COMPRESSION_DEFAULT);

		Compress = AptrCompression (ICompression::Create (Consts::UngCompressionType (iComprType)));
	}
	else
	{
		Compress = AptrCompression (ICompression::Create (Consts::UCT_COMPRESSION_NONE));
	}

	if (Compress->GetCompressionType() != m_aptrCompressor->GetCompressionType())
	{
		m_aptrCompressor = Compress;

		m_settings.DeleteParam(Consts::szUngNetSettings [Consts::UNS_COMPRESS]);
		m_settings.DeleteParam(Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE]);
		if (m_aptrCompressor->GetCompressionType() != Consts::UCT_COMPRESSION_NONE)
		{
			m_settings.AddParamStr(Consts::szUngNetSettings [Consts::UNS_COMPRESS], NULL);
			m_settings.AddParamInt(Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE], m_aptrCompressor->GetCompressionType());
		}
		return true;
	}

	return false;
}

bool CUsbServer::SetCrypt (const CUngNetSettings &settings)
{
	if (IsCryptEnable() != settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_CRYPT]))
	{
		m_settings.DeleteParam(Consts::szUngNetSettings [Consts::UNS_CRYPT]);
		if (settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_CRYPT]))
		{
			m_security.SetCrypt();
			m_settings.AddParamStr(Consts::szUngNetSettings [Consts::UNS_CRYPT], NULL);
		}
		else
		{
			m_security.ClearCrypt();
		}
		return true;
	}

	return false;
}

bool CUsbServer::SetAuth (const CUngNetSettings &settings)
{
	const CStringA oldPass = m_settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH]);
	const CStringA newPass = settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH]);

	if (oldPass == newPass)
		return true;

	m_settings.DeleteParam (Consts::szUngNetSettings [Consts::UNS_AUTH]);
	if (!newPass.IsEmpty ())
	{
		m_settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH], newPass);
		if (!m_security.SetAuth (CSystem::Decrypt (newPass), false))
		{
			eltima::ung::UserLog::Server::AuthInitError (this);
			return false;
		}
	}
	else
	{
		m_security.ClearAuth ();
	}
	return true;
}

//  **********************************************************************
//					Event
void CUsbServer::OnConnected (Transport::TPtrTransport pConnection)
{
	CAutoLock lock (&m_cs);
	if (InitSecurityConnection (pConnection))
	{
		SetStatus(Connected);
		// send info about device
		SendDeviceId ();
		// begin read from device
		StartReadDataFromDevice ();

		eltima::ung::UserLog::Server::Connected (this);
	}
	else
	{
		pConnection->Disconnect();
		OnError(ERROR_INVALID_SECURITY_DESCR);
	}
}

void CUsbServer::OnError (int iError)
{
	TPtrUsbServer Server = this;
	CAutoLock lock (&m_cs);

	switch (GetStatus())
	{
	case CBaseUsb::Created:
		ATLASSERT (false);
		break;

	case CBaseUsb::Handshake:
	case CBaseUsb::Connected:
	case CBaseUsb::Reconnecting:
		{
			const bool wasConnected = GetStatus () == CBaseUsb::Connected;
			if (wasConnected)
				eltima::ung::UserLog::Server::Disconnected (this);

			SetStatus(Created);
			Transport::TPtrTransport pTr;
			pTr = GetTrasport ();
			SetTrasport(NULL);
			if (pTr)
			{
				pTr->Disconnect();
			}
			Stop ();

			SetStatus(Connecting);
			lock.UnAttach();
			StartWaitDevice ();
		}
		break;

	case CBaseUsb::Deleting:
	case CBaseUsb::Connecting:
		// delete connection
		break;
	}
}

void CUsbServer::OnRead (LPVOID lpData, int iSize)
{
	try
	{
		CBaseUsb::OnRead(lpData, iSize);
	}
	catch (Transport::CException &e)
	{
		OnError(e.GetErrorInt());
	}
	catch (...)
	{
		OnError(0);
	}
}

// *********************************************************
//		Read
void CUsbServer::ReadDataFromDevice ()
{
	CBaseUsb::ReadDataFromDevice();
}

// Stop all thread
bool CUsbServer::Stop ()
{
	// stop thread read to socket
	m_poolRead.Stop ();

	if (IsCryptEnable ())
	{
		m_security.ClearCrypt();
		m_security.SetCrypt();
	}

	WaitStoppedReadDataFromDevice ();
	return true;
}

bool CUsbServer::ReadyToReadData (const Transport::TCP_PACKAGE *pPacket, int iSize)
{
	(void)iSize;

	if (GetType (pPacket) == Transport::IrpToTcp)
	{
		return true;
	}
	return false;
}

void CUsbServer::SendDeviceId ()
{
	Transport::TCP_PACKAGE		*pTcpPackage;
	Transport::TPtrTransport		pTransport = GetTrasport();

	if (m_strNameDev.GetLength () && pTransport)
	{
		CStringA str;
		CBuffer buffer;

		buffer.SetReserv(sizeof (Transport::TCP_PACKAGE));
		
		
		const bool isRdp = pTransport->IsRdpConnected ();
		// try to build full device data to send, using data saved in manager
		CManager* pManager = CService::GetManager ();	
		if (!pManager || !pManager->BuildStringShareEx (this, buffer, isRdp) )
		{
			// fallback to old way
			CManager::BuildStringShare (this, buffer, isRdp);
		}
		
		buffer.Add (0);
		str = CStringA ((char*)buffer.GetData ());

		pTcpPackage = (Transport::TCP_PACKAGE*)buffer.GetFullData();
		ZeroMemory (pTcpPackage, sizeof (Transport::TCP_PACKAGE));
		pTcpPackage->SizeBuffer = (str.GetLength () + 1) * sizeof (CHAR);
		pTcpPackage->Type = Transport::DeviceId;

		pTransport->Write (pTcpPackage, pTcpPackage->SizeBuffer + sizeof (Transport::TCP_PACKAGE));
	}
}

void CUsbServer::SetAllowRemoteDiscon(bool bAllow)
{
	if (bAllow)
	{
		m_settings.AddParamStr(Consts::szUngNetSettings[Consts::UNS_DEV_ALLOW_RDISCON], NULL);
	}
	else
	{
		m_settings.DeleteParam(Consts::szUngNetSettings[Consts::UNS_DEV_ALLOW_RDISCON]);
	}
}

void CUsbServer::SaveSettingsInRegistry () const
{
	CServerSettingsInRegistry regset (GetVaribleName ());
	regset.SaveSettings (m_settings);
}

void CUsbServer::LogCompressionError (Transport::TPtrTransport pTransport) const
{
	(void)pTransport;
	eltima::ung::UserLog::Server::CompressionError ((CUsbServer*)this);
}

void CUsbServer::CheckDevicePresence()
{
	if (!m_shptrDeviceTimer)
		return;

	if (GetStatus() == CBaseUsb::Connecting) 
	{
		const bool newDevicePresent = DeviceIsPresent ();
		if (newDevicePresent != m_devicePresent)
		{
			m_devicePresent = newDevicePresent;
			if (!newDevicePresent)
				eltima::ung::UserLog::Server::DeviceMissing (this);
		}
		if (!newDevicePresent)
		{
			// device is missing: let's break connection
			SetStatus (CBaseUsb::Reconnecting);
			OnError (0);
		}
	}
}

CString CUsbServer::GetUsbLoc () const
{
	do
	{
		if (!m_strUsbLoc.IsEmpty ()) break;

		// get from manager
		const CService *pService = CService::GetService ();
		if (!pService) break;
		const CManager *pManager = CService::GetManager ();
		if (!pManager) break;

		m_strUsbLoc = pManager->GetUsbLoc (m_settings);
	} 
	while(0);

	return m_strUsbLoc;
}
