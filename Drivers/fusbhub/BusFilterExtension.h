/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BusFilterExtension.h

Abstract:

   USB hub filter class. Is used for USB devices interception

Environment:

    Kernel mode

--*/
#ifndef BUF_FILTER_EXTENSION
#define BUF_FILTER_EXTENSION

#include "PdoStubExtension.h"
#include "Core\List.h"
#include "Core\Lock.h"

class CControlDeviceExtension;

class CBusFilterExtension : public CFilterExtension , public CDeviceRelations
{
public:
	CBusFilterExtension (BOOLEAN bHubDecect);
	~CBusFilterExtension ();

	NEW_ALLOC_DEFINITION ('SUBE')

// Variable
public:
	CControlDeviceExtension		*m_pControl;
	WCHAR						m_szDriverKeyName [200];
	LIST_ENTRY					*m_pListControl;
    PDEVICE_OBJECT				m_PhysicalDeviceObject;
	CList						m_listEventDevRelations;
	CPdoStubExtension			m_PdoHub;
	// detect port hub through hub
	BOOLEAN						m_bHubDecect;
	unsigned short				m_wId;

// virtual major function
public:
	void InitName (PDEVICE_OBJECT PhysicalDeviceObject);
	void InitUnicId (PDEVICE_OBJECT PhysicalDeviceObject);

	virtual NTSTATUS DispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);

	virtual ULONG GetUsbDeviceAddressFromHub (PDEVICE_OBJECT PhysicalDeviceObject, bool bLock = true);
	virtual ULONG GetUsbDeviceAddress (PDEVICE_OBJECT PhysicalDeviceObject);

	static NTSTATUS GetUsbDeviceConnectionInfo (PDEVICE_OBJECT PhysicalDeviceObject, ULONG  IoControlCode, PVOID pInfo, int iSize);
	bool IsUsbDeviceHub (PDEVICE_OBJECT PhysicalDeviceObject, int iUsbPort);

	virtual ULONG GetCountUsbDevs ();
	virtual void QueryRelations (PDEVICE_RELATIONS Relations, PDEVICE_OBJECT PhysicalDeviceObject);
	void ClearEvent ();
	bool DelEvent (PKEVENT pEvent);
	bool WaitEventRelations (int iMSec);

	virtual NTSTATUS DispatchIntrenalDevCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	TPtrPdoStubExtension FindDevice (DWORD DeviceAddress, bool bLock = true);

public:
	// completion routine
	static NTSTATUS FilterCompletionRoutineRelations (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
	// 	Attach to Device
	virtual NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);

	// Work with PDO
	virtual TPtrPdoBase AddPdo (PDEVICE_OBJECT PhysicalDeviceObject);
	virtual TPtrPdoBase IsExistPdo (PDEVICE_OBJECT PhysicalDeviceObject);

	virtual bool VerifyShared (TPtrPdoStubExtension pPdo, PDEVICE_OBJECT PhysicalDeviceObject);

#if _FEATURE_ENABLE_DEBUGPRINT
	void ShowDeviceRelation (PDEVICE_RELATIONS Relations);
#endif
};

#endif