/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbDevToTcp.cpp

Abstract:

	Layer class between real and virtual USB devices (CPdoExtension)

Environment:

    Kernel mode

--*/
#include "UsbDevToTcp.h"
#include "PdoExtension.h"
#include "VirtualUsbHub.h"
#include "Common\common.h"
#include "Core\Utils.h"
#include "Core\WorkItem.h"
#include "Core\Registry.h"
#include "Core\Thread.h"
#include "PoolCompletingIrp.h"
#include "UsbInterface.h"

int evuh::CUsbDevToTcp::m_DueTimeCancel = 8;

// *****************************************************
//		CUsbDevToTcp 

evuh::CUsbDevToTcp::CUsbDevToTcp (CVirtualUsbHubExtension *pHub)
	: CBaseClass ()
	, m_Isoch()
	, m_bWork (true)
	, m_bIrpToTcp (false)
	, m_lCount (0)
	, m_pHub (pHub)
    , m_AllocNumber (NonPagedPool, sizeof (IRP_NUMBER))
    , m_iCountThread (0)
    , m_iCurUse (0)
	, m_bcdUSB (0x0200)
	, m_maxDevice (0)
	, m_iGetIrpToTcp (0)
	, m_iIrpToQuery (0)
	, m_iNextIrp ()
	, m_timerCancel (tagUsb)
	, m_bStop (false)
	, m_pPdo (NULL)
	, m_bNewDriverWork (false)
	, m_bAutoCompleteBulkOut (false)
	, m_bBulkOutPending (false)
	, m_bBulkOutNotPending (false)
{
	NTSTATUS			status;

	SetDebugName ("CUsbDevToTcp");

	m_Isoch.SetBase(this);

	// create device for WorkItem
	status = IoCreateDevice (DriverBase::CBaseDeviceExtension::GetDriverObject (),
                             10,
                             NULL,  // No Name
                             FILE_DEVICE_UNKNOWN,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &m_DevObject);
	if (!NT_SUCCESS (status))
	{
		m_DevObject = NULL;
	}

#ifdef _WIN64
	DebugPrint (DebugInfo, "Enable x64");
	m_bIs64 = true;
#else
	DebugPrint (DebugInfo, "Enable x86");
	m_bIs64 = false;
#endif

	m_timerCancel.InitTimer (&evuh::CUsbDevToTcp::CancelIrpDpc, this);
	m_timerCancel.SetTimer(m_DueTimeCancel);

	KeInitializeEvent(&m_eventCancel, SynchronizationEvent, TRUE);

	RtlZeroMemory (&m_idleInfo, sizeof (m_idleInfo));
	RtlZeroMemory (m_szDosDeviceUser, sizeof (m_szDosDeviceUser));
	RtlZeroMemory (m_szUserSid, sizeof (m_szDosDeviceUser));

	// Load advanced param
	{
		CRegistry reg;
		CUnicodeString str (*DriverBase::CBaseDeviceExtension::GetRegistryPath (), NonPagedPool);

		str += L"\\Parameters";

		if (NT_SUCCESS (reg.Open (str)))
		{
			LONG	lValue;
			if (NT_SUCCESS (reg.GetNumber (CUnicodeString (L"AutoCompleteBulkOut"), lValue)))
			{
				m_bAutoCompleteBulkOut = lValue?true:false;
			}
			if (NT_SUCCESS (reg.GetNumber (CUnicodeString (L"BulkOutPending"), lValue)))
			{
				m_bBulkOutPending = lValue?true:false;
			}
			if (NT_SUCCESS (reg.GetNumber (CUnicodeString (L"BulkOutNotPending"), lValue)))
			{
				m_bBulkOutNotPending = lValue?true:false;
			}
		}
	}
}

evuh::CUsbDevToTcp::~CUsbDevToTcp(void)
{
    StopAllThread ();
	ClearInterfaceInfo ();
	if (m_DevObject != NULL)
	{
		IoDeleteDevice (m_DevObject);
		m_DevObject = NULL;
	}
}

void evuh::CUsbDevToTcp::StartPool(bool bInOut)
{
	if (bInOut)
	{
		// read from service
		++m_WaitUse;
		Thread::Create(CUsbDevToTcp::stItemWriteToDriver, this);
	}
	else
	{
		// write to service
		++m_WaitUse;
		Thread::Create(CUsbDevToTcp::stItemWriteToService, this);
	}
	SetEventStart();
}

void evuh::CUsbDevToTcp::StopAllThread ()
{
    PVOID hThread;

	KeWaitForSingleObject (&m_eventCancel, Executive, KernelMode, FALSE, NULL);

	m_spinQueue.Lock ();
    m_listQueueComplete.Add (NULL);
	m_spinQueue.UnLock();

	// stop ItemWriteToService
	if (m_bNewDriverWork)
	{
		m_listIrpDevice.Add(NULL);
	}
	// stop ItemWriteToDriver
	if (m_bNewDriverWork)
	{
		m_poolWriteToDriver.GetCurNodeWrite();
		m_poolWriteToDriver.PushCurNodeWrite();
	}


	m_timerCancel.CancelTimer();

	m_spinQueue.Lock ();
    if (!m_bStop)
    {	
		m_bStop = true;
		m_spinQueue.UnLock();
        m_WaitUse.WaitRemove ();
    }
	else
	{
		m_bStop = true;
		m_spinQueue.UnLock();
	}

	KeSetEvent (&m_eventCancel, IO_NO_INCREMENT, FALSE);
}

// ****************************************
//			Debug info
// ****************************************
PCHAR evuh::CUsbDevToTcp::UrbFunctionString(USHORT Func)
{
#if DBG
	switch (Func)
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		return "URB_FUNCTION_SELECT_CONFIGURATION";
	case URB_FUNCTION_SELECT_INTERFACE:
        return "URB_FUNCTION_SELECT_INTERFACE";
	case URB_FUNCTION_ABORT_PIPE:
		return "URB_FUNCTION_ABORT_PIPE";
	case URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL:
		return "URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL";
	case URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL:
		return "URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL";
	case URB_FUNCTION_GET_FRAME_LENGTH:
		return "URB_FUNCTION_GET_FRAME_LENGTH";
	case URB_FUNCTION_SET_FRAME_LENGTH:
		return "URB_FUNCTION_SET_FRAME_LENGTH";
	case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
		return "URB_FUNCTION_GET_CURRENT_FRAME_NUMBER";
	case  URB_FUNCTION_CONTROL_TRANSFER:
		return "URB_FUNCTION_CONTROL_TRANSFER";
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		return "URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER";
	case URB_FUNCTION_ISOCH_TRANSFER:
		return "URB_FUNCTION_ISOCH_TRANSFER";
	case URB_FUNCTION_RESET_PIPE:
		return "URB_FUNCTION_RESET_PIPE";
	//case URB_FUNCTION_SYNC_RESET_PIPE:
	//	return "URB_FUNCTION_SYNC_RESET_PIPE";
	//case URB_FUNCTION_SYNC_CLEAR_STALL:
	//	return "URB_FUNCTION_SYNC_CLEAR_STALL";
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		return "URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE";
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
		return "URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT";
	case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
		return "URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE";
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
		return "URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT";
	case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
		return "URB_FUNCTION_SET_FEATURE_TO_DEVICE";
	case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
		return "URB_FUNCTION_SET_FEATURE_TO_INTERFACE";
	case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
		return "URB_FUNCTION_SET_FEATURE_TO_ENDPOINT";
	case URB_FUNCTION_SET_FEATURE_TO_OTHER:
		return "URB_FUNCTION_SET_FEATURE_TO_OTHER";
	case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE";
	case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE";
	case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT";
	case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_OTHER";
	case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
		return "URB_FUNCTION_GET_STATUS_FROM_DEVICE";
	case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
		return "URB_FUNCTION_GET_STATUS_FROM_INTERFACE";
	case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
		return "URB_FUNCTION_GET_STATUS_FROM_ENDPOINT";
	case URB_FUNCTION_GET_STATUS_FROM_OTHER:
		return "URB_FUNCTION_GET_STATUS_FROM_OTHER";
	case URB_FUNCTION_VENDOR_DEVICE:
		return "URB_FUNCTION_VENDOR_DEVICE";
	case URB_FUNCTION_VENDOR_INTERFACE:
		return "URB_FUNCTION_VENDOR_INTERFACE";
	case URB_FUNCTION_VENDOR_ENDPOINT:
		return "URB_FUNCTION_VENDOR_ENDPOINT";
	case URB_FUNCTION_VENDOR_OTHER:
		return "URB_FUNCTION_VENDOR_OTHER";
	case URB_FUNCTION_CLASS_DEVICE:
		return "URB_FUNCTION_CLASS_DEVICE";
	case URB_FUNCTION_CLASS_INTERFACE:
		return "URB_FUNCTION_CLASS_INTERFACE";
	case URB_FUNCTION_CLASS_ENDPOINT:
		return "URB_FUNCTION_CLASS_ENDPOINT";
	case URB_FUNCTION_CLASS_OTHER:
		return "URB_FUNCTION_CLASS_OTHER";
	case URB_FUNCTION_GET_CONFIGURATION:
		return "URB_FUNCTION_GET_CONFIGURATION";
	case URB_FUNCTION_GET_INTERFACE:
		return "URB_FUNCTION_GET_INTERFACE";
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		return "URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE";
	case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:	
		return "URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE";
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        return "URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR";
    case URB_FUNCTION_SYNC_RESET_PIPE:
        return "URB_FUNCTION_SYNC_RESET_PIPE";
    case URB_FUNCTION_SYNC_CLEAR_STALL:
        return "URB_FUNCTION_SYNC_CLEAR_STALL";
    case URB_FUNCTION_CONTROL_TRANSFER_EX:
        return "URB_FUNCTION_CONTROL_TRANSFER_EX";
        break;
#if (_WIN32_WINNT < 0x0600)
    case URB_FUNCTION_SET_PIPE_IO_POLICY:
        return "URB_FUNCTION_SET_PIPE_IO_POLICY";
        break;
    case URB_FUNCTION_GET_PIPE_IO_POLICY:
        return "URB_FUNCTION_GET_PIPE_IO_POLICY";
        break;
#endif
	}

	return 0;
#else
	return 0;
#endif
}

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


#define IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO    CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_HUB_CONFIG_INFO, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)
#endif


PCHAR evuh::CUsbDevToTcp::UsbIoControl (LONG lFunc)
{
#if DBG

	switch (lFunc)
	{
	case IOCTL_INTERNAL_USB_SUBMIT_URB:
		return "IOCTL_INTERNAL_USB_SUBMIT_URB";
	case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
		return "IOCTL_INTERNAL_USB_GET_PORT_STATUS";
	case IOCTL_INTERNAL_USB_RESET_PORT:
		return "IOCTL_INTERNAL_USB_RESET_PORT";
	case IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO:
		return "IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO";
	case IOCTL_INTERNAL_USB_ENABLE_PORT:
		return "IOCTL_INTERNAL_USB_ENABLE_PORT";
	case IOCTL_INTERNAL_USB_GET_HUB_COUNT:
		return "IOCTL_INTERNAL_USB_GET_HUB_COUNT";
	case IOCTL_INTERNAL_USB_CYCLE_PORT:
		return "IOCTL_INTERNAL_USB_CYCLE_PORT";
	case IOCTL_INTERNAL_USB_GET_HUB_NAME:
		return "IOCTL_INTERNAL_USB_GET_HUB_NAME";
	case IOCTL_INTERNAL_USB_GET_BUS_INFO:
		return "IOCTL_INTERNAL_USB_GET_BUS_INFO";
	case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
		return "IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME";
	case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE";
	case IOCTL_INTERNAL_USB_GET_BUSGUID_INFO:
		return "IOCTL_INTERNAL_USB_GET_BUSGUID_INFO";
	case IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO:
		return "IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO";
	case IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY:
		return "IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY";
	case IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND:
		return "IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND";
	case IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME:
		return "IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME";
	case IOCTL_INTERNAL_USB_RECORD_FAILURE:
		return "IOCTL_INTERNAL_USB_RECORD_FAILURE";
	case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX";
	case IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE:
		return "IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE";
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
		return "IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS";
	case IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO";
	case IOCTL_USB_HCD_GET_STATS_1:
		return "IOCTL_USB_HCD_GET_STATS_1";
	//case IOCTL_USB_HCD_GET_STATS_2:
	//	return "IOCTL_USB_HCD_GET_STATS_2";
	case IOCTL_USB_HCD_DISABLE_PORT:
		return "IOCTL_USB_HCD_DISABLE_PORT";
	case IOCTL_USB_HCD_ENABLE_PORT:
		return "IOCTL_USB_HCD_ENABLE_PORT";
	case IOCTL_USB_DIAGNOSTIC_MODE_ON:
		return "IOCTL_USB_DIAGNOSTIC_MODE_ON";
	case IOCTL_USB_DIAGNOSTIC_MODE_OFF:
		return "IOCTL_USB_DIAGNOSTIC_MODE_OFF";
	case IOCTL_USB_GET_ROOT_HUB_NAME:
		return "IOCTL_USB_GET_ROOT_HUB_NAME";
	//case IOCTL_GET_HCD_DRIVERKEY_NAME:
	//	return "IOCTL_GET_HCD_DRIVERKEY_NAME";
	//case IOCTL_USB_GET_NODE_INFORMATION:
	//	return "IOCTL_USB_GET_NODE_INFORMATION";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION";
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
		return "IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION";
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		return "IOCTL_USB_GET_NODE_CONNECTION_NAME";
	case IOCTL_USB_DIAG_IGNORE_HUBS_ON:
		return "IOCTL_USB_DIAG_IGNORE_HUBS_ON";
	case IOCTL_USB_DIAG_IGNORE_HUBS_OFF:
		return "IOCTL_USB_DIAG_IGNORE_HUBS_OFF";
	//case IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME:
	//	return "IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME";
	case IOCTL_USB_GET_HUB_CAPABILITIES:
		return "IOCTL_USB_GET_HUB_CAPABILITIES";
	case IOCTL_USB_HUB_CYCLE_PORT:
		return "IOCTL_USB_HUB_CYCLE_PORT";
	case IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES:
		return "IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX";
	//case IOCTL_USB_RESET_HUB:
	//	return "IOCTL_USB_RESET_HUB";


	case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
		return "IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION";
	//case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
	//	return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX";
	}
	return 0;
#else
	return 0;
#endif
}
//

void evuh::CUsbDevToTcp::SetInterfaceFunction (PFUNC_INTERFACE pInterace, CPdoExtension* pPdo, PVOID lpParam)
{
    PIRP				newIrp;
    PIO_STACK_LOCATION	irpSp;
    KEVENT				Event;
	PIRP_NUMBER			irpNumber;
    KIRQL               OldIrql;

	DebugPrint (DebugTrace, "SetInterfaceFunction");

	if (!m_bWork)
	{
		DebugPrint (DebugTrace, "Device is not present PDO = 0x%x", this);
		return;
	}

    newIrp = IoAllocateIrp( 2, FALSE );
	if (!newIrp)
	{
		return ;
	}

    newIrp->CurrentLocation--;

    irpSp = IoGetNextIrpStackLocation( newIrp );
    newIrp->Tail.Overlay.CurrentStackLocation = irpSp;

	++pPdo->m_irpCounterPdo;
	++pPdo->m_irpTcp;

	irpSp = IoGetCurrentIrpStackLocation( newIrp );
    irpSp->MajorFunction = (UCHAR) 0xFF;
	irpSp->Parameters.Others.Argument1 = pInterace;
    irpSp->Parameters.Others.Argument2 = lpParam;


    irpNumber = NewIrpNumber (newIrp, pPdo);

	IrpToQuery (irpNumber);

	if (irpNumber->bWait)
	{
		KeWaitForSingleObject(
			&irpNumber->Event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
	}
	IoFreeIrp (newIrp);
	DeleteIrpNumber(irpNumber);
	--pPdo->m_irpCounterPdo;
	--pPdo->m_irpTcp;
}

void evuh::CUsbDevToTcp::WiatCompleted(PIRP_NUMBER irpNumber)
{
	m_spinUsbIn.Lock();
	m_listIrpAnswer.Add(irpNumber);
	m_spinUsbIn.UnLock();
}

bool evuh::CUsbDevToTcp::GetIrpToRing3 (PIRP Irp)
{
	PIRP_NUMBER	stDevice = NULL;
	bool		bRet = false;

	DebugPrint (DebugTrace, "GetIrpToRing3 - 0x%x", Irp);

	m_spinUsbOut.Lock ();
	if (m_listIrpDevice.GetCount () == 0 || m_bIrpToTcp)
	{
		IoMarkIrpPending (Irp);
		m_listGetIrpToTcp.Add (Irp);
		m_spinUsbOut.UnLock ();
		return false;
	}

	InterlockedIncrement (&m_iGetIrpToTcp);

	m_bIrpToTcp = true;
	m_spinUsbOut.UnLock ();

    DebugPrint (DebugTrace, "GetIrpToRing3 - Unlock", Irp);

	bRet = OutFillGetIrps (Irp);

	m_spinUsbOut.Lock ();
	m_bIrpToTcp = false;
	m_spinUsbOut.UnLock ();

	NextIrpToRing3 ();
	return bRet;
}

NTSTATUS evuh::CUsbDevToTcp::IoCompletionError(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context)
{
    CUsbDevToTcp *pBase = (CUsbDevToTcp*)Context;
    pBase->DebugPrint (DebugError, "IoCompletionError");

    return STATUS_MORE_PROCESSING_REQUIRED;
}

bool evuh::CUsbDevToTcp::IrpToQuery (PVOID Irp)
{
	PIRP		irpControl = NULL;
	PIRP_NUMBER	irpNumber = (PIRP_NUMBER)Irp;

	DebugPrint (DebugTrace, "GetIrpToRing3 - %d", irpNumber->Count);

    // queue verification of device's IRPs
	m_spinUsbOut.Lock ();
	m_listIrpDevice.Add (Irp);

	if (m_bNewDriverWork)
	{
		// work through share memory
		m_spinUsbOut.UnLock ();
		return false;
	}

	if (m_listGetIrpToTcp.GetCount () && !m_bIrpToTcp)
	{
		m_bIrpToTcp = true;
		irpControl = (PIRP)m_listGetIrpToTcp.PopFirst ();
	}
	m_spinUsbOut.UnLock ();
	
    // sending devices IRPs to 3 ring
	if (irpControl)
	{
		InterlockedIncrement (&m_iIrpToQuery);
		OutFillGetIrps (irpControl);
		// OutFillGetIrps free
		m_spinUsbOut.Lock ();
		m_bIrpToTcp = false;
		m_spinUsbOut.UnLock ();

		// next irp
		NextIrpToRing3 ();
	}
	else
	{
		IoSetCancelRoutine (irpNumber->Irp, &evuh::CUsbDevToTcp::CancelRoutineDevice);
		DebugPrint (DebugSpecial, "IrpToQuery false - %d ", irpNumber->Count);
	}
	return irpControl?true:false;
}

void evuh::CUsbDevToTcp::NextIrpToRing3 ()
{
	PIRP		irpControl = NULL;

	m_spinUsbOut.Lock ();
	if (m_listIrpDevice.GetCount () && m_listGetIrpToTcp.GetCount () && !m_bIrpToTcp)
	{
		irpControl = (PIRP)m_listGetIrpToTcp.PopFirst ();
		m_bIrpToTcp = true;
	}
	m_spinUsbOut.UnLock ();
	// next irp to ring 0
	if (irpControl)
	{
		OutFillGetIrps (irpControl);

		InterlockedIncrement (&m_iNextIrp);
		// OutFillGetIrps free
		m_spinUsbOut.Lock ();
		m_bIrpToTcp = false;
		m_spinUsbOut.UnLock ();

		// next irp
		NextIrpToRing3 ();
	}
}

// ********************************************************************
//				Fill and complete 
// ********************************************************************
void evuh::CUsbDevToTcp::OutFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize)
{
	PIO_STACK_LOCATION  irpStackDevice;

	DebugPrint (DebugTrace, "FillBuffer");

	irpStackDevice = IoGetCurrentIrpStackLocation (irpDevice);

	switch (irpStackDevice->MajorFunction)
	{
	case IRP_MJ_PNP:
		{
			RtlCopyMemory (pOutBuffer, pInBuffer, lSize);
		}
		break;
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
	case IRP_MJ_DEVICE_CONTROL:
		OutUrbFillBuffer (irpDevice, pInBuffer, pOutBuffer, lSize);
		break;
	case 0xFF:
		FuncInterfaceFillBuffer (irpDevice, pInBuffer, pOutBuffer, lSize, irpStackDevice->Parameters.Others.Argument2);
		break;
	}
}

NTSTATUS evuh::CUsbDevToTcp::InFillNewData (PIRP_NUMBER irpNumber, PIRP_SAVE pIrpSave)
{
	PIO_STACK_LOCATION  irpStackDevice;
	PIRP				irpDevice;
	NTSTATUS			status;

	DebugPrint (DebugTrace, "FillNewData - %d",irpNumber->Count);

	irpDevice = irpNumber->Irp;
	if (!irpDevice)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (VerifyMultiCompleted (irpDevice))
	{
		DebugPrint (DebugError, "FillNewData MultiCompleted - %d", irpNumber->Count);

		CompleteIrp (irpNumber);
		return STATUS_INVALID_PARAMETER;
	}

	if (irpDevice->Cancel)
	{
        irpDevice->Cancel = pIrpSave->Cancel?TRUE:FALSE;
	}

	irpStackDevice = IoGetCurrentIrpStackLocation (irpDevice);
	status = irpDevice->IoStatus.Status;
	irpDevice->IoStatus.Status = pIrpSave->Status;
	irpDevice->IoStatus.Information = (ULONG_PTR)pIrpSave->Information;
	pIrpSave->Status = status;
	irpStackDevice->MajorFunction = pIrpSave->StackLocation.MajorFunction;
	irpStackDevice->MinorFunction = pIrpSave->StackLocation.MinorFunction;

	switch (irpStackDevice->MajorFunction)
	{
	case IRP_MJ_POWER:
		break;
	case IRP_MJ_PNP:
		InPnpFillNewData (irpDevice, pIrpSave, irpNumber->pPdo, irpNumber);
		break;
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		InUrbFillNewData(irpNumber, pIrpSave, irpNumber->pPdo);
		break;
	case IRP_MJ_DEVICE_CONTROL:
		InUsbIoFillNewData (irpDevice, pIrpSave);
		break;
	case 0xFF:
		FuncInterfaceFillNewData (irpDevice, pIrpSave);
		break;
    default:
        DebugPrint (DebugSpecial, "FillNewData no major");
        break;
	}

	static int iCount;

	if (irpDevice->Cancel)
	{
		DebugPrint (DebugWarning, "Cancel IRP - 0x%x", irpDevice);
	}

	IoSetCancelRoutine (irpNumber->Irp, NULL);

	if (irpNumber->bWait && !irpNumber->bManualPending)
	{
        InterlockedIncrement (&irpNumber->Lock);
		KeSetEvent (&irpNumber->Event, IO_NO_INCREMENT, FALSE);
	}
	else
	{
		if (m_bNewDriverWork)
		{
			if (irpNumber->bIsoch)
			{
				m_Isoch.CompletingIrp(irpNumber);
			}
			else
			{
				CPoolCompletingIrp::Instance()->AddIrpToCompleting(irpNumber);
			}
		}
		else if (!NT_SUCCESS (SetCompleteIrpWorkItm (irpNumber, WorkItemNewThreadToPool)))
		{
			return STATUS_INVALID_PARAMETER;
		}

	}

	return STATUS_SUCCESS;
}

NTSTATUS evuh::CUsbDevToTcp::InCompleteIrp (PIRP irpControl)
{
	PIO_STACK_LOCATION  irpStackControl;
    PIRP_SAVE			pIrpSave;
	PIRP_NUMBER			irpNumber = NULL;
	LONG				iCurPos = 0;
	LONG				iSize;
	LPBYTE				pBuffer;

	DebugPrint (DebugTrace, "CompleteIrp irpControl - 0x%x", irpControl);

	irpStackControl = IoGetCurrentIrpStackLocation (irpControl);
	pBuffer = (LPBYTE)irpControl->AssociatedIrp.SystemBuffer;
	iSize = irpStackControl->Parameters.DeviceIoControl.InputBufferLength;

	while (iCurPos < iSize)
	{
		pIrpSave = (PIRP_SAVE)(pBuffer + iCurPos);

		m_spinUsbIn.Lock ();
		if (pIrpSave->Irp == -1)
		{
			m_spinUsbIn.UnLock ();
			if (!IdleNotification (pIrpSave))
			{
				m_Isoch.SetTimerPeriod ((int)pIrpSave->Information);
			}
			m_spinUsbIn.Lock ();
		}
		else
		{
			for (int a = 0; a < m_listIrpAnswer.GetCount (); a++)
			{
				irpNumber = (PIRP_NUMBER)m_listIrpAnswer.GetItem (a);
				if (irpNumber->Count == pIrpSave->Irp)
				{
					m_listIrpAnswer.Remove (a);
					break;
				}
				irpNumber = NULL;
			}
		}
		if (irpNumber == NULL)
		{
			m_spinUsbIn.UnLock ();
			iCurPos += pIrpSave->NeedSize;
			continue;
		}
		// fixme: del
		if (iCurPos > 0)
		{
			iCurPos = iCurPos;
		}
		m_spinUsbIn.UnLock ();

		DebugPrint (DebugSpecial, "InCompleteIrp - %d", irpNumber->Count);

		InFillNewData (irpNumber, pIrpSave);

		iCurPos += pIrpSave->NeedSize;
	}

    return STATUS_SUCCESS;
}

NTSTATUS evuh::CUsbDevToTcp::SetCompleteIrpWorkItm(PVOID Context, PKSTART_ROUTINE pRoutine)
{
	PIRP_NUMBER			irpNumber = PIRP_NUMBER(Context);
    LONG                lDel;
	
    DebugPrint (DebugTrace, "SetCompleteIrpWorkItm");

	if (m_bWork)
	{
		m_spinQueue.Lock ();
		if (m_iCountThread <= m_iCurUse && m_iCurUse < 10)
		{
			m_spinQueue.UnLock ();
			StartNewThreadToPool (pRoutine);
			m_spinQueue.Lock ();
		}
		m_listQueueComplete.Add (Context);
		m_spinQueue.UnLock ();
	}
	else
	{
		CompleteIrp (irpNumber);
	}

    return STATUS_SUCCESS;
}

VOID evuh::CUsbDevToTcp::IoComplitionWorkItm (PDEVICE_OBJECT DeviceObject, PVOID Context)
{
}

VOID evuh::CUsbDevToTcp::CancelIrpDpc(struct _KDPC *Dpc, PVOID  DeferredContext, PVOID  SystemArgument1, PVOID  SystemArgument2)
{
    PIRP_NUMBER			irpNumber = NULL;
    CUsbDevToTcp        *pBase = (CUsbDevToTcp*)DeferredContext;

    ++pBase->m_WaitUse;
	pBase->m_spinUsbIn.Lock ();

	for (int a = 0; a < pBase->m_listIrpAnswer.GetCount (); a++)
	{
        irpNumber = (PIRP_NUMBER)pBase->m_listIrpAnswer.GetItem (a);

		if (!irpNumber)
		{
			pBase->DebugPrint (DebugWarning, "CancelIrpDpc can't get IRP_NUMBER %d %d", a, pBase->m_listIrpAnswer.GetCount ());
			continue;
		}
		#ifndef SET_CANCEL_ROUTINE
			if (irpNumber->Irp->Cancel && !irpNumber->Cancel)
			{
				irpNumber->Cancel = true;
				pBase->AddIrpToQueue (irpNumber, weIrpToQuery);
				pBase->DebugPrint (DebugInfo, "CancelIrpDpc - 0x%x", irpNumber->Count);
			}
		#endif
		if (irpNumber->CountWait && irpNumber->bWait)
		{
			--irpNumber->CountWait;
			if (irpNumber->CountWait == 0)
			{
				IoMarkIrpPending(irpNumber->Irp);
				irpNumber->bWait = false;
				irpNumber->bManualPending = false;
				KeSetEvent (&irpNumber->Event, IO_NO_INCREMENT, FALSE);
			}
		}
	}

	pBase->m_spinUsbIn.UnLock ();

	if (pBase->m_bWork)
	{
		pBase->m_timerCancel.SetTimer(m_DueTimeCancel);
	}
    --pBase->m_WaitUse;
}

NTSTATUS evuh::CUsbDevToTcp::IoCompletionIrpError(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context)
{
    CUsbDevToTcp *pBase = (CUsbDevToTcp*)Context;
    pBase->DebugPrint (DebugTrace, "IoCompletionIrpError");
    pBase->DebugPrint (DebugSpecial, "IoCompletionIrpError");
    return STATUS_SUCCESS;
}

NTSTATUS evuh::CUsbDevToTcp::OutIrpToTcp (CPdoExtension *pPdo, PIRP Irp)
{
	NTSTATUS            status = STATUS_PENDING;
    KEVENT				Event;
	PIRP_NUMBER			irpNumber;
	PIO_STACK_LOCATION  stack;
	PVOID				pTemp;

	if (pPdo->m_DeviceIndef == -1 || pPdo->m_DeviceIndef == -2)
	{
		DebugPrint (DebugInfo, "PdoHub or HcdHub");
	}
	
	stack = IoGetCurrentIrpStackLocation (Irp);
	++pPdo->m_irpCounterPdo;
	++pPdo->m_irpTcp;
	if (!pPdo->m_bPresent)
	{
		if (Irp->PendingReturned) 
		{
			IoMarkIrpPending(Irp);
		}

		DebugPrint (DebugError, "OutIrpToTcp Device is not present PDO = 0x%x", this);
		status = STATUS_DEVICE_NOT_CONNECTED;
		CPdoExtension::CancelUrb (Irp);
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--pPdo->m_irpTcp;
		--pPdo->m_irpCounterPdo;
		return status;
	}

	pTemp = stack->Parameters.Others.Argument1;
    // Defence from unknown interface
	if (stack->MajorFunction == IRP_MJ_PNP)
	{
		switch (stack->MinorFunction)
		{
		case IRP_MN_QUERY_INTERFACE:
		{
			GUID GuidUsb = { 0xb1a96a13, 0x3de0, 0x4574, 0x9b, 0x1, 0xc0, 0x8f, 0xea, 0xb3, 0x18, 0xd6 };
			GUID GuidUsbCamd = { 0x2bcb75c0, 0xb27f, 0x11d1, 0xba, 0x41, 0x0, 0xa0, 0xc9, 0xd, 0x2b, 0x5 };
			if ((*stack->Parameters.QueryInterface.InterfaceType != GuidUsbCamd) &&
				(*stack->Parameters.QueryInterface.InterfaceType != GuidUsb))
			{
				//CPdoExtension::CancelUrb (Irp);

				if (Irp->PendingReturned) 
				{
					IoMarkIrpPending(Irp);
				}

				status = Irp->IoStatus.Status;//STATUS_NOT_SUPPORTED;

				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				--pPdo->m_irpCounterPdo;
				--pPdo->m_irpTcp;
				return status;
			}
			break;
		}
		case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
			status = Irp->IoStatus.Status;//STATUS_NOT_SUPPORTED;

			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			--pPdo->m_irpCounterPdo;
			--pPdo->m_irpTcp;
			return status;
		}
	}

	// verify isoch
    if (!m_Isoch.IsochVerify (Irp, pPdo))
    {
        KIRQL Old;
		KIRQL OldRestore;
		bool bRestore;

		// for D-link
		bRestore = VerifyRaiseIrql (Irp, &OldRestore);

		// get cur Irql level
	    Old = KeGetCurrentIrql ();

		// create new data save
        irpNumber = NewIrpNumber (Irp, pPdo);

		// if Old is dispatch_level to don't wait
		if (Old != PASSIVE_LEVEL)
		{
			UrbPending(Irp, irpNumber);
			irpNumber->bWait = false;
		}
		else if (UrbPending (Irp, irpNumber))
		{
			irpNumber->bWait = false;
			Old = ~PASSIVE_LEVEL;
		}

		if (VerifyAutoComplete (irpNumber, true))
		{
			Old = ~PASSIVE_LEVEL;	// no wait
		}

		if (!irpNumber->bWait)
		{
			IoMarkIrpPending (Irp);
		}
		IrpToQuery (irpNumber);

		if (Old == PASSIVE_LEVEL)
		{

			KeWaitForSingleObject(
				&irpNumber->Event,
				Executive,
				KernelMode,
				FALSE,
				NULL
				);
		}
		if (bRestore)
		{
			KIRQL				OldIrql;
			KeRaiseIrql (OldRestore, &OldIrql);
		}

		if (irpNumber->bWait)
        {
			if (Irp->PendingReturned) 
			{
				IoMarkIrpPending(Irp);
			}


	        status = Irp->IoStatus.Status;  
	        IoCompleteRequest (Irp, IO_NO_INCREMENT);
	        --pPdo->m_irpCounterPdo;
			--pPdo->m_irpTcp;
        }
        else
        {
            status = STATUS_PENDING;
		}


		DeleteIrpNumber (irpNumber);
    }

	return status;
}

bool evuh::CUsbDevToTcp::OutFillGetIrp (PIRP_SAVE pIrpSave, PIRP_NUMBER stDevice)
{
	LONG				lBufferSize = 0;		// Need data size
	BYTE				*pBuffer = NULL;			// pointer to data
	bool				bRet = false;
    bool                bPending = false;
	PIRP				irpDevice = stDevice->Irp;

	DebugPrint (DebugTrace, "OutFillGetIrp irpDevice - %d", stDevice->Count);

	if (!stDevice->SaveIrp)
	{

		pIrpSave->Cancel = stDevice->Cancel;
		pIrpSave->NeedSize = sizeof (IRP_SAVE);
		pIrpSave->Is64 = m_bIs64;
		pIrpSave->NoAnswer = stDevice->bNoAnswer;
		pIrpSave->Device = stDevice->pPdo->m_DeviceIndef?0:1;
		pIrpSave->Dispath = false;
		pIrpSave->Irp = stDevice->Count;


		if (!stDevice->Cancel)
		{
			PIO_STACK_LOCATION  irpStackDevice;
			irpStackDevice = IoGetCurrentIrpStackLocation(irpDevice);

			pIrpSave->Status = irpDevice->IoStatus.Status;
			pIrpSave->Information = (ULONG)irpDevice->IoStatus.Information;
			pIrpSave->StackLocation.MajorFunction = irpStackDevice->MajorFunction;
			pIrpSave->StackLocation.MinorFunction = irpStackDevice->MinorFunction;
			pIrpSave->StackLocation.Parameters.Others.Argument1 = (ULONG64)irpStackDevice->Parameters.Others.Argument1;
			pIrpSave->StackLocation.Parameters.Others.Argument2 = (ULONG64)irpStackDevice->Parameters.Others.Argument2;
			pIrpSave->StackLocation.Parameters.Others.Argument3 = (ULONG64)irpStackDevice->Parameters.Others.Argument3;
			pIrpSave->StackLocation.Parameters.Others.Argument4 = (ULONG64)irpStackDevice->Parameters.Others.Argument4;

			if (stDevice->Irp->CancelRoutine != evuh::CUsbDevToTcp::CancelRoutine)
			{
				#ifdef SET_CANCEL_ROUTINE
					IoSetCancelRoutine (stDevice->Irp, evuh::CUsbDevToTcp::CancelRoutine);
				#endif
			}

			// get buffer size and pointer to data
			switch (irpStackDevice->MajorFunction)
			{
			case IRP_MJ_POWER:
				break;
			case IRP_MJ_PNP:
				OutPnpBufferSize (irpDevice, &pBuffer, &lBufferSize, stDevice);
				break;
			case IRP_MJ_INTERNAL_DEVICE_CONTROL:
				OutUrbBufferSize (irpDevice, &pBuffer, &lBufferSize, stDevice);
				break;
			case IRP_MJ_DEVICE_CONTROL:
				OutUsbIoBufferSize (irpDevice, &pBuffer, &lBufferSize, stDevice);
				break;
			case 0xFF:
				FuncInterfaceBufSize (irpDevice, &pBuffer, &lBufferSize);
				break;
			}
		}

		pIrpSave->NeedSize += lBufferSize;

		if (pIrpSave->Size < pIrpSave->NeedSize)
		{
			pIrpSave->Size = sizeof (IRP_SAVE);
			bRet = false;
		}
		else
		{
			// copy data
			if (!stDevice->Cancel)
			{
				OutFillBuffer (irpDevice, pBuffer, pIrpSave->Buffer, lBufferSize);
			}
			pIrpSave->BufferSize = lBufferSize;

			if (!stDevice->Cancel) // // add IRP into queue that waits for answer
			{
				if (!stDevice->bNoAnswer)
				{
					WiatCompleted(stDevice);
				}
			}
			bRet = true;
		}
	}
	else
	{
		if (pIrpSave->Size < stDevice->SaveIrp->NeedSize)
		{
			pIrpSave->Size = sizeof (IRP_SAVE);
			bRet = false;
		}
		else
		{
			// Copy irp save
			LONG lSize = pIrpSave->Size;
			RtlCopyMemory (pIrpSave, stDevice->SaveIrp, stDevice->SaveIrp->NeedSize);
			pIrpSave->Size = lSize;
			pIrpSave->Irp = stDevice->Count; 

			bRet = true;

			// free stDevice if it is not answer
			if (!stDevice->SaveIrp->NoAnswer)
			{
				WiatCompleted(stDevice);
			}
		}
	}
	return bRet;
}

bool evuh::CUsbDevToTcp::OutFillGetIrps (PIRP irpControl/*, PIRP_NUMBER stDevice*/)
{
	PIO_STACK_LOCATION  irpStackControl;
    PIRP_SAVE			pIrpSave;
	LONG				lBufferSize = 0;		// Need data size
	BYTE				*pBuffer;
	LONG				lCurPos = 0;
	PIRP_NUMBER			stDevice = NULL;

	DebugPrint (DebugTrace, "OutFillGetIrps irpControl - 0x%x", irpControl);

	// verify irp
	irpStackControl = IoGetCurrentIrpStackLocation (irpControl);
	if (irpStackControl->MajorFunction != IRP_MJ_DEVICE_CONTROL)
	{
		DebugPrint (DebugError, "0x%x did not IRP_MJ_DEVICE_CONTROL", irpControl);
		irpControl->IoStatus.Status = STATUS_INVALID_PARAMETER;
		IoCompleteRequest (irpControl, IO_NO_INCREMENT);
		--m_pHub->m_pControl->m_irpCounter;
		return false;
	}

	pBuffer = (LPBYTE)irpControl->AssociatedIrp.SystemBuffer;
	lBufferSize = irpStackControl->Parameters.DeviceIoControl.OutputBufferLength;
	irpControl->IoStatus.Status = STATUS_SUCCESS;

	// verify queue is not empty
	m_spinUsbOut.Lock ();
	if (m_listIrpDevice.GetCount () == 0)
	{
		m_spinUsbOut.UnLock ();
		return false;
	}
	m_spinUsbOut.UnLock ();

	while (lBufferSize > (lCurPos + LONG (sizeof (IRP_SAVE))))
	{
		if (lCurPos > 0 && m_listIrpDevice.GetCount ())
		{
			m_maxDevice++;
			stDevice = NULL;
		}

		// get device irp
		stDevice = NULL;
		m_spinUsbOut.Lock ();
		if (m_listIrpDevice.GetCount () > 0)
		{
			stDevice = (PIRP_NUMBER)m_listIrpDevice.PopFirst ();
		}
		m_spinUsbOut.UnLock ();

		// end queue
		if (stDevice == NULL)
		{
			break;
		}
		// data preparation
		pIrpSave = (PIRP_SAVE)(pBuffer + lCurPos);
		pIrpSave->Size = lBufferSize - lCurPos;

		if (!OutFillGetIrp (pIrpSave, stDevice))
		{
			// buffer is small
			m_spinUsbOut.Lock ();
			m_listIrpDevice.Add (stDevice);
			m_spinUsbOut.UnLock ();

			if (lCurPos == 0)
			{
				// necessary to increase the size of beffera
				DebugPrint (DebugWarning, "OutFillGetIrps STATUS_BUFFER_OVERFLOW");
				irpControl->IoStatus.Status = STATUS_BUFFER_OVERFLOW;//STATUS_BUFFER_TOO_SMALL;
				lCurPos = sizeof (IRP_SAVE);
			}
			break;
		}

		lCurPos += pIrpSave->NeedSize;

		if (stDevice->bNoAnswer || stDevice->Cancel)
		{
			DeleteIrpNumber(stDevice);
		}

		break;
	};
	irpControl->IoStatus.Information = lCurPos;

	// complete control device
	IoSetCancelRoutine (irpControl, NULL);
	IoCompleteRequest (irpControl, IO_NO_INCREMENT);
	--m_pHub->m_pControl->m_irpCounter;
	return true;
}

VOID evuh::CUsbDevToTcp::CancelRoutine( PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIRP_NUMBER			irpNumber = NULL;
	PIRP_NUMBER			irpNumberCancel = NULL;
	KIRQL				OldCancelIrql = Irp->CancelIrql;

	if (!DeviceObject)
	{
		IoReleaseCancelSpinLock ( OldCancelIrql );
		return;
	}

	CPdoExtension		*pPdo = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass (DeviceObject);
    CUsbDevToTcp        *pBase = pPdo->m_pTcp;
	bool				bPause = true;

	if (pBase)
	{
		pBase->DebugPrint (DebugTrace, "CancelRoutine irp- 0x%x", Irp);
		pBase->DebugPrint (DebugError, "CancelRoutine irp- 0x%x", Irp);
		++pBase->m_WaitUse;

		pBase->m_spinUsbIn.Lock ();
		for (int a = 0; a < pBase->m_listIrpAnswer.GetCount (); a++)
		{
			irpNumber = (PIRP_NUMBER)pBase->m_listIrpAnswer.GetItem (a);
			if (!irpNumber)
			{
				continue;
			}

			if (irpNumber->Irp == Irp)
			{
				bPause = false;
				// del list answer
				pBase->DebugPrint (DebugError, "Cancel del from list %d, all %d", irpNumber->Count, pBase->m_listIrpAnswer.GetCount ());
				pBase->m_listIrpAnswer.Remove (a);

				pBase->m_spinUsbIn.UnLock ();

				if (pBase->m_bWork)
				{
					// send cancel to server
					irpNumberCancel = (PIRP_NUMBER)pBase->m_AllocNumber.Alloc ();
					if (irpNumberCancel)
					{
						RtlZeroMemory(irpNumberCancel, sizeof (IRP_NUMBER));
						pBase->m_NumberCount++;
						irpNumberCancel->Count = irpNumber->Count;
						irpNumberCancel->pPdo = irpNumber->pPdo;
						irpNumberCancel->Cancel = true;
						//decrease counter 
						pBase->DeleteIrpNumber(irpNumberCancel);

						pBase->IrpToQuery (irpNumberCancel);
						pBase->DebugPrint (DebugWarning, "CancelRoutine - %d", irpNumberCancel->Count);
					}
				}

				// completed irp of canceled
				irpNumber->Cancel = true;

				pBase->DebugPrint(DebugWarning, "UrbCancel (Irp);");
				pBase->UrbCancel (Irp);
				pBase->DebugPrint(DebugWarning, "CompleteIrpFull (irpNumber)");
				pBase->CompleteIrpFull (irpNumber);
				pBase->DebugPrint(DebugWarning, "<<CompleteIrpFull (irpNumber)");

				--pBase->m_WaitUse;

				IoReleaseCancelSpinLock( OldCancelIrql );
				pBase->DebugPrint (DebugError, "Cancel 0x%x complete", Irp);
				return;

			}
		}
		pBase->m_spinUsbIn.UnLock ();

		IoReleaseCancelSpinLock( OldCancelIrql );

		pBase->DebugPrint (DebugError, "Cancel 0x%x not found", Irp);
		KIRQL OldIrql = KeGetCurrentIrql ();
		if (OldIrql <= APC_LEVEL && bPause)
		{
			LARGE_INTEGER time;
			time.QuadPart = (LONGLONG)-1000000;
			KeDelayExecutionThread (KernelMode, FALSE, &time);
		}
		--pBase->m_WaitUse;
	}
	else
	{
		IoReleaseCancelSpinLock( OldCancelIrql );
	}

    return;
}

VOID evuh::CUsbDevToTcp::CancelRoutineDevice ( PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIRP_NUMBER			irpNumber = NULL;
	PIRP_NUMBER			irpNumberCancel = NULL;

	if (!DeviceObject)
	{
		IoReleaseCancelSpinLock ( Irp->CancelIrql );
		return;
	}

	CPdoExtension		*pPdo = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass (DeviceObject);
    CUsbDevToTcp        *pBase = pPdo->m_pTcp;
	bool				bPause = true;

	if (pBase)
	{
		pBase->DebugPrint (DebugTrace, "CancelRoutineDevice irp- 0x%x", Irp);
		pBase->DebugPrint (DebugError, "CancelRoutineDevice irp- 0x%x", Irp);
		++pBase->m_WaitUse;

		IoReleaseCancelSpinLock( Irp->CancelIrql );

		pBase->m_spinUsbOut.Lock ();
		for (int a = 0; a < pBase->m_listIrpDevice.GetCount (); a++)
		{
			irpNumber = (PIRP_NUMBER)pBase->m_listIrpDevice.GetItem (a);
			if (irpNumber->Irp == Irp)
			{
				bPause = false;
				// del list answer
				pBase->m_listIrpDevice.Remove (a);
				pBase->m_spinUsbOut.UnLock ();

				// completed irp of canceled
				irpNumber->Cancel = true;
				InterlockedDecrement (&irpNumber->Lock);

				pBase->UrbCancel (Irp);

				pBase->CompleteIrpFull(irpNumber);
				--pBase->m_WaitUse;
				return;

			}
		}
		pBase->m_spinUsbOut.UnLock ();
		--pBase->m_WaitUse;
	}
	else
	{
		IoReleaseCancelSpinLock( Irp->CancelIrql );
	}

    return;
}

// *******************************************************
//				PNP data
// *******************************************************
void evuh::CUsbDevToTcp::OutPnpBufferSize (PIRP Irp, BYTE **pBuffer, LONG *lSize, PIRP_NUMBER irpNumber)
{
	*pBuffer = NULL;
	*lSize = 0;
	PIO_STACK_LOCATION irpStack;

	DebugPrint (DebugTrace, "PnpBufferSize irp 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	switch (irpStack->MinorFunction)
	{
	case IRP_MN_QUERY_CAPABILITIES:
		if (irpStack->Parameters.DeviceCapabilities.Capabilities != 0)
		{
			*lSize = PDEVICE_CAPABILITIES (irpStack->Parameters.DeviceCapabilities.Capabilities)->Size;
			(*pBuffer) = (BYTE*)irpStack->Parameters.DeviceCapabilities.Capabilities;
			DebugPrint (DebugInfo, "IRP_MN_QUERY_CAPABILITIES pBuffer = 0x%x size - %d", pBuffer, *lSize);
		}
		break;
	case IRP_MN_QUERY_INTERFACE:
		if (irpStack->Parameters.QueryInterface.Size == 0 
			&& irpStack->Parameters.QueryInterface.Interface
			&& irpStack->Parameters.QueryInterface.Interface->Size)
		{
			irpStack->Parameters.QueryInterface.Size = irpStack->Parameters.QueryInterface.Interface->Size;
		}
		if (irpStack->Parameters.QueryInterface.Size != 0)
		{
			*lSize += sizeof (GUID);
            (*pBuffer) = (BYTE*)irpStack->Parameters.QueryInterface.InterfaceType;
			DebugPrint (DebugInfo, "IRP_MN_QUERY_INTERFACE pBuffer = 0x%x size - %d", pBuffer, *lSize);
		}
		break;
	case IRP_MN_QUERY_DEVICE_RELATIONS:
		irpNumber->iRes2 = Irp->IoStatus.Information;
		break;
	case IRP_MN_READ_CONFIG:
	case IRP_MN_WRITE_CONFIG:

	default:
		break;
	}
}

void evuh::CUsbDevToTcp::InPnpFillNewData (PIRP Irp, PIRP_SAVE irpSave, CPdoExtension *pPdo, PIRP_NUMBER irpNumber)
{
	PIO_STACK_LOCATION irpStack;

	DebugPrint (DebugTrace, "PnpFillNewData irp 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);

    DebugPrint (DebugSpecial, "%s %d", DriverBase::CBaseDeviceExtension::PnPMinorFunctionString (Irp), Irp->IoStatus.Status);

	switch (irpStack->MinorFunction)
	{
	case IRP_MN_QUERY_INTERFACE:
		if (irpStack->Parameters.QueryInterface.Size != 0)
		{
            GUID GuidUsb = {0xb1a96a13, 0x3de0, 0x4574, 0x9b, 0x1, 0xc0, 0x8f, 0xea, 0xb3, 0x18, 0xd6};
            GUID GuidUsbCamd = {0x2bcb75c0, 0xb27f, 0x11d1, 0xba, 0x41, 0x0, 0xa0, 0xc9, 0xd, 0x2b, 0x5};

            if (*irpStack->Parameters.QueryInterface.InterfaceType == GuidUsb)
            {
                int iOldVersion = irpStack->Parameters.QueryInterface.Version;

			    PUSB_BUS_INTERFACE_USBDI_V3_FULL pInterface = (PUSB_BUS_INTERFACE_USBDI_V3_FULL)irpStack->Parameters.QueryInterface.Interface;
				RtlCopyMemory(pInterface, irpSave->Buffer, min(irpSave->BufferSize, irpStack->Parameters.QueryInterface.Size));
				if (irpSave->BufferSize)
			    {
				    // version 0
				    RtlCopyMemory (pInterface, irpSave->Buffer, min (irpSave->BufferSize, sizeof (USB_BUS_INTERFACE_USBDI_V3_FULL)));

                    if (iOldVersion == USB_BUSIF_USBDI_VERSION_3_FULL && pInterface->Version == USB_BUSIF_USBDI_VERSION_2_FULL)
                    {
                        pInterface->Version = USB_BUSIF_USBDI_VERSION_3_FULL;
                        pInterface->Size = sizeof (USB_BUS_INTERFACE_USBDI_V3_FULL);
                    }

                    ::CUsbInterface::InitUsbInterface (pInterface, pPdo);
			    }
            }
            else if (*irpStack->Parameters.QueryInterface.InterfaceType == GuidUsbCamd)
            {
			    RtlCopyMemory (irpStack->Parameters.QueryInterface.Interface, irpSave->Buffer, min (irpSave->BufferSize, irpStack->Parameters.QueryInterface.Size));
				::CUsbInterface::FillInterfaceUsbCamd(irpStack->Parameters.QueryInterface.Interface, pPdo);
            }
			DebugPrint (DebugInfo, "IRP_MN_QUERY_INTERFACE irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);
		}
		break;
	case IRP_MN_QUERY_CAPABILITIES:
		if (irpStack->Parameters.DeviceCapabilities.Capabilities != 0 &&
			irpSave->BufferSize && irpSave->Buffer)
		{
			RtlCopyMemory (irpStack->Parameters.DeviceCapabilities.Capabilities, irpSave->Buffer, min (irpSave->BufferSize, PDEVICE_CAPABILITIES (irpSave->Buffer)->Size));
			DebugPrint (DebugInfo, "IRP_MN_QUERY_CAPABILITIES irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);

			RtlZeroMemory(irpStack->Parameters.DeviceCapabilities.Capabilities, sizeof(DEVICE_CAPABILITIES));

			irpStack->Parameters.DeviceCapabilities.Capabilities->Size = sizeof(DEVICE_CAPABILITIES);
			irpStack->Parameters.DeviceCapabilities.Capabilities->Version = 1;
			irpStack->Parameters.DeviceCapabilities.Capabilities->Removable = 1;
			irpStack->Parameters.DeviceCapabilities.Capabilities->UniqueID = 1;
			irpStack->Parameters.DeviceCapabilities.Capabilities->SurpriseRemovalOK = 1;
			irpStack->Parameters.DeviceCapabilities.Capabilities->Address = m_pPdo->m_uNumberPort;
			irpStack->Parameters.DeviceCapabilities.Capabilities->UINumber = 0;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceState[PowerSystemUnspecified] = PowerDeviceUnspecified;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceState[PowerSystemWorking] = PowerDeviceD0;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceState[PowerSystemSleeping1] = PowerDeviceD3;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceState[PowerSystemSleeping2] = PowerDeviceD3;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceState[PowerSystemHibernate] = PowerDeviceD3;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceState[PowerSystemShutdown] = PowerDeviceD3;
			irpStack->Parameters.DeviceCapabilities.Capabilities->SystemWake = PowerSystemSleeping1;
			irpStack->Parameters.DeviceCapabilities.Capabilities->DeviceWake = PowerDeviceD0;
		}
		else
		{
			Irp->IoStatus.Information = 0;
		}
		break;
	case IRP_MN_QUERY_DEVICE_RELATIONS:
		Irp->IoStatus.Information = irpNumber->iRes2;
		if (irpSave->BufferSize && irpSave->Buffer)
		{
			DebugPrint (DebugInfo, "IRP_MN_QUERY_DEVICE_RELATIONS irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);
		}
		if ((Irp->IoStatus.Status == STATUS_SUCCESS) && (Irp->IoStatus.Information == 0))
		{
			Irp->IoStatus.Status = irpSave->Status;
		}
		break;
	case IRP_MN_QUERY_RESOURCES:
		if (irpSave->BufferSize && irpSave->Buffer)
		{
			Irp->IoStatus.Information = (ULONG_PTR)ExAllocatePoolWithTag (PagedPool, irpSave->BufferSize, tagUsb);
			RtlCopyMemory ((void*)Irp->IoStatus.Information, irpSave->Buffer, irpSave->BufferSize);
			DebugPrint (DebugInfo, "IRP_MN_QUERY_RESOURCES irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);
		}
		else
		{
			Irp->IoStatus.Information = 0;
		}
		break;
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
		if (irpSave->BufferSize && irpSave->Buffer)
		{
			Irp->IoStatus.Information = (ULONG_PTR)ExAllocatePoolWithTag (PagedPool, irpSave->BufferSize, tagUsb);
			RtlCopyMemory ((void*)Irp->IoStatus.Information, irpSave->Buffer, irpSave->BufferSize);
			DebugPrint (DebugInfo, "IRP_MN_QUERY_RESOURCE_REQUIREMENTS irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);
		}
		else
		{
			Irp->IoStatus.Information = 0;
		}
		break;
	case IRP_MN_QUERY_BUS_INFORMATION:
		if (irpSave->BufferSize && irpSave->Buffer)
		{
			Irp->IoStatus.Information = (ULONG_PTR)ExAllocatePoolWithTag (PagedPool, irpSave->BufferSize, tagUsb);
			RtlCopyMemory ((void*)Irp->IoStatus.Information, irpSave->Buffer, irpSave->BufferSize);
			DebugPrint (DebugInfo, "IRP_MN_QUERY_BUS_INFORMATION irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);
		}
		else
		{
			Irp->IoStatus.Information = 0;
		}
		break;
	case IRP_MN_QUERY_ID:
		if (irpSave->BufferSize && irpSave->Buffer)
		{
			if (irpStack->Parameters.QueryId.IdType == BusQueryInstanceID)
			{
				BYTE *pBuff;
				pBuff = (BYTE*)ExAllocatePoolWithTag (PagedPool, irpSave->BufferSize + 2, tagUsb);
				RtlCopyMemory (pBuff, irpSave->Buffer, irpSave->BufferSize);
				Irp->IoStatus.Information = (ULONG_PTR)pBuff;
			}
			else
			{
				Irp->IoStatus.Information = (ULONG_PTR)ExAllocatePoolWithTag (PagedPool, irpSave->BufferSize, tagUsb);
				RtlCopyMemory ((void*)Irp->IoStatus.Information, irpSave->Buffer, irpSave->BufferSize);
			}
			DebugPrint (DebugInfo, "IRP_MN_QUERY_ID irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);
		}
		else
		{
			Irp->IoStatus.Information = 0;
		}
		break;
	case IRP_MN_QUERY_DEVICE_TEXT:
		if (irpSave->BufferSize && irpSave->Buffer)
		{
			Irp->IoStatus.Information = (ULONG_PTR)ExAllocatePoolWithTag (PagedPool, irpSave->BufferSize, tagUsb);
			RtlCopyMemory ((void*)Irp->IoStatus.Information, irpSave->Buffer, irpSave->BufferSize);
			DebugPrint (DebugInfo, "IRP_MN_QUERY_DEVICE_TEXT irpSave->Buffer = 0x%x size - %d", irpSave->Buffer, irpSave->BufferSize);
		}
		else
		{
			Irp->IoStatus.Information = 0;
		}
		break;
    case IRP_MN_READ_CONFIG:
	case IRP_MN_WRITE_CONFIG:
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		DebugPrint (DebugError, "Did not work");

	default:
		DebugPrint (DebugWarning, "default %d", irpStack->MinorFunction);
		break;
	}

	switch (irpStack->MinorFunction)
	{
	case IRP_MN_QUERY_PNP_DEVICE_STATE:
		Irp->IoStatus.Information = (ULONG_PTR)irpSave->Information;
		Irp->IoStatus.Status = 0;
		break;
	}
}

// ************************************************************************
//					Internal device control
// ************************************************************************
void evuh::CUsbDevToTcp::UrbCancel(PIRP Irp, NTSTATUS Status /*= STATUS_CANCELLED*/, NTSTATUS UsbdStatus /*= USBD_STATUS_CANCELED*/)
{
	PURB_FULL     urb;
	PIO_STACK_LOCATION irpStack;

	DebugPrint (DebugTrace, "UrbCancel irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);
	urb = (PURB_FULL)irpStack->Parameters.Others.Argument1;

	CPdoExtension::CancelUrb(Irp, Status, UsbdStatus);
	if (irpStack->Parameters.DeviceIoControl.IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		DebugPrint (DebugSpecial, "%s",  UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode));
		return;
	}

    DebugPrint (DebugSpecial, "%s", UrbFunctionString (urb->UrbHeader.Function));

	switch (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_CONTROL_TRANSFER:
		urb->UrbControlTransfer.TransferBufferLength = 0;
		break;
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		DebugPrint (DebugSpecial, "%s %d %d",  UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode), urb->UrbBulkOrInterruptTransfer.TransferBufferLength, urb->UrbBulkOrInterruptTransfer.TransferFlags);
		urb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
		break;
	case URB_FUNCTION_ISOCH_TRANSFER:
		urb->UrbIsochronousTransfer.TransferBufferLength = 0;
		break;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
		urb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		break;
	case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
	case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_SET_FEATURE_TO_OTHER:
	case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
	case URB_FUNCTION_GET_STATUS_FROM_OTHER:
		break;
	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER :
		urb->UrbControlVendorClassRequest.TransferBufferLength = 0;
		break;
	case URB_FUNCTION_GET_CONFIGURATION:
		urb->UrbControlGetConfigurationRequest.TransferBufferLength = 0;
		break;
	case URB_FUNCTION_GET_INTERFACE:
		urb->UrbControlGetInterfaceRequest.TransferBufferLength = 0;
		break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
		urb->UrbOSFeatureDescriptorRequest.TransferBufferLength = 0;
		break;
	}
}

bool evuh::CUsbDevToTcp::UrbPending (PIRP Irp, PIRP_NUMBER pNumber)
{
	PURB_FULL		urb;
	bool			bRet = false;
	PIO_STACK_LOCATION  irpStack;
	CPdoExtension	*pPdo;

	DebugPrint(DebugTrace, "UrbPending irp - %d", pNumber->Irp);
	DebugPrint(DebugSpecial, "UrbPending irp - 0x%x", pNumber->Irp);
	irpStack = IoGetCurrentIrpStackLocation (Irp);

	if (irpStack->Parameters.DeviceIoControl.IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
		{
		case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
			bRet = true;
		}
		return bRet;
	}

	urb = (PURB_FULL)irpStack->Parameters.Others.Argument1;

	DebugPrint(DebugSpecial, "OUT UrbPending %s - 0x%x", UrbFunctionString(urb->UrbHeader.Function), pNumber->Irp);

	pNumber->Reserved = urb;
	pNumber->iRes1 = urb->UrbHeader.Length;

	// calc size buffer
	switch (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		pNumber->CountWait = 100; // need for usb network  d-link
		pNumber->bManualPending = true;
		break;
	case URB_FUNCTION_CONTROL_TRANSFER:
		if (USBD_TRANSFER_DIRECTION_OUT != USBD_TRANSFER_DIRECTION(urb->UrbControlTransfer.TransferFlags))
        {
            bRet = true;
        }
		else
		{
			pNumber->bManualPending = true;
		}
		break;
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbBulkOrInterruptTransfer.TransferFlags))
		{
			#ifdef BULK_OR_INTERRUPT_OUT_PENDING_TRUE
				bRet = true;
			#endif

			if (m_bBulkOutPending)
			{
				bRet = true;
				break;
			}
			if (m_bBulkOutNotPending)
			{
				bRet = false;
				break;
			}
		}
        else
        {
            // need true 
            //  - Creative Extigy sound blaster (only Win7->WinXp)
            //  - 
            // Need false
            //  - Epson CP91

			#ifdef BULK_OR_INTERRUPT_IN_PENDING_TRUE
				bRet = true;
			#endif
        }
		#ifdef BULK_OR_INTERRUPT_IN_DYNAMIC
			bRet = GetPipePing (urb->UrbBulkOrInterruptTransfer.PipeHandle, pNumber, USBD_TRANSFER_DIRECTION(urb->UrbBulkOrInterruptTransfer.TransferFlags), urb->UrbBulkOrInterruptTransfer.TransferFlags);
		#endif
		break;

	case URB_FUNCTION_ISOCH_TRANSFER:
		pNumber->iRes2 = urb->UrbIsochronousTransfer.TransferBufferLength;
		pNumber->bIsoch = 1;

		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
		{
			#ifdef ISOCH_OUT_PENDING_TRUE
				bRet = true;
			#endif
		}
        else
        {
			#ifdef ISOCH_IN_ADJOURNED_PENDING_TRUE
				pNumber->CountWait = 3;
				pNumber->bManualPending = true;
			#endif
			#ifdef ISOCH_IN_PENDING_TRUE
				bRet = true;
			#endif
        }
		break;

	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		pNumber->CountWait = 5000;
		break;

	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER:

		bRet = true;
		break;
	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
        // need true all
        //  - device extigy bRet 
        // need (in true)/(out false)
        //  - Logitech web cam
		// need false all
		//	- Usb to ethernet

		#ifdef CONTROL_MANUAL_PENDING
			pNumber->bManualPending = true;
		#endif

		if (USBD_TRANSFER_DIRECTION_OUT != USBD_TRANSFER_DIRECTION(urb->UrbControlVendorClassRequest.TransferFlags))
        {
			#ifdef CONTROL_IN_PENDING_TRUE
				bRet = true;
				//
			#endif

			// need for WinXp32 -> Win7x64 for web cam
			if (urb->UrbControlVendorClassRequest.TransferFlags & USBD_SHORT_TRANSFER_OK)
			{
				bRet = false;
			}
			else
			{
				bRet = true;
			}
        }
		else
		{
			pNumber->bManualPending = true;
			#ifdef CONTROL_MANUAL_PENDING_OUT
				pNumber->bManualPending = true;
			#endif
			#ifdef CONTROL_OUT_PENDING_TRUE
				pNumber->bManualPending = true;
			#endif
		}
		break;
	}

    return bRet;
}

void evuh::CUsbDevToTcp::OutUrbBufferSize (PIRP Irp, BYTE **pBuffer, LONG* lSize, PIRP_NUMBER stDevice)
{
	PURB_FULL     urb;
	PIO_STACK_LOCATION irpStack;

	DebugPrint (DebugTrace, "UrbBufferSize irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);
	urb = (PURB_FULL)irpStack->Parameters.Others.Argument1;
	stDevice->Urb = urb;

	if (irpStack->Parameters.DeviceIoControl.IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		OutUsbBufferSize (Irp, pBuffer, lSize);
		return;
	}

	DebugPrint (DebugInfo, "%s", UrbFunctionString (urb->UrbHeader.Function));
	DebugPrint (DebugSpecial, "OUT OutUrbBufferSize %s - %d", UrbFunctionString (urb->UrbHeader.Function), stDevice->Irp);
	
	if (urb->UrbHeader.Length == 0)
	{
		switch (urb->UrbHeader.Function)
		{
		case URB_FUNCTION_SELECT_INTERFACE: {
			urb->UrbHeader.Length = sizeof (_URB_SELECT_INTERFACE) - sizeof (_USBD_PIPE_INFORMATION);
            int iCountPipe = (urb->UrbSelectInterface.Interface.Length - sizeof (_USBD_INTERFACE_INFORMATION) + sizeof (_USBD_PIPE_INFORMATION))/(sizeof (_USBD_PIPE_INFORMATION));
            urb->UrbHeader.Length += (sizeof (_USBD_PIPE_INFORMATION) * max (iCountPipe, 0));
			}break;
		}
	}

    *pBuffer = (BYTE*)urb;
	*lSize = urb->UrbHeader.Length;

	// calc size buffer
	switch (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		if (urb->UrbSelectConfiguration.ConfigurationDescriptor)
		{
			if (urb->UrbSelectConfiguration.ConfigurationDescriptor)
			{
				*lSize = *lSize + urb->UrbSelectConfiguration.ConfigurationDescriptor->wTotalLength;
			}
		}
		break;
	case URB_FUNCTION_CONTROL_TRANSFER:
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbControlTransfer.TransferFlags))
		{
			*lSize = *lSize + urb->UrbControlTransfer.TransferBufferLength;
		}
		break;
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		if (!USB_ENDPOINT_DIRECTION_IN (GetPipeEndpoint (urb->UrbBulkOrInterruptTransfer.PipeHandle, true)))
		{
			*lSize = *lSize + urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
		}
		if (GetPipeType(urb->UrbBulkOrInterruptTransfer.PipeHandle, true) == UsbdPipeTypeInterrupt)
		{
		}
		break;
	case URB_FUNCTION_ISOCH_TRANSFER:
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
		{
			*lSize = *lSize + urb->UrbIsochronousTransfer.TransferBufferLength;
		}
		m_Isoch.BuildIsochStartFrame(urb);
		stDevice->bIsoch = true;
		
		break;

	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		break;
	case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
		*lSize = *lSize + urb->UrbControlDescriptorRequest.TransferBufferLength;
		break;
	case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
	case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_SET_FEATURE_TO_OTHER:
	case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
		break;
	case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
	case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
	case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
	case URB_FUNCTION_GET_STATUS_FROM_OTHER:
		*lSize = *lSize + urb->UrbControlGetStatusRequest.TransferBufferLength;
		break;

	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER :
	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
        // need true all
        //  - device extigy bRet 
        // Nedd (in true)/(out false)
        //  - Logitech web cam
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbControlVendorClassRequest.TransferFlags))
		{
			*lSize = *lSize + urb->UrbControlVendorClassRequest.TransferBufferLength;
		}
		break;
	case URB_FUNCTION_GET_CONFIGURATION:
		*lSize = *lSize + urb->UrbControlGetConfigurationRequest.TransferBufferLength;
		break;
	case URB_FUNCTION_GET_INTERFACE:
		*lSize = *lSize + urb->UrbControlGetInterfaceRequest.TransferBufferLength;
		break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
		*lSize = *lSize + urb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
		break;
	}
}

void evuh::CUsbDevToTcp::OutUrbFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize)
{
	PURB_FULL     urb;
	PIO_STACK_LOCATION irpStack;
	PMDL	pMdlTemp = NULL;
	PVOID	pBuffer = NULL;
	LONG	lSizeBuffer = 0;
	BYTE	*pTemp;
	static	int	iCount = 1;

	DebugPrint (DebugTrace, "UrbFillBuffer irp - 0x%x", irpDevice);

	irpStack = IoGetCurrentIrpStackLocation (irpDevice);

	if (irpStack->Parameters.DeviceIoControl.IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		OutUsbFillBuffer (irpDevice, pInBuffer, pOutBuffer, lSize);
		return;
	}

	urb = (PURB_FULL)irpStack->Parameters.Others.Argument1;

    // Copy URB data
	RtlCopyMemory (pOutBuffer, pInBuffer, urb->UrbHeader.Length);
	pTemp = pOutBuffer + urb->UrbHeader.Length;

	switch (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		if (urb->UrbSelectConfiguration.ConfigurationDescriptor)
		{
			pBuffer = (PVOID)urb->UrbSelectConfiguration.ConfigurationDescriptor;
			lSizeBuffer = urb->UrbSelectConfiguration.ConfigurationDescriptor->wTotalLength;
		}
		break;
	case URB_FUNCTION_CONTROL_TRANSFER:
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbControlTransfer.TransferFlags))
		{
			pMdlTemp = urb->UrbControlTransfer.TransferBufferMDL;
			pBuffer = urb->UrbControlTransfer.TransferBuffer;
			lSizeBuffer = urb->UrbControlTransfer.TransferBufferLength;
		}
		break;
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbBulkOrInterruptTransfer.TransferFlags))
		{
			pMdlTemp = urb->UrbBulkOrInterruptTransfer.TransferBufferMDL;
			pBuffer = urb->UrbBulkOrInterruptTransfer.TransferBuffer;
			lSizeBuffer = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
		}
		break;
	case URB_FUNCTION_ISOCH_TRANSFER:
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
		{
			pMdlTemp = urb->UrbIsochronousTransfer.TransferBufferMDL;
			pBuffer = urb->UrbIsochronousTransfer.TransferBuffer;
			lSizeBuffer = urb->UrbIsochronousTransfer.TransferBufferLength;
		}
		break;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		break;
	case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
		pMdlTemp = urb->UrbControlDescriptorRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlDescriptorRequest.TransferBuffer;
		lSizeBuffer = urb->UrbControlDescriptorRequest.TransferBufferLength;
		break;
	case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
	case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
	case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
	case URB_FUNCTION_GET_STATUS_FROM_OTHER:
		pMdlTemp = urb->UrbControlGetStatusRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlGetStatusRequest.TransferBuffer;
		lSizeBuffer = urb->UrbControlGetStatusRequest.TransferBufferLength;
		break;	case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
	case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_SET_FEATURE_TO_OTHER:
	case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
		break;
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER :
	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbControlVendorClassRequest.TransferFlags))
		{
			pMdlTemp = urb->UrbControlVendorClassRequest.TransferBufferMDL;
			pBuffer = urb->UrbControlVendorClassRequest.TransferBuffer;
			lSizeBuffer = urb->UrbControlVendorClassRequest.TransferBufferLength;
		}
		break;
	case URB_FUNCTION_GET_CONFIGURATION:
		pMdlTemp = urb->UrbControlGetConfigurationRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlGetConfigurationRequest.TransferBuffer;
		lSizeBuffer = urb->UrbControlGetConfigurationRequest.TransferBufferLength;
		break;
	case URB_FUNCTION_GET_INTERFACE:
		pMdlTemp = urb->UrbControlGetInterfaceRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlGetInterfaceRequest.TransferBuffer;
		lSizeBuffer = urb->UrbControlGetInterfaceRequest.TransferBufferLength;
		break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
		pMdlTemp = urb->UrbOSFeatureDescriptorRequest.TransferBufferMDL;
		pBuffer = urb->UrbOSFeatureDescriptorRequest.TransferBuffer;
		lSizeBuffer = urb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        break;
	}

	if (pBuffer)
	{
		// Copy Data
		RtlCopyMemory (pTemp, pBuffer, lSizeBuffer);
	}
	else if (pMdlTemp)
	{
		LONG	lSizeMdl;
		if (lSizeBuffer > 0)
		{
			pBuffer = MmGetSystemAddressForMdlSafe(pMdlTemp, NormalPagePriority);
			lSizeMdl = MmGetMdlByteCount(pMdlTemp);
			if (lSizeMdl < lSizeBuffer)
			{
				DebugPrint (DebugError, "Mdl size not equal");
				return;
			}
			RtlCopyMemory (pTemp, pBuffer, lSizeBuffer);
		}
	}
	else
	{
        DebugPrint (DebugInfo, "No buffer irp 0x%x", irpDevice);
	}
}

void evuh::CUsbDevToTcp::InUrbCopyBufferIsoch (PURB_FULL urb, BYTE *pInBuffer, BYTE *pOutBuffer)
{
	ULONG iOffset = 0;

	DebugPrint (DebugTrace, "InUrbCopyBufferIsoch");

	for (ULONG a = 0; a < urb->UrbIsochronousTransfer.NumberOfPackets; a++)
	{
		if (NT_SUCCESS (urb->UrbIsochronousTransfer.IsoPacket [a].Status))
		{
			RtlCopyMemory ( pInBuffer + urb->UrbIsochronousTransfer.IsoPacket [a].Offset, pOutBuffer + iOffset, urb->UrbIsochronousTransfer.IsoPacket [a].Length);
			iOffset += urb->UrbIsochronousTransfer.IsoPacket [a].Length;
		}
	}
}

void evuh::CUsbDevToTcp::InUrbFillNewData(PIRP_NUMBER irpNumber, PIRP_SAVE irpSave, CPdoExtension *pPdo)
{
	PURB_FULL     urb;
	PURB_FULL     urbNew;
	PIO_STACK_LOCATION irpStack;
	PMDL	pMdlTemp = NULL;
	PVOID	pBufferTemp = NULL;
	PVOID	pBuffer = NULL;
	PMDL	*ppMdlTemp = NULL;
	PVOID	*ppBuffer = NULL;
	LONG	lSizeBuffer = 0;
	LONG	lSizeOffset = 0;
	BYTE	*pTemp;

	PIRP	Irp = irpNumber->Irp;

	DebugPrint (DebugTrace, "UrbFillNewData irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	if (irpStack->Parameters.DeviceIoControl.IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		InUsbFillNewData (Irp, irpSave);
		return;
	}

	urb = (PURB_FULL)irpStack->Parameters.Others.Argument1;
	urbNew = (PURB_FULL)irpSave->Buffer;

	DebugPrint (DebugSpecial, "IN InUrbFillNewData %s - %d", UrbFunctionString (urb->UrbHeader.Function), irpSave->Irp);

	if (!urb)
	{
		DebugPrint (DebugError, "Not found Urb irp - 0x%x", Irp);
		return ;
	}

	DebugPrint (DebugInfo, "%s", UrbFunctionString (urb->UrbHeader.Function));
	pTemp = irpSave->Buffer + urb->UrbHeader.Length;
	lSizeOffset = irpSave->BufferSize - urb->UrbHeader.Length;
	irpSave->IsIsoch = false;

	switch (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		pBuffer = urb->UrbSelectConfiguration.ConfigurationDescriptor;
		ppBuffer = (PVOID*)&urb->UrbSelectConfiguration.ConfigurationDescriptor;
		lSizeBuffer = irpSave->BufferSize - urbNew->UrbHeader.Length;
		ppMdlTemp = NULL;
		if (urb->UrbSelectConfiguration.ConfigurationDescriptor)
		{
			InitInterfaceInfo (&urbNew->UrbSelectConfiguration.Interface, 
							urb->UrbSelectConfiguration.ConfigurationDescriptor->bNumInterfaces);
		}
        break;
	case URB_FUNCTION_SELECT_INTERFACE:
        SetNewInterface (&urbNew->UrbSelectInterface.Interface);
		break;
	case URB_FUNCTION_CONTROL_TRANSFER:
		pMdlTemp = urb->UrbControlTransfer.TransferBufferMDL;
		pBuffer = urb->UrbControlTransfer.TransferBuffer;
		ppMdlTemp = &urb->UrbControlTransfer.TransferBufferMDL;
		ppBuffer = &urb->UrbControlTransfer.TransferBuffer;
		if (irpSave->BufferSize >= LONG (urb->UrbHeader.Length + urbNew->UrbControlTransfer.TransferBufferLength))
		{
			lSizeBuffer = urbNew->UrbControlTransfer.TransferBufferLength;
		}

		break;
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		pMdlTemp = urb->UrbBulkOrInterruptTransfer.TransferBufferMDL;
		pBuffer = urb->UrbBulkOrInterruptTransfer.TransferBuffer;
		ppMdlTemp = &urb->UrbBulkOrInterruptTransfer.TransferBufferMDL;
		ppBuffer = &urb->UrbBulkOrInterruptTransfer.TransferBuffer;
		lSizeBuffer = min (LONG (urbNew->UrbBulkOrInterruptTransfer.TransferBufferLength), irpSave->BufferSize - urb->UrbHeader.Length);

		break;
    case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
        {
            DebugPrint (DebugError, "CURRENT_FRAME_NUMBER - %d", urbNew->UrbGetCurrentFrameNumber.FrameNumber);
        }
        break;
    case IOCTL_INTERNAL_USB_RESET_PORT:
        {
            DebugPrint (DebugError, "IOCTL_INTERNAL_USB_RESET_PORT");
        }
        break;
	case URB_FUNCTION_ISOCH_TRANSFER:
        pMdlTemp = urb->UrbIsochronousTransfer.TransferBufferMDL;
		pBuffer = urb->UrbIsochronousTransfer.TransferBuffer;
		ppMdlTemp = &urb->UrbIsochronousTransfer.TransferBufferMDL;
		ppBuffer = &urb->UrbIsochronousTransfer.TransferBuffer;

		if (NT_SUCCESS (urbNew->UrbHeader.Status))
		{
			LONG lCurFrame = urbNew->UrbIsochronousTransfer.StartFrame + urbNew->UrbIsochronousTransfer.NumberOfPackets;
			DebugPrint (DebugError, "StartFrame5 %d %d", urbNew->UrbIsochronousTransfer.StartFrame, urbNew->UrbIsochronousTransfer.NumberOfPackets);
		}

		if (USBD_TRANSFER_DIRECTION_IN == USBD_TRANSFER_DIRECTION(urbNew->UrbIsochronousTransfer.TransferFlags))
		{
			lSizeBuffer = urbNew->UrbIsochronousTransfer.TransferBufferLength;
			irpSave->IsIsoch = true;
		}
		else
		{
			m_Isoch.RestartTimer();
		}

		urbNew->UrbHeader.UsbdDeviceHandle = 0;
		urbNew->UrbHeader.UsbdFlags = 0;
		break;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		lSizeBuffer = urbNew->UrbControlDescriptorRequest.TransferBufferLength;
		if (lSizeBuffer >= sizeof (USB_DEVICE_DESCRIPTOR) && urb->UrbControlDescriptorRequest.DescriptorType == USB_DEVICE_DESCRIPTOR_TYPE)
        {
            PUSB_DEVICE_DESCRIPTOR   DeviceDescriptor = (PUSB_DEVICE_DESCRIPTOR)pTemp;
            if (DeviceDescriptor->bDescriptorType == USB_DEVICE_DESCRIPTOR_TYPE)
            {
                RtlCopyMemory (&pPdo->m_DeviceDescriptor, pTemp, min (DeviceDescriptor->bLength, sizeof (USB_DEVICE_DESCRIPTOR)));
            }
        }

	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
		pMdlTemp = urb->UrbControlDescriptorRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlDescriptorRequest.TransferBuffer;
		lSizeBuffer = urbNew->UrbControlDescriptorRequest.TransferBufferLength;
		ppMdlTemp = &urb->UrbControlDescriptorRequest.TransferBufferMDL;
		ppBuffer = &urb->UrbControlDescriptorRequest.TransferBuffer;
		break;

	case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
	case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
	case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
	case URB_FUNCTION_GET_STATUS_FROM_OTHER:
		pMdlTemp = urb->UrbControlGetStatusRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlGetStatusRequest.TransferBuffer;
		lSizeBuffer = urbNew->UrbControlGetStatusRequest.TransferBufferLength;
		ppMdlTemp = &urb->UrbControlGetStatusRequest.TransferBufferMDL;
		ppBuffer = &urb->UrbControlGetStatusRequest.TransferBuffer;
		break;	

	case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
	case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_SET_FEATURE_TO_OTHER:
	case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
		break;

	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER :
		if (!NT_SUCCESS (urbNew->UrbHeader.Status))
		{
			int a = 0 +2;
			a = a;
		}
		pMdlTemp = urb->UrbControlVendorClassRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlVendorClassRequest.TransferBuffer;
		ppMdlTemp = &urb->UrbControlVendorClassRequest.TransferBufferMDL;
		ppBuffer = &urb->UrbControlVendorClassRequest.TransferBuffer;
		if (USBD_TRANSFER_DIRECTION_IN == USBD_TRANSFER_DIRECTION(urbNew->UrbControlVendorClassRequest.TransferFlags))
		{
			lSizeBuffer = urbNew->UrbControlVendorClassRequest.TransferBufferLength;
		}
		break;

	case URB_FUNCTION_GET_CONFIGURATION:
		pMdlTemp = urb->UrbControlGetConfigurationRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlGetConfigurationRequest.TransferBuffer;
		lSizeBuffer = urbNew->UrbControlGetConfigurationRequest.TransferBufferLength;
		ppMdlTemp = &urb->UrbControlGetConfigurationRequest.TransferBufferMDL;
		ppBuffer = &urb->UrbControlGetConfigurationRequest.TransferBuffer;
		break;

	case URB_FUNCTION_GET_INTERFACE:
		pMdlTemp = urb->UrbControlGetInterfaceRequest.TransferBufferMDL;
		pBuffer = urb->UrbControlGetInterfaceRequest.TransferBuffer;
		lSizeBuffer = urbNew->UrbControlGetInterfaceRequest.TransferBufferLength;
		ppMdlTemp = &urb->UrbControlGetInterfaceRequest.TransferBufferMDL;
		ppBuffer = &urb->UrbControlGetInterfaceRequest.TransferBuffer;
		break;

    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
		pMdlTemp = urb->UrbOSFeatureDescriptorRequest.TransferBufferMDL;
		pBuffer = urb->UrbOSFeatureDescriptorRequest.TransferBuffer;
		lSizeBuffer = urbNew->UrbOSFeatureDescriptorRequest.TransferBufferLength;
		ppMdlTemp = &urb->UrbOSFeatureDescriptorRequest.TransferBufferMDL;
		ppBuffer = &urb->UrbOSFeatureDescriptorRequest.TransferBuffer;
        break;

	case URB_FUNCTION_ABORT_PIPE:
    case URB_FUNCTION_RESET_PIPE:
		break;

	}

 	if (pBuffer)
	{
		// Copy Data
		if (urb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
		{
			if (lSizeBuffer)
			{
				InUrbCopyBufferIsoch (urbNew, (BYTE*)pBuffer, pTemp);
			}
		}
		else
		{
			RtlCopyMemory (pBuffer, pTemp, lSizeBuffer);
		}
	}
	else if (pMdlTemp)
	{
		LONG	lSizeMdl;
		pBufferTemp = MmGetSystemAddressForMdlSafe (pMdlTemp, NormalPagePriority);
		lSizeMdl = MmGetMdlByteCount(pMdlTemp);
		if (lSizeMdl < lSizeBuffer || lSizeOffset > lSizeMdl)
		{
			DebugPrint (DebugError, "Mdl size not equal");
			return;
		}
		if (urb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
		{
			if (lSizeBuffer)
			{
				InUrbCopyBufferIsoch (urbNew, (BYTE*)pBufferTemp, pTemp);
			}
		}
		else
		{
			RtlCopyMemory (pBufferTemp, pTemp, lSizeBuffer);
		}
	}
	else
	{
        DebugPrint (DebugInfo, "No data");
	}

	switch (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:

		urb->UrbHeader.Function = urbNew->UrbHeader.Function;
		DetectUsbVersion (urb);
		break;
	}


	// Copy URB
	RtlCopyMemory (urb, irpSave->Buffer, urb->UrbHeader.Length);
	// restore pointer to buffer
	if (ppBuffer && pBuffer)
	{
        *ppBuffer = pBuffer;
		if (ppMdlTemp)
		{
			*ppMdlTemp = NULL;
		}
	}
	else if (ppMdlTemp && pMdlTemp)
	{
		*ppMdlTemp = pMdlTemp;
		if (ppBuffer)
		{
			*ppBuffer = NULL;
		}
	}
}

// **********************************************************
//							USB
// **********************************************************

void evuh::CUsbDevToTcp::OutUsbBufferSize (PIRP Irp, BYTE **pBuffer, LONG* lSize)
{
	PIO_STACK_LOCATION irpStack;

	DebugPrint (DebugTrace, "UsbBufferSize irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);

    DebugPrint (DebugSpecial, "%s",  UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode));

	switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
		*pBuffer = NULL;
		*lSize = 0;
		RtlCopyMemory (&m_idleInfo, irpStack->Parameters.DeviceIoControl.Type3InputBuffer, min (irpStack->Parameters.DeviceIoControl.InputBufferLength, sizeof (m_idleInfo)));
		break;
	case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
		*pBuffer = (BYTE*)irpStack->Parameters.Others.Argument1;
		*lSize = sizeof (ULONG);
		break;
	case IOCTL_INTERNAL_USB_RESET_PORT:
	case IOCTL_INTERNAL_USB_ENABLE_PORT:
	case IOCTL_INTERNAL_USB_CYCLE_PORT:
	//case IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO:
	//case IOCTL_INTERNAL_USB_GET_HUB_COUNT:
	//case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE:
		*pBuffer = 0;
		*lSize = 0;
		break;
	case IOCTL_INTERNAL_USB_GET_HUB_NAME:
		*pBuffer = (BYTE*)Irp->AssociatedIrp.SystemBuffer;
		*lSize = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
		break;
	case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
		*pBuffer = (BYTE*)irpStack->Parameters.Others.Argument1;
		*lSize = ULONG ((ULONG_PTR)irpStack->Parameters.Others.Argument2);
		if (*lSize != sizeof (USB_HUB_NAME))
		{
			*lSize = *((ULONG*)irpStack->Parameters.Others.Argument2);
		}
		break;
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
		*pBuffer = (BYTE*)irpStack->Parameters.Others.Argument1;
		*lSize = sizeof (USB_TOPOLOGY_ADDRESS_FULL);		
		break;
	}
}

void evuh::CUsbDevToTcp::OutUsbFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize)
{
	DebugPrint (DebugTrace, "UsbFillBuffer");
	RtlCopyMemory (pOutBuffer, pInBuffer, lSize);
}

void evuh::CUsbDevToTcp::InUsbFillNewData (PIRP Irp, PIRP_SAVE irpSave)
{
	PIO_STACK_LOCATION irpStack;

	DebugPrint (DebugTrace, "UsbFillNewData irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
		if (irpSave->BufferSize)
		{
			RtlCopyMemory (irpStack->Parameters.Others.Argument1, irpSave->Buffer, min (irpSave->BufferSize, sizeof (ULONG)));
		}
		break;
	case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
	case IOCTL_INTERNAL_USB_RESET_PORT:
	case IOCTL_INTERNAL_USB_ENABLE_PORT:
	case IOCTL_INTERNAL_USB_CYCLE_PORT:
		break;
	case IOCTL_INTERNAL_USB_GET_HUB_NAME:
		if (irpSave->BufferSize)
		{
			RtlCopyMemory (Irp->AssociatedIrp.SystemBuffer, irpSave->Buffer, min ((ULONG)irpSave->BufferSize, irpStack->Parameters.DeviceIoControl.OutputBufferLength));
		}
		break;
	case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
		if (irpSave->BufferSize)
		{
			RtlCopyMemory (irpStack->Parameters.Others.Argument1, irpSave->Buffer, min ((ULONG)irpSave->BufferSize, ULONG ((ULONG_PTR)irpStack->Parameters.Others.Argument2)));
		}
		break;
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
		if (irpSave->BufferSize)
		{
			RtlCopyMemory (irpStack->Parameters.Others.Argument1, irpSave->Buffer, min ((ULONG)irpSave->BufferSize, sizeof (USB_TOPOLOGY_ADDRESS_FULL)));
		}
		break;
	}
}

// **************************************************************
//					func
// **************************************************************
void evuh::CUsbDevToTcp::FuncInterfaceBufSize (PIRP Irp, BYTE **pBuffer, LONG* lSize)
{
	PIO_STACK_LOCATION irpStack;
	PFUNC_INTERFACE	   pInterface;

	DebugPrint (DebugTrace, "FuncInterfaceBufSize irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);
	pInterface = (PFUNC_INTERFACE)irpStack->Parameters.Others.Argument1;
	*pBuffer = (BYTE*)pInterface;
	*lSize = sizeof (FUNC_INTERFACE);

	switch (pInterface->lNumFunc)
	{
    case FI_QueryBusInformation:
		*lSize += pInterface->Param3;
        break;
    case FI_USBCAMD_WaitOnDeviceEvent:
    case FI_USBCAMD_BulkReadWrite:
        *lSize += pInterface->Param2;
        break;
    case FI_USBCAMD_SetVideoFormat:
        *lSize += pInterface->Param1;
        break;
	}
}

void evuh::CUsbDevToTcp::FuncInterfaceFillBuffer (PIRP irpDevice, BYTE *pInBuffer, BYTE *pOutBuffer, LONG lSize, PVOID pParam)
{
    PFUNC_INTERFACE		pInterface = (PFUNC_INTERFACE)pInBuffer;    	
	BYTE				*pBuff;

	DebugPrint (DebugTrace, "FuncInterfaceFillBuffer irp - 0x%x", irpDevice);

	RtlCopyMemory (pOutBuffer, pInterface, sizeof (FUNC_INTERFACE));

	pBuff = pOutBuffer;
	pBuff += sizeof (FUNC_INTERFACE);
	switch (pInterface->lNumFunc)
	{
    case FI_QueryBusInformation:
		RtlCopyMemory (pBuff, pParam, pInterface->Param3);
        break;
    case FI_USBCAMD_WaitOnDeviceEvent:
    case FI_USBCAMD_BulkReadWrite:
		RtlCopyMemory (pBuff, pParam, pInterface->Param2);
        break;
    case FI_USBCAMD_SetVideoFormat:
		RtlCopyMemory (pBuff, pParam, pInterface->Param1);
        break;
	}
}

void evuh::CUsbDevToTcp::FuncInterfaceFillNewData (PIRP Irp, PIRP_SAVE irpSave)
{

 	PIO_STACK_LOCATION irpStack;
	PFUNC_INTERFACE		pInterfaceNew = (PFUNC_INTERFACE)irpSave->Buffer;    	
    PFUNC_INTERFACE		pInterfaceOld;   
	BYTE				*pBuff;

	DebugPrint (DebugTrace, "FuncInterfaceFillNewData irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);
	pInterfaceOld = (PFUNC_INTERFACE)irpStack->Parameters.Others.Argument1;

    pInterfaceOld->Param1 = pInterfaceNew->Param1;
	if (pInterfaceOld->lNumFunc != FI_QueryBusInformation)
	{
		pInterfaceOld->Param2 = pInterfaceNew->Param2;
	}
	else
	{
		if (irpSave->BufferSize >= long(sizeof (FUNC_INTERFACE) + pInterfaceNew->Param3))
		{
			pBuff = irpSave->Buffer;
			pBuff += sizeof (FUNC_INTERFACE);
			RtlCopyMemory (irpStack->Parameters.Others.Argument2, pBuff, pInterfaceNew->Param3);
		}
	}
	pInterfaceOld->Param3 = pInterfaceNew->Param3;
	pInterfaceOld->Param4 = pInterfaceNew->Param4;
	pInterfaceOld->Param5 = pInterfaceNew->Param5;
}

// *************************************************
//				Cancel
// *************************************************
void evuh::CUsbDevToTcp::CancelingIrp (PIRP_NUMBER stDevice, KIRQL           CancelIrql)
{
	PDRIVER_CANCEL cancelRoutine;

	DebugPrint (DebugTrace, "CancelingIrp %d", stDevice->Count);

	CPdoExtension::CancelUrb (stDevice->Irp, USBD_STATUS_CANCELED);
	stDevice->Irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;

	if (stDevice->bWait)
	{
		DebugPrint (DebugSpecial, "CancelingIrp bWait %d", stDevice->Count);
		InterlockedIncrement (&stDevice->Lock);
		KeSetEvent (&stDevice->Event, IO_NO_INCREMENT, FALSE);
	}
	else
	{
		DebugPrint (DebugSpecial, "CancelingIrp IoCompleteRequest %d", stDevice->Count);
		IoCompleteRequest (stDevice->Irp, IO_NO_INCREMENT);
		--stDevice->pPdo->m_irpCounterPdo;
		--stDevice->pPdo->m_irpTcp;
	}
}

void evuh::CUsbDevToTcp::CancelIrpAnswer()
{
}

void evuh::CUsbDevToTcp::CancelIrp ()
{
	PIRP_NUMBER		stDevice;
	PIRP			pIrp;
    KIRQL           OldIrql;
	KIRQL           CancelIrql = PASSIVE_LEVEL;

	DebugPrint (DebugTrace, "CancelIrp");
	DebugPrint (DebugError, "CancelIrp");

    m_bWork = false;
	// time for finishing
	{
		LARGE_INTEGER time;
		time.QuadPart = (LONGLONG)-1000000;
		KeDelayExecutionThread (KernelMode, FALSE, &time);
	}

	// stop canceling dpc
	m_timerCancel.CancelTimer();

	//delete isoch irp
	m_Isoch.ClearIsochIrp();

	DebugPrint (DebugInfo, "Deleting wait answer irp");
	m_spinUsbOut.Lock ();
    // Delete waiting processing IRPs
	DebugPrint (DebugInfo, "Deleting wait processing irp");
	while (m_listIrpDevice.GetCount ())
	{
        stDevice = (PIRP_NUMBER)m_listIrpDevice.PopFirst ();
		if (stDevice->Irp)
		{
			if (stDevice->bNoAnswer || stDevice->Cancel)
			{
				DeleteIrpNumber (stDevice);
			}
			else if (!VerifyMultiCompleted (stDevice->Irp))
			{
				UrbCancel(stDevice->Irp, STATUS_UNSUCCESSFUL);
				CompleteIrpFull(stDevice);
			}
			else
			{
				DeleteIrpNumber (stDevice);

				--stDevice->pPdo->m_irpCounterPdo;
				--stDevice->pPdo->m_irpTcp;

				DebugPrint (DebugError, "CancelIrp m_listIrpDevice MultiCompleted - %d", stDevice->Count);
			}
		}
		else
		{
			DeleteIrpNumber (stDevice);
		}

	}

	m_spinUsbOut.UnLock ();

    // Delete waiting answer IRPs
	while (m_listIrpAnswer.GetCount ())
	{
		DebugPrint(DebugSpecial, "IoCancelIrp get irp");
        // Get first IRP waiting for the answer
		m_spinUsbIn.Lock();
		stDevice = (PIRP_NUMBER)m_listIrpAnswer.PopFirst();
		m_spinUsbIn.UnLock();

		if (!stDevice)
		{
			continue;
		}

		if (stDevice->Irp)
		{
			if (!VerifyMultiCompleted (stDevice->Irp))
			{
				UrbCancel(stDevice->Irp, STATUS_UNSUCCESSFUL, USBD_STATUS_STALL_PID);
				CompleteIrpFull(stDevice);
			}
			else
			{
				DebugPrint (DebugSpecial, "CancelIrp multicomplete");
				DeleteIrpNumber (stDevice);

				--stDevice->pPdo->m_irpCounterPdo;
				--stDevice->pPdo->m_irpTcp;

				DebugPrint (DebugError, "CancelIrp m_listIrpAnswer MultiCompleted - %d", stDevice->Count);
			}

		}
		else
		{
			DebugPrint (DebugSpecial, "CancelIrp IRP=NULL");
		}
	}


    // Delete waiting IRPs from 3rd ring
    PIRP_SAVE			pIrpSave;

	DebugPrint (DebugInfo, "Deleting wait IRPs from 3rd ring");
	m_spinUsbOut.Lock ();
	while (m_listGetIrpToTcp.GetCount ())
	{
        // Get first element 
        pIrp = (PIRP)m_listGetIrpToTcp.PopFirst ();

		if (pIrp)
		{
			if (!VerifyMultiCompleted (pIrp))
			{
				IoSetCancelRoutine (pIrp, NULL);

    			pIrpSave = (PIRP_SAVE)pIrp->AssociatedIrp.SystemBuffer;
				pIrpSave->NeedSize = 0;
				pIrp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
				pIrp->IoStatus.Information = sizeof (IRP_SAVE);

    			// Complete control device
				IoCompleteRequest (pIrp, IO_NO_INCREMENT);
			}
			else
			{
				DebugPrint (DebugError, "CancelIrp m_listGetIrpToTcp MultiCompleted");
			}

		}
	    --m_pHub->m_pControl->m_irpCounter;
	}
    m_spinUsbOut.UnLock ();

	// deleete irp completed
	m_spinQueue.Lock ();
	while (m_listQueueComplete.GetCount ())
	{
        stDevice = (PIRP_NUMBER)m_listQueueComplete.PopFirst();

		if (m_bNewDriverWork)
		{
			if (stDevice)
			{
				// free memory
				m_poolBuffer.FreeBuffer (stDevice);
			}
			continue;
		}

        if (!stDevice)
        {
            continue;
        }
		m_spinQueue.UnLock ();
		CompleteIrp (stDevice);
		m_spinQueue.Lock ();
	}
	m_spinQueue.UnLock ();

	if (KeGetCurrentIrql () == PASSIVE_LEVEL)
	{
		// drop process quote
		LARGE_INTEGER time;
		time.QuadPart = (LONGLONG)-10;
		KeDelayExecutionThread (KernelMode, FALSE, &time);
	}

	DebugPrint (DebugInfo, "Stoping all thread");
}

void evuh::CUsbDevToTcp::StatusDevNotResponToCanceled (PIRP pIrp)
{
    PIO_STACK_LOCATION  stack;
    PURB_FULL     urb;

    stack = IoGetCurrentIrpStackLocation (pIrp);

    if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL && 
        stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        urb = (PURB_FULL)stack->Parameters.Others.Argument1;
		if (urb->UrbHeader.Status == USBD_STATUS_DEV_NOT_RESPONDING)
		{
			urb->UrbHeader.Status = USBD_STATUS_CANCELED;
			pIrp->IoStatus.Status = STATUS_CANCELLED;
			pIrp->Cancel = TRUE;
		}
    }
}

bool evuh::CUsbDevToTcp::VerifyMultiCompleted (PIRP	pIrp)
{
	if (! (pIrp->CurrentLocation > (CCHAR) (pIrp->StackCount + 1) ||
		   pIrp->Type != IO_TYPE_IRP)) 
	{
		return false;
	}

	CBaseClass log;
	log.SetDebugLevel(DebugTrace);

	if (pIrp->Type == IO_TYPE_IRP)
	{
		log.DebugPrint (DebugTrace, "VerifyMultiCompleted type - 0x%x", pIrp);
	}
	else
	{
		log.DebugPrint (DebugTrace, "VerifyMultiCompleted stack - 0x%x", pIrp);	
	}
	return true;
}

void evuh::CUsbDevToTcp::CompleteIrpFull (PIRP_NUMBER	irpNum, bool bAsync)
{
	if (irpNum->bWait)
	{
		InterlockedIncrement (&irpNum->Lock);
		KeSetEvent (&irpNum->Event, IO_NO_INCREMENT, FALSE);
	}
	else
	{
		if (bAsync)
		{
			SetCompleteIrpWorkItm (irpNum, WorkItemNewThreadToPool);
		}
		else
		{
			CompleteIrp (irpNum);
		}
	}
}

void evuh::CUsbDevToTcp::CompleteIrp (PIRP_NUMBER	irpNumber)
{
	KIRQL           OldIrql;

	OldIrql = KeGetCurrentIrql ();

	if (irpNumber->bManualPending && irpNumber->bWait)
	{
		IoMarkIrpPending (irpNumber->Irp);
	}

	if (irpNumber->bManualPending)
	{
		irpNumber->bWait = false;
		KeSetEvent(&irpNumber->Event, IO_NO_INCREMENT, FALSE);
	}

	if (!VerifyMultiCompleted (irpNumber->Irp)) 
	{
		StatusDevNotResponToCanceled (irpNumber->Irp);

		IoCompleteRequest (irpNumber->Irp, IO_NO_INCREMENT);
		DebugPrint (DebugError, "CompleteIrp - %d",irpNumber->Count);
	}
	else
	{
		DebugPrint (DebugError, "CompleteIrp MultiCompleted - %d",irpNumber->Count);
	}

    --irpNumber->pPdo->m_irpCounterPdo;
	--irpNumber->pPdo->m_irpTcp;

	if (!DeleteIrpNumber (irpNumber))
	{
	}
}

VOID evuh::CUsbDevToTcp::ThreadComplete ()
{
	PIRP_NUMBER		irpNumber;

	DebugPrint (DebugTrace, "ThreadComplete");

	while (m_bWork)
	{
		m_spinQueue.Lock ();
		if (m_listQueueComplete.GetCount ())
		{
            // increase curent use thread
            ++m_iCurUse;
            irpNumber = (PIRP_NUMBER)m_listQueueComplete.PopFirst();
		}
		else
		{
			m_spinQueue.UnLock ();
			m_listQueueComplete.WaitAddElemetn ();
			continue;
		}
		m_spinQueue.UnLock ();

		DebugPrint (DebugTrace, "ThreadComplete - %d 0x%x", irpNumber->Count, irpNumber->Irp->IoStatus.Status);

        if (!irpNumber)
        {
            break;
        }

		if (irpNumber->bNoAnswer)
		{
			// drop process quote
			LARGE_INTEGER time;
			time.QuadPart = (LONGLONG)-1000;
			KeDelayExecutionThread (KernelMode, FALSE, &time);
		}

		CompleteIrp (irpNumber);

		m_spinQueue.Lock ();
		--m_iCurUse;
		m_spinQueue.UnLock ();
    }
	m_spinQueue.Lock();
    m_listQueueComplete.Add (NULL);
	m_spinQueue.UnLock();
    --m_WaitUse;
}

VOID evuh::CUsbDevToTcp::WorkItemNewThreadToPool (PVOID  Context )
{
	HANDLE              threadHandle;
    NTSTATUS            status;
	CUsbDevToTcp *pTcp = (CUsbDevToTcp *)Context;

	pTcp->ThreadComplete ();
}

void evuh::CUsbDevToTcp::StartNewThreadToPool(PKSTART_ROUTINE pRoutine)
{

	if (m_DevObject)
	{
		// Create thread 
		++m_WaitUse;
		++m_iCountThread;

		Thread::Create(pRoutine, this);
	}
}

// *****************************************************************
//			interface

NTSTATUS evuh::CUsbDevToTcp::SetNewInterface (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo)
{
    PUSBD_INTERFACE_INFORMATION pInterface;
	UCHAR						InterfaceNumber = pInterfaceInfo->InterfaceNumber;
    int							iCount = 0;
	CBuffer						pTemp (NonPagedPool);
	PUSBD_INTERFACE_INFORMATION pInterfaceNonPaged;

    DebugPrint (DebugTrace, "SetNewInterface");

	pTemp.AddArray((BYTE*)pInterfaceInfo, pInterfaceInfo->Length);
	pInterfaceNonPaged = (PUSBD_INTERFACE_INFORMATION)pTemp.GetData();

    m_spinInterface.Lock ();
    for (iCount = 0; iCount < m_listInterface.GetCount (); iCount++)
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem (iCount);
        if (pInterface->InterfaceNumber == InterfaceNumber)
        {
            m_listInterface.Remove (iCount);
            ExFreePool (pInterface);
            pInterface = NULL;
            pInterface = (PUSBD_INTERFACE_INFORMATION)ExAllocatePoolWithTag (NonPagedPool, pInterfaceNonPaged->Length, tagUsb);
            if (pInterface)
            {
                m_listInterface.Add (pInterface);
            }
            else
            {
                m_spinInterface.UnLock ();
                return STATUS_NO_MEMORY;
            }
            RtlCopyMemory (pInterface, pInterfaceNonPaged, pInterfaceNonPaged->Length);

			DebugPrint (DebugError, "NewInterface - i(%d) - a(%d) - c(%d) - sc(%d) - p(%d)", pInterfaceNonPaged->InterfaceNumber, 
							pInterfaceNonPaged->AlternateSetting, pInterfaceNonPaged->Class, pInterfaceNonPaged->SubClass, pInterfaceNonPaged->Protocol);

            break;
        }
    }
    if (iCount == m_listInterface.GetCount ())
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)ExAllocatePoolWithTag (NonPagedPool, pInterfaceNonPaged->Length, tagUsb);
        if (pInterface)
        {
            m_listInterface.Add (pInterface);
            RtlCopyMemory (pInterface, pInterfaceNonPaged, pInterfaceNonPaged->Length);
        }
        else
        {
            m_spinInterface.UnLock ();
            return STATUS_NO_MEMORY;
        }
    }
    m_spinInterface.UnLock ();
    return STATUS_SUCCESS;
}

void evuh::CUsbDevToTcp::ClearInterfaceInfo ()
{
    PUSBD_INTERFACE_INFORMATION pInterface;

    DebugPrint (DebugTrace, "ClearInterfaceInfo");

    m_spinInterface.Lock ();
    while (m_listInterface.GetCount ())
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.PopFirst();
        ExFreePool (pInterface);
    }
    m_spinInterface.UnLock ();
}

NTSTATUS evuh::CUsbDevToTcp::InitInterfaceInfo (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo, USHORT uCount)
{
    PUSBD_INTERFACE_INFORMATION pInterface;

    DebugPrint (DebugTrace, "InitInterfaceInfo %d", uCount);

    ClearInterfaceInfo ();

    for (USHORT a = 0; a < uCount; a++)
    {
	    pInterface = (PUSBD_INTERFACE_INFORMATION)ExAllocatePoolWithTag (NonPagedPool, pInterfaceInfo->Length, tagUsb);

	    if (pInterface)
	    {
		    RtlCopyMemory (pInterface, pInterfaceInfo, pInterfaceInfo->Length);
            m_listInterface.Add (pInterface);
	    }
        else
        {
            return STATUS_NO_MEMORY;
        }
		pInterfaceInfo = (PUSBD_INTERFACE_INFORMATION) (((char*)pInterfaceInfo) + pInterfaceInfo->Length);
    }
    return STATUS_SUCCESS;
}

USBD_PIPE_TYPE evuh::CUsbDevToTcp::GetPipeType(USBD_PIPE_HANDLE Handle, bool bLock)
{
	PUSBD_INTERFACE_INFORMATION pInterface;
	ULONG iCountPipe;
	USBD_PIPE_TYPE uRet = UsbdPipeTypeControl;
	bool bFind = false;
	int iCount;

	if (bLock)
	{
		m_spinInterface.Lock();
	}
	for (iCount = 0; iCount < m_listInterface.GetCount(); pInterface = NULL, iCount++)
	{
		pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem(iCount);
		for (iCountPipe = 0; iCountPipe < pInterface->NumberOfPipes; iCountPipe++)
		{
			if (pInterface->Pipes[iCountPipe].PipeHandle == Handle)
			{
				bFind = true;
				uRet = pInterface->Pipes[iCountPipe].PipeType;
				break;
			}
		}
		if (bFind)
		{
			break;
		}
	}

	if (bLock)
	{
		m_spinInterface.UnLock();
	}

	return uRet;
}

UCHAR evuh::CUsbDevToTcp::GetPipeEndpoint (USBD_PIPE_HANDLE Handle, bool bLock)
{
	PUSBD_INTERFACE_INFORMATION pInterface;
	ULONG iCountPipe;
	UCHAR uRet = 0;
	bool bFind = false;
	int iCount;
	
	if (bLock)
	{
		m_spinInterface.Lock ();
	}
    for (iCount = 0; iCount < m_listInterface.GetCount (); pInterface = NULL, iCount++)
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem (iCount);
        for (iCountPipe = 0; iCountPipe < pInterface->NumberOfPipes; iCountPipe++)
        {
            if (pInterface->Pipes [iCountPipe].PipeHandle == Handle)
            {
				bFind = true;
				uRet = pInterface->Pipes [iCountPipe].EndpointAddress;
                break;
            }
        }
		if (bFind)
		{
			break;
		}
    }

	if (bLock)
	{
		m_spinInterface.UnLock ();
	}

	return uRet;
}

UCHAR evuh::CUsbDevToTcp::GetClassDevice ()
{
    PUSBD_INTERFACE_INFORMATION pInterface;
	UCHAR bRet = 255;

    DebugPrint (DebugTrace, "ClassDevice");

    m_spinInterface.Lock ();
    if (m_listInterface.GetCount () == 1)
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem (0);

		bRet = pInterface->Class;
    }
	m_spinInterface.UnLock ();

	return bRet;
}

bool evuh::CUsbDevToTcp::GetPipePing (USBD_PIPE_HANDLE Handle, PIRP_NUMBER pNumber, ULONG Direct, ULONG  Flags)
{
	PUSBD_INTERFACE_INFORMATION pInterface = nullptr;
    int iCount;
    ULONG iCountPipe;
	bool bFind = false;
	bool bRet = false;
	bool bIsoch = false;

    DebugPrint (DebugTrace, "GetPipePing");

    m_spinInterface.Lock ();
    for (iCount = 0; iCount < m_listInterface.GetCount (); pInterface = NULL, iCount++)
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem (iCount);
        for (iCountPipe = 0; iCountPipe < pInterface->NumberOfPipes; iCountPipe++)
        {
            if (pInterface->Pipes [iCountPipe].PipeHandle == Handle)
            {
				bFind = true;
                break;
            }
        }
		if (bFind)
		{
			break;
		}
    }

	if (bFind && (pInterface->Class == USB_DEVICE_CLASS_HUMAN_INTERFACE 
			||	  pInterface->Class == 0x0b)
		)
	{
		m_spinInterface.UnLock ();
		bRet = true;
		return bRet;
	}

	if (bFind)
	{
		switch (pInterface->Pipes [iCountPipe].PipeType)
		{
		case UsbdPipeTypeInterrupt:
			if (pNumber->pPdo->m_bCardReader)
			{
				bRet = false;
				pNumber->CountWait = 3;
				pNumber->bManualPending = true;
			}
			else
			{
				bRet = true;
			}
			break;
		case UsbdPipeTypeBulk:
			if (pInterface->Class == USB_DEVICE_CLASS_PRINTER ||
				pInterface->Class == USB_DEVICE_CLASS_STORAGE)
			{
				m_spinInterface.UnLock ();
				return bRet;
			}

			// if usb type 1.1: in - true, out - false // Fritz!Card usb
			if (m_bcdUSB < 0x200)
			{
				m_spinInterface.UnLock ();

				bRet = false;
				pNumber->CountWait = 3;
				pNumber->bManualPending = true;

				return bRet;
			}
			if (pInterface->Pipes [iCountPipe].EndpointAddress & 0x80) // if in
			{
				bRet = true;
				pNumber->CountWait = 0;
				pNumber->bManualPending = true;
			}
			else
			{
				pNumber->bManualPending = true; // if false maybe recurse (for wifi corega )
			}
			for (iCount = 0; iCount < 2; iCount++)
			{
				if ((int (iCountPipe) - 1 + iCount * 2) < 0)
				{
					continue;
				}
				if ((int (iCountPipe) - 1 + iCount * 2) > int(pInterface->NumberOfPipes - 1))
				{
					continue;
				}

				if ( (pInterface->Pipes [iCountPipe].EndpointAddress & 0x7F) == 
					 (pInterface->Pipes [iCountPipe - 1 + iCount * 2].EndpointAddress & 0x7F) )
				{
					pNumber->CountWait = 10; // if 0 to freez Microchip
					bRet = (pInterface->Pipes [iCountPipe].EndpointAddress & 0x80)?true:false;
					break;
				}
			}
			if (pNumber->CountWait && bIsoch)
			{
				pNumber->CountWait = 0;
				bRet = true;
			}
			break;
		}
	}
	m_spinInterface.UnLock ();


    return bRet;
}

void evuh::CUsbDevToTcp::DetectUsbVersion (PURB_FULL urb)
{
	if(urb->UrbHeader.Function == URB_FUNCTION_CONTROL_TRANSFER)
	{
		PUSB_COMMON_DESCRIPTOR pDesc = (PUSB_COMMON_DESCRIPTOR)urb->UrbControlTransfer.TransferBuffer;
		if (!pDesc)
		{
			return;
		}

		if (pDesc->bDescriptorType == USB_DEVICE_DESCRIPTOR_TYPE)
		{
			PUSB_DEVICE_DESCRIPTOR pDevDesc = (PUSB_DEVICE_DESCRIPTOR)pDesc;
			if (pDevDesc->bcdUSB)
			{
				m_bcdUSB = pDevDesc->bcdUSB;
			}
		}
	}
}
bool evuh::CUsbDevToTcp::DeleteIrpNumber (PIRP_NUMBER	pNumber)
{
	LONG lDel = InterlockedIncrement (&pNumber->Lock);
    if (lDel == 2)
	{
		if (pNumber->SaveIrp)
		{
			ExFreePool (pNumber->SaveIrp);
			pNumber->SaveIrp = NULL;
		}
		m_AllocNumber.Free (pNumber);
		m_NumberCount--;

		return true;
	}

	return false;
}

ULONG evuh::CUsbDevToTcp::SetNumber ()
{
	return InterlockedIncrement (&m_lCount);
}

PIRP_NUMBER evuh::CUsbDevToTcp::NewIrpNumber (PIRP pIrp, CPdoExtension *pPdo)
{
    PIRP_NUMBER irpNumber = (PIRP_NUMBER)m_AllocNumber.Alloc ();
	if (irpNumber)
	{
		m_NumberCount++;

		RtlZeroMemory(irpNumber, sizeof(IRP_NUMBER));

		irpNumber->Count = SetNumber ();

		// fill base param
	    irpNumber->Irp = pIrp;
		irpNumber->pTcp = pPdo->m_pTcp;
	    irpNumber->pPdo = pPdo;
        irpNumber->Cancel = false;
        irpNumber->bWait = true;
	    KeInitializeEvent( &irpNumber->Event, NotificationEvent, FALSE );
	}
	else
	{
		DebugPrint (DebugError, "evuh::CUsbDevToTcp::NewIrpNumber didn't allocat memory");
	}
	
	return irpNumber;
}

// ***********************************************
//				CUsbDevToTcp
// ***********************************************
CHAR* IOCTL_USB(DWORD dw)
{
	switch (dw)
	{
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
		return "IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX";
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		return "IOCTL_USB_GET_NODE_CONNECTION_NAME";
	}
	return "";
}

void evuh::CUsbDevToTcp::OutUsbIoBufferSize (PIRP Irp, BYTE **pBuffer, LONG* lSize, PIRP_NUMBER stDevice)
{
	PIO_STACK_LOCATION irpStack;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	DebugPrint (DebugTrace, "UsbBufferSize irp - 0x%x", Irp);
	DebugPrint(DebugTrace, "OutUsbIoBufferSize %s irp - 0x%x", IOCTL_USB(irpStack->Parameters.DeviceIoControl.IoControlCode), Irp);


	switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		{
			*pBuffer = (BYTE*)Irp->AssociatedIrp.SystemBuffer;
			*lSize = irpStack->Parameters.DeviceIoControl.InputBufferLength;
		}
		break;
	}
}

void evuh::CUsbDevToTcp::InUsbIoFillNewData (PIRP Irp, PIRP_SAVE irpSave)
{
	PIO_STACK_LOCATION irpStack;

	DebugPrint (DebugTrace, "UsbFillNewData irp - 0x%x", Irp);

	irpStack = IoGetCurrentIrpStackLocation (Irp);

	switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
	{

	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		if (irpSave->BufferSize)
		{
			RtlCopyMemory (Irp->AssociatedIrp.SystemBuffer, irpSave->Buffer, min ((ULONG)irpSave->BufferSize, irpStack->Parameters.DeviceIoControl.OutputBufferLength));
		}
		break;
	}
}

bool evuh::CUsbDevToTcp::VerifyRaiseIrql (PIRP	pIrp, KIRQL *OldRestore)
{
	PIO_STACK_LOCATION  stack;
    PISOCH_SAVE     pIsoch;
    bool            bCreateNew = true;
	KIRQL			Old;

    DebugPrint (DebugTrace, "VerifyRaiseIrql");

	Old = KeGetCurrentIrql ();

	if (GetOsVersion () >= 600)
	{
		return false;
	}

	if (Old != DISPATCH_LEVEL)
	{
		return false;
	}

	stack = IoGetCurrentIrpStackLocation (pIrp);
	if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
		stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		PURB urb = (PURB)stack->Parameters.Others.Argument1;

        switch (urb->UrbHeader.Function)
        {
			case URB_FUNCTION_VENDOR_DEVICE:
			case URB_FUNCTION_VENDOR_INTERFACE:
			case URB_FUNCTION_VENDOR_ENDPOINT:
			case URB_FUNCTION_VENDOR_OTHER:
			case URB_FUNCTION_CLASS_DEVICE:
			case URB_FUNCTION_CLASS_INTERFACE:
			case URB_FUNCTION_CLASS_ENDPOINT:
			case URB_FUNCTION_CLASS_OTHER :
				if (USBD_TRANSFER_DIRECTION_IN == USBD_TRANSFER_DIRECTION(urb->UrbControlVendorClassRequest.TransferFlags))
				{
					if (urb->UrbControlVendorClassRequest.TransferFlags & USBD_SHORT_TRANSFER_OK)
					{
						KeLowerIrql (PASSIVE_LEVEL);
						*OldRestore = Old;
						return true;
					}
				}
			default:
				break;
        }
    }
    return false;
}

bool evuh::CUsbDevToTcp::VerifyAutoComplete (PIRP_NUMBER	pNumber, bool bCompleted)
{
	PIO_STACK_LOCATION  stack;
    PISOCH_SAVE     pIsoch;
    bool            bCreateNew = true;
    KIRQL           OldIrql;
	bool			bRet = false;

    DebugPrint (DebugTrace, "VerifyAutoComplete");

	if (pNumber->pPdo->m_bDisableAutoComplete)
	{
		return bRet;
	}

	stack = IoGetCurrentIrpStackLocation (pNumber->Irp);
	if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
		stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		PURB urb = (PURB)stack->Parameters.Others.Argument1;

        switch (urb->UrbHeader.Function)
        {
			case URB_FUNCTION_VENDOR_DEVICE:
			case URB_FUNCTION_VENDOR_INTERFACE:
			case URB_FUNCTION_VENDOR_ENDPOINT:
			case URB_FUNCTION_VENDOR_OTHER:
				if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbControlVendorClassRequest.TransferFlags))
				{
					if (bCompleted)
					{
						urb->UrbHeader.Status = STATUS_SUCCESS;
						pNumber->Irp->IoStatus.Status = STATUS_SUCCESS;
						pNumber->bNoAnswer = true;
						pNumber->bWait = true;
						//SaveIrp->NoAnswer = true;
					}

					{
						KIRQL Old;
						Old = KeGetCurrentIrql ();
						if (Old == DISPATCH_LEVEL)
						{
							DebugPrint(DebugWarning, "VerifyAutoComplete %s i-%d f-%d l-%d d-%d a-%d", UrbFunctionString(urb->UrbHeader.Function), pNumber->Count, urb->UrbControlVendorClassRequest.TransferFlags, urb->UrbControlVendorClassRequest.TransferBufferLength, Old, USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbControlVendorClassRequest.TransferFlags));
						}
					}

					bRet = true;
					break;
				}
				break;
			case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
				{
					KIRQL Old;
					Old = KeGetCurrentIrql ();

					if (USB_ENDPOINT_DIRECTION_OUT (GetPipeEndpoint (urb->UrbBulkOrInterruptTransfer.PipeHandle, true)))
					{
						if (m_bAutoCompleteBulkOut)
						{
							bRet = true;
							if (bCompleted)
							{
								urb->UrbHeader.Status = STATUS_SUCCESS;
								pNumber->Irp->IoStatus.Status = STATUS_SUCCESS;
								pNumber->bNoAnswer = true;
								pNumber->bWait = true;
								DebugPrint (DebugWarning, "VerifyAutoComplete %s i-%d f-%d l-%d d-%d a-%d", UrbFunctionString (urb->UrbHeader.Function), pNumber->Count, urb->UrbBulkOrInterruptTransfer.TransferFlags, urb->UrbBulkOrInterruptTransfer.TransferBufferLength, Old, 0);
							}
						}
					}

					break;
				}
			case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
				{
					KIRQL Old = KeGetCurrentIrql();

					if (Old == DISPATCH_LEVEL)
					{
						if (bCompleted)
						{
							urb->UrbHeader.Status = STATUS_SUCCESS;
							urb->UrbGetCurrentFrameNumber.FrameNumber = m_Isoch.GetCurrentFrame();//m_lCurrentFrame;
							pNumber->Irp->IoStatus.Status = STATUS_SUCCESS;
							pNumber->bNoAnswer = true;
							pNumber->bWait = true;
							DebugPrint(DebugWarning, "VerifyAutoComplete %s i-%d f-%d l-%d d-%d a-%d", UrbFunctionString(urb->UrbHeader.Function), pNumber->Count, urb->UrbBulkOrInterruptTransfer.TransferFlags, urb->UrbBulkOrInterruptTransfer.TransferBufferLength, Old, 0);
						}
						bRet = true;
					}
				}
				break;
			default:
				break;
        }
    }

	if (bRet == true)
	{
		FillIrpSave (pNumber);
	}

    return bRet;
}

bool evuh::CUsbDevToTcp::FillIrpSave (PIRP_NUMBER	pNumber)
{
	PIO_STACK_LOCATION  stack;
	bool bRet = true;

    DebugPrint (DebugTrace, "FillIrpSave");

	stack = IoGetCurrentIrpStackLocation (pNumber->Irp);
	if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
		stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		PURB urb = (PURB)stack->Parameters.Others.Argument1;

		{
			BYTE		*pBuf;
			LONG		lSize;
			PIRP_SAVE	SaveIrp;

			// calc szie
			OutUrbBufferSize (pNumber->Irp, &pBuf, &lSize, pNumber);
			// alloc mem
			SaveIrp = (PIRP_SAVE)ExAllocatePoolWithTag (NonPagedPool, sizeof (IRP_SAVE) + lSize, 'SIF');

			if (SaveIrp)
			{
				RtlZeroMemory (SaveIrp, sizeof (IRP_SAVE) + lSize);
				// fill irp save
				SaveIrp->NeedSize = sizeof (IRP_SAVE) + lSize;
				SaveIrp->Is64 = m_bIs64;
				SaveIrp->Device = pNumber->pPdo->m_DeviceIndef?0:1;
				SaveIrp->Irp = pNumber->Count;			// UniqueName
				SaveIrp->NoAnswer = false;
				SaveIrp->Cancel = false;
				SaveIrp->Dispath = false;

				SaveIrp->NoAnswer = true;

				switch (urb->UrbHeader.Function)
				{
					case URB_FUNCTION_VENDOR_DEVICE:
					case URB_FUNCTION_VENDOR_INTERFACE:
					case URB_FUNCTION_VENDOR_ENDPOINT:
					case URB_FUNCTION_VENDOR_OTHER:
					case URB_FUNCTION_CLASS_DEVICE:
					case URB_FUNCTION_CLASS_INTERFACE:
					case URB_FUNCTION_CLASS_ENDPOINT:
					case URB_FUNCTION_CLASS_OTHER :
						if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbControlVendorClassRequest.TransferFlags))
						{
							SaveIrp->NoAnswer = true;
						}
						break;
				case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
					{
						if (USB_ENDPOINT_DIRECTION_OUT (GetPipeEndpoint (urb->UrbBulkOrInterruptTransfer.PipeHandle, true)))
						{
							SaveIrp->NoAnswer = true;
						}
					}
				}


				SaveIrp->Status = pNumber->Irp->IoStatus.Status;
				SaveIrp->Information = (ULONG)pNumber->Irp->IoStatus.Information;
				SaveIrp->StackLocation.MajorFunction = stack->MajorFunction;
				SaveIrp->StackLocation.MinorFunction = stack->MinorFunction;
				SaveIrp->StackLocation.Parameters.Others.Argument1 = (ULONG64)stack->Parameters.Others.Argument1;
				SaveIrp->StackLocation.Parameters.Others.Argument2 = (ULONG64)stack->Parameters.Others.Argument2;
				SaveIrp->StackLocation.Parameters.Others.Argument3 = (ULONG64)stack->Parameters.Others.Argument3;
				SaveIrp->StackLocation.Parameters.Others.Argument4 = (ULONG64)stack->Parameters.Others.Argument4;

				SaveIrp->BufferSize = lSize;

				// copy data
				OutUrbFillBuffer (pNumber->Irp, pBuf, SaveIrp->Buffer, lSize);
			}

			if (SaveIrp->NoAnswer)
			{
				// Complete irp status
				urb->UrbHeader.Status = STATUS_SUCCESS;
				pNumber->Irp->IoStatus.Status = STATUS_SUCCESS;
			}
			else
			{
				IoMarkIrpPending (pNumber->Irp);
				pNumber->Irp->IoStatus.Status = STATUS_PENDING;
				bRet = false;
			}
			pNumber->SaveIrp = SaveIrp;
		}
    }
	// pending
    return bRet;
}

bool evuh::CUsbDevToTcp::IdleNotification (PIRP_SAVE pSave)
{
	if (pSave->Irp == -1 && pSave->StackLocation.MajorFunction == 0xFF)
	{
		PFUNC_INTERFACE pInterface = (PFUNC_INTERFACE)pSave->Buffer;
		if (pInterface->lNumFunc == FI_USB_SUBMIT_IDLE_NOTIFICATION)
		{
			if (m_idleInfo.IdleCallback)
			{
				m_idleInfo.IdleCallback (m_idleInfo.IdleContext);
			}
			return true;
		}
	}

	return false;
}

// *****************************************************************************
//					new work with service
// -----------------------------------------------------------------------------

VOID evuh::CUsbDevToTcp::stItemWriteToDriver (/*PDEVICE_OBJECT DeviceObject, */PVOID Context)
{
	CUsbDevToTcp *pBase = (CUsbDevToTcp*)Context;
	pBase->ItemWriteToDriver ();
}

void evuh::CUsbDevToTcp::ItemWriteToDriver ()
{
	char *pBuff;
	char *pNew;

	PRKTHREAD pThread = KeGetCurrentThread();
	PIRP_SAVE	pIrpSave;

	// wait ready service to work
	m_poolWriteToDriver.WaitReady();

	while (m_bWork)
	{
		pBuff = m_poolWriteToDriver.GetCurNodeRead();
		if (!m_bWork)
		{
			m_poolWriteToDriver.PopCurNodeRead();
			break;
		}
		if (pBuff)
		{
			pIrpSave = (PIRP_SAVE)pBuff;
			InCompleteIrpBuffer(pBuff);
			m_poolWriteToDriver.PopCurNodeRead();
		}
	}
	--m_WaitUse;
}

VOID evuh::CUsbDevToTcp::stItemWriteToService (/*PDEVICE_OBJECT DeviceObject, */PVOID Context)
{
	CUsbDevToTcp *pBase = (CUsbDevToTcp*)Context;
	pBase->ItemWriteToService ();
}

void evuh::CUsbDevToTcp::ItemWriteToService ()
{
	DebugPrint (DebugTrace, "ItemWriteToService");

	m_poolReadFromService.WaitReady();

	while (m_bWork)
	{
		m_spinUsbOut.Lock ();
		if (m_listIrpDevice.GetCount () == 0)
		{
			m_spinUsbOut.UnLock ();
			m_listIrpDevice.WaitAddElemetn ();
			continue;
		}
		m_spinUsbOut.UnLock ();


		if (!OutFillGetIrps2())
		{
			break;
		}

	}
	--m_WaitUse;
}

bool evuh::CUsbDevToTcp::OutFillGetIrps2 ()
{
	PIRP_SAVE			pIrpSave;
	LONG				lBufferSize = 0;		// Need data size
	BYTE				*pBuffer;
	PIRP_NUMBER			stDevice = NULL;

	DebugPrint (DebugTrace, "OutFillGetIrps irpControl");
	// verify queue is not empty
	m_spinUsbOut.Lock ();
	if (m_listIrpDevice.GetCount () == 0)
	{
		m_spinUsbOut.UnLock ();
		return false;
	}
	m_spinUsbOut.UnLock ();

	{

		DebugPrint(DebugSpecial, "OutFillGetIrps2");

		// get buffer
		pBuffer = (LPBYTE)m_poolReadFromService.GetCurNodeWrite();
		lBufferSize = m_poolReadFromService.GetBlockSize();

		pIrpSave = (PIRP_SAVE)pBuffer;
		if (!pIrpSave)
		{
			return false;
		}

		pIrpSave->Size = lBufferSize;

		if (!pBuffer || !m_bWork)
		{
			return false;
		}

		// get device irp
		stDevice = NULL;
		m_spinUsbOut.Lock ();
		if (m_listIrpDevice.GetCount () > 0)
		{
			stDevice = (PIRP_NUMBER)m_listIrpDevice.PopFirst();
		}
		m_spinUsbOut.UnLock ();

		// end queue
		if (stDevice == NULL)
		{
			return false;
		}

		if (!OutFillGetIrp (pIrpSave, stDevice))
		{
			pIrpSave->NeedSize = -1;
			ASSERT(false);
		}

		m_poolReadFromService.PushCurNodeWrite();

		DebugPrint (DebugSpecial, "OutFillGetIrps2 %d", stDevice->iCount);

		if (stDevice->bNoAnswer || stDevice->Cancel)
		{
			DeleteIrpNumber(stDevice);
		}
	};
	return true;
}

NTSTATUS evuh::CUsbDevToTcp::InCompleteIrpBuffer (char *pBuffer)
{
	PIRP_SAVE			pIrpSave;
	PIRP_NUMBER			irpNumber = NULL;
	LONG				iSize;
	ULONG				uIrpNumber;


	DebugPrint (DebugTrace, "InCompleteIrpBuffer irpControl");

	pIrpSave = (PIRP_SAVE)pBuffer;
	uIrpNumber = pIrpSave->Irp;

	{
		pIrpSave = (PIRP_SAVE)pBuffer;

		m_spinUsbIn.Lock ();
		if (uIrpNumber == -1)
		{
			m_spinUsbIn.UnLock();
			if (!IdleNotification (pIrpSave))
			{
				m_Isoch.SetTimerPeriod((int)pIrpSave->Information);
			}
			m_spinUsbIn.Lock();
		}
		else
		{
			for (int a = 0; a < m_listIrpAnswer.GetCount (); a++)
			{
				irpNumber = (PIRP_NUMBER)m_listIrpAnswer.GetItem (a);
				if (!irpNumber)
				{
					DebugPrint (DebugError, "InCompleteIrpBuffer m_listIrpAnswer.GetItem (%d) return NULL", a);
				}
				else if (irpNumber->Count == uIrpNumber)
				{
					m_listIrpAnswer.Remove (a);
					break;
				}
				irpNumber = NULL;
			}
		}
		if (irpNumber == NULL)
		{
			m_spinUsbIn.UnLock();
			return STATUS_UNSUCCESSFUL;
		}
		m_spinUsbIn.UnLock();

		DebugPrint (DebugSpecial, "InCompleteIrp - %d", irpNumber->Count);

		InFillNewData (irpNumber, pIrpSave);
	}

	return STATUS_SUCCESS;
}