/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ControlDeviceExtension.cpp

Abstract:

  Device class that third ring interacts with. 
     - intercept virtual USB device
     - release virtual USB device
     - data transfer between real and virtual devices

Environment:

    Kernel mode

--*/
#include "ControlDeviceExtension.h"
#include "UsbPortToTcp.h"
#include "BusFilterExtension.h"
#include "Common\common.h"

ULONG CControlDeviceExtension::m_DriverType = 47901;

CControlDeviceExtension::CControlDeviceExtension () 
	: CBaseDeviceExtension ()
{
	SetDebugName ("CControlDeviceExtension");
	RtlInitUnicodeString(&m_NameString, L"\\Device\\UHFControl"); 
	RtlInitUnicodeString(&m_LinkString, L"\\DosDevices\\UHFControl"); 
}

// ******************************************************************
//					virtual major functions
// ******************************************************************
NTSTATUS CControlDeviceExtension::DispatchCreate (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint(DebugTrace, "DispatchCreate");
	++m_IrpCounter;
	++m_IrpCounter;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_IrpCounter;
	return STATUS_SUCCESS;
}

NTSTATUS CControlDeviceExtension::DispatchClose (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint(DebugTrace, "DispatchClose");
	++m_IrpCounter;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_IrpCounter;
	--m_IrpCounter;
	return STATUS_SUCCESS;
}

CHAR* DevCtrlToString (ULONG uCommand)
{
	switch (uCommand) 
	{
	case IOCTL_USBHUB_ADD_DEV:
		return "IOCTL_USBHUB_ADD_DEV";
	case IOCTL_USBHUB_REMOVE_DEV:
		return "IOCTL_USBHUB_REMOVE_DEV";
	case IOCTL_USBHUB_WAIT_INSERT_DEV:
		return "IOCTL_USBHUB_WAIT_INSERT_DEV";
	case IOCTL_USBHUB_IRP_TO_0_RING:
		return "IOCTL_USBHUB_IRP_TO_0_RING";
	case IOCTL_USBHUB_IRP_TO_3_RING:
		return "IOCTL_USBHUB_IRP_TO_3_RING";
	case IOCTL_USBHUB_DEVICE_PRESENT:
		return "IOCTL_USBHUB_DEVICE_PRESENT";
	case IOCTL_USBHUB_CANCEL:
		return "IOCTL_USBHUB_CANCEL";
	//case IOCTL_USBHUB_DEL_CONTROL:
	//	return "IOCTL_USBHUB_DEL_CONTROL";
	case IOCTL_USBHUB_GET_DEV_ID:
		return "IOCTL_USBHUB_GET_DEV_ID";
	case IOCTL_USBHUB_INIT_USB_DESC:
		return "IOCTL_USBHUB_INIT_USB_DESC";
	case IOCTL_USBHUB_ATTACH_TO_HUB:
		return "IOCTL_USBHUB_ATTACH_TO_HUB";
	case IOCTL_USBHUB_DEVICE_RELATIONS:
		return "IOCTL_USBHUB_DEVICE_RELATIONS";
	case IOCTL_USBHUB_DEVICE_INSTANCE_ID:
		return "IOCTL_USBHUB_DEVICE_INSTANCE_ID";
	default:
		break;
	}

	return "unknown";
}

NTSTATUS CControlDeviceExtension::DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION  irpStack;
	NTSTATUS            status = STATUS_INVALID_PARAMETER;

	++m_IrpCounter;
	irpStack = IoGetCurrentIrpStackLocation (Irp);

	int iSizeSave = sizeof (IRP_SAVE);

	DebugPrint(DebugTrace, "DispatchDevCtrl %s, %#x", DevCtrlToString(irpStack->Parameters.DeviceIoControl.IoControlCode), irpStack->Parameters.DeviceIoControl.IoControlCode);

	switch (irpStack->Parameters.DeviceIoControl.IoControlCode) 
	{
	case IOCTL_USBHUB_ADD_DEV:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
            status = AddUsbPortToTcp ((PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer, Irp);
			if (irpStack->Parameters.DeviceIoControl.OutputBufferLength == sizeof (USB_DEV_SHARE))
			{
				Irp->IoStatus.Information = sizeof (USB_DEV_SHARE);
			}
		}
		break;
	case IOCTL_USBHUB_REMOVE_DEV:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
            status = RemoveUsbPortToTcp ((PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer, Irp);
		}
		break;
	case IOCTL_USBHUB_WAIT_INSERT_DEV:
        status = SetWaitIrp (Irp);
		break;
	case IOCTL_USBHUB_IRP_TO_0_RING: // deprecated
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof (IRP_SAVE))
		{
            status = PutIrpToDevice (Irp, (PBYTE)Irp->AssociatedIrp.SystemBuffer, irpStack->Parameters.DeviceIoControl.InputBufferLength);
		}
		break;
	case IOCTL_USBHUB_IRP_TO_3_RING: // deprecated
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof (IRP_SAVE))
		{
            status = GetIrpToAnswer (Irp);
			return status;
		}
		break;
	case IOCTL_USBHUB_DEVICE_PRESENT:
		status = DeviceIsPresent (Irp);
		break;
	case IOCTL_USBHUB_CANCEL:
		status = DeviceCancel (Irp);
		break;
	case IOCTL_USBHUB_GET_DEV_ID:
		status = GetDeviceId (Irp);
		break;
	case IOCTL_USBHUB_INIT_USB_DESC:
        status = InitUsbDecription (Irp);
		break;
	case IOCTL_USBHUB_ATTACH_TO_HUB:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
            status = AttachToUsbHub ((PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer);
		}
		break;
	case IOCTL_USBHUB_DEVICE_RESTART:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
			status = DeviceRestart ((PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer);
		}
		break;
	case IOCTL_USBHUB_DEVICE_IS_SHARED:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
			status = DetectDevice ((PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer);
		}
		break;
	case IOCTL_USBHUB_DEVICE_RELATIONS:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
            status = BusDeviceRelations ((PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer);
		}
		break;
	case IOCTL_USBHUB_DEVICE_INSTANCE_ID:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
			status = GetInstanceId ((PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer);		
			Irp->IoStatus.Information = sizeof (USB_DEV_SHARE);
		}
		break;
	case IOCTL_USBHUB_IRP_TO_3_RING_SET:
	case IOCTL_USBHUB_IRP_TO_0_RING_SET:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength>= sizeof (POOL_LIST) && 
			irpStack->Parameters.DeviceIoControl.OutputBufferLength>= sizeof (PVOID))
		{
			TPtrUsbPortToTcp	pUsbPort;

			pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
			if (pUsbPort.get())
			{
				status = SetPool (Irp, (irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_USBHUB_IRP_TO_3_RING_SET)?pUsbPort->m_poolWrite:pUsbPort->m_poolRead);
			}
		}
		break;

	case IOCTL_USBHUB_IRP_TO_3_RING_CLR:
	case IOCTL_USBHUB_IRP_TO_0_RING_CLR:
		{
			TPtrUsbPortToTcp	pUsbPort;
			pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
			if (pUsbPort.get())
			{
				status = ClrPool ((irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_USBHUB_IRP_TO_3_RING_CLR)?pUsbPort->m_poolWrite:pUsbPort->m_poolRead);
			}
		}
	case IOCTL_USBHUB_VERSION:
		if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG))
		{
			PULONG lVer = (PULONG)Irp->AssociatedIrp.SystemBuffer;
			*lVer = uDriverVerison;
			Irp->IoStatus.Information = sizeof(ULONG);
			status = STATUS_SUCCESS;
		}

	default:
		break;
	}

	if (status != STATUS_PENDING)
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--m_IrpCounter;
	}
	return status;
}

NTSTATUS CControlDeviceExtension::DispatchCleanup (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;
	bool				bRet;

	DebugPrint (DebugTrace, "DispatchCleanup");
	++m_IrpCounter;

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		m_spinUsb.Lock ();
		for (int a = 0; a < m_listUsbPortToTcp.GetCount (); a++)
		{
			if (pUsbPort == m_listUsbPortToTcp.GetItem (a))
			{
				m_spinUsb.UnLock ();
				if (pUsbPort->m_bIsPresent)
				{
					pUsbPort->StopTcpServer ();
				}
				if (pUsbPort->m_linkControl)
				{
					m_spinUsb.Lock ();
					bRet = m_listUsbPortToTcp.Remove (pUsbPort->m_linkControl);
					m_spinUsb.UnLock ();
				}
				else
				{
					m_spinUsb.Lock ();
					bRet = m_listUsbPortToTcp.Remove (a);
					m_spinUsb.UnLock ();
				}
				if (bRet)
				{
					DebugPrint (DebugInfo, "Delete pUsbPort - %S\\%d", pUsbPort->m_szHubDriverKeyName, pUsbPort->m_uDeviceAddress);
					pUsbPort->Delete ();
				}
				return DispatchCreate (DeviceObject, Irp);
			}
		}
		m_spinUsb.UnLock ();
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_IrpCounter;
	return STATUS_SUCCESS;
}

// default DispatchFunction 
NTSTATUS CControlDeviceExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint(DebugTrace, "DispatchDefault");

	++m_IrpCounter;
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_IrpCounter;
	return STATUS_NOT_SUPPORTED;
}

// ******************************************************************
//					Create Device
// ******************************************************************

NTSTATUS CControlDeviceExtension::CreateDevice (PDRIVER_OBJECT DriverObject)
{
    NTSTATUS            status = STATUS_SUCCESS;

	DebugPrint(DebugTrace, "CreateDevice (PDRIVER_OBJECT DriverObject)");
	// Create device
	status = IoCreateDevice (DriverObject,
							sizeof (CControlDeviceExtension*),
							&m_NameString,  // No Name
							m_DriverType,
							FILE_DEVICE_SECURE_OPEN,
							FALSE,
							&m_Self);


	if (!NT_SUCCESS (status)) 
	{
		DebugPrint (DebugError, "Did not create control device %S", m_NameString.Buffer);
		return status;
	}

	//
	// init PDEVICE_EXTENSION
	//
	*((CControlDeviceExtension**)m_Self->DeviceExtension) = this;

	// create symbolic link
	status = IoCreateSymbolicLink(&m_LinkString, &m_NameString); 

	if (!NT_SUCCESS(status)) 
	{ 
		DebugPrint (DebugError, "Did not create symbol link %S", m_LinkString.Buffer);
		IoDeleteDevice(m_Self); 
		return status; 
	} 

	DebugPrint (DebugTrace, "Create device %S", m_NameString.Buffer);
	return status;

}

//	Delete device
NTSTATUS CControlDeviceExtension::DeleteDevice ()
{
	NTSTATUS            status = STATUS_SUCCESS;

	DebugPrint (DebugTrace, "DeleteDevice - %S", m_NameString.Buffer);

	status = IoDeleteSymbolicLink (&m_LinkString);
	if (!NT_SUCCESS (status))
	{
		DebugPrint (DebugError, "IoDeleteSymbolicLink - 0x%x", status);
		return status;
	}

	IoDeleteDevice (m_Self);

	return status;
}

// ******************************************************************
//					Get USB hub control extension
// ******************************************************************
CControlDeviceExtension* CControlDeviceExtension::GetControlExtension ()
{
	PDEVICE_OBJECT				deviceObject;

	if (m_DriverObject == NULL)
	{
		return NULL;
	}

	deviceObject = m_DriverObject->DeviceObject;
	while (deviceObject)
	{
        if (deviceObject->DeviceType == m_DriverType)
		{
			CControlDeviceExtension *pControl;
			pControl = (CControlDeviceExtension*)GetExtensionClass (deviceObject);
			pControl->DebugPrint(DebugInfo, "GetControlExtension - 0x%x", pControl);
			return pControl;
		}
		deviceObject = deviceObject->NextDevice;
	}
	return NULL;
}

// ******************************************************************
//					work with PDO
// ******************************************************************
NTSTATUS CControlDeviceExtension::AddUsbPortToTcp (PUSB_DEV_SHARE	pUsbDev, PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;
	ULONG			   uMinor;

	DebugPrint (DebugTrace, "AddUsbPortToTcp %S %d %d", pUsbDev->HubDriverKeyName, pUsbDev->DeviceAddress, pUsbDev->ServerIpPort);

	if (Irp->Tail.Overlay.OriginalFileObject->FsContext != 0)
	{
		return STATUS_INVALID_PARAMETER;
	}
	if (!GetUsbPortToTcp (pUsbDev).get ())
	{
		TPtrPdoStubExtension	pPdo;

        pUsbPort = new CUsbPortToTcp (pUsbDev);
		Common::intrusive_ptr_add_ref (pUsbPort.get ());

		pUsbDev->Status = 1;
		// add pdo to list
		m_spinUsb.Lock();
		pUsbPort->m_linkControl = m_listUsbPortToTcp.Add (pUsbPort.get ());
		m_spinUsb.UnLock();
		pUsbPort->m_bNeedDeviceRelations = true;
		Irp->Tail.Overlay.OriginalFileObject->FsContext = pUsbPort.get ();
		// Find device
		pPdo = FindPdo (pUsbDev);
		if (pPdo.get())
		{
			pUsbPort->m_spinBase.Lock ('ADD');
			pPdo->m_pParent = pUsbPort;
			pUsbPort->m_bIsPresent = true;

			if (pUsbPort->m_pPdoStub)
			{
				DebugPrint (DebugError, "m_listPdoStub list clear");
			}

			pUsbPort->m_pPdoStub = pPdo.get();
			pUsbPort->m_spinBase.UnLock ();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS CControlDeviceExtension::RemoveUsbPortToTcp (PUSB_DEV_SHARE	pUsbDev, PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;
	bool				bRet = false;

	DebugPrint (DebugTrace, "RemoveUsbPortToTcp");

	pUsbPort = GetUsbPortToTcp (pUsbDev);
	if (pUsbPort.get ())
	{
		if (Irp->Tail.Overlay.OriginalFileObject->FsContext == pUsbPort.get ())
		{
			CAutoLock lockRemove (&pUsbPort->m_lockRemove, 'Del');
			DebugLevel OldDl = SetDebugLevel(DebugSpecial);
				 
			Irp->Tail.Overlay.OriginalFileObject->FsContext = NULL;

			m_spinUsb.Lock ();
            if (pUsbPort.get () && pUsbPort->m_irpWait)
            {
                pUsbPort->CompleteWaitIrp (pUsbPort->m_irpWait, false);
                pUsbPort->m_irpWait = NULL;
            }
			// stop thread
			pUsbPort->m_bWork = false;

			m_listUsbPortToTcp.Remove (pUsbPort->m_linkControl);
			if (pUsbPort->m_bAdditionDel)
			{
				DebugPrint (DebugSpecial, "pUsbPort->m_bAdditionDel");
				pUsbPort->m_bDelayedDelete = true;
			}
			if (pUsbPort->m_bIsPresent && pUsbPort->m_pPdoStub)
			{
				DebugPrint (DebugSpecial, "pUsbPort->m_bIsPresent && pUsbPort->m_listPdoStub.GetCount ()");
				pUsbPort->m_bDelayedDelete = true;
			}
			m_spinUsb.UnLock ();

			if (!pUsbPort->m_bDelayedDelete)
			{
				DebugPrint (DebugSpecial, "!pUsbPort->m_bDelayedDelete");
				DebugPrint (DebugInfo, "Delete pUsbPort - %S\\%d", pUsbPort->m_szHubDriverKeyName, pUsbPort->m_uDeviceAddress);
				pUsbPort->m_spinBase.Lock ('REM');
				if (pUsbPort->m_pPdoStub)
				{
					pUsbPort->m_pPdoStub->m_pParent = TPtrUsbPortToTcp ();
				}
				pUsbPort->m_spinBase.UnLock ();
				pUsbPort->Delete ();
			}
			else
			{
				DebugPrint (DebugSpecial, "pUsbPort->m_bDelayedDelete");
			}

			bRet = true;
			SetDebugLevel(OldDl);
		}
		else
		{
			DebugPrint (DebugError, "FsContext != pPdo, 0x%x != 0x%x", Irp->Tail.Overlay.OriginalFileObject->FsContext, pUsbPort);
            bRet = false;;
		}
	}
	return bRet?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
}

TPtrPdoStubExtension CControlDeviceExtension::FindPdo (PUSB_DEV_SHARE	pUsbDev)
{
	return (pUsbDev->DeviceAddress == 0)?FindPdoUsbDev (pUsbDev):FindPdoUsbPort (pUsbDev);
}

TPtrPdoStubExtension CControlDeviceExtension::FindPdoUsbPort (PUSB_DEV_SHARE	pUsbDev)
{
	// Find device
	CBusFilterExtension		*pBus;
	TPtrPdoStubExtension	pPdo;

	DebugPrint(DebugTrace, "FindPdoUsbPort");

	m_spinHub.Lock();
	for (int a = 0; a < m_listUsbHub.GetCount (); a++)
	{
		pBus = (CBusFilterExtension*)m_listUsbHub.GetItem (a);
		if (_wcsicmp (pBus->m_szDriverKeyName, pUsbDev->HubDriverKeyName) == 0)
		{
			pBus->SpinDr ().Lock ();
			for (int b = 0; b < pBus->GetList ().GetCount ();b++)
			{
				pPdo = (CPdoStubExtension*)pBus->GetChild (b).get();

				if (pPdo->m_uDeviceAddress == pUsbDev->DeviceAddress)
				{
					pBus->SpinDr ().UnLock ();
					m_spinHub.UnLock ();

					return pPdo;
				}
			}
			pBus->SpinDr ().UnLock ();
		}
	}
	m_spinHub.UnLock ();

	return NULL;
}

TPtrPdoStubExtension CControlDeviceExtension::FindPdoUsbDev (PUSB_DEV_SHARE	pUsbDev)
{
	// Find device
	CBusFilterExtension		*pBus;
	TPtrPdoStubExtension	pPdo;

	DebugPrint(DebugTrace, "FindPdoUsbDev");

	m_spinHub.Lock ();
	for (int a = 0; a < m_listUsbHub.GetCount (); a++)
	{
		pBus = (CBusFilterExtension*)m_listUsbHub.GetItem (a);

		pBus->SpinDr ().Lock ();
		for (int b = 0; b < pBus->GetList ().GetCount ();b++)
		{
			pPdo = (CPdoStubExtension*)pBus->GetChild (b).get ();

			if (_wcsicmp (pPdo->m_szDevId, pUsbDev->HubDriverKeyName) == 0)
			{
				pBus->SpinDr ().UnLock ();
				m_spinHub.UnLock ();

				return pPdo;
			}
		}
		pBus->SpinDr ().UnLock ();
	}
	m_spinHub.UnLock ();

	return NULL;
}

NTSTATUS CControlDeviceExtension::DetectDevice (PUSB_DEV_SHARE pUsbDev)
{
	TPtrUsbPortToTcp pUsbPort;
	DebugPrint (DebugTrace, "DetectDevice (PUSB_DEV_SHARE	pUsbDev)");

	m_spinUsb.Lock();

	// find share usb port
	for (int a = 0; a < m_listUsbPortToTcp.GetCount (); a++)
	{
		pUsbPort = (CUsbPortToTcp*) m_listUsbPortToTcp.GetItem (a);
		if (pUsbPort->m_uDeviceAddress != 0)
		{
			if ((_wcsnicmp (pUsbDev->HubDriverKeyName, pUsbPort->m_szHubDriverKeyName, 200) == 0) &&
				(pUsbDev->DeviceAddress == pUsbPort->m_uDeviceAddress))
			{
				m_spinUsb.UnLock();
				return STATUS_SUCCESS;
			}
		}
	}

	// find share usb port
	for (int a = 0; a < m_listUsbPortToTcp.GetCount (); a++)
	{
		pUsbPort = (CUsbPortToTcp*) m_listUsbPortToTcp.GetItem (a);
		if (pUsbPort->m_uDeviceAddress == 0)
		{
			if (_wcsnicmp (pUsbDev->HubDriverKeyName, pUsbPort->m_szDevIndef, 200) == 0)
			{
				m_spinUsb.UnLock();
				return STATUS_SUCCESS;
			}
		}
	}

	m_spinUsb.UnLock();
	return STATUS_UNSUCCESSFUL;
}

TPtrUsbPortToTcp CControlDeviceExtension::GetUsbPortToTcp (CPdoStubExtension	*pPdo)
{
	TPtrUsbPortToTcp pUsbPort;
	CBusFilterExtension *pBus = (CBusFilterExtension *)pPdo->m_pBus;

	DebugPrint (DebugTrace, "GetUsbPortToTcp (CPdoStubExtension	*pPdo)");

	m_spinUsb.Lock();

	// find share usb port
	for (int a = 0; a < m_listUsbPortToTcp.GetCount (); a++)
	{
		pUsbPort = (CUsbPortToTcp*) m_listUsbPortToTcp.GetItem (a);

		if (pUsbPort->m_uDeviceAddress != 0)
		{
			if ((_wcsnicmp (pBus->m_szDriverKeyName, pUsbPort->m_szHubDriverKeyName, 200) == 0) &&
				(pPdo->m_uDeviceAddress == pUsbPort->m_uDeviceAddress))
			{
				m_spinUsb.UnLock();
				return pUsbPort;
			}
		}
	}

	// find share usb device
	for (int a = 0; a < m_listUsbPortToTcp.GetCount (); a++)
	{
		pUsbPort = (CUsbPortToTcp*) m_listUsbPortToTcp.GetItem (a);
		if (pUsbPort->m_uDeviceAddress == 0)
		{
			if (_wcsnicmp (pPdo->m_szDevId, pUsbPort->m_szDevIndef, 200) == 0)
			{
				m_spinUsb.UnLock ();
				return pUsbPort;
			}
		}
	}

	m_spinUsb.UnLock ();
	return NULL;
}

TPtrUsbPortToTcp CControlDeviceExtension::GetUsbPortToTcp (PUSB_DEV_SHARE	pUsbDev)
{
	TPtrUsbPortToTcp pUsbPort;

	DebugPrint (DebugTrace, "GetUsbPortToTcp (PUSB_DEV_SHARE	pUsbDev)");

	m_spinUsb.Lock();

	// find share usb port
	for (int a = 0; a < m_listUsbPortToTcp.GetCount (); a++)
	{
		pUsbPort = (CUsbPortToTcp*) m_listUsbPortToTcp.GetItem (a);
		if (pUsbPort->m_uDeviceAddress != 0)
		{
			if ((_wcsnicmp (pUsbDev->HubDriverKeyName, pUsbPort->m_szHubDriverKeyName, 200) == 0) &&
				(pUsbDev->DeviceAddress == pUsbPort->m_uDeviceAddress))
			{
				m_spinUsb.UnLock();
				return pUsbPort;
			}
		}
	}

	// find share usb device
	for (int a = 0; a < m_listUsbPortToTcp.GetCount (); a++)
	{
		pUsbPort = (CUsbPortToTcp*) m_listUsbPortToTcp.GetItem (a);
		if (pUsbPort->m_uDeviceAddress == 0)
		{
			if (_wcsnicmp (pUsbDev->HubDriverKeyName, pUsbPort->m_szDevIndef, 200) == 0)
			{
				m_spinUsb.UnLock();
				return pUsbPort;
			}
		}
	}

	m_spinUsb.UnLock();
	return NULL;
}

void CControlDeviceExtension::ClearListUsbPortToTcp ()
{
	DebugPrint (DebugTrace, "ClearListPdo");
}

NTSTATUS CControlDeviceExtension::SetWaitIrp (PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;
    CPdoStubExtension	*pPdo;

	DebugPrint (DebugTrace, "SetWaitIrp");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		pUsbPort->m_spinBase.Lock ('CUSB');
		pUsbPort->m_bCancelControl = false;

		if (pUsbPort->m_bIsPresent && !pUsbPort->m_bAdditionDel)
		{
			BYTE bRestart = 0;
			if (pUsbPort->m_pPdoStub)
            {
				bRestart = pUsbPort->m_pPdoStub->m_bRestart;
				pUsbPort->m_pPdoStub->m_bRestart = 0;

            }
			DebugPrint (DebugInfo, "Wait ok");
			pUsbPort->m_spinBase.UnLock ();
            return bRestart?STATUS_INVALID_PARAMETER:STATUS_SUCCESS;
		}
		else
		{
			if (!pUsbPort->m_irpWait)
			{
				pUsbPort->m_irpWait = Irp;
			}
		}

		IoSetCancelRoutine (Irp, CControlDeviceExtension::CancelWaitIrp);
		IoMarkIrpPending (Irp);
		pUsbPort->m_spinBase.UnLock ();

		DebugPrint (DebugInfo, "Wait PENDING");
		return STATUS_PENDING;
	}

	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CControlDeviceExtension::PutIrpToDevice (PIRP Irp, PBYTE Buffer, LONG lSize)
{
	TPtrUsbPortToTcp pUsbPort;

	DebugPrint (DebugTrace, "PutIrpToDevice");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		pUsbPort->m_spinBase.Lock ('PIT');
		if (pUsbPort->m_bCancelControl)
		{
			pUsbPort->m_spinBase.UnLock ();
			return STATUS_INVALID_PARAMETER;
		}
		pUsbPort->m_spinBase.UnLock ();
		if (pUsbPort->m_bIsPresent && pUsbPort->m_pPdoStub)
		{
			return pUsbPort->InPutIrpToDevice (Buffer, lSize);
		}
	}

	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CControlDeviceExtension::GetIrpToAnswer (PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;

	DebugPrint (DebugTrace, "GetIrpToAnswer");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		pUsbPort->m_spinBase.Lock ('ITA');
		if (pUsbPort->m_bCancelControl)
		{
			pUsbPort->m_spinBase.UnLock ();
			return STATUS_INVALID_PARAMETER;
		}
		pUsbPort->m_spinBase.UnLock ();

		if (pUsbPort->m_bIsPresent && pUsbPort->m_pPdoStub)
		{
			return pUsbPort->GetIrpToAnswer (Irp);
		}
	}

	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CControlDeviceExtension::DeviceIsPresent (PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;

	DebugPrint (DebugTrace, "DeviceIsPresent");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get () && pUsbPort->m_bIsPresent && !pUsbPort->m_bAdditionDel)
	{
		return STATUS_SUCCESS;
	}

	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CControlDeviceExtension::DeviceCancel (PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;

	DebugPrint (DebugTrace, "DeviceCancel");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get () && pUsbPort->m_bIsPresent)
	{
		pUsbPort->m_bCancel = true;
		pUsbPort->CancelControlIrp ();
		pUsbPort->m_irpCounter.WaitStop ();
		pUsbPort->m_bCancel = false;
		pUsbPort->m_irpCounter.Reset ();

		return STATUS_SUCCESS;
	}

	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CControlDeviceExtension::InitUsbDecription (PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;
    CPdoStubExtension	*pPdo;

	DebugPrint (DebugTrace, "InitUsbDecription");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		CAutoLock AutoLock (&pUsbPort->m_spinBase, 'UDSC');
		pUsbPort->m_bCancelControl = false;

		if (pUsbPort->m_bIsPresent)
		{
			if (pUsbPort->m_pPdoStub)
            {
                pUsbPort->m_pPdoStub->ReadandSelectDescriptors ();
            }
			DebugPrint (DebugInfo, "Wait ok");
            return STATUS_SUCCESS;
		}
	}

	return STATUS_INVALID_PARAMETER;

}

NTSTATUS CControlDeviceExtension::GetDeviceId (PIRP Irp)
{
	TPtrUsbPortToTcp	pUsbPort;
    NTSTATUS            status = STATUS_INVALID_PARAMETER;
    CPdoStubExtension	*pPdo;
	int					iSize;
    PIO_STACK_LOCATION  irpStack;

	DebugPrint (DebugTrace, "GetDeviceId");

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		pUsbPort->m_spinBase.Lock ('GID');
		pUsbPort->m_bCancelControl = false;

		if (pUsbPort->m_bIsPresent)
		{
			if (pUsbPort->m_pPdoStub)
            {
                pPdo = pUsbPort->m_pPdoStub;
				if (!pPdo->m_szCompapatible.GetSize())
				{
					status = STATUS_NO_DATA_DETECTED;
				}
				else
				{
					pUsbPort->m_spinBase.UnLock();
					iSize = pPdo->m_szCompapatible.GetSize();
					if (iSize > int (irpStack->Parameters.DeviceIoControl.OutputBufferLength))
					{
						status = STATUS_BUFFER_TOO_SMALL;
					}
					else
					{
						RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, pPdo->m_szCompapatible.GetBuffer (), iSize);
						Irp->IoStatus.Information = iSize;
						status = STATUS_SUCCESS;
					}
					pUsbPort->m_spinBase.Lock('GID2');
				}
            }
		}
		pUsbPort->m_spinBase.UnLock ();
	}

	return status;
}

NTSTATUS CControlDeviceExtension::AttachToUsbHub (PUSB_DEV_SHARE HubDev)
{
	int						i;
	UNICODE_STRING			DirString;
	OBJECT_ATTRIBUTES		DirAttr;
	NTSTATUS				NtStatus;
	HANDLE					hDir;
	CHAR					Buf[512];
	WCHAR					DriverPath[256];
	BOOLEAN					bFirst = TRUE;
	ULONG					ObjectIndex = 0;
	ULONG					LengthReturned = 0;
	ULONG					Index = 0;
	POBJECT_NAMETYPE_INFO	pObjName = (POBJECT_NAMETYPE_INFO) Buf;
	PDRIVER_OBJECT			pDriverObject;
	PDEVICE_OBJECT			pDeviceObject;

	bFirst = TRUE;
	ObjectIndex = 0;
	LengthReturned = 0;
	Index = 0;

	DebugPrint (DebugTrace, "AttachToUsbHub");

	RtlInitUnicodeString(&DirString, L"\\Driver");
	InitializeObjectAttributes(&DirAttr, &DirString, OBJ_CASE_INSENSITIVE, NULL, NULL);
	NtStatus = ZwOpenDirectoryObject(&hDir, DIRECTORY_QUERY, &DirAttr);
	if (NtStatus == STATUS_SUCCESS)
	{
		while (ZwQueryDirectoryObject(hDir, Buf, sizeof(Buf), FALSE, bFirst, &ObjectIndex, &LengthReturned) >= 0)
		{
			bFirst = FALSE;
			for (i=0; Index < ObjectIndex; ++Index, ++i)
			{
				RtlStringCbCopyW(DriverPath, sizeof(DriverPath), L"\\Driver\\");
				if ((sizeof(DriverPath)-wcslen(DriverPath)-sizeof(WCHAR)) > pObjName[i].ObjectName.Length)
				{
					RtlStringCbCatW(DriverPath, sizeof(DriverPath), pObjName[i].ObjectName.Buffer);
					DebugPrint (DebugSpecial, "%S", DriverPath);
					pDriverObject = (PDRIVER_OBJECT)GetObjectByPath(DriverPath);
					if (pDriverObject != NULL)
					{
						pDeviceObject = pDriverObject->DeviceObject;
						while (pDeviceObject)
						{
							NtStatus = IoGetDeviceProperty (pDeviceObject, 
								DevicePropertyPhysicalDeviceObjectName,
								256,
								DriverPath,
								&LengthReturned);         

							if (NT_SUCCESS (NtStatus))
							{
								DebugPrint (DebugSpecial, "  %S", DriverPath);
							}
							
							NtStatus = IoGetDeviceProperty (pDeviceObject, 
								DevicePropertyDriverKeyName,
								256,
								DriverPath,
								&LengthReturned);         

							if (NT_SUCCESS (NtStatus))
							{
								DebugPrint (DebugSpecial, "    %S", DriverPath);
								if (wcscmp (DriverPath, HubDev->HubDriverKeyName) == 0)
								{
									if (NT_SUCCESS (FilterAddDevice (m_DriverObject, pDeviceObject)))
									{
										ZwClose(hDir);
										IoInvalidateDeviceRelations (pDeviceObject, BusRelations);
										return STATUS_SUCCESS;
									}
								}
							}
							pDeviceObject = pDeviceObject->NextDevice;
						}
					}
				}
			}
		}
		ZwClose(hDir);
	}

	DebugPrint (DebugSpecial, "%S not found", HubDev->HubDriverKeyName);
	return STATUS_UNSUCCESSFUL;
}

PVOID CControlDeviceExtension::GetObjectByPath(PWSTR pwszObjectName)
{
	PVOID              pObject = NULL;
	UNICODE_STRING     DeviceName;
	NTSTATUS           NtStatus = 0;
	POBJECT_TYPE	   ObjType;
	static ULONG	   uVersion = 0;

	DebugPrint (DebugTrace, "GetObjectByPath");

	if (uVersion == 0)
	{
		ULONG	   uMinor;
		PsGetVersion (&uVersion, &uMinor, NULL, NULL);
		uVersion *= 100;
		uVersion += uMinor;
	}

	if (!pwszObjectName || KeGetCurrentIrql() > PASSIVE_LEVEL)
		return pObject;


	RtlInitUnicodeString(&DeviceName, pwszObjectName);

	__try
	{
	    if (uVersion < 601)
		{
			ObjType = (POBJECT_TYPE)IoDriverObjectType;
		}
		else
		{
			ObjType = *((POBJECT_TYPE*)IoDriverObjectType); 
		}

		NtStatus = ObReferenceObjectByName(&DeviceName, OBJ_CASE_INSENSITIVE, NULL, 0, ObjType, KernelMode, NULL, (PVOID*)&pObject);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		NtStatus = STATUS_ACCESS_VIOLATION;
	}
	if (NT_SUCCESS(NtStatus))
	{
		ObDereferenceObject((PVOID)pObject);
	}

	return pObject;
}


NTSTATUS CControlDeviceExtension::BusDeviceRelations (PUSB_DEV_SHARE HubDev)
{
	NTSTATUS				status = STATUS_UNSUCCESSFUL;
	KIRQL					OldIrql;
	CBusFilterExtension		*pBus;
	TPtrUsbPortToTcp		pUsb;

	KEVENT					eventDeviceRelations;

	DebugPrint (DebugTrace, "BusDeviceRelations");

	KeInitializeEvent(&eventDeviceRelations, NotificationEvent, FALSE);

	m_spinHub.Lock ();
	for (int a = 0; a < m_listUsbHub.GetCount (); a++)
	{
		pBus = (CBusFilterExtension*)m_listUsbHub.GetItem (a);
		if (_wcsicmp (pBus->m_szDriverKeyName, HubDev->HubDriverKeyName) == 0)
		{
			pUsb = GetUsbPortToTcp (HubDev);
			if (pUsb.get () && pUsb->m_bNeedDeviceRelations)
			{
				LARGE_INTEGER	time;
				time.QuadPart = (LONGLONG)-6000000000;		// 30 sec

				pUsb->m_bNeedDeviceRelations = false;
				pBus->m_listEventDevRelations.Add (&eventDeviceRelations);
				IoInvalidateDeviceRelations (pBus->m_PhysicalDeviceObject, BusRelations);

				m_spinHub.UnLock ();

				KeWaitForSingleObject(&eventDeviceRelations, 
									  Executive, 
									  KernelMode, 
									  FALSE, 
									  &time);

				return STATUS_SUCCESS;
			}
			break;
		}
	}
	m_spinHub.UnLock ();

	return status;
}

NTSTATUS CControlDeviceExtension::GetInstanceId (PUSB_DEV_SHARE HubDev)
{
	TPtrPdoStubExtension	pPdo;
	CBusFilterExtension		*pBus;
	KIRQL					OldIrql;
	NTSTATUS				status = STATUS_UNSUCCESSFUL;

	DebugPrint (DebugTrace, "GetInstanceId %S %d %d", HubDev->HubDriverKeyName, HubDev->DeviceAddress, HubDev->ServerIpPort);

	// Find device
	m_spinHub.Lock ();
	for (int a = 0; a < m_listUsbHub.GetCount (); a++)
	{
		pBus = (CBusFilterExtension*)m_listUsbHub.GetItem (a);
		//pBus->InitName (pBus->m_PhysicalDeviceObject);
		if (_wcsicmp (pBus->m_szDriverKeyName, HubDev->HubDriverKeyName) == 0)
		{
			pBus->SpinDr().Lock ();
			for (int b = 0; b < pBus->GetList ().GetCount ();b++)
			{
				pPdo = (CPdoStubExtension*)pBus->GetChild (b).get();
				if (pPdo->m_uDeviceAddress == HubDev->DeviceAddress)
				{
					break;
				}
			}
			pBus->SpinDr().UnLock ();
			break;
		}
		pPdo = NULL;
	}
	m_spinHub.UnLock ();

	if (pPdo.get() && NT_SUCCESS (pPdo->GetInstanceId (HubDev->HubDriverKeyName, 200)))
	{
		status = STATUS_SUCCESS;
	}

	return status;
}

NTSTATUS CControlDeviceExtension::CycleUsbPort(PDEVICE_OBJECT DeviceObject) // usb port PDO 
{ 
	NTSTATUS status; 
	KEVENT Event; 
	IO_STATUS_BLOCK iostatus; 
	PIRP Irp; 
	PIO_STACK_LOCATION stack; 

	DebugPrint (DebugTrace, "CycleUsbPort");

	KeInitializeEvent(&Event, NotificationEvent, FALSE); 

	Irp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_CYCLE_PORT, DeviceObject, NULL, 0, NULL, 0, TRUE, &Event, &iostatus); 
	stack = IoGetNextIrpStackLocation(Irp); 
	stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL; 
	stack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_CYCLE_PORT; 

    __try
    {
		status = IoCallDriver(DeviceObject, Irp); 
		if ( status == STATUS_PENDING ) 
		{ 
			KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL); 
			status = iostatus.Status; 
		} 
	}
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
		DebugPrint (DebugError, "CycleUsbPort __except");
        status = GetExceptionCode();
    }

	if (!NT_SUCCESS(status)) 
	{ 
		DebugPrint (DebugError, "CycleUsbPort error");
	} 
	return status; 
}	

NTSTATUS CControlDeviceExtension::DeviceRestart (PUSB_DEV_SHARE HubDev)
{
	NTSTATUS				status = STATUS_UNSUCCESSFUL;
	KIRQL					OldIrql;
	CBusFilterExtension		*pBus;
	TPtrPdoStubExtension	pPdo;
	static ULONG	   uVersion = 0;

	KEVENT					eventDeviceRelations;

	DebugPrint (DebugSpecial, "DeviceRestart");

	KeInitializeEvent(&eventDeviceRelations, NotificationEvent, FALSE);

	m_spinHub.Lock ();
	for (int a = 0; a < m_listUsbHub.GetCount (); a++)
	{
		pBus = (CBusFilterExtension*)m_listUsbHub.GetItem (a);
		if (_wcsicmp (pBus->m_szDriverKeyName, HubDev->HubDriverKeyName) == 0)
		{
			pBus->SpinDr ().Lock ();
			for (int b = 0; b < pBus->GetList ().GetCount ();b++)
			{
				pPdo = (CPdoStubExtension*)pBus->GetChild (b).get();
				if (pPdo->m_uDeviceAddress == HubDev->DeviceAddress)
				{
					pBus->SpinDr ().UnLock ();
					m_spinHub.UnLock ();

					if (pPdo->m_pParent.get())
					{
						DebugPrint (DebugSpecial, "m_bAdditionDel = true");
						pPdo->m_pParent->m_bAdditionDel = true;
					}
					{

						status = CycleUsbPort (pPdo->m_PhysicalDeviceObject);

						if (!pPdo->WaitRemoveDevice (4000))
						{
							DebugPrint (DebugSpecial, "WaitRemoveDevice false");
							status = STATUS_NOT_SUPPORTED;
						}
						else
						{
							DebugPrint (DebugSpecial, "WaitRemoveDevice true");
						}
					}
					if (!NT_SUCCESS (status) && pPdo->m_pParent.get())
					{
						DebugPrint (DebugSpecial, "m_bAdditionDel = false");
						pPdo->m_pParent->m_bAdditionDel = false;
					}
					return status;
				}
			}
			pBus->SpinDr ().UnLock ();
			break;
		}
	}
	m_spinHub.UnLock ();

	DebugPrint (DebugSpecial, "DeviceRestart not found");
	return status;
}

VOID CControlDeviceExtension::CancelWaitIrp ( PDEVICE_OBJECT DeviceObject, PIRP Irp )
{
	TPtrUsbPortToTcp	pUsbPort;
    CPdoStubExtension	*pPdo;

	CControlDeviceExtension::GetControlExtension ()->DebugPrint (DebugTrace, "CancelWaitIrp");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		PIRP	irpWait = NULL;
		pUsbPort->m_spinBase.Lock ('CWIr');
		if (pUsbPort->m_irpWait == Irp)
		{
			irpWait = pUsbPort->m_irpWait;
			pUsbPort->m_irpWait = NULL;
		}
		pUsbPort->m_spinBase.UnLock ();

		IoReleaseCancelSpinLock ( Irp->CancelIrql );
		if (irpWait)
		{
			irpWait->IoStatus.Status = STATUS_UNSUCCESSFUL;
			IoCompleteRequest (irpWait, IO_NO_INCREMENT);
			--CControlDeviceExtension::GetControlExtension ()->m_IrpCounter;
		}
	}
	else
	{
		IoReleaseCancelSpinLock ( Irp->CancelIrql );
		CControlDeviceExtension::GetControlExtension ()->DebugPrint (DebugError, "CancelWaitIrp");
	}
}

VOID CControlDeviceExtension::CancelIrpToAnswer ( PDEVICE_OBJECT DeviceObject, PIRP Irp )
{
	TPtrUsbPortToTcp	pUsbPort;
    CPdoStubExtension	*pPdo;

	CControlDeviceExtension::GetControlExtension ()->DebugPrint (DebugTrace, "CancelIrpToAnswer");

	pUsbPort = (CUsbPortToTcp*)Irp->Tail.Overlay.OriginalFileObject->FsContext;

	if (pUsbPort.get ())
	{
		PIRP	pIrpSaved = NULL;
		pUsbPort->m_spinBase.Lock ('CAIr');
		for (int a = 0; a < pUsbPort->m_listIrpControlAnswer.GetCount (); a++)
		{
			pIrpSaved = (PIRP)pUsbPort->m_listIrpControlAnswer.GetItem (a);
			if (pIrpSaved == Irp)
			{
				pUsbPort->m_listIrpControlAnswer.Remove (a);
				break;
			}
			 
		}
		pUsbPort->m_spinBase.UnLock ();
		IoReleaseCancelSpinLock ( Irp->CancelIrql );

		if (pIrpSaved)
		{
			pIrpSaved->IoStatus.Status = STATUS_UNSUCCESSFUL;
			IoCompleteRequest (pIrpSaved, IO_NO_INCREMENT);
			--CControlDeviceExtension::GetControlExtension ()->m_IrpCounter;
		}
	}
	else
	{
		IoReleaseCancelSpinLock ( Irp->CancelIrql );

		CControlDeviceExtension::GetControlExtension ()->DebugPrint (DebugError, "CancelIrpToAnswer");
		Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);

	}
}


NTSTATUS CControlDeviceExtension::SetPool (PIRP Irp, CPoolListDrv &pPool)
{
	PIO_STACK_LOCATION  irpStack;

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	if (!pPool.GetRawData() && irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (PVOID))
	{
		*((PVOID*)Irp->UserBuffer) = pPool.Init(irpStack->Parameters.DeviceIoControl.Type3InputBuffer);
		if (*((PVOID*)Irp->UserBuffer))
		{
			return STATUS_SUCCESS;
		}
	}
	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CControlDeviceExtension::ClrPool (CPoolListDrv &pool)
{
	pool.Clear();
	return STATUS_SUCCESS;
}

