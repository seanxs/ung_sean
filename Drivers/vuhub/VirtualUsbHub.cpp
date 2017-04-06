/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    VirtualUsbHub.cpp

Abstract:

	Virtual USB host controller class.

Environment:

    Kernel mode

--*/

#include "VirtualUsbHub.h"
#include "PdoExtension.h"
#include "UsbDevToTcp.h"
#include "Core\WorkItem.h"
#include "Core\SecurityUtils.h"
#include "FilterStub.h"
#include "Insulation.h"

CVirtualUsbHubExtension::CVirtualUsbHubExtension ()
	: CBaseDeviceExtension ()
	, m_NextLowerDriver (NULL)
	, m_PhysicalDeviceObject (NULL)
	, m_lTag ('CVUH')
    , m_bLinkNameHcd (false)
	, m_pHubStub (NULL)
	, m_pControl (NULL)
	, m_bDel (false)
	, m_pRegValume (NULL)
	, m_uMaxPort (0)
	, m_arrPdo (NonPagedPool)
{
	SetDebugName ("CVirtualUsbHubExtension");
	RtlInitUnicodeString (&m_DeviceName, L"\\Device\\VEHCD");
    RtlStringCchCopyW (m_strHcd, 30, L"\\DosDevices\\HCD0");
    RtlInitUnicodeString (&m_LinkNameHcd, m_strHcd);
    m_pHubStub = new CHubStubExtension (this);
    m_pControl = new evuh::CControlDeviceExtension (this);

	KeInitializeEvent(&m_eventRemoveDev, SynchronizationEvent, TRUE);

	m_poolInsulation.Start(Insulation::ItemSymbolLynk, this);
}

// ***********************************************************
//					virtual major function
// ***********************************************************
NTSTATUS CVirtualUsbHubExtension::DispatchCreate (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "DispatchCreate");
	++m_irpCounter;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounter;
	return STATUS_SUCCESS;
}

NTSTATUS CVirtualUsbHubExtension::DispatchClose (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	USB_DEV	usbDev;
	DebugPrint (DebugTrace, "DispatchClose");

	++m_irpCounter;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounter;
	return STATUS_SUCCESS;
}

NTSTATUS CVirtualUsbHubExtension::DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "DispatchDevCtrl");

	PIO_STACK_LOCATION  irpStack;
	NTSTATUS            status = STATUS_INVALID_PARAMETER;

	++m_irpCounter;
	irpStack = IoGetCurrentIrpStackLocation (Irp);

    DebugPrint (DebugTrace, "DispatchDevCtrl %s", evuh::CUsbDevToTcp::UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode));


	switch (irpStack->Parameters.DeviceIoControl.IoControlCode) 
	{
        // hub
    case IOCTL_USB_GET_ROOT_HUB_NAME:
        {
            PUSB_HCD_DRIVERKEY_NAME RootName;
            ULONG NeedSize = wcslen (L"VHubStub")*sizeof (WCHAR) + sizeof (WCHAR) + sizeof (USB_HCD_DRIVERKEY_NAME);
            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < NeedSize && 
                irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (USB_HCD_DRIVERKEY_NAME))
            {
                RootName = (PUSB_HCD_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;
                RootName->ActualLength = NeedSize;
                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof (USB_HCD_DRIVERKEY_NAME);
            }
            else if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= NeedSize)
            {
                RootName = (PUSB_HCD_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;
                RootName->ActualLength = NeedSize;
                RtlStringCbCopyW (RootName->DriverKeyName, NeedSize, L"VHubStub");
                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = NeedSize;
            }
            break;
        }
        break;
    case IOCTL_GET_HCD_DRIVERKEY_NAME:
        {
            ULONG NeedSize = 0;
            ULONG NeedSizeStruct = 0;
            PUSB_HCD_DRIVERKEY_NAME RootName;
            IoGetDeviceProperty (m_PhysicalDeviceObject, DevicePropertyDriverKeyName, 0, NULL, &NeedSize);

            NeedSizeStruct = NeedSize + sizeof (USB_HCD_DRIVERKEY_NAME);
            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < NeedSizeStruct && 
                irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (USB_HCD_DRIVERKEY_NAME))
            {
                RootName = (PUSB_HCD_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;
                RootName->ActualLength = NeedSizeStruct;
                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof (USB_HCD_DRIVERKEY_NAME);
            }
            else if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= NeedSizeStruct)
            {
                RootName = (PUSB_HCD_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;
                RootName->ActualLength = NeedSizeStruct;
                IoGetDeviceProperty (m_PhysicalDeviceObject, DevicePropertyDriverKeyName, NeedSize, RootName->DriverKeyName, &NeedSize);
                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = NeedSizeStruct;
            }
            
            break;        
        }
        break;
	default:
        DebugPrint (DebugWarning, "IOCTL not supported");
		break;
	}

	if (STATUS_PENDING != status)
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--m_irpCounter;
	}
	return status;
}

NTSTATUS CVirtualUsbHubExtension::DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

    status = STATUS_SUCCESS;
    irpStack = IoGetCurrentIrpStackLocation (Irp);

	DebugPrint (DebugTrace, "DispatchPower - %s", PowerMinorFunctionString (irpStack->MinorFunction));
	//
    // If the device has been removed, the driver should 
    // not pass the IRP down to the next lower driver.
    //
    
    if (m_DevicePnPState == DriverBase::Deleted) 
	{
        PoStartNextPowerIrp (Irp);
        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE ;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    powerType = irpStack->Parameters.Power.Type;
    powerState = irpStack->Parameters.Power.State;

    ++m_irpCounter;
    //
    // If the device is not stated yet, just pass it down.
    //
    
    if((irpStack->MinorFunction == IRP_MN_SET_POWER) && (m_DevicePnPState != DriverBase::NotStarted)) 
	{
        DebugPrint (DebugInfo, "Request to set %s state to %s",
						((powerType == SystemPowerState) ?  "System" : "Device"),
						((powerType == SystemPowerState) ? SystemPowerString(powerState.SystemState) : DevicePowerString(powerState.DeviceState)));
    }

    PoStartNextPowerIrp (Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    status =  PoCallDriver (m_NextLowerDriver, Irp);    
    --m_irpCounter;

    return status;
}

NTSTATUS CVirtualUsbHubExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
	KEVENT					event;

    irpStack = IoGetCurrentIrpStackLocation (Irp);

	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	DebugPrint (DebugTrace, "%s", PnPMinorFunctionString (Irp));
    
    //
    // If the device has been removed, the driver should 
    // not pass the IRP down to the next lower driver.
    //
    
    ++m_irpCounter;
    
    switch (irpStack->MinorFunction) 
	{
    case IRP_MN_START_DEVICE:
		KeInitializeEvent(&event, NotificationEvent, FALSE);

		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp,
							FilterStartCompletionRoutine,
							&event,
							TRUE,
							TRUE,
							TRUE
							);

		status = IoCallDriver(m_NextLowerDriver, Irp);

		//
		// Wait for lower drivers to be done with the IRP.
		// Important thing to note here is when you allocate 
		// the memory for an event in the stack you must do a  
		// KernelMode wait instead of UserMode to prevent 
		// the stack from getting paged out.
		//
		
		if (status == STATUS_PENDING) 
		{
			KeWaitForSingleObject(&event,
									Executive,
									KernelMode,
									FALSE,
									NULL
									);
			status = Irp->IoStatus.Status;
		}

        {
            GUID hubInterface = {0x3abf6f2d, 0x71c4, 0x462a, 0x8a, 0x92, 0x1e, 0x68, 0x61, 0xe6, 0xaf, 0x27};
            status = IoRegisterDeviceInterface (
                        m_PhysicalDeviceObject,
                        (LPGUID) &hubInterface,
                        NULL,
                        &m_LinkInterface);

            if (NT_SUCCESS (status))
            {
                IoSetDeviceInterfaceState (&m_LinkInterface, true);
            }
        }

        if (m_pHubStub)
        {
            m_pHubStub->CreateDevice ();
        }

        if (m_pControl)
        {
            m_pControl->CreateDevice ();
        }

		if (NT_SUCCESS(status)) 
		{
			//
			// Initialize your device with the resources provided
			// by the PnP manager to your device.
			//
		    SetNewPnpState (DriverBase::Started);
		}

		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--m_irpCounter;

		return status;

    case IRP_MN_QUERY_STOP_DEVICE:

        //
        // The PnP manager is trying to stop the device
        // for resource rebalancing. Fail this now if you 
        // cannot stop the device in response to STOP_DEVICE.
        // 
        SetNewPnpState (DriverBase::StopPending);
        Irp->IoStatus.Status = STATUS_SUCCESS; // You must not fail the IRP.
        break;
        
    case IRP_MN_CANCEL_STOP_DEVICE:

        //
        // The PnP Manager sends this IRP, at some point after an 
        // IRP_MN_QUERY_STOP_DEVICE, to inform the drivers for a 
        // device that the device will not be stopped for 
        // resource reconfiguration.
        //
        //
        // First check to see whether you have received cancel-stop
        // without first receiving a query-stop. This could happen if
        //  someone above us fails a query-stop and passes down the subsequent
        // cancel-stop.
        //
        
        if(DriverBase::StopPending == m_DevicePnPState)
        {
            //
            // We did receive a query-stop, so restore.
            //             
            RestorePnpState ();
        }
        Irp->IoStatus.Status = STATUS_SUCCESS; // We must not fail the IRP.
        break;
        
    case IRP_MN_STOP_DEVICE:
    
        //
        // Stop device means that the resources given during Start device
        // are now revoked. Note: You must not fail this Irp.
        // But before you relieve resources make sure there are no I/O in 
        // progress. Wait for the existing ones to be finished.
        // To do that, first we will decrement this very operation.
        // When the counter goes to 1, Stop event is set.
        //

        --m_irpCounter;

		m_irpCounter.WaitStop ();
        //
        // Increment the counter back because this IRP has to
        // be sent down to the lower stack.
        //
        
        ++m_irpCounter;

        SetNewPnpState (DriverBase::Stopped);

        // Set the current stack location to the next stack location and
        // call the next device object.
        //
        
        Irp->IoStatus.Status = STATUS_SUCCESS;
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

        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //
        
        //
        // First check to see whether you have received cancel-remove
        // without first receiving a query-remove. This could happen if 
        // someone above us fails a query-remove and passes down the 
        // subsequent cancel-remove.
        //
        
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

        //
        // The device has been unexpectedly removed from the machine 
        // and is no longer available for I/O. Bus_RemoveFdo clears
        // all the resources, frees the interface and de-registers
        // with WMI, but it doesn't delete the FDO. That's done
        // later in Remove device query.
        //

        SetNewPnpState (DriverBase::SurpriseRemovePending);
        IoDeleteSymbolicLink  (&m_LinkNameHcd);
		//m_pHubStub->m_bDel = true;

        Irp->IoStatus.Status = STATUS_SUCCESS; // You must not fail the IRP.
        break;
        
    case IRP_MN_REMOVE_DEVICE:
           
        //
        // The Plug & Play system has dictated the removal of this device.  
        // We have no choice but to detach and delete the device object.
        //

        //
        // Check the state flag to see whether you are surprise removed
        //

        if(m_DevicePnPState != DriverBase::SurpriseRemovePending)
        {
            IoDeleteSymbolicLink  (&m_LinkNameHcd);

			if (m_pRegValume)
			{
				IoUnregisterPlugPlayNotification (m_pRegValume);
			}
        }

        SetNewPnpState (DriverBase::Deleted);

        //
        // Wait for all outstanding requests to complete.
        // We need two decrements here, one for the increment in 
        // the beginning of this function, the other for the 1-biased value of
        // OutstandingIO.
        // 

		if (m_bDel)
		{
			--m_irpCounter;
			--m_irpCounter;
			m_irpCounter.WaitRemove ();
		}
		else
		{
			--m_irpCounter;
			m_irpCounter.WaitStop ();
		}

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (m_NextLowerDriver, Irp);
        //
        // Detach from the underlying devices.
        //
		if (m_LinkInterface.Length)
		{
			IoSetDeviceInterfaceState (&m_LinkInterface, false);
			RtlFreeUnicodeString (&m_LinkInterface);
		}
		IoDeleteSymbolicLink(&m_LinkNameHcd);

		IoDetachDevice (m_NextLowerDriver);
		m_Self->DeviceExtension = 0;

		IoDeleteDevice (m_Self);
		DebugPrint(DebugInfo, "Deleting Virtual Usb Hub");

		// delete hub
		{
			if (m_pHubStub && m_pHubStub->m_DevicePnPState == DriverBase::Deleted)
			{
				IoDeleteSymbolicLink (&m_pHubStub->m_LinkName);
				m_pHubStub->m_Self->DeviceExtension = 0;
				IoDeleteDevice (m_pHubStub->m_Self);
				m_pHubStub->m_Self = NULL;

				delete m_pHubStub;
				m_pHubStub = NULL;
			}
			if (m_pControl && m_pControl->m_DevicePnPState == DriverBase::Deleted)
			{
				IoDeleteSymbolicLink (&m_pControl->m_LinkName);
				m_pControl->m_Self->DeviceExtension = 0;
				IoDeleteDevice (m_pControl->m_Self);
				m_pControl->m_Self = NULL;

				delete m_pControl;
				m_pControl = NULL;
			}
		}
		delete this;

        return status;
        
    case IRP_MN_QUERY_DEVICE_RELATIONS:

        if (BusRelations != irpStack->Parameters.QueryDeviceRelations.Type) 
		{
            //
            // We don't support any other Device Relations
            //
            break;
        }

		status = RelationsPdos (Irp);

 
        break;

    case IRP_MN_QUERY_INTERFACE:
        DebugPrint (DebugWarning, "DispatchPnp - no support");
        break;

    default:

        //
        // In the default case we merely call the next driver.
        // We must not modify Irp->IoStatus.Status or complete the IRP.
        //
        
        break;
    }

    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (m_NextLowerDriver, Irp);
    --m_irpCounter;
    return status;
}

// default DispatchFunction 
NTSTATUS CVirtualUsbHubExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "DispatchDefault");

	++m_irpCounter;
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounter;
	return STATUS_NOT_SUPPORTED;
}



//***************************************************************************************
//				completion routine
//***************************************************************************************
NTSTATUS CVirtualUsbHubExtension::FilterStartCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PKEVENT					event = (PKEVENT)Context;
    CVirtualUsbHubExtension		*pBase = (CVirtualUsbHubExtension*)GetExtensionClass (DeviceObject);


    UNREFERENCED_PARAMETER (DeviceObject);

    pBase->DebugPrint(DebugTrace, "FilterStartCompletionRoutine");
    //
    // If the lower driver didn't return STATUS_PENDING, we don't need to 
    // set the event because we won't be waiting on it. 
    // This optimization avoids grabbing the dispatcher lock, and improves perf.
    //
    if (Irp->PendingReturned == TRUE) 
	{
        KeSetEvent (event, IO_NO_INCREMENT, FALSE);
    }
    //
    // The dispatch routine will have to call IoCompleteRequest
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

}

//***************************************************************************************
//						AttachToDevice 
//***************************************************************************************
NTSTATUS CVirtualUsbHubExtension::AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS                status = STATUS_SUCCESS;

	DebugPrint (DebugTrace, "AttachToDevice");
	//
    // Create a filter device object.
    //
    status = IoCreateDevice (DriverObject,
                             sizeof (CVirtualUsbHubExtension*),
                             &m_DeviceName,  // No Name
                             FILE_DEVICE_UNKNOWN,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &m_Self);


    if (!NT_SUCCESS (status)) 
	{
        //
        // Returning failure here prevents the entire stack from functioning,
        // but most likely the rest of the stack will not be able to create
        // device objects either, so it is still OK.
        //
		DebugPrint (DebugError, "Could not create FDO");
        return status;
    }

    DebugPrint (DebugTrace, "AddDevice PDO (0x%x) FDO (0x%x)",
                    PhysicalDeviceObject, m_Self);

    *((CVirtualUsbHubExtension**)m_Self->DeviceExtension) = this;

	m_NextLowerDriver = IoAttachDeviceToDeviceStack (
									m_Self,
									PhysicalDeviceObject);
    //
    // Failure for attachment is an indication of a broken plug & play system.
    //

    if(NULL == m_NextLowerDriver) 
	{
		DebugPrint (DebugError, "Did not Attach to PDO: 0x%x", PhysicalDeviceObject);
        IoDeleteDevice(m_Self);
		m_Self = NULL;
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Set the initial state of the Filter DO
    //
    m_DevicePnPState =  DriverBase::NotStarted;
    m_PreviousPnPState = DriverBase::NotStarted;

    DebugPrint(DebugTrace, "AddDevice: %x to %x->%x", m_Self,
                       m_NextLowerDriver,
                       PhysicalDeviceObject);

    m_Self->Flags &= ~DO_DEVICE_INITIALIZING;
	m_PhysicalDeviceObject = PhysicalDeviceObject;

    return STATUS_SUCCESS;
}

// **************************************************************************
//						work with pdo
// **************************************************************************
NTSTATUS CVirtualUsbHubExtension::RelationsPdos (PIRP Irp)
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status = STATUS_SUCCESS;;
    PDEVICE_RELATIONS		relations, oldRelations;
    ULONG					length, prevCount, numPdosPresent;
	CPdoExtension			*pPdo;
    KIRQL                   OldIrql;

    irpStack = IoGetCurrentIrpStackLocation (Irp);

	DebugPrint (DebugTrace, "RelationsPdos");

    oldRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information;
    if (oldRelations) 
	{
        prevCount = oldRelations->Count; 
    }
    else  
	{
        prevCount = 0;
    }

    numPdosPresent = (m_pHubStub && m_pHubStub->m_Self)?1:0;
    numPdosPresent = (m_pControl && m_pControl->m_Self)?(numPdosPresent+1):numPdosPresent;
    
    length = sizeof(DEVICE_RELATIONS) +
            ((numPdosPresent + prevCount) * sizeof (PDEVICE_OBJECT));

    relations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag (PagedPool, length, m_lTag);

    if (NULL == relations) 
	{
        //
        // Failed the IRP
        //
        Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    //
    // Copy in the already existing device objects
    //
    if (prevCount) 
	{
        RtlCopyMemory (relations->Objects, oldRelations->Objects, prevCount * sizeof (PDEVICE_OBJECT));
    }
    
    relations->Count = prevCount + numPdosPresent;

    //
    // For each PDO present on this bus add a pointer to the device relations
    // buffer, being sure to take out a reference to that object.
    // The Plug & Play system will dereference the object when it is done 
    // with it and free the device relations buffer.
    if (m_pHubStub && m_pHubStub->m_Self)
    {
        relations->Objects[prevCount] = m_pHubStub->m_Self;
        ObReferenceObject (m_pHubStub->m_Self);
        prevCount++;
    }
    if (m_pControl && m_pControl->m_Self)
    {
        relations->Objects[prevCount] = m_pControl->m_Self;
		ObReferenceObject (m_pControl->m_Self);
        prevCount++;
    }

	DebugPrint (DebugInfo, "PDOs present - %d, PDOs reported - %d", m_arrPdo.GetCount (), relations->Count);
    
    //
    // Replace the relations structure in the IRP with the new
    // one.
    //
    if (oldRelations) 
	{
        ExFreePool (oldRelations);
    }
    Irp->IoStatus.Information = (ULONG_PTR) relations;


    Irp->IoStatus.Status = STATUS_SUCCESS;
	return status;
}

void CVirtualUsbHubExtension::SetUsbPort (CPdoExtension *pPdo)
{
	ULONG uPort = 1;
	TPtrPdoExtension	pPdoDev;

	DebugPrint (DebugTrace, "SetUsbPort");

	do 
	{
		for (int a = 0; a < m_arrPdo.GetCount(); a++)
		{
			pPdoDev = m_arrPdo [a];

			if (pPdoDev->m_uNumberPort == uPort)
			{
				++uPort;
				continue;
			}
		}
	} while (false);

	if (m_uMaxPort < uPort)
	{
		m_uMaxPort = uPort;
	}
	pPdo->m_uNumberPort = uPort;
}

NTSTATUS CVirtualUsbHubExtension::AddNewPdo (PIRP Irp, PUSB_DEV NewDev)
{
	TPtrPdoExtension	pPdo;
	NTSTATUS        status = STATUS_SUCCESS;

	CAutoLock lock(&m_lockCreateDel);

	DebugPrint (DebugTrace, "AddNewPdo %x:%d USB port %d", NewDev->HostIp, NewDev->ServerIpPort, NewDev->DeviceAddress);
	DebugPrint (DebugSpecial, "AddNewPdo %x:%d USB port %d", NewDev->HostIp, NewDev->ServerIpPort, NewDev->DeviceAddress);

    if (!m_bLinkNameHcd)
    {
        WCHAR *pChar = m_LinkNameHcd.Buffer + wcslen (m_LinkNameHcd.Buffer) - 1;
        for (int a = 0; a < 10; a++)
        {
            *pChar = L'0' + a;
	        if (NT_SUCCESS (IoCreateSymbolicLink (&m_LinkNameHcd, &m_DeviceName)))
	        {
                break;
	        }
        }
        m_bLinkNameHcd = true;
    }

	if (Irp->Tail.Overlay.OriginalFileObject->FsContext != 0)
	{
		return STATUS_INVALID_PARAMETER;
	}

	m_spinPdo.Lock ();
	// verify that this PDO does not exist
	pPdo = FindPdo (NewDev);
	if (pPdo.get ())
	{
		m_spinPdo.UnLock ();
		DebugPrint (DebugError, "PDO with parameters: %x:%d USB port %d already exists", NewDev->HostIp, NewDev->ServerIpPort, NewDev->DeviceAddress);
		return STATUS_INVALID_PARAMETER;
	}
	m_spinPdo.UnLock ();

	pPdo = new CPdoExtension ();
	if (NT_SUCCESS (status = pPdo->CreateDevice (NewDev)))
	{
		m_spinPdo.Lock();
		SetUsbPort (pPdo.get ());

		pPdo->IncMyself();
		m_arrPdo.Add (pPdo);
		m_spinPdo.UnLock();

		pPdo->m_pParent = this;

		// Work item
		{
			PVOID pWorkItem;
			//pWorkItem = CWorkItem::Create (pPdo->m_Self);
			pWorkItem = CWorkItem::Create (m_Self);
			if (!pWorkItem)
			{
				return STATUS_INVALID_PARAMETER;
			}
			CWorkItem::AddContext (pWorkItem, pPdo.get ());

			// init tcp wrapper
			pPdo->m_pTcp = new evuh::CUsbDevToTcp (this);
			pPdo->m_pTcp->m_pPdo = pPdo.get ();
			++pPdo->m_irpCounterPdo;

			CWorkItem::Run (pWorkItem, CPdoExtension::IoWorkItmInit);
		}
	}
	Irp->Tail.Overlay.OriginalFileObject->FsContext = pPdo.get ();
	return status;
}

NTSTATUS CVirtualUsbHubExtension::RemovePdo (PIRP Irp, PUSB_DEV RemoveDev)
{
	TPtrPdoExtension pPdo;
    evuh::CUsbDevToTcp    *pUsb = NULL;
	LARGE_INTEGER	DueTime = {-60000 * 1000 * 10, -1};
	NTSTATUS		Status;

	CAutoLock lock(&m_lockCreateDel);

	DebugPrint (DebugTrace, "RemovePdo %x:%d USB port %d", RemoveDev->HostIp, RemoveDev->ServerIpPort, RemoveDev->DeviceAddress);
	DebugPrint (DebugSpecial, "RemovePdo %x:%d USB port %d", RemoveDev->HostIp, RemoveDev->ServerIpPort, RemoveDev->DeviceAddress);

	m_spinPdo.Lock ();
	pPdo = FindPdo (RemoveDev);
	if (pPdo.get ())
	{
		pPdo->m_bPresent = false;
	}
	m_spinPdo.UnLock ();

	Status = KeWaitForSingleObject (&m_eventRemoveDev, Executive, KernelMode, FALSE, &DueTime);

	m_spinPdo.Lock ();
	// verify this pdo is not exist
	pPdo = FindPdo (RemoveDev);
	if (pPdo.get ())
	{
		if (Irp->Tail.Overlay.OriginalFileObject->FsContext == pPdo)
		{

			DebugPrint(DebugSpecial, "RemovePdo - Found PDO 0x%x", pPdo->m_Self);

			Irp->Tail.Overlay.OriginalFileObject->FsContext = NULL;
			pUsb = pPdo->m_pTcp;
			pUsb->SetEventStart ();

			++pUsb->m_WaitUse;
			pPdo->m_bPresent = false;
			pUsb->m_bWork = false;
			RemovePdo(pPdo);
			m_spinPdo.UnLock();

			// set PowerDeviceD3
			{
				POWER_STATE state;
				state.DeviceState = PowerDeviceD3;

				PoSetPowerState(m_Self, DevicePowerState, state);
			}


			{
				// time to complete all processing
				LARGE_INTEGER time;
				time.QuadPart = (LONGLONG)-1000 * 10 * 10;
				KeDelayExecutionThread(KernelMode, FALSE, &time);
			}


            --pUsb->m_WaitUse;

			DebugPrint(DebugSpecial, "RemovePdo - Cancaling");
			pUsb->SetDebugLevel (DebugAll);
			pPdo->SetDebugLevel (DebugAll);

            pUsb->CancelIrp ();
			DebugPrint(DebugSpecial, "RemovePdo - Cancaled");

            // Device did not start
            if (pPdo->m_DevicePnPState == DriverBase::NotStarted)
            {
				DebugPrint(DebugInfo, "Deleting PDO no started");
				pPdo->m_Self->DeviceExtension = 0;
			    pPdo->m_irpCounterPdo.WaitRemove ();
			    IoDeleteDevice (pPdo->m_Self);
			    DebugPrint(DebugInfo, "Deleted PDO");
				KeSetEvent (&m_eventRemoveDev, IO_NO_INCREMENT, FALSE);
            }
            else
            {
				DebugPrint (DebugSpecial, "RemovePdo delay started");
				pPdo->m_pEventRomove = &m_eventRemoveDev;
				DebugPrint (DebugSpecial, "RemovePdo wait starting");
				for (int a = 0; a < 10; a++)
				{
					if (pPdo->m_DevicePnPState == DriverBase::Starting)
					{
						LARGE_INTEGER time;
						time.QuadPart = (LONGLONG)-5000000;
						KeDelayExecutionThread (KernelMode, FALSE, &time);
						DebugPrint (DebugSpecial, "RemovePdo delay");
					}
					else
					{
						break;
					}
				}
                if (m_pHubStub)
                {
					m_pHubStub->SetDeviceRelationsTimer ();
				}
            }

			DebugPrint(DebugSpecial, "Deleting PDO ok");
			return STATUS_SUCCESS;
		}
		else
		{
			DebugPrint (DebugError, "FsContext != pPdo, 0x%x != 0x%x", Irp->Tail.Overlay.OriginalFileObject->FsContext, pPdo);
			m_spinPdo.UnLock ();
            return STATUS_INVALID_PARAMETER;
		}
	}
	m_spinPdo.UnLock ();

	KeSetEvent (&m_eventRemoveDev, IO_NO_INCREMENT, FALSE);

	DebugPrint (DebugError, "Could not find PDO with parameters: %x:%d USB port %d", RemoveDev->HostIp, RemoveDev->ServerIpPort, RemoveDev->DeviceAddress);
	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CVirtualUsbHubExtension::GetIrpToTcp (PIRP Irp)
{
	CPdoExtension	*pPdo;
    evuh::CUsbDevToTcp    *pTcp;
	bool			bPanding;

	DebugPrint (DebugTrace, "GetIrpToTcp");

	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo && pPdo->m_bPresent)
	{
        pTcp = pPdo->m_pTcp;
        if (pTcp)
        {
            ++pTcp->m_WaitUse;
			pTcp->SetEventStart ();
			bPanding = !pTcp->GetIrpToRing3 (Irp);
            --pTcp->m_WaitUse;
        }
		if (!bPanding)
		{
			return STATUS_SUCCESS;
		}
		return STATUS_PENDING;
	}
	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CVirtualUsbHubExtension::SetUserSid (PIRP Irp, PUSB_DEV_SHARE pPref)
{
	CPdoExtension	*pPdo;
	evuh::CUsbDevToTcp    *pTcp;
	bool			bPanding;

	DebugPrint (DebugTrace, "SetUserSid");

	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo && pPdo->m_bPresent)
	{
		pTcp = pPdo->m_pTcp;
		if (pTcp)
		{
			RtlStringCbPrintfW (pTcp->m_szUserSid, sizeof (pTcp->m_szUserSid), L"D:P(A;;GA;;;SY)(A;;GR;;;%s)", pPref->HubDriverKeyName);

			{
				LPBYTE lpSid = LPBYTE (pPref->HubDriverKeyName) + (wcslen (pPref->HubDriverKeyName) + 1) * sizeof (WCHAR);
				if (RtlValidSid (lpSid))
				{
					pTcp->m_sid.SetSize(RtlLengthSid(lpSid));
					RtlCopySid(RtlLengthSid(lpSid), pTcp->m_sid.GetData(), lpSid);

					bool bSecurity = wcslen (pTcp->m_szUserSid)?true:false;
					if (bSecurity && !pTcp->m_pPdo->m_pAcl)
					{
						pTcp->m_pPdo->m_pAcl = CSecurityUtils::BuildSecurityDescriptor(pTcp->m_sid.GetData (), pTcp->m_pPdo->m_sd);
					}
					if (bSecurity && pTcp->m_pPdo)
					{
						CSecurityUtils::SetSecurityDescriptor(pTcp->m_pPdo->m_Self, &pTcp->m_pPdo->m_sd);
					}
				}
			}
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_PARAMETER;
}
NTSTATUS CVirtualUsbHubExtension::SetDosDevPref (PIRP Irp, PUSB_DEV_SHARE pPref)
{
	CPdoExtension	*pPdo;
    evuh::CUsbDevToTcp    *pTcp;
	bool			bPanding;

	DebugPrint (DebugTrace, "SetDosDevPref");

	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo && pPdo->m_bPresent)
	{
        pTcp = pPdo->m_pTcp;
		if (pTcp)
		{
			RtlStringCchCopyW (pTcp->m_szDosDeviceUser, sizeof (pTcp->m_szDosDeviceUser), pPref->HubDriverKeyName);
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CVirtualUsbHubExtension::AnswerTcpToIrp (PIRP Irp)
{
    NTSTATUS status;
	CPdoExtension	*pPdo;
    evuh::CUsbDevToTcp    *pTcp;

	DebugPrint (DebugTrace, "AnswerTcpToIrp");

	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo && pPdo->m_bPresent)
	{
        pTcp = pPdo->m_pTcp;
        if (pTcp)
        {
            ++pTcp->m_WaitUse;
			pTcp->SetEventStart ();
            status = pTcp->InCompleteIrp (Irp);
            --pTcp->m_WaitUse;
		    return status;
        }
	}

	return STATUS_INVALID_PARAMETER;
}

// wmi
NTSTATUS CVirtualUsbHubExtension::DispatchSysCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS    status;

    DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));

    ++m_irpCounter;

    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (m_NextLowerDriver, Irp);

    --m_irpCounter;
    return status;
}


PVOID CVirtualUsbHubExtension::CreateUnicInstanceID (WCHAR *szHardwareId, WCHAR *szInstanceID, ULONG &uSize)
{
	PWCHAR pTemp = (PWCHAR)ExAllocatePoolWithTag (NonPagedPool, uSize, tagUsb);
	TPtrPdoExtension pPdo;

	DebugPrint (DebugTrace, "CreateUnicInstanceID");
	DebugPrint (DebugInfo, "CreateUnicInstanceID HwId=%S InstId=%S", szHardwareId, szInstanceID);

	RtlCopyMemory (pTemp, szInstanceID, uSize);

	m_spinPdo.Lock ();
	for (int a = 0; a < m_arrPdo.GetCount (); a++)
	{
		pPdo = m_arrPdo [a];
		if (pPdo->m_pQueryId_BusQueryHardwareIDs && (wcscmp ((PWCHAR)pPdo->m_pQueryId_BusQueryHardwareIDs, szHardwareId) == 0))
		{
			if (pPdo->m_pQueryId_BusQueryInstanceID && (wcscmp ((PWCHAR)pPdo->m_pQueryId_BusQueryInstanceID, szInstanceID) == 0))
			{
				PWCHAR pTemp2;
				m_spinPdo.UnLock ();
				uSize += 2;

				pTemp2 = (PWCHAR)ExAllocatePoolWithTag (PagedPool, uSize, tagUsb);
				RtlCopyMemory (pTemp2, szInstanceID, uSize);
				RtlStringCbCatW (pTemp2, uSize, L"1");

				ExFreePool (pTemp);

				pTemp = (PWCHAR)CreateUnicInstanceID (szHardwareId, pTemp2, uSize);
				ExFreePool (pTemp2);

				return pTemp;
			}
		}
	}
	m_spinPdo.UnLock ();

	return pTemp;
}

NTSTATUS CVirtualUsbHubExtension::DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION  irpStack;
	NTSTATUS            status = STATUS_INVALID_PARAMETER;

	DebugPrint (DebugTrace, "DispatchIntrenalDevCtrl");

	irpStack = IoGetCurrentIrpStackLocation (Irp);

    DebugPrint (DebugSpecial, "DispatchIntrenalDevCtrl %s", evuh::CUsbDevToTcp::UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode));

	return DispatchDefault (DeviceObject, Irp);
}

bool CVirtualUsbHubExtension::RemovePdo (TPtrPdoExtension pDev)
{
	TPtrPdoExtension pPdo = NULL;

	DebugPrint (DebugTrace, "FindPdo");

	for (int a = 0; a < m_arrPdo.GetCount (); a++)
	{
		pPdo = m_arrPdo [a];
		if (pPdo.get() == pDev.get ())
		{
			m_arrPdo.Remove(a);
			return true;
		}
	}

	return false;
}

TPtrPdoExtension CVirtualUsbHubExtension::FindPdo (PUSB_DEV Dev)
{
	TPtrPdoExtension pPdo = NULL;

	DebugPrint (DebugTrace, "FindPdo");

	for (int a = 0; a < m_arrPdo.GetCount (); a++)
	{
		pPdo = m_arrPdo [a];
		if (Dev->HostIp == pPdo->m_HostIp &&
			Dev->DeviceAddress == pPdo->m_uAddressDevice &&
			Dev->ServerIpPort == pPdo->m_uServerPort)
		{
			return pPdo;
		}
	}

	return NULL;
}

VOID CVirtualUsbHubExtension::WorkItmDeleteUsbDev (PDEVICE_OBJECT DeviceObject, PVOID Context)
{
	evuh::CUsbDevToTcp	*pTcp = (evuh::CUsbDevToTcp*)CWorkItem::GetContext (Context, 0);

	LARGE_INTEGER time;

	pTcp->DebugPrint (DebugTrace, "WorkItmDeleteUsbDev");

	pTcp->DebugPrint (DebugSpecial, "WorkItmDeleteUsbDev delay %x", pTcp);

	time.QuadPart = (LONGLONG)-50000000;
	KeDelayExecutionThread (KernelMode, FALSE, &time);

	pTcp->StopAllThread ();
	delete pTcp;
}

evuh::CUsbDevToTcp* CVirtualUsbHubExtension::FindTcp (PIRP pIrp)
{
	evuh::CUsbDevToTcp		*pRet = NULL;
	TPtrPdoExtension	pPdo;

	DebugPrint (DebugTrace, "FindTcp");

	m_spinPdo.Lock();
	for (int a = 0; a < m_arrPdo.GetCount(); a++)
	{
		pPdo = m_arrPdo [a];
		if (pPdo.get () && pPdo->m_pTcp)
		{
			pPdo->m_pTcp->m_spinUsbOut.Lock ();
			for (int a = 0; a < pPdo->m_pTcp->m_listIrpAnswer.GetCount (); a++)
			{
				PIRP_NUMBER irpNumber = (PIRP_NUMBER)pPdo->m_pTcp->m_listIrpAnswer.GetItem (a);
				if (irpNumber->Irp == pIrp)
				{
					pRet = pPdo->m_pTcp;
					break;
				}
			}
			pPdo->m_pTcp->m_spinUsbOut.UnLock();
		}
		if (pRet)
		{
			break;
		}
	}
	m_spinPdo.UnLock();

	return pRet;
}

NTSTATUS CVirtualUsbHubExtension::SetPoolTcp (PIRP Irp)
{
	CPdoExtension	*pPdo;
	evuh::CUsbDevToTcp    *pTcp;
	PIO_STACK_LOCATION  irpStack;

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo && pPdo->m_bPresent)
	{
		pTcp = pPdo->m_pTcp;

		pTcp->m_bNewDriverWork = true;
		CPoolListDrv *pPool = (irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_VUHUB_IRP_TO_TCP_SET)?&pTcp->m_poolReadFromService:&pTcp->m_poolWriteToDriver;

		//return SetPool (Irp, );
		if (!pPool->GetRawData() && irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (PVOID))
		{
			*((PVOID*)Irp->UserBuffer) = pPool->Init(irpStack->Parameters.DeviceIoControl.Type3InputBuffer);
			if (*((PVOID*)Irp->UserBuffer))
			{
				pTcp->StartPool(irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_VUHUB_IRP_TO_TCP_SET);
				return STATUS_SUCCESS;
			}
		}
	}
	return STATUS_INVALID_PARAMETER;
}

NTSTATUS CVirtualUsbHubExtension::ClrPoolTcp (PIRP Irp)
{
	CPdoExtension			*pPdo;
	evuh::CUsbDevToTcp		*pTcp;
	PIO_STACK_LOCATION		irpStack;

	irpStack = IoGetCurrentIrpStackLocation (Irp);
	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo && pPdo->m_bPresent)
	{
		pTcp = pPdo->m_pTcp;
		CPoolListDrv *pPool = (irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_VUHUB_IRP_TO_TCP_SET)?&pTcp->m_poolReadFromService:&pTcp->m_poolWriteToDriver;
		if (pPool->GetRawData())
		{
			pPool->Clear();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_PARAMETER;
}