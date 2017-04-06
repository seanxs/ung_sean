/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ControlDeviceExtension.h

Abstract:

   Device class that third ring interacts with. 
     - intercept virtual USB device
     - release virtual USB device
     - data transfer between real and virtual devices

Environment:

    Kernel mode

--*/
#ifndef ControlDeviceExtension_Class
#define ControlDeviceExtension_Class

#include "Core\BaseDeviceExtension.h"
#include "Core\CounterIrp.h"
#include "Core\list.h"
#include "Core\lock.h"
#include "UsbPortToTcp.h"
#include "Common\public.h"
#include "PdoStubExtension.h"

class CUsbPortToTcp;

class CControlDeviceExtension : public DriverBase::CBaseDeviceExtension
{
// variable
protected:
	UNICODE_STRING	m_NameString; 
	UNICODE_STRING	m_LinkString; 


// variable
public:
	CList			m_listUsbPortToTcp;
	CList			m_listUsbHub;
	CCounterIrp		m_IrpCounter;
	CSpinLock		m_spinUsb;
	CSpinLock		m_spinHub;

public:
	CControlDeviceExtension ();

	// type driver
    static ULONG	m_DriverType;

// virtual major function
public:
	virtual NTSTATUS DispatchCreate				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchClose				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDevCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchCleanup			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDefault			(PDEVICE_OBJECT DeviceObject, PIRP Irp);

public: 
	// Create/delete device
	virtual NTSTATUS CreateDevice (PDRIVER_OBJECT DriverObject);
	virtual NTSTATUS DeleteDevice ();
	// get control device
	static CControlDeviceExtension* GetControlExtension ();
	// work with PDO
	NTSTATUS AddUsbPortToTcp (PUSB_DEV_SHARE	pUsbDev, PIRP Irp);
	NTSTATUS RemoveUsbPortToTcp (PUSB_DEV_SHARE	pUsbDev, PIRP Irp);
	NTSTATUS SetWaitIrp (PIRP Irp);
	NTSTATUS GetInstanceId (PUSB_DEV_SHARE HubDev);
	NTSTATUS PutIrpToDevice (PIRP Irp, PBYTE Buffer, LONG lSize);
	NTSTATUS GetIrpToAnswer (PIRP Irp);
    NTSTATUS DeviceIsPresent (PIRP Irp);
	NTSTATUS DeviceCancel (PIRP Irp);
    NTSTATUS InitUsbDecription (PIRP Irp);
	NTSTATUS GetDeviceId (PIRP Irp);
	NTSTATUS AttachToUsbHub (PUSB_DEV_SHARE HubDev);
	NTSTATUS BusDeviceRelations (PUSB_DEV_SHARE HubDev);

	PVOID GetObjectByPath(PWSTR pwszObjectName);

	TPtrUsbPortToTcp GetUsbPortToTcp (PUSB_DEV_SHARE	pUsbDev);
	TPtrUsbPortToTcp GetUsbPortToTcp (CPdoStubExtension	*pPdo);
	TPtrPdoStubExtension FindPdo (PUSB_DEV_SHARE	pUsbDev);
	TPtrPdoStubExtension FindPdoUsbPort (PUSB_DEV_SHARE	pUsbDev);
	TPtrPdoStubExtension FindPdoUsbDev (PUSB_DEV_SHARE	pUsbDev);

	VOID CancelControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
	void ClearListUsbPortToTcp ();

	NTSTATUS CycleUsbPort(PDEVICE_OBJECT DeviceObject);
	NTSTATUS DeviceRestart (PUSB_DEV_SHARE pUsbDev);

	static VOID CancelWaitIrp ( PDEVICE_OBJECT DeviceObject, PIRP Irp );
	static VOID CancelIrpToAnswer ( PDEVICE_OBJECT DeviceObject, PIRP Irp );

	NTSTATUS DetectDevice (PUSB_DEV_SHARE pUsbDev);

// new pool
	NTSTATUS SetPool (PIRP Irp, CPoolListDrv &pool);
	NTSTATUS ClrPool (CPoolListDrv &pool);
};

#endif
