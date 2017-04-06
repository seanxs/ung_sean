/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    evuh.h

Abstract:

    This module contains the entry points for a virtual hub driver.

Environment:

    Kernel mode

--*/
#ifndef	ELTIMA_VIRTUAL_USB_HUB
#define ELTIMA_VIRTUAL_USB_HUB

#include "Core\BaseClass.h"

#pragma warning( disable : 4200 )


extern "C" {
// usb
#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)

#include "usbdi.h"
#include "usbdlib.h"
#include "usbioctl.h"
#include "urb.h"
#include "usbbusif.h"
#include "usbioctl.h"

#pragma pack(pop)
}
#define tagUsb 'evuh'
#include "Common\Public.h"
#include "private.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );

VOID
UnloadDriver(
    IN PDRIVER_OBJECT DriverObject
    );

#define LOG_WAIT

};

#endif