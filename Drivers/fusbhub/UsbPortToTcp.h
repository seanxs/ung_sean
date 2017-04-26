/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbPortToTcp.h

Abstract:

	Layer class between real and virtual USB devices (CPdoExtension)

Environment:

    Kernel mode

--*/
#ifndef USB_PORT_TO_TCP
#define USB_PORT_TO_TCP

#include "common\public.h"
#include "private.h"
#include "Core\list.h"
#include "Core\CounterIrp.h"
#include "Core\PagedLookasideList.h"
#include "Core\Buffer.h"
#include "Common\common.h"
#include "Core\Lock.h"
#include "Core\PullBuffs.h"
#include "urbDiff.h"
#include "Core\PoolListDrv.h"
#include "UrbIsoch.h"

//	way irp is from inside to outside
//
// NewIrp -> m_listWaitCompleted -> m_listIrpAnswer
//


class CPdoStubExtension;
class CControlDeviceExtension;
class CUsbPortToTcp;

typedef enum {
 
   IRPLOCK_CANCELABLE,
   IRPLOCK_CANCEL_STARTED,
   IRPLOCK_CANCEL_COMPLETE,
   IRPLOCK_COMPLETED
 
} IRPLOCK;

typedef enum {
 
   IRPLOCKI_FREE,   // free
   IRPLOCKI_SET,    // set worksave
   IRPLOCKI_USED,   // Working
   IRPLOCKI_COMPLETED, // Comleted
   IRPLOCKI_DELETED,

} IRPLOCK_ISOCH;

class CUsbPortToTcp : public CBaseClass
{
// variable
public:
	friend class CUrbIsoch;
public:
	CUsbPortToTcp (PUSB_DEV_SHARE pUsbDev);
	~CUsbPortToTcp ();

	NEW_ALLOC_DEFINITION ('VRSE')

// variable
public:
	// ISOCH
	//TAG
	LONG				m_lTag;

	// Usb hub giud
	WCHAR				m_szHubDriverKeyName [200];
	//	usb port
	ULONG				m_uDeviceAddress;
	// usb dev indef
	WCHAR				m_szDevIndef [200];

	// Work irp to tcp
	CList				m_listIrpAnswer;
	// uncomleted irps queue
	CList				m_listWaitCompleted;
	// uncomleted irps counter
	CCounterIrp			m_irpCounter;

	// list of irps waiting from service (irp is completed)
	CList				m_listIrpControlAnswer;

	// stub pointer
	CPdoStubExtension	*m_pPdoStub;

	//	Is 64 bit 
	bool				m_bIs64;
	// Present device
	bool				m_bIsPresent;

	// link into list 
	LIST_ENTRY			*m_linkControl;
	// Irp waiting for devices
	PIRP				m_irpWait;

	// need device relations
	bool				m_bNeedDeviceRelations;
	// delete when deleting pdo
	bool				m_bDelayedDelete;
	bool				m_bAdditionDel;
	// canceled irp
	bool				m_bCancel;
	// canceled Control Irp
	bool				m_bCancelControl;

	// work with irp queue
	bool				m_bWorkIrp;

	#ifdef ISOCH_FAST_MODE
		CList           m_listIsoch;
	#endif

	// buffer
	CPullBuffer			m_BufferMain;
	CPullBuffer			m_BufferSecond;
	// alloc memory from 
	CPagedLookasideList m_AllocListWorkSave;

public:
	// lockers
	CSpinLock	m_spinBase;
	CSpinLock	m_spinUsbIn;
	CSpinLock	m_spinUsbOut;
	CEventLock	m_lockRemove;

public:
	CUrbIsoch	m_Isoch;

public:
    PWORKING_IRP InitWorkIrp (PIRP_SAVE irpSave, CPdoStubExtension *pPdo);
public:
	void Init (PUSB_DEV_SHARE usbDev);

	// TCP
	NTSTATUS StartTcpServer (CPdoStubExtension *pPdoStub);
	NTSTATUS StopTcpServer (bool bDeletePdo = true);
	NTSTATUS StopTcpServer (CPdoStubExtension *pPdoStub);
    void CompleteWaitIrp (PIRP Irp, bool bOk);

	// additional
	VOID CancelControlIrp ();
	void CancelWairIrp ();
	void CancelWait ();

	void DeleteIrp (PWORKING_IRP pWorkSave);

	// run irp
	NTSTATUS InPutIrpToDevice (PBYTE Buffer, LONG lSize);
	NTSTATUS GetIrpToAnswer (PIRP Irp);
	void OutFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer); 

	// completion routine
	static NTSTATUS IoCompletionIrp(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context);
	bool OutFillAnswer (PIRP irpControl/*, PIRP irpDevice, PWORKING_IRP pWorkSave*/);
	bool OutFillIrpSave (PIRP_SAVE pIrpSave, PWORKING_IRP pWorkSave);

	// PnP
	void InPnpFillIrp (PIRP pnpIrp, PIRP_SAVE irpSave, PWORKING_IRP pWorkSave);
	LONG OutPnpGetSizeBuffer (PIRP Irp, PWORKING_IRP pWorkSave);

	// URB
	void InUrbFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave);
	LONG OutUrbBufferSize (PWORKING_IRP pWorkSave);
	void OutUrbFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer); 
	void OutUrbCopyBufferIsochIn (PURB_FULL pUrb, BYTE *pInBuffer, BYTE *pOutBuffer); 

	// URB different Size
	ULONG CalcSizeUrbToDiffUrb (PURB_FULL Urb);
	ULONG CalcSizeDiffUrbToUrb (PURB_DIFF Urb, PVOID SecondBuf);
	// URB Interface fill in if cross-platform
	void OutInterfaceUrbToUrbDiff (PUSBD_INTERFACE_INFORMATION Interface, USHORT uCount, PUSBD_INTERFACE_INFORMATION_DIFF InterfaceDeff);
	void InInterfaceUrbDiffToUrb (PUSBD_INTERFACE_INFORMATION_DIFF InterfaceDeff, USHORT uCount, PUSBD_INTERFACE_INFORMATION Interface);
    void InInterfaceSelect (PUSBD_INTERFACE_INFORMATION_DIFF InterfaceDeff, PUSBD_INTERFACE_INFORMATION Interface, CPdoStubExtension* pPdo);

	// IRP fill in if cross-platform
	void InUrbFillIrpDifferent (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave);
    void OutUrbFillBufferDifferent (PWORKING_IRP pWorkSave, BYTE *pInBuffer); 
	// USB
	void InUsbFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave);
	LONG OutUsbBufferSize (PWORKING_IRP pWorkSave);
	void OutUsbFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer); 
	// USB
	void InUsbIoFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave);
	LONG OutUsbIoBufferSize (PWORKING_IRP pWorkSave);
	void OutUsbIoFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer); 
	// interface
	void InInterfaceFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave);
	LONG OutInterfaceBufferSize (PWORKING_IRP pWorkSave);
	void OutInterfaceFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer); 
	// thread work with TCP
	void IoCompletionIrpThread (PVOID  Context);
	bool GetIrpToAnswerThread (PIRP Irp);
	void NextAnswer ();

    static NTSTATUS CCFunction (PVOID  DeviceContext, PVOID  CommandContext, NTSTATUS  NtStatus);

	void BuildStartFrame (ULONG &uStartFrame, ULONG uCountFrame);

	void Delete ();
	static VOID WorkItemDelete (PDEVICE_OBJECT DeviceObject, PVOID Context);

protected:
	PVOID	m_pDel;

public:
	void SendIdleNotification (CPdoStubExtension *pPdoStub); // ms


	// new pool work
public:
	CPoolListDrv	m_poolRead;
	CPoolListDrv	m_poolWrite;
	bool			m_bWork;

protected:
	static VOID stItemWriteToSrv (/*PDEVICE_OBJECT DeviceObject, */PVOID Context);
	void ItemWriteToSrv ();

	static VOID stItemReadFromSrv (/*PDEVICE_OBJECT DeviceObject, */PVOID Context);
	void ItemReadFromSrv ();

	bool OutFillAnswer2 ();

protected:
};

#if _FEATURE_ENABLE_DEBUGPRINT
#define PushLogPul(z,x,y) z->m_pool.PushLog (x,y);
#else
#define PushLogPul(z, x,y);
#endif

#define URB_FUNCTION_CONTROL_TRANSFER_EX             0x0032
#define URB_FUNCTION_SET_PIPE_IO_POLICY              0x0033
#define URB_FUNCTION_GET_PIPE_IO_POLICY              0x0034
#define URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR       0x002A
#define URB_FUNCTION_SYNC_RESET_PIPE                 0x0030
#define URB_FUNCTION_SYNC_CLEAR_STALL                0x0031

typedef boost::intrusive_ptr <CUsbPortToTcp> TPtrUsbPortToTcp;
#endif

