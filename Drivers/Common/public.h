/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    public.h

Abstract:

	Description of ICTRL and structures for interaction with third ring

Environment:

    Kernel/user mode

--*/
	
#ifndef __PUBLIC_H
#define __PUBLIC_H

#define VUHUB_IOCTL(_index_) \
    CTL_CODE (FILE_DEVICE_UNKNOWN, _index_, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define VUHUB_IOCTL_NEITHER(_index_) \
	CTL_CODE (FILE_DEVICE_UNKNOWN, _index_, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_VUHUB_ADD						VUHUB_IOCTL (0x0)		//0x220000
#define IOCTL_VUHUB_REMOVE					VUHUB_IOCTL (0x1)		//0x220004
#define IOCTL_VUHUB_IRP_TO_TCP				VUHUB_IOCTL (0x2)		//0x220008
#define IOCTL_VUHUB_IRP_ANSWER_TCP			VUHUB_IOCTL (0x3)		//0x22000C
#define IOCTL_VUHUB_SET_DOSDEV_PREF			VUHUB_IOCTL (0x4)		//0x22000C
#define IOCTL_VUHUB_SET_USER_SID			VUHUB_IOCTL (0x5)		//0x22000C

#define IOCTL_VUHUB_IRP_TO_TCP_SET			VUHUB_IOCTL_NEITHER (0x6)		//0x220008
#define IOCTL_VUHUB_IRP_TO_TCP_CLR			VUHUB_IOCTL_NEITHER (0x7)		//0x220008
#define IOCTL_VUHUB_IRP_ANSWER_SET			VUHUB_IOCTL_NEITHER (0x8)		//0x22000C
#define IOCTL_VUHUB_IRP_ANSWER_CLR			VUHUB_IOCTL_NEITHER (0x9)		//0x22000C
#define IOCTL_VUHUB_VERSION					VUHUB_IOCTL (0x10)		//0x22000C

#define USBHUB_FILTER_IOCTL(_index_) \
    CTL_CODE (FILE_DEVICE_UNKNOWN, _index_, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define USBHUB_FILTER_IOCTL_NEITHER(_index_) \
	CTL_CODE (FILE_DEVICE_UNKNOWN, _index_, METHOD_NEITHER, FILE_ANY_ACCESS)


#define IOCTL_USBHUB_ADD_DEV				USBHUB_FILTER_IOCTL (0x02)		
#define IOCTL_USBHUB_REMOVE_DEV				USBHUB_FILTER_IOCTL (0x03)
#define IOCTL_USBHUB_WAIT_INSERT_DEV		USBHUB_FILTER_IOCTL (0x04)
#define IOCTL_USBHUB_IRP_TO_0_RING			USBHUB_FILTER_IOCTL (0x05)
#define IOCTL_USBHUB_IRP_TO_3_RING			USBHUB_FILTER_IOCTL (0x06)
#define IOCTL_USBHUB_DEVICE_PRESENT			USBHUB_FILTER_IOCTL (0x07)
#define IOCTL_USBHUB_CANCEL					USBHUB_FILTER_IOCTL (0x08)
//#define IOCTL_USBHUB_DEL_CONTROL			USBHUB_FILTER_IOCTL (0x09)
#define IOCTL_USBHUB_INIT_USB_DESC			USBHUB_FILTER_IOCTL (0x10)
#define IOCTL_USBHUB_GET_DEV_ID				USBHUB_FILTER_IOCTL (0x11)
#define IOCTL_USBHUB_ATTACH_TO_HUB			USBHUB_FILTER_IOCTL (0x12)
#define IOCTL_USBHUB_DEVICE_RELATIONS		USBHUB_FILTER_IOCTL (0x13)
#define IOCTL_USBHUB_DEVICE_INSTANCE_ID		USBHUB_FILTER_IOCTL (0x14)
#define IOCTL_USBHUB_DEVICE_RESTART			USBHUB_FILTER_IOCTL (0x15)
#define IOCTL_USBHUB_IRP_TO_3_RING_SET		USBHUB_FILTER_IOCTL_NEITHER (0x16)
#define IOCTL_USBHUB_IRP_TO_3_RING_CLR		USBHUB_FILTER_IOCTL_NEITHER (0x17)
#define IOCTL_USBHUB_IRP_TO_0_RING_SET		USBHUB_FILTER_IOCTL_NEITHER (0x18)
#define IOCTL_USBHUB_IRP_TO_0_RING_CLR		USBHUB_FILTER_IOCTL_NEITHER (0x19)
#define IOCTL_USBHUB_DEVICE_IS_SHARED		USBHUB_FILTER_IOCTL (0x20)
#define IOCTL_USBHUB_VERSION				USBHUB_FILTER_IOCTL (0x21)		//0x22000C



typedef struct _USB_DEV
{
	ULONG	HostIp;
	ULONG	DeviceAddress;
	ULONG	ServerIpPort;
}USB_DEV, *PUSB_DEV;

typedef struct _USB_DEV_SARE
{
	WCHAR	HubDriverKeyName [200];
	ULONG	DeviceAddress;
	ULONG	ServerIpPort;
	ULONG	Status;
}USB_DEV_SHARE, *PUSB_DEV_SHARE;

#pragma pack(push, 1)

typedef struct _IO_STACK_LOCATION_SAVE {
    UCHAR MajorFunction;
    UCHAR MinorFunction;
	UCHAR Reserved0 [6];

	union {
        struct {
            ULONG64 Argument1;
            ULONG64 Argument2;
            ULONG64 Argument3;
            ULONG64 Argument4;
        } Others;

    } Parameters;

} IO_STACK_LOCATION_SAVE, *PIO_STACK_LOCATION_SAVE;
typedef LONG NTSTATUS, *PNTSTATUS;
typedef unsigned char BYTE;
typedef int BOOL;

#pragma warning( disable : 4200 )
//
typedef struct _IRP_SAVE
{
	LONG					Size;			// size of current buffer
	LONG					NeedSize;		// needed buffer size
	LONG64					Device;			// Identification of device
    BYTE					Is64:1;			// Detect 64
    BYTE                    IsIsoch:1;      // isoch data
	BYTE					NoAnswer:1;		// Is don't need answer
	BYTE					Dispath:1;		// Dispatch
    BYTE                    Res2:4;         // Reserve
	BYTE					Reserved0[3];
	ULONG					Irp;			// IRP Number
	NTSTATUS				Status;			// current IRP status
	LONG					Reserved1;
	ULONG64   				Information;
	BOOL					Cancel;			// Cancel IRP flag
	LONG					Reserved2;
	IO_STACK_LOCATION_SAVE	StackLocation;	// Stack location info
	LONG					BufferSize;
	LONG					Reserved3;
	BYTE					Buffer [0];		// Data
}IRP_SAVE, *PIRP_SAVE;

#pragma pack(pop)

enum
{
	FI_NULL,
	FI_InterfaceReference,
	FI_InterfaceDereference,
	FI_GetUSBDIVersion,
	FI_QueryBusTime,
	FI_SubmitIsoOutUrb,
	FI_QueryBusInformation,
	FI_IsDeviceHighSpeed,
	FI_EnumLogEntry,
    FI_QueryBusTimeEx,
    FI_QueryControllerType,
    FI_USBCAMD_InterfaceReference,
    FI_USBCAMD_InterfaceDereference,
    FI_USBCAMD_WaitOnDeviceEvent,
    FI_USBCAMD_BulkReadWrite,
    FI_USBCAMD_SetIsoPipeState,
    FI_USBCAMD_CancelBulkReadWrite,
    FI_USBCAMD_SetVideoFormat,
	FI_ISOCH_OUT_TIMER,
	FI_USB_SUBMIT_IDLE_NOTIFICATION,
};

#endif
