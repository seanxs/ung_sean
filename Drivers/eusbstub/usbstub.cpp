/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    usbstub.cpp

Abstract:

    This module contains the entry points for a USB stub.

Environment:

    Kernel mode

--*/
#include "usbstub.h"
#include "StubDeviceExtension.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
	DriverBase::CBaseDeviceExtension::Init (DriverObject, RegistryPath);
    DriverObject->DriverExtension->AddDevice		        = FilterAddDevice;
    DriverObject->DriverUnload								= FilterUnload;

	return STATUS_SUCCESS;
}

NTSTATUS
FilterAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
{
	CStubDeviceExtension *pBase = new CStubDeviceExtension ();
	return pBase->AttachToDevice (DriverObject, PhysicalDeviceObject);
}

VOID
FilterUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
	ASSERT(DriverObject->DeviceObject == NULL);
	DriverBase::CBaseDeviceExtension::Unload();
}
