/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   fusbhub.h

Abstract:

   USB device filter class. It is used for USB devices interception.

Environment:

    Kernel mode

--*/
#ifndef PDO_STUB_DEVICE_EXTENSION
#define PDO_STUB_DEVICE_EXTENSION

#include "Core\FilterExtension.h"
#include "Core\DeviceRelations.h"
#include "fusbhub.h"
#include "urb.h"
#include "UsbPortToTcp.h"
#include "Core\UnicodeString.h"

// usb
#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)
#include <strmini.h>
#include <ksmedia.h>
#include "usbdi.h"
#include "usbcamdi.h"
#pragma pack(pop)
}
#undef DebugPrint

class CUsbPortToTcp;
class CDeviceRelations;
class CPdoRedirectExtension;

class CPdoStubExtension : public CFilterExtension , /*public CDeviceRelations, */public CPdoBase
{
// QueryDeviceId
public:
	static WCHAR *m_szBusQueryDeviceID;
	static int m_iSizeBusQueryDeviceID;
	static WCHAR *m_szBusQueryHardwareIDs;
	static int m_iSizeBusQueryHardwareIDs;
	static WCHAR *m_szBusQueryCompatibleIDs;
	static int m_iSizeBusQueryCompatibleIDs;
	static WCHAR *m_szDeviceTextDescription;
	static int m_iSizeDeviceTextDescription;
// variable
public:
	ULONG				m_uTcpPort;
	ULONG				m_uDeviceAddress;
	TPtrUsbPortToTcp	m_pParent;
	CDeviceRelations	*m_pBus;
	bool				m_bIsFilter;
	bool				m_bDelete;
	bool				m_bMain;
	bool				m_bRestart;

	CUnicodeString		m_szCompapatible;

	KEVENT				m_eventRemove;
	WCHAR				m_szDevId [200];
	PDEVICE_OBJECT		m_PhysicalDeviceObject;
	ULONG				m_uInstance;
    CList                               m_listInterface;
	USBD_CONFIGURATION_HANDLE	        m_ConfigurationHandle;
    USB_BUS_INTERFACE_USBDI_V3_FULL     m_Interface;
    USBCAMD_INTERFACE                   m_InterfaceUsbCamd;
    LONG                                m_IncInterface;
    LONG                                m_IncInterfaceUsbCamd;
	USB_DEVICE_DESCRIPTOR				m_DeviceDescription;

	CSpinLock	m_spinPdo;

public:
	CPdoStubExtension (CDeviceRelations *pBus);
	virtual ~CPdoStubExtension ();
	NEW_ALLOC_DEFINITION ('ESPE')
	//void StartDevice ();

public:
    USBD_PIPE_HANDLE_DIFF GetPipeHandleDiff (USBD_PIPE_HANDLE Handle);
    USBD_PIPE_HANDLE GetPipeHandle (USBD_PIPE_HANDLE_DIFF Handle);
    LONG GetPipeType (ULONG64 Handle, UCHAR *Class);
	UCHAR GetPipeEndpoint (ULONG64 Handle, bool bLock = false);
        
    NTSTATUS SetNewInterface (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo);
	NTSTATUS InitInterfaceInfo (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo, USHORT uCount);
    void ClearInterfaceInfo ();
	// reset device
	void ResetInterface (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo);
	bool RequestPipe (PDEVICE_OBJECT  DeviceObject, USBD_PIPE_HANDLE pipeHandle, USHORT iFunction);
	static NTSTATUS SendUrb (PDEVICE_OBJECT  DeviceObject, PURB urb);
	static NTSTATUS SendUrb (PDEVICE_OBJECT  DeviceObject, PURB urb, LARGE_INTEGER &TimeOut);
	static bool GetUsbDeviceDesc (PDEVICE_OBJECT PhysicalDeviceObject, USB_DEVICE_DESCRIPTOR *pDevDesk);
	static WCHAR* GetQueryId (PDEVICE_OBJECT PhysicalDeviceObject, BUS_QUERY_ID_TYPE Type); // IRP_MN_QUERY_ID
	bool InitDeviceDesc ();
	void ClearDeviceDesc ();
	//void FindShare ();
	
	NTSTATUS ResetUsbConfig ();

// virtual major function
public:
	virtual NTSTATUS DispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDefault			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPower				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchSysCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);


	virtual NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);

public:
	// Additional PnP functions
	NTSTATUS PnpQueryDeviceId (PIRP Irp);
	NTSTATUS PnpQueryDeviceRelations (PIRP Irp);
	NTSTATUS PnpQueryDeviceCaps (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	NTSTATUS PnpStart (PIRP Irp);
	NTSTATUS PnpRemove (PIRP Irp);

	// completion routine
	static NTSTATUS CompletionRoutineDeviceCaps (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
	static NTSTATUS CompletionRoutineDeviceId (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
    static NTSTATUS CompletionRoutineDeviceText (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

	static void ApplyXPSilentInstallHack (ULONG uInstance);

	// Before init device 
	static NTSTATUS IrpCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

    NTSTATUS CallUSBD(IN PURB Urb);
    NTSTATUS ReadandSelectDescriptors();
    NTSTATUS ConfigureDevice();

    static NTSTATUS IoCompletionSync(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context);

	void InitUsbInterace ();
	void BuildDevId ();

	NTSTATUS GetInstanceId (WCHAR *szId, LONG lSize);
	NTSTATUS DeconfigureDevice(PDEVICE_OBJECT DeviceObject);
	static VOID UsbIdleCallBack(PVOID Context);

	bool WaitRemoveDevice (int iMSec);
};

typedef boost::intrusive_ptr <CPdoStubExtension> TPtrPdoStubExtension;

typedef struct _DEVOBJ_EXTENSION_EX {

    CSHORT          Type;
    USHORT          Size;

    //
    // Public part of the DeviceObjectExtension structure
    //

    PDEVICE_OBJECT  DeviceObject;               // owning device object

// end_ntddk end_nthal end_ntifs end_wdm end_ntosp

    //
    // Universal Power Data - all device objects must have this
    //

    ULONG           PowerFlags;             // see ntos\po\pop.h
                                            // WARNING: Access via PO macros
                                            // and with PO locking rules ONLY.

    //
    // Pointer to the non-universal power data
    //  Power data that only some device objects need is stored in the
    //  device object power extension -> DOPE
    //  see po.h
    //

    struct          _DEVICE_OBJECT_POWER_EXTENSION  *Dope;

    //
    // power state information
    //

    //
    // Device object extension flags.  Protected by the IopDatabaseLock.
    //

    ULONG ExtensionFlags;

    //
    // PnP manager fields
    //

    PVOID           DeviceNode;

    //
    // AttachedTo is a pointer to the device object that this device
    // object is attached to.  The attachment chain is now doubly
    // linked: this pointer and DeviceObject->AttachedDevice provide the
    // linkage.
    //

    PDEVICE_OBJECT  AttachedTo;

    //
    // The next two fields are used to prevent recursion in IoStartNextPacket
    // interfaces.
    //

    LONG           StartIoCount;       // Used to keep track of number of pending start ios.
    LONG           StartIoKey;         // Next startio key
    ULONG          StartIoFlags;       // Start Io Flags. Need a separate flag so that it can be accessed without locks
    PVPB           Vpb;                // If not NULL contains the VPB of the mounted volume.
                                       // Set in the filesystem's volume device object.
                                       // This is a reverse VPB pointer.

// begin_ntddk begin_wdm begin_nthal begin_ntifs begin_ntosp

} DEVOBJ_EXTENSION_EX, *PDEVOBJ_EXTENSION_EX;


#endif
