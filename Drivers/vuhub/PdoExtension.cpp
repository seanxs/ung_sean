/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PdoExtension.cpp

Abstract:

	Virtual USB device class.

Environment:

    Kernel mode

--*/

#include "PdoExtension.h"
#include "VirtualUsbHub.h"
#include "UsbDevToTcp.h"
#include "Core\WorkItem.h"
#include "Core\Buffer.h"
#include "FilterStub.h"
#include "wdmsec.h"
#include <initguid.h>
#include "Common\common.h"
#include "Core\SecurityUtils.h"
#include "Core\Thread.h"
#include "PoolCompletingIrp.h"

#pragma warning(disable : 4995)

CPdoExtension::CPdoExtension ()
	: CBaseDeviceExtension ()
	, m_SystemPowerState (PowerSystemUnspecified)
	, m_DevicePowerState (PowerDeviceUnspecified)
	, m_pParent (NULL)
	, m_bPresent (false)
	, m_bInit(false)
	, m_uServerPort (0)
	, m_uAddressDevice (0)
	, m_DeviceIndef (0)
	, m_pQuyryDeviceText_Description (NULL)
	, m_uQuyryDeviceText_Description (0)
	, m_pQuyryDeviceText_LocalInf (NULL)
	, m_uQuyryDeviceText_LocalInf (0)
	, m_uQuyryDeviceText_uLocalId (0x0409)
	, m_pQueryId_BusQueryDeviceID (NULL)
	, m_uQueryId_BusQueryDeviceID (0)
	, m_pQueryId_BusQueryHardwareIDs (NULL)
	, m_uQueryId_BusQueryHardwareIDs (0)
	, m_pQueryId_BusQueryCompatibleIDs (NULL)
	, m_uQueryId_BusQueryCompatibleIDs (0)
	, m_pQueryId_BusQueryInstanceID (NULL)
	, m_uQueryId_BusQueryInstanceID (0)
	, m_pTcp (NULL)
	, m_bCardReader (false)
	, m_pEventRomove (NULL)
	, m_uNumberPort (0)
	, m_pAcl (NULL)
	, m_bDisableAutoComplete(false)
{
	SetDebugName ("CPdoExtension");
    RtlZeroMemory (&m_DeviceDescriptor, sizeof (USB_DEVICE_DESCRIPTOR));
    RtlZeroMemory (&m_LinkInterface, sizeof (UNICODE_STRING));
}

CPdoExtension::~CPdoExtension ()
{
	if (m_pQuyryDeviceText_Description != NULL)
	{
		ExFreePool (m_pQuyryDeviceText_Description);
		m_pQuyryDeviceText_Description = NULL;
	}
	if (m_pQuyryDeviceText_LocalInf != NULL)
	{
		ExFreePool (m_pQuyryDeviceText_LocalInf);
		m_pQuyryDeviceText_LocalInf = NULL;
	}
	// IRP_MN_QUERY_ID
	if (m_pQueryId_BusQueryDeviceID != NULL)
	{
		ExFreePool (m_pQueryId_BusQueryDeviceID);
        m_pQueryId_BusQueryDeviceID = NULL;
	}

	if (m_pQueryId_BusQueryHardwareIDs != NULL)
	{
		ExFreePool (m_pQueryId_BusQueryHardwareIDs);
		m_pQueryId_BusQueryHardwareIDs = NULL;
	}
	if (m_pQueryId_BusQueryCompatibleIDs != NULL)
	{
		ExFreePool (m_pQueryId_BusQueryCompatibleIDs);
		m_pQueryId_BusQueryCompatibleIDs = NULL;
	}
    if (m_pQueryId_BusQueryInstanceID != NULL)
	{
		ExFreePool (m_pQueryId_BusQueryInstanceID);
		m_pQueryId_BusQueryInstanceID  = NULL;
	}

	if (m_pAcl)
	{
		ExFreePool (m_pAcl);
		m_pAcl = NULL;
	}
}

// default DispatchFunction 
NTSTATUS CPdoExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS            status = STATUS_NOT_SUPPORTED;//STATUS_PENDING;

	DebugPrint (DebugError, "No tcp %s", MajorFunctionString (Irp));

	++m_irpCounterPdo;
	CancelUrb (Irp);
	Irp->IoStatus.Status = status;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounterPdo;
	return status;
}

NTSTATUS CPdoExtension::DispatchDefaultTcp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS            status = STATUS_NOT_SUPPORTED;
    evuh::CUsbDevToTcp  *pTcp = m_pTcp; 

	DebugPrint (DebugTrace, "DispatchDefaultTcp %s", MajorFunctionString (Irp));

	if (m_bPresent && pTcp)
	{
        ++pTcp->m_WaitUse;
		status = pTcp->OutIrpToTcp (this, Irp);
        --pTcp->m_WaitUse;
        return status;
	}

	if (CancelUrb(Irp, STATUS_NO_SUCH_DEVICE, USBD_STATUS_INVALID_PARAMETER) && m_pTcp)
	{
		IoMarkIrpPending (Irp);
		status = STATUS_PENDING;
		{
			++m_irpCounterPdo;
			++m_irpTcp;

			PIRP_NUMBER irpNumberCancel = (PIRP_NUMBER)m_pTcp->NewIrpNumber(Irp, this);
			if (irpNumberCancel)
			{
				//decrease counter 
				m_pTcp->DeleteIrpNumber(irpNumberCancel);

				CPoolCompletingIrp::Instance ()->AddIrpToCompleting(irpNumberCancel);
				DebugPrint(DebugWarning, "CancelRoutine - %d", irpNumberCancel->Count);
			}
		}
	}
	else
	{
		DebugPrint (DebugWarning, "DispatchDefaultTcp not present - %s", MajorFunctionString (Irp));
		status = STATUS_NO_SUCH_DEVICE;
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
	}

    return status;
}

NTSTATUS CPdoExtension::DispatchCreate (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS            status = Irp->IoStatus.Status;//STATUS_PENDING;

	DebugPrint (DebugError, "%s", MajorFunctionString (Irp));

	++m_irpCounterPdo;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounterPdo;
    return status;
}

NTSTATUS CPdoExtension::DispatchClose (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS            status = Irp->IoStatus.Status;//STATUS_PENDING;

	DebugPrint (DebugError, "%s", MajorFunctionString (Irp));

	++m_irpCounterPdo;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounterPdo;
    return status;
}

NTSTATUS CPdoExtension::PnpDeivceRelations (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS	status;
	PIO_STACK_LOCATION      irpStack;
	PDEVICE_RELATIONS       Relations = NULL;

	DebugPrint (DebugTrace, "PnpDeivceRelations");

    irpStack = IoGetCurrentIrpStackLocation (Irp);
	if (m_bPresent)
	{
		PDEVICE_RELATIONS deviceRelations;

		switch (irpStack->Parameters.QueryDeviceRelations.Type) 
		{
		case TargetDeviceRelation:  

			deviceRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information; 
			if (deviceRelations) 
			{
				//
				// Only PDO can handle this request. Somebody above
				// is not playing by rules.
				//
				DebugPrint(DebugError, "Someone above is handling TagerDeviceRelation");      
			}

			deviceRelations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag (PagedPool, sizeof(DEVICE_RELATIONS), tagUsb);
			if (!deviceRelations) 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			// 
			// There is only one PDO pointer in the structure 
			// for this relation type. The PnP Manager removes 
			// the reference to the PDO when the driver or application 
			// un-registers for notification on the device. 
			//

			deviceRelations->Count = 1;
			deviceRelations->Objects[0] = m_Self;
			ObReferenceObject(m_Self);

			status = STATUS_SUCCESS;
			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = (ULONG_PTR) deviceRelations;

			DebugPrint (DebugInfo, "TargetDeviceRelation - 0x%x", m_Self);
			break;

		case BusRelations:
			{
				if (m_pTcp)
				{
					bool bSecurity = wcslen (m_pTcp->m_szUserSid)?true:false;
					if (bSecurity && !m_pAcl)
					{
						m_pAcl = CSecurityUtils::BuildSecurityDescriptor(m_pTcp->m_sid.GetData (), m_sd);
					}
					if (bSecurity)
					{
						CSecurityUtils::SetSecurityDescriptor(m_Self, &m_sd);
					}
				}
			}
			Relations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
			if (Relations)
			{
				QueryRelations (Relations);
			}

		default:
			status = Irp->IoStatus.Status;
			break;
		}
	}
	else
	{
		if (irpStack->Parameters.QueryDeviceRelations.Type == BusRelations)
		{

			Relations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
			if (!Relations)
			{
				Relations = (PDEVICE_RELATIONS)ExAllocatePoolWithTag (PagedPool, sizeof (DEVICE_RELATIONS), 'RLR');
			}
			Relations->Count = 0;
			status = STATUS_SUCCESS;
		}
		status = STATUS_SUCCESS;
	}

	return status;
}

NTSTATUS CPdoExtension::PnpDeivceText (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS	status;
	PIO_STACK_LOCATION      irpStack;
	PVOID					pTemp;

	DebugPrint (DebugTrace, "PnpDeivceText");

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	switch (irpStack->Parameters.QueryDeviceText.DeviceTextType) 
	{
	case DeviceTextDescription:
		if (m_uQuyryDeviceText_Description)
		{
			pTemp = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_uQuyryDeviceText_Description, tagUsb);
			if (!pTemp) 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			RtlCopyMemory (pTemp, m_pQuyryDeviceText_Description, m_uQuyryDeviceText_Description);
			Irp->IoStatus.Information = (ULONG_PTR) pTemp;
			Irp->IoStatus.Status = STATUS_SUCCESS;

			DebugPrint (DebugInfo, "DeviceTextType - %S %d", pTemp, m_uQuyryDeviceText_Description);
		}
		else
		{
			status = Irp->IoStatus.Status;
		}
		break;
	case DeviceTextLocationInformation:
		if (m_uQuyryDeviceText_LocalInf)
		{
			pTemp = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_uQuyryDeviceText_LocalInf, tagUsb);
			if (!pTemp) 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			RtlCopyMemory (pTemp, m_pQuyryDeviceText_LocalInf, m_uQuyryDeviceText_LocalInf);
			Irp->IoStatus.Information = (ULONG_PTR) pTemp;
			Irp->IoStatus.Status = STATUS_SUCCESS;

			DebugPrint (DebugInfo, "DeviceTextType - %S %d", pTemp, m_uQuyryDeviceText_LocalInf);
		}
		else
		{
			status = Irp->IoStatus.Status;
		}
		break;
	default:
		status = Irp->IoStatus.Status;
		break;
	}

	return status;
}

NTSTATUS CPdoExtension::PnpQueryId (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS	status;
	PIO_STACK_LOCATION      irpStack;
	PVOID					pTemp;

	DebugPrint (DebugTrace, "PnpQueryId");

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	switch (irpStack->Parameters.QueryId.IdType) 
	{
	case BusQueryDeviceID:
		pTemp = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_uQueryId_BusQueryDeviceID, tagUsb);
		if (!pTemp) 
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlCopyMemory (pTemp, m_pQueryId_BusQueryDeviceID, m_uQueryId_BusQueryDeviceID);
		Irp->IoStatus.Information = (ULONG_PTR) pTemp;
		Irp->IoStatus.Status = STATUS_SUCCESS;

		DebugPrint (DebugInfo, "BusQueryDeviceID - %S %d", pTemp, m_uQueryId_BusQueryDeviceID);
		break;

	case BusQueryInstanceID:
		pTemp = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_uQueryId_BusQueryInstanceID, tagUsb);
		if (!pTemp) 
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlCopyMemory (pTemp, m_pQueryId_BusQueryInstanceID, m_uQueryId_BusQueryInstanceID);
		Irp->IoStatus.Information = (ULONG_PTR) pTemp;
		Irp->IoStatus.Status = STATUS_SUCCESS;
		DebugPrint (DebugInfo, "BusQueryDeviceID - %S %d", pTemp, m_uQueryId_BusQueryInstanceID);
		break;

	case BusQueryHardwareIDs:

		pTemp = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_uQueryId_BusQueryHardwareIDs, tagUsb);
		if (!pTemp) 
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlCopyMemory (pTemp, m_pQueryId_BusQueryHardwareIDs, m_uQueryId_BusQueryHardwareIDs);
		Irp->IoStatus.Information = (ULONG_PTR) pTemp;
		Irp->IoStatus.Status = STATUS_SUCCESS;
		DebugPrint (DebugInfo, "BusQueryDeviceID - %S %d", pTemp, m_uQueryId_BusQueryHardwareIDs);
		break;

	case BusQueryCompatibleIDs:

		pTemp = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_uQueryId_BusQueryCompatibleIDs, tagUsb);
		if (!pTemp) 
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlCopyMemory (pTemp, m_pQueryId_BusQueryCompatibleIDs, m_uQueryId_BusQueryCompatibleIDs);
		Irp->IoStatus.Information = (ULONG_PTR) pTemp;
		Irp->IoStatus.Status = STATUS_SUCCESS;
		DebugPrint (DebugInfo, "BusQueryDeviceID - %S %d", pTemp, m_uQueryId_BusQueryCompatibleIDs);
		break;

	default:

		status = Irp->IoStatus.Status;
	}

	return status;
}

NTSTATUS CPdoExtension::PnpRemoveDevice (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION      irpStack;
	NTSTATUS                status = STATUS_SUCCESS;
	KEVENT					*pEventRemove = NULL;

	DebugPrint (DebugTrace, "PnpRemoveDevice");

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	//
	// Check the state flag to see whether you are surprise removed
	//
	--m_irpCounterPdo;

	SetNewPnpState (DriverBase::Deleted);

	Irp->IoStatus.Status = STATUS_SUCCESS;

	//
	// Delete device
	//
	{
		TPtrPdoExtension	pPdo;
		bool				bDelet = true;

		if (m_pParent)
		{
			// Verify exist pPdo
			m_pParent->m_spinPdo.Lock ();
			for (int a = 0; a < m_pParent->m_arrPdo.GetCount (); a++)
			{
				pPdo = m_pParent->m_arrPdo [a];
				if (pPdo.get () == this)
				{
					DebugPrint(DebugSpecial, "Remove Find PDO");
					bDelet = false;
					break;
				}
			}
			m_pParent->m_spinPdo.UnLock ();
		}

		ClearLink ();

		if (m_LinkInterface.Length)
		{
			IoSetDeviceInterfaceState (&m_LinkInterface, false);
			RtlFreeUnicodeString (&m_LinkInterface);
		}

		if (bDelet)
		{
			m_bPresent = false;
			m_Self->DeviceExtension = 0;

			m_irpCounterPdo.WaitRemove ();

			if (m_pTcp)
			{
				m_pTcp->CancelIrp();

				PVOID pWorkItem = CWorkItem::Create(m_pParent->GetDeviceObject());
				CWorkItem::AddContext(pWorkItem, m_pTcp);
				CWorkItem::Run(pWorkItem, CVirtualUsbHubExtension::WorkItmDeleteUsbDev);
				m_pTcp = NULL;
			}

			DebugPrint(DebugInfo, "Deleting PDO %x", m_Self);
			IoDeleteDevice (m_Self);
			DebugPrint(DebugSpecial, "Deleted PDO");

			pEventRemove = m_pEventRomove;
			//delete this;
			DecMyself();
		}
	}

	IoCompleteRequest (Irp, IO_NO_INCREMENT);

	if (pEventRemove)
	{
		KeSetEvent(pEventRemove, IO_NO_INCREMENT, FALSE);
	}

	return status;
}

NTSTATUS CPdoExtension::PnpStartDevice (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION      irpStack;
	NTSTATUS                status = STATUS_SUCCESS;

	DebugPrint (DebugTrace, "PnpStartDevice");

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	// reg usb interface
	GUID usbInterface = {0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED};
	status = IoRegisterDeviceInterface (
		m_Self,
		(LPGUID) &usbInterface ,
		NULL,
		&m_LinkInterface);

	if (NT_SUCCESS (status))
	{
		IoSetDeviceInterfaceState (&m_LinkInterface, true);
	}
	SetNewPnpState (DriverBase::Started);

	// verify smart card reader
	{
		const WCHAR szGuidSmartReader [] = L"{50dd5230-ba8a-11d1-bf5d-0000f805f530}";
		const WCHAR szGuidDingle[] = L"{57e68e91-05a7-11d4-ade7-005004769436}";
		CBuffer	Buffer;
		ULONG	uSize = 0;

		Buffer.SetSize (1024);

		IoGetDeviceProperty (m_Self, DevicePropertyClassGuid, Buffer.GetBufferSize (),
			Buffer.GetData (), &uSize);

		m_bCardReader = (RtlCompareMemory (szGuidSmartReader, Buffer.GetData (), min (uSize, sizeof (GUID))) == sizeof (GUID));
		m_bDisableAutoComplete = (RtlCompareMemory(szGuidDingle, Buffer.GetData(), min(uSize, sizeof(GUID))) == sizeof(GUID)) ? true : m_bCardReader;
	}

	return status;
}

NTSTATUS CPdoExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status = STATUS_SUCCESS;

	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	DebugPrint (DebugTrace, "%s", PnPMinorFunctionString (Irp));
	DebugPrint(DebugSpecial, "%s %x", PnPMinorFunctionString(Irp), Irp);

    
    irpStack = IoGetCurrentIrpStackLocation (Irp);
    //
    // If the device has been removed, the driver should 
    // not pass the IRP down to the next lower driver.
    //

    if (m_DevicePnPState == DriverBase::NotStarted)
    {
        DebugPrint (DebugInfo, "Starting PNP request");
        SetNewPnpState (DriverBase::Starting);
    }

	++m_irpCounterPdo;
    
    switch (irpStack->MinorFunction) 
	{
    case IRP_MN_QUERY_DEVICE_RELATIONS:
		status = PnpDeivceRelations (DeviceObject, Irp);
		break;
	case IRP_MN_QUERY_DEVICE_TEXT:
		if (irpStack->Parameters.QueryDeviceText.LocaleId != 0x409)
		{
			if (m_bPresent)
			{
				--m_irpCounterPdo;
				return DispatchDefaultTcp (DeviceObject, Irp);
			}
			else
			{
				status = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
				IoCompleteRequest (Irp, IO_NO_INCREMENT);
				--m_irpCounterPdo;
			}
			return status;
		}
		status = PnpDeivceText (DeviceObject, Irp);
		break;
	case IRP_MN_QUERY_ID:
		status = PnpQueryId (DeviceObject, Irp);
		break;
    case IRP_MN_QUERY_REMOVE_DEVICE:

        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //

        SetNewPnpState (DriverBase::RemovePending);
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
        
    case IRP_MN_CANCEL_REMOVE_DEVICE:

        
        if(DriverBase::RemovePending == m_DevicePnPState)
        {
            //
            // We did receive a query-remove, so restore.
            //             
            RestorePnpState ();
        }
        Irp->IoStatus.Status = STATUS_SUCCESS;// You must not fail the IRP.
        break;
        
    case IRP_MN_SURPRISE_REMOVAL:


        SetNewPnpState (DriverBase::SurpriseRemovePending);

        Irp->IoStatus.Status = STATUS_SUCCESS; // You must not fail the IRP.
        break;
        
    case IRP_MN_REMOVE_DEVICE:
           
        //
        // The Plug & Play system has dictated the removal of this device.  
        // We have no choice but to detach and delete the device object.
        //
		status = PnpRemoveDevice(DeviceObject, Irp);

		return status;

    case IRP_MN_START_DEVICE:
        
        status = PnpStartDevice (DeviceObject, Irp);

    default:
		if (m_bPresent)
		{
			--m_irpCounterPdo;
			return DispatchDefaultTcp (DeviceObject, Irp);
		}
		else
		{
			status = STATUS_NO_SUCH_DEVICE;
		}
    }

	DebugPrint(DebugSpecial, "%s %x out", PnPMinorFunctionString(Irp), Irp);
	status = Irp->IoStatus.Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    --m_irpCounterPdo;
    return status;
}

NTSTATUS CPdoExtension::DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION  stack;

	DebugPrint (DebugTrace, "DispatchIntrenalDevCtrl");

	stack = IoGetCurrentIrpStackLocation (Irp);
	if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB 
		|| stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_GET_PORT_STATUS
		|| stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_RESET_PORT
		|| stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_ENABLE_PORT
		|| stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_CYCLE_PORT
		|| stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS
		|| stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION
		)
	{
		return DispatchDefaultTcp (DeviceObject, Irp);
	}

	if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME)
	{
		return GetControllerName (Irp);
	}

	if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_GET_HUB_NAME)
	{
		return GetHubName(Irp);
	}

	if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO)
	{
		++m_irpCounterPdo;
		*((PVOID*)stack->Parameters.Others.Argument1) = m_pParent->m_PhysicalDeviceObject;
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--m_irpCounterPdo;
		return STATUS_SUCCESS;
	}

	if (m_pTcp)
	{
		DebugPrint (DebugError, "Not supported - %s", m_pTcp->UsbIoControl (stack->Parameters.DeviceIoControl.IoControlCode));
		DebugPrint (DebugSpecial, "Not supported - %s", m_pTcp->UsbIoControl (stack->Parameters.DeviceIoControl.IoControlCode));
	}

    return CPdoExtension::DispatchDefault (DeviceObject, Irp);    	
}


NTSTATUS CPdoExtension::DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	
    NTSTATUS            status;
    PIO_STACK_LOCATION  stack;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

	++m_irpCounterPdo;
    stack = IoGetCurrentIrpStackLocation (Irp);
    powerType = stack->Parameters.Power.Type;
    powerState = stack->Parameters.Power.State;

	DebugPrint (DebugTrace, "%s", PowerMinorFunctionString (stack->MinorFunction));
	DebugPrint (DebugSpecial, "%s", PowerMinorFunctionString (stack->MinorFunction));

    switch (stack->MinorFunction) 
	{
    case IRP_MN_SET_POWER:
    
        DebugPrint (DebugTrace, "Setting %s power state to %s",
                     ((powerType == SystemPowerState) ?  "System" : "Device"),
                     ((powerType == SystemPowerState) ? \
                        SystemPowerString(powerState.SystemState) : \
                        DevicePowerString(powerState.DeviceState)));
                     
        switch (powerType) 
		{
            case DevicePowerState:
                PoSetPowerState (m_Self, powerType, powerState);
                status = STATUS_SUCCESS;
                break;

            case SystemPowerState:
                status = STATUS_SUCCESS;
                break;

            default:
                status = STATUS_NOT_SUPPORTED;
                break;
        }
        break;

    case IRP_MN_QUERY_POWER:
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_WAIT_WAKE:
        //
        // We cannot support wait-wake because we are root-enumerated 
        // driver, and our parent, the PnP manager, doesn't support wait-wake. 
        // If you are a bus enumerated device, and if  your parent bus supports
        // wait-wake,  you should send a wait/wake IRP (PoRequestPowerIrp)
        // in response to this request. 
        // If you want to test the wait/wake logic implemented in the function
        // driver (toaster.sys), you could do the following simulation: 
        // a) Mark this IRP pending.
        // b) Set a cancel routine.
        // c) Save this IRP in the device extension
        // d) Return STATUS_PENDING.
        // Later on if you suspend and resume your system, your BUS_FDO_POWER
        // will be called to power the bus. In response to IRP_MN_SET_POWER, if the
        // powerstate is PowerSystemWorking, complete this Wake IRP. 
        // If the function driver, decides to cancel the wake IRP, your cancel routine
        // will be called. There you just complete the IRP with STATUS_CANCELLED.
        //
    case IRP_MN_POWER_SEQUENCE:
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (status != STATUS_NOT_SUPPORTED) {

        Irp->IoStatus.Status = status;
    }

    PoStartNextPowerIrp(Irp);
    status = Irp->IoStatus.Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounterPdo;

    return status;
}

// ***********************************************************
//			Create and delete PDO
// ***********************************************************
NTSTATUS CPdoExtension::CreateDevice ()
{
    NTSTATUS            status;
    UNICODE_STRING      strDevice;
    WCHAR               szBuffer [50] = L"\\Device\\USBPDO-%lu";

	DebugPrint (DebugTrace, "CreateDevice");

    for (int a = 0; a < 100; a++)
    {
        swprintf (szBuffer, L"\\Device\\USBPDO-%lu", a);
        RtlInitUnicodeString (&strDevice, szBuffer);

		{
			status = IoCreateDevice(m_DriverObject,
			            sizeof (CPdoExtension*),
			            &strDevice,
			            FILE_DEVICE_UNKNOWN,
			            /*FILE_AUTOGENERATED_DEVICE_NAME | */FILE_DEVICE_SECURE_OPEN,
			            false,
			            &m_Self);
		}
        if (NT_SUCCESS (status))
        {
            break;
        }
    }


	if (!NT_SUCCESS (status)) 
	{
        //
        // Returning failure here prevents the entire stack from functioning,
        // but most likely the rest of the stack will not be able to create
        // device objects either, so it is still OK.
        //
		DebugPrint (DebugError, "Did not create to PDO");
        return status;
    }

    *((CPdoExtension**)m_Self->DeviceExtension) = this;
	m_bPresent = true;

    // Init power state
	m_DevicePowerState = PowerDeviceD3;
    m_SystemPowerState = PowerSystemWorking;
    // Init power for device
    m_Self->Flags |= DO_POWER_PAGABLE;
	m_Self->Flags &= ~DO_DEVICE_INITIALIZING;

	m_DeviceIndef = (ULONG64)m_Self;

	// for Bluetooth
	m_Self->StackSize++;

	return status;
}

NTSTATUS CPdoExtension::CreateDevice (PUSB_DEV	Param)
{
    NTSTATUS            status;
    UNICODE_STRING      strDevice;
    WCHAR               szBuffer [50] = L"\\Device\\USBPDO-%lu";


	DebugPrint (DebugTrace, "CreateDevice %x:%d USB port: %d", Param->HostIp, Param->ServerIpPort, Param->DeviceAddress);

	status = CreateDevice ();
	if (!NT_SUCCESS (status))
	{
		DebugPrint (DebugError, "Did not create to PDO");
		return status;
	}

	// init param from pdo
	m_HostIp = Param->HostIp;
	m_uServerPort = Param->ServerIpPort;
	m_uAddressDevice = Param->DeviceAddress;

	return status;
}

NTSTATUS CPdoExtension::DeleteDevice ()
{
	DebugPrint (DebugTrace, "DeleteDevice");

	if (m_Self)
	{
		IoDeleteDevice (m_Self);
	}
	m_Self = NULL;

	return 0;
}

NTSTATUS CPdoExtension::IoCompletionErrCancel(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context)
{
	return STATUS_SUCCESS;
}

NTSTATUS CPdoExtension::SendDate (evuh::CUsbDevToTcp  *pTcp, PVOID irpNumber, KEVENT	*Event)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	DebugPrint (DebugTrace, "SendDate");

    {
		pTcp->IrpToQuery (irpNumber);
		while (status != STATUS_SUCCESS)
		{
			LARGE_INTEGER	time;
			time.QuadPart = (LONGLONG)-1000000;		// 10 sec

			status = KeWaitForSingleObject(
						Event,
						Executive,
						KernelMode,
						FALSE,
						&time
						);
			if (!m_bPresent)
			{
				break;
			}
		}
		
    }
	return status;
}

PVOID CPdoExtension::CopyToNonPagedMemory (PVOID Dest, ULONG uSize)
{
	DebugPrint (DebugTrace, "CopyToNonPagedMemory");
	PVOID pTemp = ExAllocatePoolWithTag (NonPagedPool, uSize, 'YPC');
	RtlCopyMemory (pTemp, Dest, uSize);
	return pTemp;
}

void CPdoExtension::InitPnpIrp (PIRP pIrp, char cMajor, char cMinor)
{
	PIO_STACK_LOCATION		irpStack;

	irpStack = IoGetCurrentIrpStackLocation( pIrp );

	RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );
	irpStack->MajorFunction = cMajor;
	irpStack->MinorFunction = cMinor;
	pIrp->IoStatus.Status = 0;
	pIrp->IoStatus.Information = 0;
}

bool CPdoExtension::SendPnpIrp (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber)
{
	KeInitializeEvent( &pNumber->Event, NotificationEvent, FALSE );
	pNumber->Count = pTcp->SetNumber();

	if (!m_bPresent || !pTcp->m_bWork)
	{
		return false;
	}	    

	++m_irpCounterPdo;
	pNumber->bWait = true;
	SendDate(pTcp, pNumber, &pNumber->Event);
	--m_irpCounterPdo;

	return true;
}

bool CPdoExtension::PnpBusQueryDeviceID (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber)
{
	PIO_STACK_LOCATION		irpStack;
	irpStack = IoGetCurrentIrpStackLocation( pNumber->Irp );

	InitPnpIrp(pNumber->Irp, IRP_MJ_PNP, IRP_MN_QUERY_ID);
	irpStack->Parameters.QueryId.IdType = BusQueryDeviceID;

	if (!SendPnpIrp(pTcp, pNumber))
	{
		return false;
	}
	if (m_pQueryId_BusQueryDeviceID)
	{
		ExFreePool (m_pQueryId_BusQueryDeviceID);
		m_pQueryId_BusQueryDeviceID = NULL;
	}
	m_uQueryId_BusQueryDeviceID = 0;
	PWCHAR pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
	if (pTemp)
	{
		m_uQueryId_BusQueryDeviceID = wcslen (pTemp) + 1;
		m_uQueryId_BusQueryDeviceID = (m_uQueryId_BusQueryDeviceID + 1)*sizeof (WCHAR);
		m_pQueryId_BusQueryDeviceID = CopyToNonPagedMemory (pTemp, m_uQueryId_BusQueryDeviceID);
		ExFreePool (pTemp);
	}

	return true;
}

bool CPdoExtension::PnpBusQueryHardwareIDs (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber)
{
	PIO_STACK_LOCATION		irpStack;
	irpStack = IoGetCurrentIrpStackLocation( pNumber->Irp );

	InitPnpIrp(pNumber->Irp, IRP_MJ_PNP, IRP_MN_QUERY_ID);
	irpStack->Parameters.QueryId.IdType = BusQueryHardwareIDs;

	if (!SendPnpIrp(pTcp, pNumber))
	{
		return false;
	}

	if (m_pQueryId_BusQueryHardwareIDs)
	{
		ExFreePool (m_pQueryId_BusQueryHardwareIDs);
		m_pQueryId_BusQueryHardwareIDs = NULL;
	}
	m_uQueryId_BusQueryHardwareIDs = 0;

	PWCHAR pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
	if (pTemp)
	{
		m_pQueryId_BusQueryHardwareIDs = pTemp;

		while (wcslen (pTemp))
		{
			m_uQueryId_BusQueryHardwareIDs = m_uQueryId_BusQueryHardwareIDs + wcslen (pTemp) + 1;
			pTemp += wcslen (pTemp) + 1;
		}
		m_uQueryId_BusQueryHardwareIDs = (m_uQueryId_BusQueryHardwareIDs + 1)*sizeof (WCHAR);

		// non page pool
		m_pQueryId_BusQueryHardwareIDs = CopyToNonPagedMemory (m_pQueryId_BusQueryHardwareIDs, m_uQueryId_BusQueryHardwareIDs);
		pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
		ExFreePool (pTemp);
	}

	return true;
}

bool CPdoExtension::PnpBusQueryCompatibleIDs (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber)
{
	PIO_STACK_LOCATION		irpStack;
	irpStack = IoGetCurrentIrpStackLocation( pNumber->Irp );

	InitPnpIrp(pNumber->Irp, IRP_MJ_PNP, IRP_MN_QUERY_ID);
	irpStack->Parameters.QueryId.IdType = BusQueryCompatibleIDs;

	if (!SendPnpIrp(pTcp, pNumber))
	{
		return false;
	}

	if (m_pQueryId_BusQueryCompatibleIDs)
	{
		ExFreePool (m_pQueryId_BusQueryCompatibleIDs);
		m_pQueryId_BusQueryCompatibleIDs = NULL;
	}
	m_uQueryId_BusQueryCompatibleIDs = 0;
	PWCHAR pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
	if (pTemp)
	{
		m_pQueryId_BusQueryCompatibleIDs = pTemp;

		while (wcslen (pTemp))
		{
			m_uQueryId_BusQueryCompatibleIDs = m_uQueryId_BusQueryCompatibleIDs + wcslen (pTemp) + 1;
			pTemp += wcslen (pTemp) + 1;
		}
		m_uQueryId_BusQueryCompatibleIDs = (m_uQueryId_BusQueryCompatibleIDs + 1)*sizeof (WCHAR);

		m_pQueryId_BusQueryCompatibleIDs = CopyToNonPagedMemory (m_pQueryId_BusQueryCompatibleIDs, m_uQueryId_BusQueryCompatibleIDs);
		pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
		ExFreePool (pTemp);
	}

	return true;
}

bool CPdoExtension::PnpBusQueryInstanceID (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber)
{
	PIO_STACK_LOCATION		irpStack;
	irpStack = IoGetCurrentIrpStackLocation( pNumber->Irp );

	InitPnpIrp(pNumber->Irp, IRP_MJ_PNP, IRP_MN_QUERY_ID);
	irpStack->Parameters.QueryId.IdType = BusQueryInstanceID;

	if (!SendPnpIrp(pTcp, pNumber))
	{
		return false;
	}

	if (m_pQueryId_BusQueryInstanceID)
	{
		ExFreePool (m_pQueryId_BusQueryInstanceID);
		m_pQueryId_BusQueryInstanceID = NULL;
	}
	m_uQueryId_BusQueryInstanceID = 0;
	PWCHAR pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
	if (pTemp)
	{
		WCHAR	*pInstanceID;

		m_uQueryId_BusQueryInstanceID = (wcslen (pTemp) + 1)*sizeof (WCHAR);

		pInstanceID = (PWCHAR)CopyToNonPagedMemory (pTemp, m_uQueryId_BusQueryInstanceID);

		m_pQueryId_BusQueryInstanceID = m_pParent->CreateUnicInstanceID ((PWCHAR)m_pQueryId_BusQueryHardwareIDs, pInstanceID, m_uQueryId_BusQueryInstanceID);

		ExFreePool (pTemp);
		ExFreePool (pInstanceID);
	}
	return true;
}

bool CPdoExtension::PnpDeviceTextDescription (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber)
{
	PIO_STACK_LOCATION		irpStack;
	irpStack = IoGetCurrentIrpStackLocation( pNumber->Irp );

	InitPnpIrp(pNumber->Irp, IRP_MJ_PNP, IRP_MN_QUERY_DEVICE_TEXT);
	irpStack->Parameters.QueryDeviceText.DeviceTextType = DeviceTextDescription;
	irpStack->Parameters.QueryDeviceText.LocaleId = m_uQuyryDeviceText_uLocalId;

	if (!SendPnpIrp(pTcp, pNumber))
	{
		return false;
	}
	if (m_pQuyryDeviceText_Description)
	{
		ExFreePool (m_pQuyryDeviceText_Description);
		m_pQuyryDeviceText_Description = NULL;
	}
	m_uQuyryDeviceText_Description = 0;
	PWCHAR pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
	if (pTemp)
	{
		m_pQuyryDeviceText_Description = pTemp;
		m_uQuyryDeviceText_Description = (wcslen (pTemp) + 1)*sizeof (WCHAR);
	}

	return true;
}

bool CPdoExtension::PnpDeviceTextLocationInformation (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber)
{
	PIO_STACK_LOCATION		irpStack;
	irpStack = IoGetCurrentIrpStackLocation( pNumber->Irp );

	InitPnpIrp(pNumber->Irp, IRP_MJ_PNP, IRP_MN_QUERY_DEVICE_TEXT);
	irpStack->Parameters.QueryDeviceText.DeviceTextType = DeviceTextLocationInformation;
	irpStack->Parameters.QueryDeviceText.LocaleId = m_uQuyryDeviceText_uLocalId;

	if (!SendPnpIrp(pTcp, pNumber))
	{
		return false;
	}
	if (m_pQuyryDeviceText_LocalInf)
	{
		ExFreePool (m_pQuyryDeviceText_LocalInf);
		m_pQuyryDeviceText_LocalInf = NULL;
	}
	m_uQuyryDeviceText_LocalInf = 0;
	PWCHAR pTemp = (PWCHAR)pNumber->Irp->IoStatus.Information;
	if (pTemp)
	{
		m_pQuyryDeviceText_LocalInf = pTemp;
		m_uQuyryDeviceText_LocalInf = (wcslen (pTemp) + 1)*sizeof (WCHAR);
	}
	return true;
}

NTSTATUS CPdoExtension::InitConstParam ()
{
	NTSTATUS				status;
    PIRP					newIrp;
    IO_STATUS_BLOCK			ioStatus;
	PIO_STACK_LOCATION		irpStack;
	PIRP_NUMBER				irpNumber;
	PWCHAR					pTemp;
    KIRQL                   OldIrql;
    evuh::CUsbDevToTcp      *pTcp = m_pTcp;

	DebugPrint (DebugTrace, "InitConstParam");


	// Build Irp
    newIrp = IoAllocateIrp(2, false);

	if (newIrp == NULL || pTcp == NULL) 
	{
		DebugPrint (DebugInfo, "newIrp == %d || pTcp == %d", newIrp, pTcp);
		return STATUS_INSUFFICIENT_RESOURCES;
    }

    ++pTcp->m_WaitUse;

	DebugPrint (DebugInfo, "m_poolToTcpRead.WaitReady");

	pTcp->m_eventStart.eventStart.Wait(-1);

	if (pTcp->m_bNewDriverWork)
	{
		pTcp->m_poolReadFromService.WaitReady();
	}

	if (!pTcp->m_bWork)
	{
		DebugPrint (DebugInfo, "pTcp->m_bWork false");
		--pTcp->m_WaitUse;

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irpNumber = pTcp->NewIrpNumber (newIrp, this);
	irpNumber->Irp = newIrp;
	irpNumber->pPdo = this;

    do
    {
		// Init irp
        newIrp->CurrentLocation--;
        irpStack = IoGetNextIrpStackLocation( newIrp );
        newIrp->Tail.Overlay.CurrentStackLocation = irpStack;

		// BusQueryDeviceID
		DebugPrint (DebugInfo, "BusQueryDeviceID");
		if (!PnpBusQueryDeviceID (pTcp, irpNumber))
		{
			DebugPrint (DebugInfo, "error BusQueryDeviceID");
			break;
		}
		// BusQueryHardwareIDs
		DebugPrint (DebugInfo, "BusQueryHardwareIDs");
		if (!PnpBusQueryHardwareIDs(pTcp, irpNumber))
		{
			DebugPrint (DebugInfo, "error BusQueryHardwareIDs");
			break;
		}
		// BusQueryCompatibleIDs
		DebugPrint (DebugInfo, "BusQueryCompatibleIDs");
		if (!PnpBusQueryCompatibleIDs(pTcp, irpNumber))
		{
			DebugPrint (DebugInfo, "error BusQueryCompatibleIDs");
			break;
		}
		// PnpBusQueryInstanceID
		DebugPrint (DebugInfo, "PnpBusQueryInstanceID");
		if (!PnpBusQueryInstanceID(pTcp, irpNumber))
		{
			DebugPrint (DebugInfo, "error PnpBusQueryInstanceID");
			break;
		}
		// DeviceTextDescription
		DebugPrint (DebugInfo, "DeviceTextDescription");
		if (!PnpDeviceTextDescription (pTcp, irpNumber))
		{
			DebugPrint (DebugInfo, "error DeviceTextDescription");
			break;
		}
		// DeviceTextLocationInformation
		DebugPrint (DebugInfo, "DeviceTextLocationInformation");
		if (!PnpDeviceTextLocationInformation (pTcp, irpNumber))
		{
			DebugPrint (DebugInfo, "error DeviceTextLocationInformation");
			break;
		}

		// device ready to notice to pnp manager
		m_bInit = true;

    }while (false);

	irpNumber->Lock = 1;	// decrement count
	pTcp->DeleteIrpNumber (irpNumber);  // del

	IoFreeIrp (newIrp);

	DebugPrint (DebugInfo, "Init const is finished");


    --pTcp->m_WaitUse;

    return STATUS_SUCCESS;
}

VOID CPdoExtension::IoWorkItmInit (PDEVICE_OBJECT DeviceObject, PVOID Context)
{
	CPdoExtension*		pBase = (CPdoExtension*)CWorkItem::GetContext (Context, 0);
	
	pBase->DebugPrint (DebugInfo, "IoWorkItmInit - Ok ");

	if (NT_SUCCESS (pBase->InitConstParam ()))
	{
        if (pBase->m_pParent && pBase->m_pParent->m_pHubStub && pBase->m_bPresent)
        {
			// always run only one
			pBase->m_pParent->m_pHubStub->SetDeviceRelationsTimer ();
        }
	}

	--pBase->m_irpCounterPdo;
}

bool CPdoExtension::CancelUrb(PIRP pIrp, NTSTATUS Status/* = STATUS_CANCELLED*/, NTSTATUS UsbdStatus /*= USBD_STATUS_CANCELED*/)
{
    PIO_STACK_LOCATION  stack;
    PURB_FULL     urb;
	bool bRet = false;

    stack = IoGetCurrentIrpStackLocation (pIrp);

	IoSetCancelRoutine(pIrp, NULL);

	pIrp->IoStatus.Status = Status;
    if (stack && stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL && 
        stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        urb = (PURB_FULL)stack->Parameters.Others.Argument1;
		urb->UrbHeader.Status = UsbdStatus;//USBD_STATUS_CANCELED;
		bRet = true;
		switch (urb->UrbHeader.Function)
		{
			case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
				urb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
				urb->UrbHeader.Status = USBD_STATUS_XACT_ERROR;
				break;
			case URB_FUNCTION_CONTROL_TRANSFER:
				urb->UrbControlTransfer.TransferBufferLength = 0;
				break;
			case URB_FUNCTION_ISOCH_TRANSFER:
				urb->UrbIsochronousTransfer.TransferBufferLength = 0;
				urb->UrbHeader.Status = USBD_STATUS_ISOCH_REQUEST_FAILED;
				break;
			case URB_FUNCTION_CLASS_DEVICE:
			case URB_FUNCTION_CLASS_INTERFACE:
			case URB_FUNCTION_CLASS_ENDPOINT:
			case URB_FUNCTION_CLASS_OTHER :
			case URB_FUNCTION_VENDOR_DEVICE:
			case URB_FUNCTION_VENDOR_INTERFACE:
			case URB_FUNCTION_VENDOR_ENDPOINT:
			case URB_FUNCTION_VENDOR_OTHER:
				urb->UrbControlVendorClassRequest.TransferBufferLength = 0;
				break;
		};
    }

	return bRet;
}

TPtrPdoBase CPdoExtension::AddPdo (PDEVICE_OBJECT PhysicalDeviceObject)
{
	CFilterStub *pStub = new CFilterStub (this, PhysicalDeviceObject, this);
	TPtrPdoBase pBase (pStub);

	DebugPrint (DebugTrace, "AddPdo");

	pStub->AttachToDevice (m_DriverObject, PhysicalDeviceObject);

	SpinDr().Lock();
	pBase->IncMyself();
	GetList ().Add (pBase);
	SpinDr().UnLock();

	return pStub;
}

void CPdoExtension::AddLink (PUNICODE_STRING strDev, PUNICODE_STRING strSymboleLink)
{
	CBuffer			*pBuf;
	TSymboleLink	*pLinks;
	ULONG			uSize;

	DebugPrint (DebugTrace, "AddLink - %S/%S", strDev->Buffer, strSymboleLink->Buffer);
	DebugPrint (DebugError, "AddLink - %S/%S", strDev->Buffer, strSymboleLink->Buffer);

	uSize = sizeof (TSymboleLink) + strDev->Length + strSymboleLink->Length + 10 + sizeof (WCHAR)*2;
	pBuf = new (NonPagedPool) CBuffer (NonPagedPool, uSize);

	pLinks = (TSymboleLink*)pBuf->GetData ();
	pLinks->lOffsetDev = ((BYTE*) pBuf->GetData ()) + sizeof (TSymboleLink);
	pLinks->lOffsetLink = ((BYTE*)pBuf->GetData ()) + sizeof (TSymboleLink) + strDev->Length  + sizeof (WCHAR);

	RtlCopyMemory (	pLinks->lOffsetDev, strDev->Buffer, strDev->Length + sizeof (WCHAR));
	RtlCopyMemory (	pLinks->lOffsetLink, strSymboleLink->Buffer, strSymboleLink->Length + sizeof (WCHAR) );

	m_listLink.Add (pBuf);
}

void CPdoExtension::DelLink (PUNICODE_STRING strDev)
{
	CBuffer			*pBuf;
	TSymboleLink	*pLinks;
	BOOL			m_bFind = true;

	DebugPrint (DebugTrace, "DelLink - %S", strDev->Buffer);
	DebugPrint (DebugError, "DelLink - %S", strDev->Buffer);

	do
	{
		m_bFind = false;
		m_spin.Lock ();
		for (int a = 0; a < m_listLink.GetCount (); a++)
		{
			pBuf = (CBuffer*)m_listLink.GetItem (a);
			if (pBuf)
			{
				pLinks = (TSymboleLink*)pBuf->GetData ();

				if (_wcsicmp ((WCHAR*)pLinks->lOffsetDev, strDev->Buffer) == 0)
				{
					UNICODE_STRING		strDev;

					m_bFind = true;
					m_listLink.Remove (a);
					m_spin.UnLock ();

					RtlInitUnicodeString (&strDev, (PWCHAR)pLinks->lOffsetLink);

					IoDeleteSymbolicLink (&strDev);
					delete pBuf;

					m_spin.Lock ();

					break;
				}
			}
		}
		m_spin.UnLock ();
	}
	while (m_bFind);
}

void CPdoExtension::ClearLink ()
{
	CBuffer			strBuffer (NonPagedPool);
	CBuffer			*pBuf;
	TSymboleLink	*pLinks;

	DebugPrint (DebugTrace, "ClearLink");
	DebugPrint (DebugError, "ClearLink");

	while (m_listLink.GetCount ())
	{
		m_spin.Lock ();
		pBuf = (CBuffer*)m_listLink.GetItem (0);
		if (pBuf)
		{
			UNICODE_STRING		strDev;

			pLinks = (TSymboleLink*)pBuf->GetData ();
			strBuffer.SetSize(sizeof (WCHAR) * (wcslen ((PWCHAR)pLinks->lOffsetDev) + 30));

			wcscpy ((PWCHAR)strBuffer.GetData(), (PWCHAR)pLinks->lOffsetDev);

			RtlInitUnicodeString (&strDev, (PWCHAR)strBuffer.GetData());
			m_spin.UnLock ();


			DelLink (&strDev);
		}
		else
		{
			m_spin.UnLock ();
		}
	}
}

NTSTATUS CPdoExtension::GetControllerName (PIRP pIrp)
{
	PIO_STACK_LOCATION	irpStack;
	PUSB_HUB_NAME		pHub;
	ULONG				uSize;

	DebugPrint (DebugTrace, "GetControllerName");

	irpStack = IoGetCurrentIrpStackLocation (pIrp);

	pHub = (PUSB_HUB_NAME)irpStack->Parameters.Others.Argument1;
	uSize = ULONG ((ULONG_PTR)irpStack->Parameters.Others.Argument2);
	if (uSize == sizeof (USB_HUB_NAME))
	{
		pHub->ActualLength = wcslen (m_pParent->m_LinkNameHcd.Buffer) * sizeof (WCHAR) + 6;
	}
	else
	{
		pHub->ActualLength = wcslen (m_pParent->m_LinkNameHcd.Buffer) * sizeof (WCHAR) + 6;
		uSize = *((ULONG*)irpStack->Parameters.Others.Argument2);
		wcsncpy (pHub->HubName, m_pParent->m_LinkNameHcd.Buffer, min ((uSize - sizeof (ULONG)) / sizeof (WCHAR),pHub->ActualLength) );
	}
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS CPdoExtension::GetHubName (PIRP pIrp)
{
	PIO_STACK_LOCATION	irpStack;
	PUSB_HUB_NAME		pHub;
	ULONG				uSize;

	DebugPrint (DebugTrace, "GetHubName");

	irpStack = IoGetCurrentIrpStackLocation (pIrp);

	pHub = (PUSB_HUB_NAME)pIrp->AssociatedIrp.SystemBuffer;
	uSize = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
	pHub->ActualLength = wcslen (m_pParent->m_pHubStub->m_LinkInterface.Buffer) * sizeof (WCHAR) + 6;
	if (uSize > sizeof (USB_HUB_NAME))
	{
		wcsncpy (pHub->HubName, m_pParent->m_pHubStub->m_LinkInterface.Buffer, (uSize - sizeof (ULONG)) / sizeof (WCHAR));
		pIrp->IoStatus.Information = min (uSize, pHub->ActualLength);
	}
	else
	{
		pIrp->IoStatus.Information = sizeof (ULONG);
	}

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}