/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BaseDeviceExtension.cpp

Abstract:

   Basic class of kernel driver framework. Includes all MajorFunction handlers.

Environment:

    Kernel mode

--*/
#include "BaseDeviceExtension.h"

#include <stdio.h>
#include <ntddk.h>
#include <minmax.h>

PDRIVER_OBJECT DriverBase::CBaseDeviceExtension::m_DriverObject = NULL;
UNICODE_STRING DriverBase::CBaseDeviceExtension::m_strRegistry;
bool DriverBase::CBaseDeviceExtension::m_bInit = false;

// **********************************************************************
//						Constructor
// **********************************************************************

DriverBase::CBaseDeviceExtension::CBaseDeviceExtension ()
	: CBaseClass ()
	, m_Self (NULL)
	, m_DevicePnPState (NotStarted)
	, m_PreviousPnPState (NotStarted)
{
	SetDebugName ("CBaseDeviceExtension");
}

DriverBase::CBaseDeviceExtension::~CBaseDeviceExtension ()
{
}

// **********************************************************************
//						Driver Enter: init major function
// **********************************************************************

void DriverBase::CBaseDeviceExtension::Init (PDRIVER_OBJECT  DriverObject, PUNICODE_STRING strRegistry)
{
    PDRIVER_DISPATCH  * dispatch = NULL;
	ULONG ulIndex;

    for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
         ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
         ulIndex++, dispatch++) 
	{

        *dispatch = stDispatchOther;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE]						= stDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]						= stDispatchClose;
	DriverObject->MajorFunction[IRP_MJ_READ]						= stDispatchRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE]						= stDispatchWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]				= stDispatchDevCtrl;
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL]		= stDispatchIntrenalDevCtrl;
	DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]					= stDispatchShutdown;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP]						= stDispatchCleanup;
	DriverObject->MajorFunction[IRP_MJ_POWER]						= stDispatchPower;
	DriverObject->MajorFunction[IRP_MJ_PNP]							= stDispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]				= stDispatchSysCtrl;
	
	m_DriverObject = DriverObject;
	//SetRegistry (strRegistry);
	{
		CUnicodeString str (*strRegistry, NonPagedPool);
		m_strRegistry = str.Detach();
		m_bInit = true;
	}
}

void DriverBase::CBaseDeviceExtension::Unload ()
{
	if (m_bInit)
	{
		m_bInit = false;
		ExFreePool (m_strRegistry.Buffer);
		RtlZeroMemory (&m_strRegistry, sizeof (UNICODE_STRING));
	}
}

DriverBase::CBaseDeviceExtension* DriverBase::CBaseDeviceExtension::GetExtensionClass (PDEVICE_OBJECT DeviceObject)
{
	if (DeviceObject->DeviceExtension)
	{
		return *((CBaseDeviceExtension**)DeviceObject->DeviceExtension);
	}
	return NULL;
}

NTSTATUS DriverBase::CBaseDeviceExtension::Complete (PIRP pIrp, NTSTATUS status)
{
	pIrp->IoStatus.Status = status;
	IoCompleteRequest (pIrp, IO_NO_INCREMENT);

	return status;
}
// **********************************************************************
//						static major function
// **********************************************************************

//			Create
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchCreate (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchCreate (DeviceObject, Irp);
	}

	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Close
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchClose (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchClose (DeviceObject, Irp);
	}

	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Read		
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchRead (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchRead (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Write
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchWrite (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchWrite (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Device Control
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchDevCtrl (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Internal Device Control
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchIntrenalDevCtrl (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Shutdown
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchShutdown (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchShutdown (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Cleanup
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchCleanup (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchCleanup (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Power
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchPower (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			System Control
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchSysCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchSysCtrl (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			PlugAndPlay
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		NTSTATUS status = pBase->DispatchPnp(DeviceObject, Irp);
		return status;
	}

	return Complete (Irp, STATUS_ACCESS_DENIED);
}

//			Other major function
NTSTATUS DriverBase::CBaseDeviceExtension::stDispatchOther (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CBaseDeviceExtension *pBase = GetExtensionClass (DeviceObject);
	if (pBase)
	{
		return pBase->DispatchOther (DeviceObject, Irp);
	}
	return Complete (Irp, STATUS_ACCESS_DENIED);
}

// **********************************************************************
//						virtual major function
// **********************************************************************

//			Create
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchCreate (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Close
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchClose (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Read
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchRead (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Write
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchWrite (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Device Control
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Internal Device Control
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Shutdown
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchShutdown (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Cleanup
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchCleanup (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Power
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			System contro
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchSysCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			PlugAndPlay
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	DebugPrint (DebugTrace, "%s", PnPMinorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}

//			Other major function
NTSTATUS DriverBase::CBaseDeviceExtension::DispatchOther (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return DispatchDefault (DeviceObject, Irp);
}
// **********************************************************************
//						Default dispatch
// **********************************************************************

NTSTATUS DriverBase::CBaseDeviceExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	return STATUS_SUCCESS;
}
//***************************************************************************************
//				Plug and play state
//***************************************************************************************

// Set new pnp state
void DriverBase::CBaseDeviceExtension::SetNewPnpState (DEVICE_PNP_STATE NewState)
{
	m_PreviousPnPState = m_DevicePnPState;
	m_DevicePnPState = NewState;
}

// restore pnp state
void DriverBase::CBaseDeviceExtension::RestorePnpState ()
{
	m_DevicePnPState = m_PreviousPnPState;
}


void DriverBase::CBaseDeviceExtension::DeleteDevice ()
{
	if (m_Self)
	{
		m_Self->DeviceExtension = 0;
		IoDeleteDevice(m_Self);
		m_Self = NULL;
		delete this;
	}
}

// **********************************************************************
//						Additional Debug Functions
// **********************************************************************
PCHAR DriverBase::CBaseDeviceExtension::PnpQueryIdString(BUS_QUERY_ID_TYPE Type)
{
#if _FEATURE_ENABLE_DEBUGPRINT
	switch (Type)
	{
	case BusQueryDeviceID:
		return "BusQueryDeviceID";
	case BusQueryHardwareIDs:
		return "BusQueryHardwareIDs";
	case BusQueryCompatibleIDs:
		return "BusQueryCompatibleIDs";
	case BusQueryInstanceID:
		return "BusQueryInstanceID";
	case BusQueryDeviceSerialNumber:
		return "BusQueryDeviceSerialNumber";
	}
	return "Unknow";
#else
	return NULL;
#endif
}

PCHAR DriverBase::CBaseDeviceExtension::PnPMinorFunctionString (UCHAR cMinorFunction)
{
#if _FEATURE_ENABLE_DEBUGPRINT
	switch (cMinorFunction)
	{
	case IRP_MN_START_DEVICE:
		return "IRP_MN_START_DEVICE";
	case IRP_MN_QUERY_REMOVE_DEVICE:
		return "IRP_MN_QUERY_REMOVE_DEVICE";
	case IRP_MN_REMOVE_DEVICE:
		return "IRP_MN_REMOVE_DEVICE";
	case IRP_MN_CANCEL_REMOVE_DEVICE:
		return "IRP_MN_CANCEL_REMOVE_DEVICE";
	case IRP_MN_STOP_DEVICE:
		return "IRP_MN_STOP_DEVICE";
	case IRP_MN_QUERY_STOP_DEVICE:
		return "IRP_MN_QUERY_STOP_DEVICE";
	case IRP_MN_CANCEL_STOP_DEVICE:
		return "IRP_MN_CANCEL_STOP_DEVICE";
	case IRP_MN_QUERY_DEVICE_RELATIONS:
		return "IRP_MN_QUERY_DEVICE_RELATIONS";
	case IRP_MN_QUERY_INTERFACE:
		return "IRP_MN_QUERY_INTERFACE";
	case IRP_MN_QUERY_CAPABILITIES:
		return "IRP_MN_QUERY_CAPABILITIES";
	case IRP_MN_QUERY_RESOURCES:
		return "IRP_MN_QUERY_RESOURCES";
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
		return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
	case IRP_MN_QUERY_DEVICE_TEXT:
		return "IRP_MN_QUERY_DEVICE_TEXT";
	case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
		return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
	case IRP_MN_READ_CONFIG:
		return "IRP_MN_READ_CONFIG";
	case IRP_MN_WRITE_CONFIG:
		return "IRP_MN_WRITE_CONFIG";
	case IRP_MN_EJECT:
		return "IRP_MN_EJECT";
	case IRP_MN_SET_LOCK:
		return "IRP_MN_SET_LOCK";
	case IRP_MN_QUERY_ID:
		return "IRP_MN_QUERY_ID";
	case IRP_MN_QUERY_PNP_DEVICE_STATE:
		return "IRP_MN_QUERY_PNP_DEVICE_STATE";
	case IRP_MN_QUERY_BUS_INFORMATION:
		return "IRP_MN_QUERY_BUS_INFORMATION";
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
	case IRP_MN_SURPRISE_REMOVAL:
		return "IRP_MN_SURPRISE_REMOVAL";

	default:
		return "unknown_pnp_irp";
	}
	return 0;
#else
	return 0;
#endif
}

PCHAR DriverBase::CBaseDeviceExtension::PnPMinorFunctionString (PIRP Irp)
{
#if _FEATURE_ENABLE_DEBUGPRINT
	PIO_STACK_LOCATION         irpStack;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	return PnPMinorFunctionString (irpStack->MinorFunction);
#else
	return 0;
#endif
}

PCHAR DriverBase::CBaseDeviceExtension::MajorFunctionString (PIRP Irp)
{
#if _FEATURE_ENABLE_DEBUGPRINT
	PIO_STACK_LOCATION         irpStack;
	irpStack = IoGetCurrentIrpStackLocation(Irp);

	switch (irpStack->MajorFunction)
	{
	case IRP_MJ_CREATE:
		return "IRP_MJ_CREATE";
	case IRP_MJ_CREATE_NAMED_PIPE:
		return "IRP_MJ_CREATE_NAMED_PIPE";
	case IRP_MJ_CLOSE:
		return "IRP_MJ_CLOSE";
	case IRP_MJ_READ:
		return "IRP_MJ_READ";
	case IRP_MJ_WRITE:
		return "IRP_MJ_WRITE";
	case IRP_MJ_QUERY_INFORMATION:
		return "IRP_MJ_QUERY_INFORMATION";
	case IRP_MJ_SET_INFORMATION:
		return "IRP_MJ_SET_INFORMATION";
	case IRP_MJ_QUERY_EA:
		return "IRP_MJ_QUERY_EA";
	case IRP_MJ_SET_EA:
		return "IRP_MJ_SET_EA";
	case IRP_MJ_FLUSH_BUFFERS:
		return "IRP_MJ_FLUSH_BUFFERS";
	case IRP_MJ_QUERY_VOLUME_INFORMATION:
		return "IRP_MJ_QUERY_VOLUME_INFORMATION";
	case IRP_MJ_SET_VOLUME_INFORMATION:
		return "IRP_MJ_SET_VOLUME_INFORMATION";
	case IRP_MJ_DIRECTORY_CONTROL:
		return "IRP_MJ_DIRECTORY_CONTROL";
	case IRP_MJ_FILE_SYSTEM_CONTROL:
		return "IRP_MJ_FILE_SYSTEM_CONTROL";
	case IRP_MJ_DEVICE_CONTROL:
		return "IRP_MJ_DEVICE_CONTROL";
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		return "IRP_MJ_INTERNAL_DEVICE_CONTROL";
	case IRP_MJ_SHUTDOWN:
		return "IRP_MJ_SHUTDOWN";
	case IRP_MJ_LOCK_CONTROL:
		return "IRP_MJ_LOCK_CONTROL";
	case IRP_MJ_CLEANUP:
		return "IRP_MJ_CLEANUP";
	case IRP_MJ_CREATE_MAILSLOT:
		return "IRP_MJ_CREATE_MAILSLOT";
	case IRP_MJ_QUERY_SECURITY:
		return "IRP_MJ_QUERY_SECURITY";
	case IRP_MJ_SET_SECURITY:
		return "IRP_MJ_SET_SECURITY";
	case IRP_MJ_POWER:
		return "IRP_MJ_POWER";
	case IRP_MJ_SYSTEM_CONTROL:
		return "IRP_MJ_SYSTEM_CONTROL";
	case IRP_MJ_DEVICE_CHANGE:
		return "IRP_MJ_DEVICE_CHANGE";
	case IRP_MJ_QUERY_QUOTA:
		return "IRP_MJ_QUERY_QUOTA";
	case IRP_MJ_SET_QUOTA:
		return "IRP_MJ_SET_QUOTA";
	case IRP_MJ_PNP:
		return "IRP_MJ_PNP";
	}

	return "unknown";
#else
	return 0;
#endif
}

// ***********************************************************
//					Debug functions
// ***********************************************************
PCHAR DriverBase::CBaseDeviceExtension::PowerMinorFunctionString (UCHAR MinorFunction)
{
#if _FEATURE_ENABLE_DEBUGPRINT
	switch (MinorFunction)
	{
	case IRP_MN_SET_POWER:
		return "IRP_MN_SET_POWER";
	case IRP_MN_QUERY_POWER:
		return "IRP_MN_QUERY_POWER";
	case IRP_MN_POWER_SEQUENCE:
		return "IRP_MN_POWER_SEQUENCE";
	case IRP_MN_WAIT_WAKE:
		return "IRP_MN_WAIT_WAKE";
	default:
		return "unknown_power_irp";
	}
#else
	return 0;
#endif
}

PCHAR DriverBase::CBaseDeviceExtension::SystemPowerString(SYSTEM_POWER_STATE Type)
{
#if _FEATURE_ENABLE_DEBUGPRINT
	switch (Type)
	{
	case PowerSystemUnspecified:
		return "PowerSystemUnspecified";
	case PowerSystemWorking:
		return "PowerSystemWorking";
	case PowerSystemSleeping1:
		return "PowerSystemSleeping1";
	case PowerSystemSleeping2:
		return "PowerSystemSleeping2";
	case PowerSystemSleeping3:
		return "PowerSystemSleeping3";
	case PowerSystemHibernate:
		return "PowerSystemHibernate";
	case PowerSystemShutdown:
		return "PowerSystemShutdown";
	case PowerSystemMaximum:
		return "PowerSystemMaximum";
	default:
		return "UnKnown System Power State";
	}
#else
	return 0;
#endif
}

PCHAR DriverBase::CBaseDeviceExtension::DevicePowerString(DEVICE_POWER_STATE Type)
{
#if _FEATURE_ENABLE_DEBUGPRINT
	switch (Type)
	{
	case PowerDeviceUnspecified:
		return "PowerDeviceUnspecified";
	case PowerDeviceD0:
		return "PowerDeviceD0";
	case PowerDeviceD1:
		return "PowerDeviceD1";
	case PowerDeviceD2:
		return "PowerDeviceD2";
	case PowerDeviceD3:
		return "PowerDeviceD3";
	case PowerDeviceMaximum:
		return "PowerDeviceMaximum";
	default:
		return "UnKnown Device Power State";
	}
#else
	return NULL;
#endif
}

