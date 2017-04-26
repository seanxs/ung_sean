/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    evuh.cpp

Abstract:

    This module contains the entry points for a virtual hub driver.

Environment:

    Kernel mode

--*/
#include "evuh.h"
#include "Core\BaseDeviceExtension.h"
#include "Core\Timer.h"
#include "Core\WorkItem.h"
#include "Core\Buffer.h"
#include "VirtualUsbHub.h"
#include "Core\SystemEnum.h"
#include "UsbDevToTcp.h"
#include "FilterStub.h"
#include "Insulation.h"
#include "PoolCompletingIrp.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
	NTSTATUS            status = STATUS_SUCCESS;

	DriverBase::CBaseDeviceExtension::Init (DriverObject, RegistryPath);
    DriverObject->DriverExtension->AddDevice		        = AddDevice;
    DriverObject->DriverUnload								= UnloadDriver;

	return status;
}

NTSTATUS
AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
{
	CVirtualUsbHubExtension	*pVuh;
	NTSTATUS				status;

	pVuh = new CVirtualUsbHubExtension ();
	if (!NT_SUCCESS (status = pVuh->AttachToDevice (DriverObject, PhysicalDeviceObject)))
	{
		delete pVuh;
	}

	//GUID DEVINTERFACE_DISK = { 0x53f56307L, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } };
	GUID DEVINTERFACE_VOLUME = { 0x53f5630dL, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } };

    status = IoRegisterPlugPlayNotification (
					EventCategoryDeviceInterfaceChange,
					PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
					(PVOID)&DEVINTERFACE_VOLUME,
					DriverObject,
					(PDRIVER_NOTIFICATION_CALLBACK_ROUTINE)Insulation::Detect_NewStorage,
					(PVOID)pVuh,
					&pVuh->m_pRegValume);


	pVuh->m_timerSymbolLink.InitTimer(Insulation::FindSymbolLynk, pVuh);

	CPoolCompletingIrp::Initialization();
	return status;
}

VOID UnloadDriver (IN PDRIVER_OBJECT DriverObject)
{
	ASSERT(DriverObject->DeviceObject == NULL);
	DriverBase::CBaseDeviceExtension::Unload();

	CPoolCompletingIrp::Shutdown();
	Thread::Sleep(1000);
}