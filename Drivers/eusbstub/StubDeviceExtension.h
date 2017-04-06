/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BaseDeviceExtension.h

Abstract:

   Stub class that appears in the system instead of captured USB device

Environment:

    Kernel mode

--*/
#ifndef STUB_DEVICE_EXTENSION
#define STUB_DEVICE_EXTENSION

#include "core\BaseDeviceExtension.h"
// usb
#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)
#include "usbdi.h"
#include "usbdlib.h"
#include "usbioctl.h"
#ifdef AMD64
	#include "usbbusif.h"
#else
	//#include "USB\w2k\usbbusif.h"
#endif
#include "usbioctl.h"
#pragma pack(pop)
}


class CStubDeviceExtension : public DriverBase::CBaseDeviceExtension
{
public:
	CStubDeviceExtension ();
// Variable
public:
	// The top of the stack before this filter was added.
    PDEVICE_OBJECT	m_NextLowerDriver;

// virtual major functions
public:
	virtual NTSTATUS DispatchPower				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchSysCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchOther				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDefault			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    virtual NTSTATUS DispatchDevCtrl            (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchCreate	            (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchClose	            (PDEVICE_OBJECT DeviceObject, PIRP Irp);

public:
	// completion routine
	static NTSTATUS DispatchPnpComplete (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
	// 	Attach to Device
	virtual NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);

    void SendAdditionPnpRequest (PDEVICE_OBJECT DeviceObject);
    NTSTATUS CallUSBD(IN PURB Urb);
    NTSTATUS ReadandSelectDescriptors();
    NTSTATUS ConfigureDevice();
    NTSTATUS CallPnp(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION ioStack, PVOID *Info);
    static VOID InitUsbDev (PDEVICE_OBJECT DeviceObject, PVOID Context);

};

typedef struct _WOTK_ITEM
{
    PIO_WORKITEM WorkItem;
    PVOID        pContext;
}WOTK_ITEM, *PWOTK_ITEM;


#endif