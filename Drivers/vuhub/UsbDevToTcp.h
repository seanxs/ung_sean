/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbDevToTcp.h

Abstract:

	Layer class between real and virtual USB devices (CPdoExtension)

Environment:

    Kernel mode

--*/
#ifndef USB_TO_TPC_H
#define USB_TO_TPC_H

#include "evuh.h"
#include "cUrbIsoch.h"
#include "Core\BaseClass.h"
#include "Core\List.h"
#include "Core\CounterIrp.h"
#include "Core\PagedLookasideList.h"
#include "Core\Timer.h"
#include "Core\Buffer.h"
#include "Core\Lock.h"
#include "Core\PoolListDrv.h"
#include "Core\PullBuffs.h"
#include "Core\PoolListDrv.h"
#include "Core\PullBuffs.h"
#include "Core\ArrayT.h"
#include "Core\Thread.h"
#include "Core\KEvent.h"

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

class CPdoExtension;
class CVirtualUsbHubExtension;

#ifdef _FEATURE_ENABLE_DEBUGPRINT
#define LOG_WAIT
#endif

namespace evuh
{
	class CUsbDevToTcp;
};

namespace evuh
{

class CUsbDevToTcp : public virtual CBaseClass
{
	friend class CUsbInterface;
public:
	CUsbDevToTcp(CVirtualUsbHubExtension *pHub);
	~CUsbDevToTcp(void);

	NEW_ALLOC_DEFINITION ('TTDU')

public:
	CSpinLock			m_spinUsbIn;
	CSpinLock			m_spinUsbOut;
	CSpinLock			m_spinQueue;
private:
	bool			m_bAutoCompleteBulkOut;
	bool			m_bBulkOutPending;
	bool			m_bBulkOutNotPending;

	// isoch
	CcUrbIsoch			m_Isoch;


public:
	WCHAR				m_szDosDeviceUser [200];
	WCHAR				m_szUserSid [200];
	CBuffer				m_sid;
	CPdoExtension		*m_pPdo;	// Thread work
	bool				m_bWork;
	bool				m_bIrpToTcp;
	// Counter IRP;
	LONG				m_lCount;
	// queue of devices IRPs
	CList				m_listIrpDevice;			// m_spinUsbOut
	// IRP queue waiting to be sent to TCP
	CList				m_listGetIrpToTcp;			// m_spinUsbOut
	// IRP queue waiting for TCP answer
	CList				m_listIrpAnswer;			// m_spinUsbIn
	// Pointer to main hub
	CVirtualUsbHubExtension *m_pHub;
	//	Is 64 bit 
	bool				m_bIs64;

	// Cancel time
	static int			m_DueTimeCancel;

	// device for WorkItem
	PDEVICE_OBJECT          m_DevObject;

    // Allocate memory 
    CPagedLookasideList	m_AllocNumber;
	CCounterIrp				m_NumberCount;

    CCounterIrp         m_WaitUse;
	CTimer				m_timerCancel;
	KEVENT				m_eventCancel;

	// temp count
	int	m_maxDevice;

	// USB version
	SHORT	m_bcdUSB;

	// long
	long		m_iGetIrpToTcp;
	long		m_iIrpToQuery;
	long		m_iNextIrp;

	bool		m_bStop;
	// usb idle
	USB_IDLE_CALLBACK_INFO_FULL	m_idleInfo;

public:
//DPC:
	static VOID CancelIrpDpc(struct _KDPC *Dpc,PVOID  DeferredContext,PVOID  SystemArgument1,PVOID  SystemArgument2);

public:
    static NTSTATUS IoCompletionError(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context);
	// Working thread
    static VOID ThreadTcpWork (PVOID Context);
	bool IrpToQuery (PVOID Irp); // irp device to queue
	bool GetIrpToRing3 (PIRP Irp); // irp control to queue
	void NextIrpToRing3 ();	// get next iro to ring 3
	void WiatCompleted(PIRP_NUMBER pNum);

	// debug // URB TYPE
	static PCHAR UrbFunctionString(USHORT Func);
	static PCHAR UsbIoControl (LONG lFunc);

	// Fill and complete 
	NTSTATUS OutIrpToTcp (CPdoExtension *pPdo, PIRP Irp);
	bool OutFillGetIrps (PIRP irpControl/*, PIRP_NUMBER stDevice*/);
	bool OutFillGetIrp (PIRP_SAVE pIrpSave, PIRP_NUMBER stDevice);
	void OutFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize); 

	static VOID IoComplitionWorkItm (PDEVICE_OBJECT DeviceObject, PVOID Context);

	NTSTATUS InCompleteIrp (PIRP irpControl);
    NTSTATUS InFillNewData (PIRP_NUMBER irpNumber, PIRP_SAVE pIrpSave);

	// Size and pointer to IRP buffer
	void OutPnpBufferSize (PIRP Irp, BYTE **pBuffer, LONG* lSize, PIRP_NUMBER irpNumber);
	void InPnpFillNewData (PIRP Irp, PIRP_SAVE irpSave, CPdoExtension *pPdo, PIRP_NUMBER irpNumber);

	// Internal device control
	void OutUrbBufferSize (PIRP Irp, BYTE **pBuffer, LONG* lSize, PIRP_NUMBER stDevice); 
	void OutUrbFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize); 
	void InUrbFillNewData(PIRP_NUMBER irpNumber, PIRP_SAVE irpSave, CPdoExtension *pPdo);
	void InUrbCopyBufferIsoch (PURB_FULL, BYTE *pIn, BYTE *pOut); 

	void UrbCancel(PIRP Irp, NTSTATUS Status = STATUS_CANCELLED, NTSTATUS UsbdStatus = USBD_STATUS_CANCELED);
	bool UrbPending (PIRP Irp, PIRP_NUMBER pNumber); 

	// usb
	void OutUsbBufferSize (PIRP Irp, BYTE **pBuffer, LONG* lSize); 
	void OutUsbFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize); 
	void InUsbFillNewData (PIRP Irp, PIRP_SAVE irpSave);

	// device control
	void OutUsbIoBufferSize (PIRP Irp, BYTE **pBuffer, LONG* lSize, PIRP_NUMBER stDevice); 
	void InUsbIoFillNewData (PIRP Irp, PIRP_SAVE irpSave);

	// Cancel all IRPs
	void CancelIrp ();
	void CancelIrpAnswer();
	void CancelingIrp (PIRP_NUMBER stDevice, KIRQL           CancelIrql);
    void StopAllThread ();

	// func
	void FuncInterfaceBufSize (PIRP Irp, BYTE **pBuffer, LONG* lSize); 
	void FuncInterfaceFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize, PVOID pParam); 
	void FuncInterfaceFillNewData (PIRP Irp, PIRP_SAVE irpSave); 

    void SetInterfaceFunction (PFUNC_INTERFACE pInterace, CPdoExtension* pPdo = NULL, PVOID lpParam = NULL); 

	// completion routine
	static NTSTATUS IoCompletionIrpError(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context);

    /// Thread of complete pools
	void CompleteIrp (PIRP_NUMBER	irpNum);


	// NEED DEL
	void CompleteIrpFull (PIRP_NUMBER	irpNum, bool bAsync = false);


    int     m_iCountThread;
    int     m_iCurUse;
    CList   m_listQueueComplete;

     static VOID CUsbDevToTcp::WorkItemQueryBusTime ( PDEVICE_OBJECT  DeviceObject, PVOID  Context );

	// status
	static void StatusDevNotResponToCanceled (PIRP pIrp);

	// **********************************************************
	//						interface
	CSpinLock					m_spinInterface;
    CList                       m_listInterface;
    NTSTATUS SetNewInterface (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo);
	NTSTATUS InitInterfaceInfo (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo, USHORT uCount);
    void ClearInterfaceInfo ();
    bool GetPipePing (USBD_PIPE_HANDLE Handle, PIRP_NUMBER pNumber, ULONG Direct, ULONG  Flags);
	UCHAR GetPipeEndpoint (USBD_PIPE_HANDLE Handle, bool bLock = false);
	USBD_PIPE_TYPE GetPipeType(USBD_PIPE_HANDLE Handle, bool bLock = false);
	// cancel
	static VOID CancelRoutine( PDEVICE_OBJECT DeviceObject, PIRP Irp);
	static VOID CancelRoutineDevice ( PDEVICE_OBJECT DeviceObject, PIRP Irp);
	// detect version 
	void DetectUsbVersion (PURB_FULL urb);

	//*********************************************
	bool DeleteIrpNumber (PIRP_NUMBER	pNumber);
	PIRP_NUMBER NewIrpNumber (PIRP pIrp, CPdoExtension *pPdo);
	ULONG SetNumber ();


	// Auto completed
	bool VerifyAutoComplete (PIRP_NUMBER	pNumber, bool bCompleted);
	bool FillIrpSave (PIRP_NUMBER	pNumber);
	static bool VerifyMultiCompleted (PIRP	pIrp);

	bool VerifyRaiseIrql (PIRP	pIrp, KIRQL *Old);

	UCHAR GetClassDevice ();

	bool IdleNotification (PIRP_SAVE pSave);

	// depricatet
private:
	NTSTATUS SetCompleteIrpWorkItm(PVOID Context, PKSTART_ROUTINE pRoutine);
	static VOID WorkItemNewThreadToPool (PDEVICE_OBJECT  DeviceObject, PVOID  Context );
	static VOID WorkItemNewThreadToPool(PVOID  Context);
	void StartNewThreadToPool(PKSTART_ROUTINE pRoutine);
	VOID ThreadComplete (); 

public:
	bool				m_bNewDriverWork;
	CPoolListDrv		m_poolReadFromService;
	CPoolListDrv		m_poolWriteToDriver;
	CPullBuffer			m_poolBuffer;
	struct EV_START
	{
		EV_START() : eventStart(NotificationEvent, false), bStart (false) {}
		CKEvent				eventStart;
		bool				bStart;

	}m_eventStart;

	static VOID stItemWriteToDriver (/*PDEVICE_OBJECT DeviceObject, */PVOID Context);
	void ItemWriteToDriver ();
	static VOID stItemWriteToService (/*PDEVICE_OBJECT DeviceObject, */PVOID Context);
	void ItemWriteToService ();

	void StartPool(bool bInOut);
	void SetEventStart() { if (!m_eventStart.bStart){ m_eventStart.eventStart.SetEvent(); m_eventStart.bStart = true; } }

	bool OutFillGetIrps2 ();

	NTSTATUS InCompleteIrpBuffer (char *pBuffer);

};

};

#endif