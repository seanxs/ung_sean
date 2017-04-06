/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    VirtualUsbHub.cpp

Abstract:

	Virtual USB host controller class. 

Environment:

    Kernel mode

--*/
#ifndef VirtualUsbHub_H
#define VirtualUsbHub_H

#include "Core\BaseDeviceExtension.h"
#include "Core\CounterIrp.h"
#include "Core\ArrayT.h"
#include "Core\Timer.h"
#include "Core\Lock.h"
#include "Core\Thread.h"
#include "Common\public.h"
#include "HubStub.h"
#include "ControlDeviceExtension.h"
#include "PdoExtension.h"


typedef CArrayT <TPtrPdoExtension> TArrayPdo;

class CVirtualUsbHubExtension : public DriverBase::CBaseDeviceExtension
{
public:
	CVirtualUsbHubExtension ();

	NEW_ALLOC_DEFINITION ('EHUV')

	// variable
public:
	CSpinLock				m_spinPdo;
public:
    WCHAR					m_strHcd [30];
	CCounterIrp				m_irpCounter;
	TArrayPdo				m_arrPdo;
	PDEVICE_OBJECT			m_NextLowerDriver;
	PDEVICE_OBJECT			m_PhysicalDeviceObject;
    UNICODE_STRING			m_LinkNameHcd;
	UNICODE_STRING			m_DeviceName;
    UNICODE_STRING			m_LinkInterface;
	LONG					m_lTag;
    CHubStubExtension       *m_pHubStub;
    evuh::CControlDeviceExtension *m_pControl;
    bool					m_bLinkNameHcd;
    CBaseDeviceExtension	m_stub;
	bool					m_bDel;

	PVOID					m_pRegValume;

	KEVENT					m_eventRemoveDev;
	ULONG					m_uMaxPort;
	CTimer					m_timerSymbolLink;
	Thread::CThreadPoolTask	m_poolInsulation;

	CEventLock				m_lockCreateDel;


// virtual major function
public:
	virtual NTSTATUS DispatchCreate				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchClose				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDevCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPower				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPnp				(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	// default DispatchFunction 
	virtual NTSTATUS DispatchDefault			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    virtual NTSTATUS DispatchSysCtrl			(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp);

public:
	// completion routine
	static NTSTATUS FilterStartCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
	// Attach to Device
	virtual NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);
	// work with PDO
	NTSTATUS RelationsPdos (PIRP Irp);
	NTSTATUS AddNewPdo (PIRP Irp, PUSB_DEV NewDev);
	NTSTATUS RemovePdo (PIRP Irp, PUSB_DEV RemoveDev);
	NTSTATUS GetIrpToTcp (PIRP Irp);
	NTSTATUS AnswerTcpToIrp (PIRP Irp);
	NTSTATUS SetDosDevPref (PIRP Irp, PUSB_DEV_SHARE pPref);
	NTSTATUS SetUserSid (PIRP Irp, PUSB_DEV_SHARE pPref);

	TPtrPdoExtension FindPdo (PUSB_DEV Dev);
	bool RemovePdo (TPtrPdoExtension pDev);
	void SetUsbPort (CPdoExtension *pPdo);

	PVOID CreateUnicInstanceID (WCHAR *szHardwareId, WCHAR *szInstanceID, ULONG &uSize);

	static VOID WorkItmDeleteUsbDev (PDEVICE_OBJECT DeviceObject, PVOID Context);

	evuh::CUsbDevToTcp* FindTcp (PIRP pIrp);

	NTSTATUS SetPoolTcp (PIRP Irp);
	NTSTATUS ClrPoolTcp (PIRP Irp);

};

#endif