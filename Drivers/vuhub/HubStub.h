/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    HubStub.h

Abstract:

   Virtual USB hub class. Virtual USB devices appear on this hub.

Environment:

    Kernel mode

--*/
#ifndef HUBSTUB_H
#define HUBSTUB_H

#include "Core\basedeviceextension.h"
#include "Core\CounterIrp.h"
#include "Core\list.h"
#include "PdoExtension.h"

class CHubStubExtension : public DriverBase::CBaseDeviceExtension
{
public:
    CHubStubExtension(PVOID pVHub);
    ~CHubStubExtension();

	NEW_ALLOC_DEFINITION ('BUHV')

	friend class CUsbDevToTcp;

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
	CPdoExtension		m_PdoHub;

public:
    CCounterIrp     m_irpCounter;
	BOOLEAN			m_bDel;

protected:
	friend class CVirtualUsbHubExtension;
	UNICODE_STRING	m_LinkName;
	UNICODE_STRING	m_DeviceName;
    PVOID           m_pVHub;
    ULONG           m_uInstance;
	KTIMER  m_pTimerDeviceRelations;
	KDPC    m_pKdpcDeviceRelations;

public:
    UNICODE_STRING  m_LinkInterface;

public:
    NTSTATUS CreateDevice ();

    virtual NTSTATUS DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchCreate	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchClose	(PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp);
    virtual NTSTATUS DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp);

    NTSTATUS QueryBusInformation(PIRP   Irp);
    NTSTATUS QueryResourceRequirements(PIRP   Irp);
    NTSTATUS QueryDeviceCaps(PIRP   Irp);
    NTSTATUS QueryDeviceId (PIRP Irp);
    NTSTATUS RelationsPdos (PIRP Irp);

	TPtrPdoExtension GetDevByPort (ULONG uPort);

    NTSTATUS GetDeviceCapabilities(PDEVICE_OBJECT DeviceObject, PDEVICE_CAPABILITIES DeviceCapabilities);
    void ApplyXPSilentInstallHack (ULONG uInstance);

	static VOID DeviceRelationsDpc(struct _KDPC *Dpc,PVOID  DeferredContext,PVOID  SystemArgument1,PVOID  SystemArgument2);
	static VOID CancelRoutine( PDEVICE_OBJECT DeviceObject, PIRP Irp);
	void SetDeviceRelationsTimer ();

};

#endif