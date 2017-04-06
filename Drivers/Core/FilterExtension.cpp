/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   FilterExtension.cpp

Abstract:

	Basic class of device filter

Environment:

    Kernel mode

--*/
#include "FilterExtension.h"

// usb
#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)
#include "usbdi.h"
#include "usbdlib.h"
#include "usbioctl.h"
#include "usbbusif.h"
#include "usbioctl.h"
#pragma pack(pop)
}


CFilterExtension::CFilterExtension () 
	: CBaseDeviceExtension ()
	, m_NextLowerDriver(NULL)
{
	SetDebugName ("CFilterExtension");
}

CFilterExtension::~CFilterExtension ()
{
}
//***************************************************************************************
//				Dispatch functions
//***************************************************************************************
//	Power
NTSTATUS CFilterExtension::DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS    status;
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));

	++m_irpCounter;

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    status = PoCallDriver(m_NextLowerDriver, Irp);

	--m_irpCounter;

    return status;
}

//	Pnp
NTSTATUS CFilterExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION		irpStack;
    NTSTATUS				status;
    KEVENT					event;

	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	DebugPrint (DebugTrace, "%s", PnPMinorFunctionString (Irp));

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	++m_irpCounter;

	switch (irpStack->MinorFunction) 
	{
	case IRP_MN_START_DEVICE:

		//
		// The device is starting.
		// We cannot touch the device (send it any non pnp irps) until a
		// start device has been passed down to the lower drivers.
		//
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp,
								(PIO_COMPLETION_ROUTINE) FilterStartCompletionRoutine,
								&event,
								TRUE,
								TRUE,
								TRUE);

		status = IoCallDriver(m_NextLowerDriver, Irp);
	    
		//
		// Wait for lower drivers to be done with the IRP. Important thing to
		// note here is when you allocate memory for an event in the stack  
		// you must do a KernelMode wait instead of UserMode to prevent 
		// the stack from getting paged out.
		//
		if (status == STATUS_PENDING) 
		{

			KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);          
			status = Irp->IoStatus.Status;
		}

		if (NT_SUCCESS (status)) 
		{

			//
			// As we are successfully back now, we will
			// first set our state to Started.
			//

			SetNewPnpState(DriverBase::Started);

			//
			// On the way up inherit FILE_REMOVABLE_MEDIA during Start.
			// This characteristic is available only after the driver stack is started!.
			//
			if (m_NextLowerDriver->Characteristics & FILE_REMOVABLE_MEDIA) 
			{
				DeviceObject->Characteristics |= FILE_REMOVABLE_MEDIA;
			}
		}
	    
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--m_irpCounter;
		return status;

	case IRP_MN_REMOVE_DEVICE:

		//
		// Wait for all outstanding requests to complete
		//
		DebugPrint(DebugInfo,"Waiting for outstanding requests");
		--m_irpCounter;
		m_irpCounter.WaitRemove ();

		IoSkipCurrentIrpStackLocation(Irp);

		status = IoCallDriver(m_NextLowerDriver, Irp);

		SetNewPnpState (DriverBase::Deleted);
	    
		*((CFilterExtension**)m_Self->DeviceExtension) = NULL;

		IoDetachDevice(m_NextLowerDriver);
		IoDeleteDevice(DeviceObject);
		delete this;

		return status;


	case IRP_MN_QUERY_STOP_DEVICE:
		SetNewPnpState (DriverBase::StopPending);
		status = STATUS_SUCCESS;
		break;

	case IRP_MN_CANCEL_STOP_DEVICE:

		//
		// Check to see whether you have received cancel-stop
		// without first receiving a query-stop. This could happen if someone
		// above us fails a query-stop and passes down the subsequent
		// cancel-stop.
		//

		if(DriverBase::StopPending == m_DevicePnPState)
		{
			//
			// We did receive a query-stop, so restore.
			//
			RestorePnpState ();
		}
		status = STATUS_SUCCESS; // We must not fail this IRP.
		break;

	case IRP_MN_STOP_DEVICE:
		SetNewPnpState (DriverBase::Stopped);
		status = STATUS_SUCCESS;
		break;

	case IRP_MN_QUERY_REMOVE_DEVICE:

		SetNewPnpState (DriverBase::RemovePending);
		status = STATUS_SUCCESS;
		break;

	case IRP_MN_SURPRISE_REMOVAL:

		SetNewPnpState (DriverBase::SurpriseRemovePending);
		status = STATUS_SUCCESS;
		break;

	case IRP_MN_CANCEL_REMOVE_DEVICE:

		//
		// Check to see whether you have received cancel-remove
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

		status = STATUS_SUCCESS; // We must not fail this IRP.
		break;

	default:
		//
		// If you don't handle any IRP you must leave the
		// status as is.
		//
		status = Irp->IoStatus.Status;

		break;
	}

	//
	// Pass the IRP down and forget it.
	//
	Irp->IoStatus.Status = status;
	IoSkipCurrentIrpStackLocation (Irp);
	status = IoCallDriver (m_NextLowerDriver, Irp);
	--m_irpCounter;
	return status;
}

//	Default
NTSTATUS CFilterExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS    status;

	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));

	++m_irpCounter;

	IoSkipCurrentIrpStackLocation (Irp);
	status = IoCallDriver (m_NextLowerDriver, Irp);

	--m_irpCounter;

	return status;
}

//***************************************************************************************
//				completion routine
//***************************************************************************************

NTSTATUS CFilterExtension::FilterStartCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PKEVENT					event = (PKEVENT)Context;
    CFilterExtension		*pBase = (CFilterExtension*)GetExtensionClass (DeviceObject);


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
//				Attach to Device
//***************************************************************************************
NTSTATUS CFilterExtension::AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS                status = STATUS_SUCCESS;
    PDEVICE_OBJECT          deviceObject = NULL;
    ULONG                   deviceType = FILE_DEVICE_UNKNOWN;
    //
    // IoIsWdmVersionAvailable(1, 0x20) returns TRUE on os after Windows 2000.
    //
    if (!IoIsWdmVersionAvailable(1, 0x20)) 
	{
        //
        // Win2K system bugchecks if the filter attached to a storage device
        // doesn't specify the same DeviceType as the device it's attaching
        // to. This bugcheck happens in the filesystem when you disable
        // the devicestack whose top level deviceobject doesn't have a VPB.
        // To workaround we will get the toplevel object's DeviceType and
        // specify that in IoCreateDevice.
        //
        deviceObject = IoGetAttachedDeviceReference(PhysicalDeviceObject);
        deviceType = deviceObject->DeviceType;
        ObDereferenceObject(deviceObject);
    }

    //
    // Create a filter device object.
    //

    status = IoCreateDevice (DriverObject,
                             sizeof (CFilterExtension*),
                             NULL,  // No Name
                             deviceType,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &deviceObject);


    if (!NT_SUCCESS (status)) 
	{
        //
        // Returning failure here prevents the entire stack from functioning,
        // but most likely the rest of the stack will not be able to create
        // device objects either, so it is still OK.
        //
		DebugPrint (DebugError, "Did not create to FDO");
        return status;
    }

    DebugPrint (DebugTrace, "AddDevice PDO (0x%x) FDO (0x%x)",
                    PhysicalDeviceObject, deviceObject);

    *((CFilterExtension**)deviceObject->DeviceExtension) = this;

	m_NextLowerDriver = IoAttachDeviceToDeviceStack (
									deviceObject,
									PhysicalDeviceObject);
    //
    // Failure for attachment is an indication of a broken plug & play system.
    //

    if(NULL == m_NextLowerDriver) 
	{
		DebugPrint (DebugError, "Did not Attach to PDO: 0x%x", PhysicalDeviceObject);
        IoDeleteDevice(deviceObject);
        return STATUS_UNSUCCESSFUL;
    }

    deviceObject->Flags |= m_NextLowerDriver->Flags &
                            (DO_BUFFERED_IO | DO_DIRECT_IO |
                            DO_POWER_PAGABLE );


    deviceObject->DeviceType = m_NextLowerDriver->DeviceType;

    deviceObject->Characteristics = m_NextLowerDriver->Characteristics;

    m_Self = deviceObject;

    //
    // Set the initial state of the Filter DO
    //
    m_DevicePnPState =  DriverBase::NotStarted;
    m_PreviousPnPState = DriverBase::NotStarted;

    DebugPrint(DebugTrace, "AddDevice: %x to %x->%x", deviceObject,
                       m_NextLowerDriver,
                       PhysicalDeviceObject);

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DebugPrint (DebugSpecial, "AttachToDevice PhysicalDeviceObject (0x%x) NextLowerDriver (0x%x)",
                    PhysicalDeviceObject, m_NextLowerDriver);


    return STATUS_SUCCESS;
}

NTSTATUS CFilterExtension::DetachFromDevice()
{
	if (m_NextLowerDriver)
	{
		IoDetachDevice(m_NextLowerDriver);
		IoDeleteDevice(m_Self);
		m_NextLowerDriver = NULL;
		m_Self = NULL;
		return STATUS_SUCCESS;
	}
	return STATUS_UNSUCCESSFUL;
}

#if DBG
PCHAR CFilterExtension::UsbIoControl (LONG lFunc)
{
	switch (lFunc)
	{
	case IOCTL_INTERNAL_USB_SUBMIT_URB:
		return "IOCTL_INTERNAL_USB_SUBMIT_URB";
	case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
		return "IOCTL_INTERNAL_USB_GET_PORT_STATUS";
	case IOCTL_INTERNAL_USB_RESET_PORT:
		return "IOCTL_INTERNAL_USB_RESET_PORT";
	case IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO:
		return "IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO";
	case IOCTL_INTERNAL_USB_ENABLE_PORT:
		return "IOCTL_INTERNAL_USB_ENABLE_PORT";
	case IOCTL_INTERNAL_USB_GET_HUB_COUNT:
		return "IOCTL_INTERNAL_USB_GET_HUB_COUNT";
	case IOCTL_INTERNAL_USB_CYCLE_PORT:
		return "IOCTL_INTERNAL_USB_CYCLE_PORT";
	case IOCTL_INTERNAL_USB_GET_HUB_NAME:
		return "IOCTL_INTERNAL_USB_GET_HUB_NAME";
	case IOCTL_INTERNAL_USB_GET_BUS_INFO:
		return "IOCTL_INTERNAL_USB_GET_BUS_INFO";
	case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
		return "IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME";
	case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE";
	case IOCTL_INTERNAL_USB_GET_BUSGUID_INFO:
		return "IOCTL_INTERNAL_USB_GET_BUSGUID_INFO";
	case IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO:
		return "IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO";
	case IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY:
		return "IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY";
	case IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND:
		return "IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND";
	case IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME:
		return "IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME";
	case IOCTL_INTERNAL_USB_RECORD_FAILURE:
		return "IOCTL_INTERNAL_USB_RECORD_FAILURE";
	case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX";
	case IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE:
		return "IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE";
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
		return "IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS";
	case IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO";
	case IOCTL_USB_HCD_GET_STATS_1:
		return "IOCTL_USB_HCD_GET_STATS_1";
	//case IOCTL_USB_HCD_GET_STATS_2:
	//	return "IOCTL_USB_HCD_GET_STATS_2";
	case IOCTL_USB_HCD_DISABLE_PORT:
		return "IOCTL_USB_HCD_DISABLE_PORT";
	case IOCTL_USB_HCD_ENABLE_PORT:
		return "IOCTL_USB_HCD_ENABLE_PORT";
	case IOCTL_USB_DIAGNOSTIC_MODE_ON:
		return "IOCTL_USB_DIAGNOSTIC_MODE_ON";
	case IOCTL_USB_DIAGNOSTIC_MODE_OFF:
		return "IOCTL_USB_DIAGNOSTIC_MODE_OFF";
	case IOCTL_USB_GET_ROOT_HUB_NAME:
		return "IOCTL_USB_GET_ROOT_HUB_NAME";
	//case IOCTL_GET_HCD_DRIVERKEY_NAME:
	//	return "IOCTL_GET_HCD_DRIVERKEY_NAME";
	//case IOCTL_USB_GET_NODE_INFORMATION:
	//	return "IOCTL_USB_GET_NODE_INFORMATION";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION";
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
		return "IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION";
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		return "IOCTL_USB_GET_NODE_CONNECTION_NAME";
	case IOCTL_USB_DIAG_IGNORE_HUBS_ON:
		return "IOCTL_USB_DIAG_IGNORE_HUBS_ON";
	case IOCTL_USB_DIAG_IGNORE_HUBS_OFF:
		return "IOCTL_USB_DIAG_IGNORE_HUBS_OFF";
	//case IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME:
	//	return "IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME";
	case IOCTL_USB_GET_HUB_CAPABILITIES:
		return "IOCTL_USB_GET_HUB_CAPABILITIES";
	case IOCTL_USB_HUB_CYCLE_PORT:
		return "IOCTL_USB_HUB_CYCLE_PORT";
	case IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES:
		return "IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX";
	//case IOCTL_USB_RESET_HUB:
	//	return "IOCTL_USB_RESET_HUB";


	case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
		return "IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION";
	//case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
	//	return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX";
	}
	return 0;
}
#endif

NTSTATUS CFilterExtension::DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	return CBaseDeviceExtension::DispatchIntrenalDevCtrl (DeviceObject, Irp);
}

NTSTATUS CFilterExtension::DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	return CBaseDeviceExtension::DispatchDevCtrl (DeviceObject, Irp);
}