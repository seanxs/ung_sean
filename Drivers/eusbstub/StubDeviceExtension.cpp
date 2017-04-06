/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BaseDeviceExtension.cpp

Abstract:

   Stub class that appears in the system instead of captured USB device

Environment:

    Kernel mode

--*/
#include "StubDeviceExtension.h"

CStubDeviceExtension::CStubDeviceExtension ()
	: DriverBase::CBaseDeviceExtension ()
{
	SetDebugName ("CStubDeviceExtension");
}

// ********************************************************************
//					virtual major functions
// ********************************************************************

NTSTATUS CStubDeviceExtension::DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS            status;

    DebugPrint (DebugTrace, "DispatchPower");
	DebugPrint (DebugTrace, "%s", CBaseDeviceExtension::MajorFunctionString (Irp));

    if (DriverBase::Deleted == m_DevicePnPState)
    {
        PoStartNextPowerIrp (Irp);
        status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    status = PoCallDriver(m_NextLowerDriver, Irp);
    
    return status;
}

NTSTATUS CStubDeviceExtension::DispatchSysCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS                status;

    DebugPrint (DebugTrace, "DispatchSysCtrl");
	DebugPrint (DebugTrace, "%s", CBaseDeviceExtension::MajorFunctionString (Irp));

    if (DriverBase::Deleted == m_DevicePnPState)
    {
        status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    //
    // Set up the I/O stack location for the next lower driver (the target device
    // object for the IoCallDriver call). Call IoSkipCurrentIrpStackLocation
    // because the function driver does not change any of the IRP's values in the
    // current I/O stack location. This allows the system to reuse I/O stack
    // locations.
    //
    IoSkipCurrentIrpStackLocation (Irp);

    //
    // Pass the WMI (system control) IRP down the device stack to be processed by the
    // bus driver.
    //
    status = IoCallDriver (m_NextLowerDriver, Irp);

    return status;
}

NTSTATUS CStubDeviceExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION      stack;
    NTSTATUS                status = STATUS_SUCCESS;
    PPNP_DEVICE_STATE       deviceState;
	KEVENT					event;

    DebugPrint (DebugTrace, "DispatchPnp");
	DebugPrint (DebugTrace, "%s", DriverBase::CBaseDeviceExtension::MajorFunctionString (Irp));
	DebugPrint (DebugTrace, "%s", DriverBase::CBaseDeviceExtension::PnPMinorFunctionString (Irp));

    // Get the parameters of the IRP from the function driver's location in the IRP's
    // I/O stack. The results of the function driver's processing of the IRP, if any,
    // are then stored back in the same I/O stack location.
    //
    // The PnP IRP's minor function code is stored in the IRP's MinorFunction member.
    //
    stack = IoGetCurrentIrpStackLocation (Irp);

    if (DriverBase::Deleted == m_DevicePnPState)
    {
        Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Determine the PnP IRP's minor function code which describes the PnP operation.
    //
    switch (stack->MinorFunction)
    {
    case IRP_MN_START_DEVICE:
		NTSTATUS status;
		//
		// Initialize a kernel event to an unsignaled state. The event is signaled in
		// ToasterDispatchPnpComplete when the bus driver completes the PnP IRP.
		//

		KeInitializeEvent(&event, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp,
							DispatchPnpComplete,
							&event,
							TRUE,
							TRUE,
							TRUE
							);

		status = IoCallDriver(m_NextLowerDriver, Irp);

		if (STATUS_PENDING == status)
		{
			KeWaitForSingleObject(&event,
								Executive,
								KernelMode,
								FALSE,
								NULL
								);
			status = Irp->IoStatus.Status;
		}

        if (NT_SUCCESS (status))
        {
			SetNewPnpState (DriverBase::Started);
        }

        //
        // Set the status of the IRP to the value returned by the bus driver. The
        // value returned by the bus driver is also the value returned to the caller.
        //
        Irp->IoStatus.Status = status;

        IoCompleteRequest (Irp, IO_NO_INCREMENT);

		return status;

    case IRP_MN_QUERY_STOP_DEVICE:
        SetNewPnpState (DriverBase::StopPending);

        //
        // Set the status of the IRP to STATUS_SUCCESS before passing it down the
        // device stack to indicate that the function driver successfully processed
        // the IRP.
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

   case IRP_MN_CANCEL_STOP_DEVICE:

        RestorePnpState();
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_STOP_DEVICE:
        SetNewPnpState (DriverBase::Stopped);
        Irp->IoStatus.Status = STATUS_SUCCESS;

        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:
        SetNewPnpState (DriverBase::RemovePending);
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:
        RestorePnpState ();
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

   case IRP_MN_SURPRISE_REMOVAL:
        SetNewPnpState (DriverBase::SurpriseRemovePending);
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

   case IRP_MN_REMOVE_DEVICE:
        SetNewPnpState (DriverBase::Deleted);

        IoSkipCurrentIrpStackLocation (Irp);

        status = IoCallDriver (m_NextLowerDriver, Irp);
        IoDetachDevice (m_NextLowerDriver);

		DeleteDevice ();
        return status;

    default:
        break;
    }

    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (m_NextLowerDriver, Irp);

    return status;
}

NTSTATUS CStubDeviceExtension::DispatchOther (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    DebugPrint (DebugTrace, "DispatchOther");
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS CStubDeviceExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    DebugPrint (DebugTrace, "DispatchDefault");
	return DispatchOther (DeviceObject, Irp);
}

// ********************************************************************
//					completion routine
// ********************************************************************

NTSTATUS CStubDeviceExtension::DispatchPnpComplete (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PKEVENT	event = (PKEVENT)Context;

    if (TRUE == Irp->PendingReturned)
    {
        KeSetEvent (event, IO_NO_INCREMENT, FALSE);
    }

    //
    // Do not complete the IRP here in the I/O completion routine. Instead, the IRP
    // is completed in ToasterDispatchPnP.
    //
    return STATUS_MORE_PROCESSING_REQUIRED;
}

// ********************************************************************
//					Attach to Device
// ********************************************************************
NTSTATUS CStubDeviceExtension::AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS                status = STATUS_SUCCESS;
    PDEVICE_OBJECT          deviceObject = NULL;

    DebugPrint(DebugTrace, "AddDevice PDO (0x%p)", PhysicalDeviceObject);

    status = IoCreateDevice (DriverObject,
                             sizeof (CStubDeviceExtension*),
                             NULL,
                             FILE_DEVICE_UNKNOWN,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &deviceObject);

    //
    // Test the return value with the NT_SUCCESS macro instead of testing for a
    // specific value like STATUS_SUCCESS, because system calls can return multiple
    // values that indicate success other than STATUS_SUCCESS.
    //
    if(!NT_SUCCESS (status))
    {
		DebugPrint (DebugError, "Could not create FDO");
        return status;
    }

    DebugPrint(DebugInfo, "AddDevice FDO (0x%p)", deviceObject);

    *((CStubDeviceExtension**)deviceObject->DeviceExtension) = this;

	m_DevicePnPState = DriverBase::NotStarted;
	m_PreviousPnPState = DriverBase::NotStarted;

    m_Self = deviceObject;
    deviceObject->Flags |= DO_POWER_PAGABLE;
    deviceObject->Flags |= DO_BUFFERED_IO;

	m_NextLowerDriver = IoAttachDeviceToDeviceStack (deviceObject,
                                                       PhysicalDeviceObject);
    if(NULL == m_NextLowerDriver)
    {
        //
		DebugPrint (DebugError, "Could not Attach to PDO: 0x%x", PhysicalDeviceObject);
        IoDeleteDevice(deviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return status;
}

// ********************************************************************
//					Additional info
// ********************************************************************

#if DBG
#define IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE     CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_GET_DEVICE_HANDLE, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_USB_GET_HUB_CAPABILITIES  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_HUB_CAPABILITIES,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_HUB_CYCLE_PORT  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_HUB_CYCLE_PORT,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_CONNECTION_ATTRIBUTES,\
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_CONNECTION_INFORMATION_EX,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION   \
                                            CTL_CODE(FILE_DEVICE_USB,  \
                                            USB_IDLE_NOTIFICATION,  \
                                            METHOD_NEITHER,  \
                                            FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY     CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_IDLE_NOTIFICATION_EX, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND    CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_REQ_GLOBAL_SUSPEND, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME     CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_REQ_GLOBAL_RESUME, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_RECORD_FAILURE        CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_RECORD_FAILURE, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_DEVICE_HANDLE_EX, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_TT_DEVICE_HANDLE, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_TOPOLOGY_ADDRESS, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO    CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_HUB_CONFIG_INFO, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

PCHAR UsbIoControl (LONG lFunc)
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

NTSTATUS CStubDeviceExtension::DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    DebugPrint (DebugTrace, "DispatchIntrenalDevCtrl");
	#if DBG
		PIO_STACK_LOCATION  stack;

		stack = IoGetCurrentIrpStackLocation (Irp);
		if (stack->Parameters.DeviceIoControl.IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
		{
			DebugPrint (DebugSpecial, "DispatchIntrenalDevCtrl - %s", UsbIoControl (stack->Parameters.DeviceIoControl.IoControlCode));
		}
	#endif
	return CBaseDeviceExtension::DispatchIntrenalDevCtrl (DeviceObject, Irp);
}

NTSTATUS CStubDeviceExtension::DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS                status = STATUS_SUCCESS;
    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (m_NextLowerDriver, Irp);
    return status;
}

NTSTATUS CStubDeviceExtension::DispatchCreate	(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS                status = STATUS_SUCCESS;
    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (m_NextLowerDriver, Irp);
    return status;
}

NTSTATUS CStubDeviceExtension::DispatchClose (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS                status = STATUS_SUCCESS;
    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (m_NextLowerDriver, Irp);
    return status;
}

void CStubDeviceExtension::SendAdditionPnpRequest (PDEVICE_OBJECT DeviceObject)
{
    IO_STACK_LOCATION   irpStack;
    PVOID               pInfo;
    NTSTATUS            status;

    // IRP_MN_QUERY_ID - BusQueryDeviceID
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_ID;
        irpStack.Parameters.QueryId.IdType = BusQueryDeviceID;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_ID - BusQueryHardwareIDs
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_ID;
        irpStack.Parameters.QueryId.IdType = BusQueryHardwareIDs;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_ID - BusQueryCompatibleIDs
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_ID;
        irpStack.Parameters.QueryId.IdType = BusQueryCompatibleIDs;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_ID - BusQueryInstanceID
    {
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_ID;
        irpStack.Parameters.QueryId.IdType = BusQueryInstanceID;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_DEVICE_TEXT - DeviceTextDescription
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_DEVICE_TEXT;
        irpStack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextDescription;
        irpStack.Parameters.QueryDeviceText.LocaleId = 0x409;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_DEVICE_TEXT - DeviceTextLocationInformation
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_DEVICE_TEXT;
        irpStack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextLocationInformation;
        irpStack.Parameters.QueryDeviceText.LocaleId = 0x409;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }

    // IRP_MN_QUERY_CAPABILITIES
    {
        DEVICE_CAPABILITIES DeviceCapabilities;

        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_CAPABILITIES;

        RtlZeroMemory( &DeviceCapabilities, sizeof(DEVICE_CAPABILITIES) );
        DeviceCapabilities.Size = sizeof(DEVICE_CAPABILITIES);
        DeviceCapabilities.Version = 1;
        irpStack.Parameters.DeviceCapabilities.Capabilities = (PDEVICE_CAPABILITIES)&DeviceCapabilities;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);
    }
    // IRP_MN_QUERY_RESOURCE_REQUIREMENTS
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_RESOURCE_REQUIREMENTS;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_BUS_INFORMATION
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_BUS_INFORMATION;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_RESOURCES
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_RESOURCES;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_CAPABILITIES
    {
        DEVICE_CAPABILITIES DeviceCapabilities;

        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_CAPABILITIES;

        RtlZeroMemory( &DeviceCapabilities, sizeof(DEVICE_CAPABILITIES) );
        DeviceCapabilities.Size = sizeof(DEVICE_CAPABILITIES);
        DeviceCapabilities.Version = 1;
        irpStack.Parameters.DeviceCapabilities.Capabilities = (PDEVICE_CAPABILITIES)&DeviceCapabilities;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);
    }
    // IRP_MN_QUERY_DEVICE_TEXT - DeviceTextDescription
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_DEVICE_TEXT;
        irpStack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextDescription;
        irpStack.Parameters.QueryDeviceText.LocaleId = 0x409;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_DEVICE_TEXT - DeviceTextLocationInformation
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_DEVICE_TEXT;
        irpStack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextLocationInformation;
        irpStack.Parameters.QueryDeviceText.LocaleId = 0x409;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_RESOURCE_REQUIREMENTS
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_RESOURCE_REQUIREMENTS;

        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_BUS_INFORMATION
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_BUS_INFORMATION;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_QUERY_RESOURCES
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_RESOURCES;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // 0x18
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = 0x18;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);
    }
    // IRP_MN_QUERY_RESOURCE_REQUIREMENTS
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_QUERY_RESOURCE_REQUIREMENTS;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_FILTER_RESOURCE_REQUIREMENTS
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_FILTER_RESOURCE_REQUIREMENTS;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);

        if (NT_SUCCESS (status) && pInfo)
        {
            ExFreePool (pInfo);
        }
    }
    // IRP_MN_START_DEVICE
    {
        RtlZeroMemory( &irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack.MajorFunction = IRP_MJ_PNP;
        irpStack.MinorFunction = IRP_MN_START_DEVICE;
        status = CallPnp (DeviceObject, &irpStack, &pInfo);
    }
}

NTSTATUS CStubDeviceExtension::CallPnp(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION ioStack, PVOID *Info)
{
    IO_STATUS_BLOCK     ioStatus;
    KEVENT              pnpEvent;
    NTSTATUS            status = STATUS_NOT_SUPPORTED;
    PIRP                pnpIrp;
    PIO_STACK_LOCATION  irpStack;

    // Initialize the event
    //
    *Info = NULL;
    KeInitializeEvent( &pnpEvent, NotificationEvent, FALSE );

    pnpIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP,
        DeviceObject,
        NULL,
        0,
        NULL,
        &pnpEvent,
        &ioStatus
        );
    
    if (pnpIrp) 
    {

        pnpIrp->IoStatus.Information = ioStatus.Information = 0;
        pnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        irpStack = IoGetNextIrpStackLocation( pnpIrp );

        *irpStack = *ioStack;

        // set callback result
        status = IoCallDriver( DeviceObject, pnpIrp );
        if (status == STATUS_PENDING) 
        {
            KeWaitForSingleObject(
                &pnpEvent,
                Executive,
                KernelMode,
                FALSE,
                NULL
                );
            status = ioStatus.Status;
        }
        if (NT_SUCCESS (status) && ioStatus.Information)
        {
            *Info = (PVOID)ioStatus.Information;
        }
    }
    return status;

}
NTSTATUS CStubDeviceExtension::CallUSBD(IN PURB Urb)
{
    PIRP               irp;
    KEVENT             event;
    NTSTATUS           ntStatus;
    IO_STATUS_BLOCK    ioStatus;
    PIO_STACK_LOCATION nextStack;

    //
    // initialize the variables
    //

    irp = NULL;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_SUBMIT_URB, 
                                        m_NextLowerDriver,
                                        NULL, 
                                        0, 
                                        NULL, 
                                        0, 
                                        TRUE, 
                                        &event, 
                                        &ioStatus);

    if(!irp) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    nextStack = IoGetNextIrpStackLocation(irp);
    ASSERT(nextStack != NULL);
    nextStack->Parameters.Others.Argument1 = Urb;

    ntStatus = IoCallDriver(m_NextLowerDriver, irp);

    if(ntStatus == STATUS_PENDING) {

        KeWaitForSingleObject(&event, 
                              Executive, 
                              KernelMode, 
                              FALSE, 
                              NULL);

        ntStatus = ioStatus.Status;
    }
    
    return ntStatus;
}

NTSTATUS CStubDeviceExtension::ReadandSelectDescriptors()
{
    PURB                   urb;
    ULONG                  siz;
    NTSTATUS               ntStatus;
    PUSB_DEVICE_DESCRIPTOR deviceDescriptor;
    
    //
    // initialize variables
    //

    urb = NULL;
    deviceDescriptor = NULL;

    //
    // 1. Read the device descriptor
    //

    urb = (PURB)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), 'Usb');

    if(urb) 
    {
        siz = sizeof(USB_DEVICE_DESCRIPTOR);
        deviceDescriptor = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPool, siz, 'Usb');

        if(deviceDescriptor) {

            UsbBuildGetDescriptorRequest(
                    urb, 
                    (USHORT) sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                    USB_DEVICE_DESCRIPTOR_TYPE, 
                    0, 
                    0, 
                    deviceDescriptor, 
                    NULL, 
                    siz, 
                    NULL);

            ntStatus = CallUSBD(urb);

            if(NT_SUCCESS(ntStatus)) 
            {

                ASSERT(deviceDescriptor->bNumConfigurations);
                ntStatus = ConfigureDevice();    
            }
                            
            ExFreePool(urb);                
            ExFreePool(deviceDescriptor);
        }
        else {
            ExFreePool(urb);
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else {

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

NTSTATUS CStubDeviceExtension::ConfigureDevice()
{
    PURB                          urb;
    ULONG                         siz;
    NTSTATUS                      ntStatus;
    PUSB_CONFIGURATION_DESCRIPTOR configurationDescriptor;

    //
    // initialize the variables
    //

    urb = NULL;
    configurationDescriptor = NULL;

    //
    // Read the first configuration descriptor
    // This requires two steps:
    // 1. Read the fixed sized configuration desciptor (CD)
    // 2. Read the CD with all embedded interface and endpoint descriptors
    //

    urb = (PURB)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), 'Usb');

    if(urb) {

        siz = sizeof(USB_CONFIGURATION_DESCRIPTOR);
        configurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPool, siz, 'Usb');

        if(configurationDescriptor) {

            UsbBuildGetDescriptorRequest(
                    urb, 
                    (USHORT) sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                    USB_CONFIGURATION_DESCRIPTOR_TYPE, 
                    0, 
                    0, 
                    configurationDescriptor,
                    NULL, 
                    sizeof(USB_CONFIGURATION_DESCRIPTOR), 
                    NULL);

            ntStatus = CallUSBD(urb);

            if(!NT_SUCCESS(ntStatus)) {

                goto ConfigureDevice_Exit;
            }
        }
        else {

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto ConfigureDevice_Exit;
        }

        siz = configurationDescriptor->wTotalLength;

        ExFreePool(configurationDescriptor);

        configurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPool, siz, 'Usb');

        if(configurationDescriptor) {

            UsbBuildGetDescriptorRequest(
                    urb, 
                    (USHORT)sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                    USB_CONFIGURATION_DESCRIPTOR_TYPE,
                    0, 
                    0, 
                    configurationDescriptor, 
                    NULL, 
                    siz, 
                    NULL);

            ntStatus = CallUSBD(urb);

            if(!NT_SUCCESS(ntStatus)) {

                goto ConfigureDevice_Exit;
            }
        }
        else {

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto ConfigureDevice_Exit;
        }
    }
    else {

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto ConfigureDevice_Exit;
    }

ConfigureDevice_Exit:

    if(urb) {

        ExFreePool(urb);
    }

    if(configurationDescriptor) {

        ExFreePool(configurationDescriptor);
    }

    return ntStatus;
}

VOID CStubDeviceExtension::InitUsbDev (PDEVICE_OBJECT DeviceObject, PVOID Context)
{
    PWOTK_ITEM pContext = (PWOTK_ITEM)Context;
    CStubDeviceExtension *pBase = (CStubDeviceExtension *)pContext->pContext;
    NTSTATUS                      ntStatus;

    IoFreeWorkItem (pContext->WorkItem);
    ExFreePool (Context);

    ntStatus = pBase->ReadandSelectDescriptors ();
    if (!NT_SUCCESS (ntStatus))
    {
        if (ntStatus != STATUS_NOT_SUPPORTED)
        {
            LARGE_INTEGER time;
            time.QuadPart = (LONGLONG)-1000000;
            KeDelayExecutionThread (KernelMode, FALSE, &time);

            pBase->SendAdditionPnpRequest (pBase->m_NextLowerDriver);
            ntStatus = pBase->ReadandSelectDescriptors ();
        }
   }
}