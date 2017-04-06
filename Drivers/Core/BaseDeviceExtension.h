/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BaseDeviceExtension.h

Abstract:

  Basic class of kernel driver framework. Includes all MajorFunction handlers.

Environment:

    Kernel mode

--*/
#ifndef CBaseDeviceExtension_CLASS
#define CBaseDeviceExtension_CLASS

#include "BaseClass.h"
#include "UnicodeString.h"

namespace DriverBase
{

typedef enum _DEVICE_PNP_STATE 
{

    NotStarted = 0,         // Not started yet
	Starting,
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
    Deleted                 // Device has received the REMOVE_DEVICE IRP
} DEVICE_PNP_STATE;



class CBaseDeviceExtension : public virtual CBaseClass
{
public:
	CBaseDeviceExtension ();
	virtual ~CBaseDeviceExtension ();
	NEW_ALLOC_DEFINITION ('EDBC')

protected:
	// Driver object
	static PDRIVER_OBJECT		m_DriverObject;
	static UNICODE_STRING		m_strRegistry;
	static bool					m_bInit;

public:
	DEVICE_PNP_STATE GetPnpState() { return m_DevicePnPState; }
	PDEVICE_OBJECT GetDeviceObject() { return m_Self; }
// Variable
protected:
	// self
    PDEVICE_OBJECT	m_Self;
    // current PnP state of the device
    DEVICE_PNP_STATE  m_DevicePnPState;
    // Remembers the previous pnp state
    DEVICE_PNP_STATE m_PreviousPnPState;

public:
// Driver Enter: init major function
	static void Init (PDRIVER_OBJECT  DriverObject, PUNICODE_STRING strRegistry);
	static void Unload ();
	static CBaseDeviceExtension* GetExtensionClass (PDEVICE_OBJECT DeviceObject);
	static PDRIVER_OBJECT GetDriverObject () {return m_DriverObject;}
	static PUNICODE_STRING GetRegistryPath () {return &m_strRegistry;}

public:
	static NTSTATUS stDispatchCreate			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchClose				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchRead				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchWrite				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchDevCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchShutdown			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchCleanup			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchPower				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchSysCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static NTSTATUS stDispatchOther				(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// virtual major function
public:
	virtual NTSTATUS DispatchCreate				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchClose				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchRead				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchWrite				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDevCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchShutdown			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchCleanup			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPower				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchSysCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchOther				(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// default DispatchFunction 
public:
	virtual NTSTATUS DispatchDefault			(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// plug and play status
public:
	void SetNewPnpState (DEVICE_PNP_STATE NewState);
	void RestorePnpState ();

	void DeleteDevice ();

	// additional functions
public:
	// Pnp minor function to string
	static PCHAR PnPMinorFunctionString (PIRP Irp);
	static PCHAR PnPMinorFunctionString (UCHAR cMinorFunction);
	// Pnp IRP_MN_QUERY_ID
	static PCHAR PnpQueryIdString(BUS_QUERY_ID_TYPE Type);
	// Major function to string
	static PCHAR MajorFunctionString (PIRP Irp);
	// Power minor function to string
	static PCHAR PowerMinorFunctionString (UCHAR MinorFunction);
	// System power to string
	static PCHAR SystemPowerString(SYSTEM_POWER_STATE Type);
	// system Device power to string
	static PCHAR DevicePowerString(DEVICE_POWER_STATE Type);

protected:
	static NTSTATUS Complete (PIRP pIrp, NTSTATUS status);
};

};

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#endif