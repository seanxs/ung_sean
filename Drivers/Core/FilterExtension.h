/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   FilterExtension.h

Abstract:

	Basic class of device filter

Environment:

    Kernel mode

--*/
#ifndef FILTER_EXTENSION_DRIVER
#define FILTER_EXTENSION_DRIVER

#include "BaseDeviceExtension.h"
#include "CounterIrp.h"

class CFilterExtension : public DriverBase::CBaseDeviceExtension
{
public:
	CFilterExtension  ();
	virtual ~CFilterExtension ();
	NEW_ALLOC_DEFINITION ('EFC')

// Variable
public:
	// The top of the stack before this filter was added.
    PDEVICE_OBJECT	m_NextLowerDriver;
	CCounterIrp	m_irpCounter;

// virtual major function
public:
	virtual NTSTATUS DispatchPower				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDevCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// default DispatchFunction 
public:
	virtual NTSTATUS DispatchDefault			(PDEVICE_OBJECT DeviceObject, PIRP Irp);

	// completion routine
	static NTSTATUS FilterStartCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

	// 	Attach to Device
	virtual NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);
	virtual NTSTATUS DetachFromDevice();

#if DBG
	PCHAR UsbIoControl (LONG lFunc);
#endif
};
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
#endif

#endif