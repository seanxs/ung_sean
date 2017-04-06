/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    fusbhub.cpp

Abstract:

    This module contains the entry points for a USB device filter.

Environment:

    Kernel mode

--*/
#include "fusbhub.h"
#include "Core\BaseDeviceExtension.h"
#include "BusFilterExtension.h"
#include "ControlDeviceExtension.h"
#include "UnicIndef.h"

GUID GuidUsb = {0xb1a96a13, 0x3de0, 0x4574, 0x9b, 0x1, 0xc0, 0x8f, 0xea, 0xb3, 0x18, 0xd6};
GUID GuidUsbCamd = {0x2bcb75c0, 0xb27f, 0x11d1, 0xba, 0x41, 0x0, 0xa0, 0xc9, 0xd, 0x2b, 0x5};

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
	NTSTATUS            status = STATUS_SUCCESS;

	DriverBase::CBaseDeviceExtension::Init (DriverObject, RegistryPath);
    DriverObject->DriverExtension->AddDevice		        = FilterAddDevice;
    DriverObject->DriverUnload								= FilterUnload;

	CControlDeviceExtension	*pControl = new CControlDeviceExtension	();
	if (!NT_SUCCESS (status = pControl->CreateDevice (DriverObject)))
	{
		delete pControl;
	}

	CUnicId::Init ();
	return status;
}

NTSTATUS
FilterAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
{
	BOOLEAN bHubDetect;
	if (DeviceIsHub (DriverObject, PhysicalDeviceObject, bHubDetect))
	{
		CBusFilterExtension	*pBase = new CBusFilterExtension (bHubDetect);
		return pBase->AttachToDevice (DriverObject, PhysicalDeviceObject);
	}
	
	return STATUS_SUCCESS;
}

BOOLEAN DeviceIsHubPnp (
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject,
	OUT	BOOLEAN		  &bHubDetect
	)
{
	WCHAR		szBuff [200] = L"\\Driver\\vuhub";
	WCHAR		*szTemp;
	IO_STATUS_BLOCK	ioStatus;
	NTSTATUS    status;
	ULONG		uRes;

	DriverBase::CBaseDeviceExtension		base;
	UNICODE_STRING  stEtimaDevice;

	bHubDetect = false;

	base.DebugPrint (DriverBase::CBaseDeviceExtension::DebugTrace, "FilterAddDevice");

	// Not set filter if the device is virtual USB hub
	if (_wcsnicmp (szBuff, PhysicalDeviceObject->DriverObject->DriverName.Buffer, wcslen (szBuff)) == 0)
	{
		return false;
	}

	// Find Hardware ID Root hub
	status = GetDeviceId (PhysicalDeviceObject, BusQueryHardwareIDs, &ioStatus);
	if (NT_SUCCESS (status))
	{
		status = STATUS_UNSUCCESSFUL;
		szTemp = (WCHAR*)ioStatus.Information;
		while (wcslen (szTemp))
		{
			_wcsupr (szTemp);
			base.DebugPrint (DriverBase::CBaseDeviceExtension::DebugInfo, "HdId - %S", szTemp);
			if (wcsstr (szTemp, L"ROOT_HUB") != 0 ||
				wcsstr (szTemp, L"ROOT_HUB20") != 0)
			{
				status = STATUS_SUCCESS;
				break;
			}
			szTemp += wcslen (szTemp) + 1;
		}
	}
	else
	{
		return TRUE;
	}

	if (!NT_SUCCESS (status))
	{
		szTemp = (WCHAR*)ioStatus.Information;
		while (wcslen (szTemp))
		{
			_wcsupr (szTemp);
			if (wcsstr (szTemp, L"HUB") != NULL)
			{
				bHubDetect = true;
				status = STATUS_SUCCESS;
				break;
			}
			szTemp += wcslen (szTemp) + 1;
		}
	}

	if (ioStatus.Information)
	{
		ExFreePool ((PVOID)ioStatus.Information);
		ioStatus.Information = NULL;
	}
	// Find Compatible IDs Class USB Hub (0x9)
	if (!NT_SUCCESS (status))
	{
		status = GetDeviceId (PhysicalDeviceObject, BusQueryCompatibleIDs, &ioStatus);

		if (NT_SUCCESS (status))
		{
			status = STATUS_UNSUCCESSFUL;
			szTemp = (WCHAR*)ioStatus.Information;
			while (wcslen (szTemp))
			{
				base.DebugPrint (DriverBase::CBaseDeviceExtension::DebugInfo, "CmId - %S", szTemp);

				_wcsupr (szTemp);
				// found sub class 09
				if (_wcsicmp (L"USB\\CLASS_09", szTemp) == 0 ||
					wcsstr (szTemp, L"HUB") != NULL)
				{
					status = STATUS_SUCCESS;
					break;
				}
				// found sub class 09 - 2
				if (wcsstr (szTemp, L"\\CLASS_09") != NULL)
				{
					status = STATUS_SUCCESS;
					break;
				}

				szTemp += wcslen (szTemp) + 1;
			}
		}
	}
	if (ioStatus.Information)
	{
		ExFreePool ((PVOID)ioStatus.Information);
		ioStatus.Information = NULL;
	}

	return NT_SUCCESS (status);
}


BOOLEAN DeviceIsHub (PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject, BOOLEAN &bHubDetect)
{
	WCHAR		szBuff [200] = L"\\Driver\\vuhub";
	WCHAR		*szTemp;
	NTSTATUS    status;
	ULONG		uRes;
	DriverBase::CBaseDeviceExtension	base;
	UNICODE_STRING  stEtimaDevice;
	//bool		bHubDetect = false;

	bHubDetect = false;

	base.DebugPrint (DriverBase::CBaseDeviceExtension::DebugTrace, "FilterAddDevice");

	// Not set filter if the device is virtual USB hub
	if (_wcsnicmp (szBuff, PhysicalDeviceObject->DriverObject->DriverName.Buffer, wcslen (szBuff)) == 0)
	{
		return false;
	}

	// Find Hardware ID Root hub
	status = IoGetDeviceProperty (PhysicalDeviceObject, 
		DevicePropertyHardwareID,
		200*sizeof (WCHAR),
		szBuff,
		&uRes);

	if (NT_SUCCESS (status))
	{
		status = STATUS_UNSUCCESSFUL;
		szTemp = szBuff;
		while (wcslen (szTemp))
		{
			_wcsupr (szTemp);
			base.DebugPrint (DriverBase::CBaseDeviceExtension::DebugInfo, "HdId - %S", szTemp);
			if (wcsstr (szTemp, L"ROOT_HUB") != 0 ||
				wcsstr (szTemp, L"ROOT_HUB20") != 0)
			{
				status = STATUS_SUCCESS;
				break;
			}
			szTemp += wcslen (szTemp) + 1;
		}
	}
	if (!NT_SUCCESS (status))
	{
		szTemp = szBuff;
		while (wcslen (szTemp))
		{
			_wcsupr (szTemp);
			if (wcsstr (szTemp, L"HUB") != NULL)
			{
				bHubDetect = true;
				status = STATUS_SUCCESS;
				break;
			}
			szTemp += wcslen (szTemp) + 1;
		}
	}

	// Find Compatible IDs Class USB Hub (0x9)
	if (!NT_SUCCESS (status))
	{
		status = IoGetDeviceProperty (PhysicalDeviceObject, 
			DevicePropertyCompatibleIDs,
			200*sizeof (WCHAR),
			szBuff,
			&uRes);

		if (NT_SUCCESS (status))
		{
			status = STATUS_UNSUCCESSFUL;
			szTemp = szBuff;
			while (wcslen (szTemp))
			{
				base.DebugPrint (DriverBase::CBaseDeviceExtension::DebugInfo, "CmId - %S", szTemp);

				_wcsupr (szTemp);
				// found sub class 09
				if (_wcsicmp (L"USB\\CLASS_09", szTemp) == 0 ||
					wcsstr (szTemp, L"HUB") != NULL)
				{
					status = STATUS_SUCCESS;
					break;
				}
				// found sub class 09 - 2
				if (wcsstr (szTemp, L"\\CLASS_09") != NULL)
				{
					status = STATUS_SUCCESS;
					break;
				}

				szTemp += wcslen (szTemp) + 1;
			}
		}
	}

	return NT_SUCCESS (status);
}

VOID
FilterUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
	CControlDeviceExtension	*pControl = CControlDeviceExtension::GetControlExtension ();
	pControl->m_listUsbHub.WaitClearList ();
	pControl->ClearListUsbPortToTcp ();
	pControl->DeleteDevice ();
	pControl->m_IrpCounter.WaitRemove ();
	CUnicId::Delete ();
	DriverBase::CBaseDeviceExtension::Unload();
}


NTSTATUS
GetDeviceId(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
	IN BUS_QUERY_ID_TYPE iType,
	OUT IO_STATUS_BLOCK	*pStatus
	)
{
    KEVENT              pnpEvent;
    NTSTATUS            status;
    PDEVICE_OBJECT      targetObject = NULL;
    PIO_STACK_LOCATION  irpStack;
    PIRP                pnpIrp;

    PAGED_CODE();

    //
    // Initialize the event
    //
    KeInitializeEvent( &pnpEvent, NotificationEvent, FALSE );

    targetObject = IoGetAttachedDeviceReference( PhysicalDeviceObject );

    //
    // Build an Irp
    //
    pnpIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP,
        targetObject,
        NULL,
        0,
        NULL,
        &pnpEvent,
        pStatus
        );

    if ( pnpIrp ) 
	{
		//
		// Pnp Irps all begin life as STATUS_NOT_SUPPORTED;
		//
		pnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

		//
		// Get the top of stack
		//
		irpStack = IoGetNextIrpStackLocation( pnpIrp );

		//
		// Set the top of stack
		//
		RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );
		irpStack->MajorFunction = IRP_MJ_PNP;
		irpStack->MinorFunction = IRP_MN_QUERY_ID;
		irpStack->Parameters.QueryId.IdType = iType;

		//
		// Call the driver
		//
		status = IoCallDriver( targetObject, pnpIrp );
		if (status == STATUS_PENDING) 
		{

		    LARGE_INTEGER Timeout;
		    Timeout.QuadPart = Int32x32To64(50, -10000);

			//
			// Block until the irp comes back.
			// Important thing to note here is when you allocate 
			// the memory for an event in the stack you must do a  
			// KernelMode wait instead of UserMode to prevent 
			// the stack from getting paged out.
			//

			status = KeWaitForSingleObject(&pnpEvent, Executive, KernelMode, FALSE, &Timeout);
            if (status == STATUS_TIMEOUT)
            {
				IoCancelIrp (pnpIrp);
				status = pStatus->Status;
			}
			else
			status = pStatus->Status;
		}
	}

    //
    // Done with reference
    //
	if (targetObject)
	{
		ObDereferenceObject( targetObject );
	}

    //
    // Done
    //
    return status;
}

