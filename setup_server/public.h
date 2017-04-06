/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    public.h

Abstract:

	Описание ICTRL и структур для взаимодействия с 3 кольцом

Environment:

    Kernel/user mode

--*/
	
#ifndef __PUBLIC_H
#define __PUBLIC_H


#define USBHUB_FILTER_IOCTL(_index_) \
    CTL_CODE (FILE_DEVICE_UNKNOWN, _index_, METHOD_BUFFERED, FILE_ANY_ACCESS)

//#define IOCTL_USBHUB_START					USBHUB_FILTER_IOCTL (0x0)
//#define IOCTL_USBHUB_STOP					USBHUB_FILTER_IOCTL (0x1)
#define IOCTL_USBHUB_ADD_DEV				USBHUB_FILTER_IOCTL (0x2)
#define IOCTL_USBHUB_REMOVE_DEV				USBHUB_FILTER_IOCTL (0x3)
#define IOCTL_USBHUB_WAIT_INSERT_DEV		USBHUB_FILTER_IOCTL (0x4)
#define IOCTL_USBHUB_IRP_TO_0_RING			USBHUB_FILTER_IOCTL (0x5)
#define IOCTL_USBHUB_IRP_TO_3_RING			USBHUB_FILTER_IOCTL (0x6)
#define IOCTL_USBHUB_DEVICE_PRESENT			USBHUB_FILTER_IOCTL (0x7)
#define IOCTL_USBHUB_CANCEL					USBHUB_FILTER_IOCTL (0x8)
#define IOCTL_USBHUB_DEL_CONTROL			USBHUB_FILTER_IOCTL (0x9)
#define IOCTL_USBHUB_INIT_USB_DESC			USBHUB_FILTER_IOCTL (0x10)
#define IOCTL_USBHUB_GET_DEV_ID				USBHUB_FILTER_IOCTL (0x11)
#define IOCTL_USBHUB_ATTACH_TO_HUB			USBHUB_FILTER_IOCTL (0x12)

typedef struct _USB_DEV
{
	WCHAR	HubDriverKeyName [200];
	ULONG	DeviceAddress;
	ULONG	ServerIpPort;
	ULONG	Status;
}USB_DEV, *PUSB_DEV;

typedef struct _IO_STACK_LOCATION_SAVE {
    UCHAR MajorFunction;
    UCHAR MinorFunction;

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
//
#pragma pack(push, 8)

typedef struct _IRP_SAVE
{
	LONG					Size;			// size current buffer
	LONG					NeedSize;		// need size buffer
	LONG64					Device;			// Indefication of device
    BYTE					Is64:1;			// Detect 64
    BYTE                    IsIsoch:1;      // isoch data
    BYTE                    Res1:6;         // Reserv
	LONG					Irp;			// Number Irp
	NTSTATUS				Status;			// current status Irp
	ULONG64   				Information;
	BOOL					Cancel;			// Cancel irp flag
	IO_STACK_LOCATION_SAVE	StackLocation;	// Stack location info
	LONG					BufferSize;
	LONG					Reserv;
	BYTE					Buffer [1];		// Data
}IRP_SAVE, *PIRP_SAVE;

#pragma pack(pop)


#endif


