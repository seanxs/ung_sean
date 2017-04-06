/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    usbstub.h

Abstract:

    This module contains the entry points for a USB stub.

Environment:

    Kernel mode

--*/
#ifndef	FILTER_USB_HUB_H
#define FILTER_USB_HUB_H

#include "Core\BaseDeviceExtension.h"

extern "C" {

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
FilterAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );

};

VOID
FilterUnload(
    IN PDRIVER_OBJECT DriverObject
    );
#endif