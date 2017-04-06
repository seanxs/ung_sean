/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    fusbhub.h

Abstract:

    This module contains the entry points for a USB device filter.

Environment:

    Kernel mode

--*/
#ifndef	FILTER_USB_HUB_H
#define FILTER_USB_HUB_H

#include "Core\BaseDeviceExtension.h"

#include <stdio.h>
#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)
//#include <ntddk.h>
#include "usbdi.h"
#include "usbdlib.h"
#include "usbioctl.h"
#include "usbbusif.h"

#pragma pack(pop)


#define tagUsb 'FUHB'

extern GUID GuidUsb;
extern GUID GuidUsbCamd;

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

BOOLEAN DeviceIsHub (
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject,
	OUT	BOOLEAN		  &bHubDetect
	);

BOOLEAN DeviceIsHubPnp (
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject,
	OUT	BOOLEAN		  &bHubDetect
	);

NTSTATUS
GetDeviceId(
    //IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject,
	IN BUS_QUERY_ID_TYPE iType,
	OUT IO_STATUS_BLOCK	*pStatus
	);

VOID
FilterUnload(
    IN PDRIVER_OBJECT DriverObject
    );

typedef struct _OBJECT_NAMETYPE_INFO 
{               
  UNICODE_STRING ObjectName;
  UNICODE_STRING ObjectType;
} OBJECT_NAMETYPE_INFO, *POBJECT_NAMETYPE_INFO;   

NTSYSAPI
NTSTATUS
NTAPI
ZwOpenDirectoryObject (
    OUT PHANDLE             DirectoryHandle,
    IN ACCESS_MASK          DesiredAccess,
    IN POBJECT_ATTRIBUTES   ObjectAttributes
);

ZwQueryDirectoryObject (
    IN HANDLE       DirectoryHandle,
    OUT PVOID       Buffer,
    IN ULONG        Length,
    IN BOOLEAN      ReturnSingleEntry,
    IN BOOLEAN      RestartScan,
    IN OUT PULONG   Context,
    OUT PULONG      ReturnLength OPTIONAL
);

extern POBJECT_TYPE IoDriverObjectType;

NTSTATUS ObReferenceObjectByName ( 
     IN PUNICODE_STRING ObjectName, 
     IN ULONG Attributes, 
     IN PACCESS_STATE AccessState OPTIONAL, 
     IN ACCESS_MASK DesiredAccess OPTIONAL, 
     IN POBJECT_TYPE ObjectType, 
     IN KPROCESSOR_MODE AccessMode, 
     IN OUT PVOID ParseContext OPTIONAL, 
     OUT PVOID *Object 
); 

POBJECT_TYPE ObGetObjectType(PVOID Object); 

#if (NTDDI_VERSION >= NTDDI_WIN2K)
NTKERNELAPI
NTSTATUS
ObQueryNameString(
    __in PVOID Object,
    __out_bcount_opt(Length) POBJECT_NAME_INFORMATION ObjectNameInfo,
    __in ULONG Length,
    __out PULONG ReturnLength
    );
#endif

#if (_WIN32_WINNT < 0x0501)

    #define IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX  \
                                    CTL_CODE(FILE_DEVICE_USB,  \
                                    USB_GET_NODE_CONNECTION_INFORMATION_EX,  \
                                    METHOD_BUFFERED,  \
                                    FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct _USB_NODE_CONNECTION_INFORMATION_EX {
    ULONG ConnectionIndex;  /* INPUT */
    /* usb device descriptor returned by this device
       during enumeration */
    USB_DEVICE_DESCRIPTOR DeviceDescriptor;/* OUTPUT */
    UCHAR CurrentConfigurationValue;/* OUTPUT */
    /* values for the speed field are defined in USB200.h */
    UCHAR Speed;/* OUTPUT */
    BOOLEAN DeviceIsHub;/* OUTPUT */
    USHORT DeviceAddress;/* OUTPUT */
    ULONG NumberOfOpenPipes;/* OUTPUT */
    USB_CONNECTION_STATUS ConnectionStatus;/* OUTPUT */
    USB_PIPE_INFO PipeList[0];/* OUTPUT */
} USB_NODE_CONNECTION_INFORMATION_EX, *PUSB_NODE_CONNECTION_INFORMATION_EX;
#pragma pack(pop)

#endif

};

#endif