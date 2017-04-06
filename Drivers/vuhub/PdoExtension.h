/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PdoExtension.h

Abstract:

	Virtual USB device class.

Environment:

    Kernel mode

--*/
#ifndef PdoExtension_H
#define PdoExtension_H

#include "Core\BaseDeviceExtension.h"
#include "Core\CounterIrp.h"
#include "Core\List.h"
#include "Core\DeviceRelations.h"
#include "Core\Lock.h"
#include "Core\Thread.h"
#include "Common\Public.h"
#include "UsbDevToTcp.h"

// usb
#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)
#include "usbdi.h"
#include "usbdlib.h"
#include "usbioctl.h"
#include "usbbusif.h"
#include "usbioctl.h"
#pragma pack(pop)
}

class CVirtualUsbHubExtension;
namespace evuh 
{
	class CUsbDevToTcp;
};

class CPdoExtension : public DriverBase::CBaseDeviceExtension, public virtual CDeviceRelations
{
	friend class CVirtualUsbHubExtension;
	friend class CHubStubExtension;

public:
	CPdoExtension ();
	~CPdoExtension ();

	NEW_ALLOC_DEFINITION ('ODP')	// varible
public:
	SECURITY_DESCRIPTOR		m_sd;
	PACL					m_pAcl;
	CSpinLock				m_spin;

	CCounterIrp			m_irpCounterPdo;

	CCounterIrp			m_irpTcp;

    // Stores the current system power state
    SYSTEM_POWER_STATE  m_SystemPowerState;
    // Stores current device power state
    DEVICE_POWER_STATE  m_DevicePowerState;
	// Parent class
	CVirtualUsbHubExtension *m_pParent;
    //PLIST_ENTRY			m_pLink;

	// Unic
	ULONG64				m_DeviceIndef;
	// TCP Layer
	evuh::CUsbDevToTcp		*m_pTcp;
	// This PDO is present
	bool				m_bPresent;
	bool				m_bInit;

	// param
	ULONG				m_HostIp;
	ULONG				m_uServerPort;
	ULONG				m_uAddressDevice;
	bool				m_bCardReader;
	bool				m_bDisableAutoComplete;
	ULONG				m_uNumberPort;

	// Addition param
	// IRP_MN_QUERY_DEVICE_TEXT
	PVOID				m_pQuyryDeviceText_Description;
	ULONG				m_uQuyryDeviceText_Description;
	PVOID				m_pQuyryDeviceText_LocalInf;
	ULONG				m_uQuyryDeviceText_LocalInf;
	ULONG				m_uQuyryDeviceText_uLocalId;
	// IRP_MN_QUERY_ID
	PVOID				m_pQueryId_BusQueryDeviceID;
	ULONG				m_uQueryId_BusQueryDeviceID;
	PVOID				m_pQueryId_BusQueryHardwareIDs;
	ULONG				m_uQueryId_BusQueryHardwareIDs;
	PVOID				m_pQueryId_BusQueryCompatibleIDs;
	ULONG				m_uQueryId_BusQueryCompatibleIDs;
	PVOID				m_pQueryId_BusQueryInstanceID;
	ULONG				m_uQueryId_BusQueryInstanceID;
	// IRP_MN_QUERY_INTERFACE
    UNICODE_STRING          m_LinkInterface;
    USB_DEVICE_DESCRIPTOR   m_DeviceDescriptor;

	// event Remote
	KEVENT				*m_pEventRomove;

	bool IsPnpReady(){ return m_bPresent & m_bInit; }
	//struct IRP_NUMBER;
	void InitPnpIrp (PIRP pIrp, char cMajor, char cMinor);
	bool SendPnpIrp (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber);

	bool PnpBusQueryDeviceID (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber);
	bool PnpBusQueryHardwareIDs (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber);
	bool PnpBusQueryCompatibleIDs (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber);
	bool PnpBusQueryInstanceID (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber);

	bool PnpDeviceTextDescription (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber);
	bool PnpDeviceTextLocationInformation (evuh::CUsbDevToTcp  *pTcp, PIRP_NUMBER pNumber);

	NTSTATUS SendDate (evuh::CUsbDevToTcp  *pTcp, PVOID irpNumber, KEVENT	*Event);
	NTSTATUS InitConstParam ();
	static VOID IoWorkItmInit (PDEVICE_OBJECT DeviceObject, PVOID Context);
	static VOID stSetEvent(PDEVICE_OBJECT DeviceObject, PVOID Context);


	// init global param
	virtual TPtrPdoBase AddPdo (PDEVICE_OBJECT PhysicalDeviceObject);

public:
	// default DispatchFunction 
	virtual NTSTATUS DispatchPower				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchCreate			    (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchClose				(PDEVICE_OBJECT DeviceObject, PIRP Irp);

	virtual NTSTATUS DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp);

	virtual NTSTATUS DispatchDefaultTcp			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDefault			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	// Create and delete device PDO
    NTSTATUS CreateDevice (PUSB_DEV	Param);    
	NTSTATUS CreateDevice ();    
	NTSTATUS DeleteDevice ();    

	static bool CancelUrb(PIRP pIrp, NTSTATUS Status = STATUS_CANCELLED, NTSTATUS UsbdStatus = USBD_STATUS_CANCELED);

	NTSTATUS PnpDeivceRelations (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	NTSTATUS PnpDeivceText (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	NTSTATUS PnpQueryId (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	NTSTATUS PnpRemoveDevice (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	NTSTATUS PnpStartDevice (PDEVICE_OBJECT DeviceObject, PIRP Irp);

	void IncMyself ()
	{
		intrusive_ptr_add_ref (this);
	}

	void DecMyself ()
	{
		intrusive_ptr_release (this);
	}

private:
	static NTSTATUS IoCompletionErrCancel(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context);
	PVOID CopyToNonPagedMemory (PVOID Dest, ULONG uSize);

	NTSTATUS GetControllerName (PIRP pIrp);
	NTSTATUS GetHubName (PIRP pIrp);

// work with link
public:
	void AddLink (PUNICODE_STRING strDev, PUNICODE_STRING strSymboleLink);
	void DelLink (PUNICODE_STRING strDev);
	void ClearLink ();
private:
	struct TSymboleLink
	{
		PVOID lOffsetDev;
		PVOID lOffsetLink;
	};
	CList		m_listLink;
};

typedef boost::intrusive_ptr <CPdoExtension> TPtrPdoExtension;
#endif