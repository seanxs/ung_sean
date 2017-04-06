/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbPortToTcp.cpp

Abstract:

	Layer class between real and virtual USB devices (CPdoExtension)

Environment:

    Kernel mode

--*/

#include "BusFilterExtension.h"
#include "UsbPortToTcp.h"
#include "ControlDeviceExtension.h"
#include "DebugPrintUrb.h"
#include "Core\WorkItem.h"
#include "urbDiff.h"
#include "core\Thread.h"

const int iMaxIsoch = 4;

#if (_WIN32_WINNT < 0x0600)
#define IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_TOPOLOGY_ADDRESS, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION   \
                                            CTL_CODE(FILE_DEVICE_USB,  \
                                            USB_IDLE_NOTIFICATION,  \
                                            METHOD_NEITHER,  \
                                            FILE_ANY_ACCESS)
#endif

CUsbPortToTcp::CUsbPortToTcp (PUSB_DEV_SHARE pUsbDev)
    : CBaseClass ()
	, m_AllocListWorkSave (NonPagedPool, sizeof (WORKING_IRP))
	, m_Isoch ()
    , m_bIsPresent (false)
    , m_irpWait (NULL)
    , m_lTag ('UPTT')
    , m_bCancel (false)
	, m_bNeedDeviceRelations (false)
	, m_bDelayedDelete (false)
	, m_bAdditionDel (false)
	, m_bWorkIrp (false)
	, m_pDel (NULL)
	, m_bWork (true)
	, m_pPdoStub (NULL)
{
    HANDLE              threadHandle;
    NTSTATUS            status;

	m_Isoch.SetBase (this);

    SetDebugName ("fusbhub");
    Init (pUsbDev);

#ifdef _WIN64
    DebugPrint (DebugInfo, "Enable x64");
    m_bIs64 = true;
#else
    DebugPrint (DebugInfo, "Enable x86");
    m_bIs64 = false;
#endif

	Thread::Create(CUsbPortToTcp::stItemWriteToSrv, this);
	Thread::Create(CUsbPortToTcp::stItemReadFromSrv, this);
}

CUsbPortToTcp::~CUsbPortToTcp ()
{
	m_bWork = false;
	m_poolRead.Clear();
	m_poolWrite.Clear ();
	m_BufferMain.ClearBuffer ();
	m_BufferSecond.ClearBuffer ();
}

// ***********************************************************************
//                        Init
// ***********************************************************************
void CUsbPortToTcp::Init (PUSB_DEV_SHARE usbDev)
{
    DebugPrint (DebugTrace, "InitPdo");

	m_uDeviceAddress = usbDev->DeviceAddress;

	if (usbDev->DeviceAddress)
	{
		// share usb port
		RtlStringCbCopyW(m_szHubDriverKeyName, sizeof(m_szHubDriverKeyName), usbDev->HubDriverKeyName);
		RtlZeroMemory (m_szDevIndef, sizeof (m_szDevIndef));
	}
	else
	{
		// share usb device
		RtlZeroMemory (m_szHubDriverKeyName, sizeof (m_szHubDriverKeyName));
		RtlStringCbCopyW(m_szDevIndef, sizeof(m_szDevIndef), usbDev->HubDriverKeyName);
	}
    DebugPrint (DebugInfo, "HubDriverKeyName - %S", m_szHubDriverKeyName);
	DebugPrint (DebugInfo, "DevIndef - %S", m_szDevIndef);
    DebugPrint (DebugInfo, "DeviceAddress - %d", m_uDeviceAddress);
}

NTSTATUS CUsbPortToTcp::StartTcpServer (CPdoStubExtension *pPdoStub)
{
    DebugPrint (DebugTrace, "StartTcpServer");

	if (!m_bIsPresent)
	{
		m_bIsPresent = true;
		m_irpCounter.Reset ();
	}

    return STATUS_SUCCESS;
}

void CUsbPortToTcp::CompleteWaitIrp (PIRP Irp, bool bOk)
{
    DebugPrint (DebugTrace, "CompleteWaitIrp");

	IoSetCancelRoutine (Irp, NULL);
	m_bCancelControl = false;
    Irp->IoStatus.Status = bOk?STATUS_SUCCESS:STATUS_BUFFER_OVERFLOW;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--CControlDeviceExtension::GetControlExtension ()->m_IrpCounter;
}

NTSTATUS CUsbPortToTcp::StopTcpServer (CPdoStubExtension *pPdoStub)
{
    CPdoStubExtension   *pPdo;
    PISOCH              pIsoch;
    KIRQL               OldIrql;

    DebugPrint (DebugTrace, "StopTcpServer");

	m_spinBase.Lock ('STS');
    if (m_irpWait && !m_bAdditionDel)
    {
	    PIRP	irpWait = m_irpWait;
		m_irpWait = NULL;
		IoSetCancelRoutine (irpWait, NULL);
	    irpWait->IoStatus.Status = STATUS_UNSUCCESSFUL;
	    IoCompleteRequest (irpWait, IO_NO_INCREMENT);
	    --CControlDeviceExtension::GetControlExtension ()->m_IrpCounter;
    }
	m_bIsPresent = false;
	m_spinBase.UnLock ();

	if (m_pPdoStub)
    {
		#ifdef ISOCH_FAST_MODE
	        OldIrql = m_listIsoch.Lock ();
			while (m_listIsoch.GetCount ())
			{
				pIsoch = (PISOCH)m_listIsoch.GetItem (0, false);
				IsochClear (pIsoch);
			}
			m_listIsoch.UnLock (OldIrql);
		#endif

		if (m_listWaitCompleted.GetCount ())
		{
			LARGE_INTEGER time;
			time.QuadPart = (LONGLONG)-10000 * 1000; // Specify a timeout of 5 seconds to wait for this call to complete.
			KeDelayExecutionThread (KernelMode, FALSE, &time);
		}

        if (pPdoStub)
        {
			// clear usb interfaces
			pPdoStub->ClearInterfaceInfo ();
			pPdoStub->ResetUsbConfig ();

			m_spinBase.Lock ('STS2');
			pPdoStub->m_pParent = NULL;
			m_spinBase.UnLock ();
        }

		CancelWait ();
	}

	if (m_bDelayedDelete)
	{
		Delete ();
	}
    
    return STATUS_SUCCESS;
}

NTSTATUS CUsbPortToTcp::StopTcpServer (bool bDeletePdo)
{
    PISOCH              pIsoch;

    DebugPrint (DebugTrace, "StopTcpServer");

	m_spinBase.Lock ('STS3');
    if (m_irpWait)
    {
	    PIRP	irpWait = m_irpWait;
		m_irpWait = NULL;
		IoSetCancelRoutine (irpWait, NULL);
	    irpWait->IoStatus.Status = STATUS_UNSUCCESSFUL;
	    IoCompleteRequest (irpWait, IO_NO_INCREMENT);
	    --CControlDeviceExtension::GetControlExtension ()->m_IrpCounter;
    }
	m_bIsPresent = false;
	m_spinBase.UnLock ();

	if (m_pPdoStub)
    {
		#ifdef ISOCH_FAST_MODE
	        OldIrql = m_listIsoch.Lock ();
			while (m_listIsoch.GetCount ())
			{
				pIsoch = (PISOCH)m_listIsoch.GetItem (0, false);
				IsochClear (pIsoch);
			}
			m_listIsoch.UnLock (OldIrql);
		#endif

		if (m_listWaitCompleted.GetCount ())
		{
			LARGE_INTEGER time;
			time.QuadPart = (LONGLONG)-10000 * 1000; // Specify a timeout of 5 seconds to wait for this call to complete.
			KeDelayExecutionThread (KernelMode, FALSE, &time);
		}

        if (bDeletePdo)
        {
            {
				// clear usb interfaces
				m_pPdoStub->ClearInterfaceInfo ();
				m_pPdoStub->ResetUsbConfig ();

				m_spinBase.Lock('STS4');
                m_pPdoStub->m_pParent = NULL;
				m_pPdoStub = NULL;
				m_spinBase.UnLock ();

            }
        }

		CancelWait ();
	}
    
	if (m_bDelayedDelete)
	{
		Delete ();
	}

    return STATUS_SUCCESS;
}

void CUsbPortToTcp::CancelWait ()
{
    CancelControlIrp ();
    CancelWairIrp ();
	if (!m_irpCounter.WaitStop(5000))
	{
		ASSERT(false);
		m_irpCounter.WaitStop();
	}
    m_irpCounter.Reset ();
}


void CUsbPortToTcp::CancelWairIrp ()
{
    PWORKING_IRP pWorkSave;
    // Cancel Control IRP
    while (m_listWaitCompleted.GetCount ())
    {
        m_spinUsbIn.Lock ();
        pWorkSave = (PWORKING_IRP)m_listWaitCompleted.PopFirst();
        m_spinUsbIn.UnLock ();

		if (pWorkSave)
		{
			// cancel
			if (InterlockedExchange(&pWorkSave->lLock, IRPLOCK_CANCEL_STARTED) == IRPLOCK_CANCELABLE)
			{
				IoCancelIrp(pWorkSave->pIrp);
				if (InterlockedExchange(&pWorkSave->lLock, IRPLOCK_CANCEL_COMPLETE) == IRPLOCK_COMPLETED) 
				{
					// delete irp
					DeleteIrp (pWorkSave);
				}
			}
		}
    }
}

void CUsbPortToTcp::DeleteIrp (PWORKING_IRP pWorkSave)
{
    --m_irpCounter;

	if (pWorkSave->pAllocBuffer)
	{
		m_BufferMain.FreeBuffer(pWorkSave->pAllocBuffer);
		pWorkSave->pAllocBuffer = NULL;
	}
	if (pWorkSave->pSecondBuffer)
	{
		m_BufferSecond.FreeBuffer(pWorkSave->pSecondBuffer);
		pWorkSave->pSecondBuffer = NULL;
	}
    IoFreeIrp (pWorkSave->pIrp);
    m_AllocListWorkSave.Free (pWorkSave);
    pWorkSave = NULL;
}

VOID CUsbPortToTcp::CancelControlIrp ()
{
    PIRP                    pIrp;
    KIRQL                   irql;
    PIRP                    pnpIrp;
    PIO_STACK_LOCATION      irpStack;
    IO_STATUS_BLOCK         ioStatus;
    KEVENT                  pnpEvent;
    NTSTATUS                status;

    DebugPrint (DebugTrace, "CancelControlIrp");

    // Cancel Control IRP
    m_spinBase.Lock ('CCI');
    if (m_listIrpControlAnswer.GetCount () == 0 && m_listIrpAnswer.GetCount () == 0)
    {
        m_bCancelControl = true;
        m_spinBase.UnLock ();
    }
    else
    {
        m_spinBase.UnLock ();
        while (m_listIrpControlAnswer.GetCount ())
        {
            
            m_spinBase.Lock ('CCI2');
            pIrp = (PIRP)m_listIrpControlAnswer.PopFirst();
            m_spinBase.UnLock ();
            // cancel
            IoAcquireCancelSpinLock( &irql );
            pIrp->Cancel = TRUE;
            pIrp->CancelIrql = irql;
            pIrp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoReleaseCancelSpinLock( irql );

			IoSetCancelRoutine (pIrp, NULL);
            IoCompleteRequest (pIrp, IO_NO_INCREMENT);
            --CControlDeviceExtension::GetControlExtension ()->m_IrpCounter;
        }
    }
    // work
    PWORKING_IRP    pWorkSave;
    while (m_listIrpAnswer.GetCount ())
    {
        m_spinUsbOut.Lock ();
        pWorkSave = (PWORKING_IRP)m_listIrpAnswer.PopFirst();
        m_spinUsbOut.UnLock ();

        // cancel
		if (pWorkSave)
		{
			// del irp
			DeleteIrp (pWorkSave);
		}
    }
	// send message service 
	m_spinUsbOut.Lock();
	m_listIrpAnswer.Add(NULL);
	m_spinUsbOut.UnLock();
}

// ***********************************************************************
//                    work with new irp
// ***********************************************************************
NTSTATUS CUsbPortToTcp::GetIrpToAnswer (PIRP Irp)
{
    PWORKING_IRP        irpWait = NULL;
    DebugPrint (DebugTrace, "GetIrpToAnswer - 0x%x", Irp);

	IoMarkIrpPending (Irp);
	IoSetCancelRoutine (Irp, CControlDeviceExtension::CancelIrpToAnswer);
	if (GetIrpToAnswerThread (Irp))
	{
	    return STATUS_SUCCESS;
	}
	
    return STATUS_PENDING;
}

PWORKING_IRP CUsbPortToTcp::InitWorkIrp (PIRP_SAVE irpSave, CPdoStubExtension *pPdoStub)
{
    PIRP                    newIrp;
    PIO_STACK_LOCATION      irpStack;
    PWORKING_IRP            saveData = NULL;

    DebugPrint (DebugTrace, "InitWorkIrp");

	if (!m_pPdoStub || !m_bIsPresent)
	{
		return NULL;
	}
    // allocate mem from context /1
    saveData = (PWORKING_IRP)m_AllocListWorkSave.Alloc ();
    // Build IRP
    newIrp = IoBuildAsynchronousFsdRequest(
        IRP_MJ_PNP,
        pPdoStub->m_NextLowerDriver,
        NULL,
        0,
        NULL,
        &saveData->ioStatus
        );

    if (newIrp == NULL) 
    {
        m_AllocListWorkSave.Free (saveData);
        return NULL;
    }

    irpStack = IoGetNextIrpStackLocation( newIrp );
    RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );

    // base param filling in
    newIrp->IoStatus.Status = irpSave->Status;
    newIrp->IoStatus.Information = (ULONG_PTR)irpSave->Information;
    irpStack->MajorFunction = irpSave->StackLocation.MajorFunction;
    irpStack->MinorFunction = irpSave->StackLocation.MinorFunction;
    irpStack->Parameters.Others.Argument1 = (PVOID)irpSave->StackLocation.Parameters.Others.Argument1;
    irpStack->Parameters.Others.Argument2 = (PVOID)irpSave->StackLocation.Parameters.Others.Argument2;
    irpStack->Parameters.Others.Argument3 = (PVOID)irpSave->StackLocation.Parameters.Others.Argument3;
    irpStack->Parameters.Others.Argument4 = (PVOID)irpSave->StackLocation.Parameters.Others.Argument4;

    // save info filling in
    RtlZeroMemory(saveData, sizeof (WORKING_IRP));
    saveData->pIrpIndef = (LONG_PTR)irpSave->Irp;
    saveData->MajorFunction = irpSave->StackLocation.MajorFunction;
    saveData->MinorFunction = irpSave->StackLocation.MinorFunction;
    saveData->IoControlCode = ULONG ((ULONG_PTR)irpSave->StackLocation.Parameters.Others.Argument3);
    saveData->pBase = this;
    saveData->pIrp = newIrp;
    saveData->lLock = IRPLOCK_CANCELABLE;
    saveData->pPdo = pPdoStub;
    saveData->Is64 = irpSave->Is64;
    saveData->Pending = 0;
	saveData->NoAnswer = irpSave->NoAnswer;

    // Filling other data in IRP
    switch (irpSave->StackLocation.MajorFunction)
    {
    case IRP_MJ_POWER:
        break;
    case IRP_MJ_PNP:
        InPnpFillIrp (newIrp, irpSave, saveData);
		if (newIrp->IoStatus.Status != STATUS_NOT_SUPPORTED)
		{
			newIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		}
        break;
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        InUrbFillIrp (irpSave, saveData);
        break;
	case IRP_MJ_DEVICE_CONTROL:
		InUsbIoFillIrp (irpSave, saveData);
		break;
    case 0xFF:
        ++m_irpCounter;
        InInterfaceFillIrp (irpSave, saveData);
        return STATUS_SUCCESS;
    }
    // set callback result
    IoSetCompletionRoutine(
        newIrp,
        IoCompletionIrp,
        saveData,
        TRUE,
        TRUE,
        TRUE
        );

    return saveData;
}

NTSTATUS CUsbPortToTcp::InPutIrpToDevice (PBYTE Buffer, LONG lSize)
{
    NTSTATUS                status;
	CPdoStubExtension       *pPdoStub = nullptr;
    PWORKING_IRP            saveData;
	PIRP_SAVE				irpSave;

    DebugPrint (DebugTrace, "PutIrpToDevice");

	{
		irpSave = (PIRP_SAVE)Buffer;

		m_spinBase.Lock ('PUD');
		if (m_pPdoStub)
		{
			pPdoStub = m_pPdoStub;
			if (irpSave->Device == 1) // if request to hub
			{
				CBusFilterExtension *pBus = (CBusFilterExtension *)pPdoStub->m_pBus;
				if (pBus)
				{
					pPdoStub = &pBus->m_PdoHub;
				}
				else
				{
					pPdoStub = NULL;
				}			
			}
		}
		else
		{
			DebugPrint (DebugError, "List is empty");
		}

		m_spinBase.UnLock ();

		if (!pPdoStub)
		{
			DebugPrint (DebugWarning, "PDO not found");
			return STATUS_SUCCESS;
		}

		if (irpSave->Cancel)
		{
			m_spinUsbIn.Lock ();
			for (int a = 0; a < m_listWaitCompleted.GetCount (); a++)
			{
				saveData = (PWORKING_IRP)m_listWaitCompleted.GetItem (a);
				if (saveData)
				{
					if (saveData->pIrpIndef == irpSave->Irp)
					{
						saveData->Cancel = 1;
						m_spinUsbIn.UnLock ();
						IoCancelIrp(saveData->pIrp);
						m_spinUsbIn.Lock ();
						break;
					}
				}
			}
			m_spinUsbIn.UnLock ();
	        
			return STATUS_SUCCESS;
		}

		#ifdef ISOCH_FAST_MODE
			if (IsochVerify (irpSave, pPdoStub))
			{
				return STATUS_SUCCESS;
			}
		#endif

		DebugPrint (DebugError, "InitWorkIrp %d", irpSave->Irp);

		saveData = InitWorkIrp (irpSave, pPdoStub);
		if (saveData == NULL) 
		{
			return STATUS_SUCCESS;
		}
		++m_irpCounter;

		if (saveData->Isoch)
		{
			status = m_Isoch.PutIrpToDevice(saveData, pPdoStub);
		}
		else
		{
			PIO_STACK_LOCATION					irpStack;
			irpStack = IoGetNextIrpStackLocation( saveData->pIrp );

			m_spinUsbIn.Lock();
			m_listWaitCompleted.Add (saveData);
			m_spinUsbIn.UnLock();

			if (irpStack->MajorFunction == IRP_MJ_PNP && irpStack->MinorFunction == IRP_MN_START_DEVICE)
			{
				saveData->pIrp->IoStatus.Status = STATUS_SUCCESS;
				if (saveData->pPdo)
				{
					saveData->pPdo->SetNewPnpState (DriverBase::Started);
				}
				IoCompletionIrp(pPdoStub->GetDeviceObject (), saveData->pIrp, saveData);
			}
			else
			{
				status = IoCallDriver( pPdoStub->m_NextLowerDriver, saveData->pIrp );
			}
		}

		#ifdef AUTO_SEND_PENDING
			if (status == STATUS_PENDING)
			{
				if (irpSave->StackLocation.MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL)
				{
					switch (((PURB)irpSave->Buffer)->UrbHeader.Function)
					{
					case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
					case URB_FUNCTION_VENDOR_DEVICE:
					case URB_FUNCTION_VENDOR_INTERFACE:
					case URB_FUNCTION_VENDOR_ENDPOINT:
					case URB_FUNCTION_VENDOR_OTHER:
					case URB_FUNCTION_CLASS_DEVICE:
					case URB_FUNCTION_CLASS_INTERFACE:
					case URB_FUNCTION_CLASS_ENDPOINT:
					case URB_FUNCTION_CLASS_OTHER:
						{
							PWORKING_IRP            saveDataPending = NULL;
							ULONG64                 lPipeType;
							UCHAR                   Class;
							if (irpSave->Is64 == m_bIs64)
							{
								PURB pUrb = (PURB)irpSave->Buffer;
								lPipeType = pPdoStub->GetPipeType ((ULONG64)(ULONG_PTR (pUrb->UrbBulkOrInterruptTransfer.PipeHandle)), Class);

							}
							else
							{
								PURB_DIFF pUrb = (PURB_DIFF)irpSave->Buffer;
								lPipeType = pPdoStub->GetPipeType ((ULONG64)(ULONG_PTR (pUrb->UrbBulkOrInterruptTransfer.PipeHandle)), Class);
							}

							if ((lPipeType != UsbdPipeTypeInterrupt) && (Class != USB_DEVICE_CLASS_PRINTER))
							{
								saveDataPending = (PWORKING_IRP)m_AllocListWorkSave.Alloc ();
								RtlZeroMemory (saveDataPending, sizeof (WORKING_IRP));
								saveDataPending->Pending = 1;
								saveDataPending->Is64 = irpSave->Is64;
								saveDataPending->pIrpIndef = (LONG_PTR)irpSave->Irp;
								saveDataPending->pBase = this;
								AddIrpToQueueThread (NULL, TE_IRP_CompleteD, saveDataPending);
							}
							else
							{
								DebugPrint (DebugSpecial, "PutIrpToDevice - TypeInterrupt %d", (LONG_PTR)irpSave->Irp);
							}
							break;
						}
					}
				}
			}
		#endif
	}
    return STATUS_SUCCESS;
}

void CUsbPortToTcp::OutFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer)
{
    DebugPrint (DebugTrace, "FillBuffer");
    switch (pWorkSave->MajorFunction)
    {
    case IRP_MJ_PNP:
		if (pWorkSave->pAllocBuffer)
		{
			RtlCopyMemory (pOutBuffer, pWorkSave->pAllocBuffer, pWorkSave->lSizeBuffer);
		}
        break;
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        OutUrbFillBuffer (pWorkSave, pOutBuffer);
		break;
    case IRP_MJ_DEVICE_CONTROL:
        OutUsbIoFillBuffer (pWorkSave, pOutBuffer);
        break;
    case 0xFF:
        OutInterfaceFillBuffer (pWorkSave, pOutBuffer);
        break;
    }
}

bool CUsbPortToTcp::OutFillIrpSave (PIRP_SAVE pIrpSave, PWORKING_IRP pWorkSave)
{
    PIO_STACK_LOCATION  irpStackDevice;
    bool                bRet = false;
    CPdoStubExtension    *pPdoStub;
	PIRP				irpDevice = pWorkSave->pIrp;

	DebugPrint (DebugTrace, "OutFillIrpSave Num - %d", pWorkSave->pIrpIndef);

    pPdoStub = pWorkSave->pPdo;    

    pIrpSave->Is64 = pWorkSave->Is64;
	pIrpSave->NoAnswer = false;
    pIrpSave->Irp = (LONG)pWorkSave->pIrpIndef;

    if (pWorkSave->Pending)
    {
        pIrpSave->Status = STATUS_PENDING;
        pIrpSave->NeedSize = sizeof (IRP_SAVE);
        pIrpSave->BufferSize = 0;
		DeleteIrp(pWorkSave);

        return true;
    }

    irpStackDevice = IoGetNextIrpStackLocation (irpDevice);

	pIrpSave->Status = irpDevice->IoStatus.Status;
    pIrpSave->Information = irpDevice->IoStatus.Information;
    pIrpSave->Cancel = irpDevice->Cancel;
    pIrpSave->StackLocation.MajorFunction = pWorkSave->MajorFunction;
    pIrpSave->StackLocation.MinorFunction = pWorkSave->MinorFunction;
    pIrpSave->StackLocation.Parameters.Others.Argument1 = (ULONG64)irpStackDevice->Parameters.Others.Argument1;
    pIrpSave->StackLocation.Parameters.Others.Argument2 = (ULONG64)irpStackDevice->Parameters.Others.Argument2;
    pIrpSave->StackLocation.Parameters.Others.Argument3 = (ULONG64)irpStackDevice->Parameters.Others.Argument3;
    pIrpSave->StackLocation.Parameters.Others.Argument4 = (ULONG64)irpStackDevice->Parameters.Others.Argument4;
    pIrpSave->NeedSize = sizeof (IRP_SAVE);

#ifdef ISOCH_FAST_MODE
    pIrpSave->IsIsoch = pWorkSave->Isoch;
#endif

    if (pPdoStub->m_bMain)
    {
        pIrpSave->Device = 0;
    }
    else
    {
		if (pPdoStub->GetDeviceObject () == (PDEVICE_OBJECT)-1)
        {
            pIrpSave->Device = (ULONG64)-1;
        }
		else if (pPdoStub->GetDeviceObject () == (PDEVICE_OBJECT)-2)
        {
            pIrpSave->Device = (ULONG64)-2;
        }
        else
        {
            pIrpSave->Device = (ULONG64)pPdoStub->m_PhysicalDeviceObject;
        }
    }

	LONG lSizePakage;
    // get buffer size and pointer to data
    switch (pWorkSave->MajorFunction)
    {
    case IRP_MJ_POWER:
        break;
    case IRP_MJ_PNP:
        lSizePakage = OutPnpGetSizeBuffer (irpDevice, pWorkSave);
        break;
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        lSizePakage = OutUrbBufferSize (pWorkSave);
        break;
    case IRP_MJ_DEVICE_CONTROL:
        lSizePakage = OutUsbIoBufferSize (pWorkSave);
        break;
    case 0xFF:
        lSizePakage = OutInterfaceBufferSize (pWorkSave);
        break;
    }
    pIrpSave->NeedSize += lSizePakage;


    if (irpDevice->Cancel)
    {
        DebugPrint (DebugWarning, "Cancel IRP - 0x%x", irpDevice);
    }

    if (pIrpSave->Size < pIrpSave->NeedSize)
    {
        DebugPrint (DebugWarning, "small buffer %d %d", pIrpSave->Size, pIrpSave->NeedSize);
        bRet = false;
    }
    else
    {
        // copy data
        OutFillBuffer (pWorkSave, pIrpSave->Buffer);
        pIrpSave->BufferSize = lSizePakage;
        bRet = true;
    }

#ifdef ISOCH_FAST_MODE 
    if (pWorkSave->Isoch)
    {
        IsochStartNew ((PISOCH)pWorkSave->pAddition);
        IsochDeleteSave (pWorkSave);
        bRet = true;
    }
#endif
    // Complete device
    if (bRet && pPdoStub)
    {
		DeleteIrp (pWorkSave);
    }
    else
    {
        DebugPrint (DebugSpecial, "FillAnswer - false - %d - 0x%x", bRet, pPdoStub);
    }
    return bRet;
}

bool CUsbPortToTcp::OutFillAnswer (PIRP irpControl)
{
    PIO_STACK_LOCATION  irpStackControl;
    PIRP_SAVE           pIrpSave = NULL;
    LONG                lBufferSize = 0;        // Needed data size
	LONG				lCurPos = 0;
    BYTE                *pBuffer = NULL;            // pointer to data
    CControlDeviceExtension    *pDevControl;
	PWORKING_IRP        pWorkSave = NULL;
	KIRQL				OldIrql;


	DebugPrint (DebugTrace, "OutFillAnswer irpControl - 0x%x", irpControl);

    // Get control device
    pDevControl = CControlDeviceExtension::GetControlExtension ();
    if (!pDevControl)
    {
        DebugPrint (DebugError, "Did not get control device");

        irpControl->IoStatus.Status = STATUS_INVALID_PARAMETER;
		IoSetCancelRoutine (irpControl, NULL);
        IoCompleteRequest (irpControl, IO_NO_INCREMENT);
        --pDevControl->m_IrpCounter;
        return false;
    }

    // verify irp
    irpStackControl = IoGetCurrentIrpStackLocation (irpControl);
    if (irpStackControl->MajorFunction != IRP_MJ_DEVICE_CONTROL)
    {
        DebugPrint (DebugError, "0x%x did not IRP_MJ_DEVICE_CONTROL", irpControl);
        irpControl->IoStatus.Status = STATUS_INVALID_PARAMETER;
		IoSetCancelRoutine (irpControl, NULL);
        IoCompleteRequest (irpControl, IO_NO_INCREMENT);
        --pDevControl->m_IrpCounter;
    }

    // fill main fields
	pBuffer = (LPBYTE)irpControl->AssociatedIrp.SystemBuffer;
	lBufferSize = irpStackControl->Parameters.DeviceIoControl.OutputBufferLength;
    irpControl->IoStatus.Status = STATUS_SUCCESS;

	// verify queue is not empty
	m_spinUsbOut.Lock ();
	if (m_listIrpAnswer.GetCount () == 0)
	{
		m_spinUsbOut.UnLock ();
		return false;
	}
	m_spinUsbOut.UnLock ();

	while (lBufferSize > lCurPos)
	{
		if (lCurPos > 0 && m_listIrpAnswer.GetCount ())
		{
			pWorkSave = NULL;
		}
		pWorkSave = NULL;
		// Get device irp
	    m_spinUsbOut.Lock ();
		if (m_listIrpAnswer.GetCount ())
		{
			pWorkSave = (PWORKING_IRP) m_listIrpAnswer.PopFirst ();
		}
		m_spinUsbOut.UnLock ();

		if (pWorkSave == NULL)
		{
			break;
		}

		pIrpSave = (PIRP_SAVE)(pBuffer + lCurPos);
		pIrpSave->Size = lBufferSize - lCurPos;

		if (!OutFillIrpSave (pIrpSave, pWorkSave))
		{
			m_spinUsbOut.Lock ();
			m_listIrpAnswer.Add (pWorkSave);
			m_spinUsbOut.UnLock ();

			if (lCurPos == 0)
			{
				DebugPrint (DebugError, "OutFillAnswer STATUS_BUFFER_OVERFLOW");
				irpControl->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
				lCurPos = sizeof (IRP_SAVE);
			}
			break;
		}
		lCurPos += pIrpSave->NeedSize;

		//FIXME: for linux
		break;
	}

	if (!pIrpSave)
	{
		return false;
	}

// complete control device
	if (NT_SUCCESS (irpControl->IoStatus.Status) && (pIrpSave->NeedSize > pIrpSave->Size))
	{
		ASSERT (false);
	}

	irpControl->IoStatus.Information = lCurPos;
	IoSetCancelRoutine (irpControl, NULL);
    IoCompleteRequest (irpControl, IO_NO_INCREMENT);
    --pDevControl->m_IrpCounter;

#ifdef ISOCH_FAST_MODE 
    if (pWorkSave->Isoch)
    {
        IsochStartNew ((PISOCH)pWorkSave->pAddition);
        IsochDeleteSave (pWorkSave);
        bRet = true;
    }
#endif
    return true;
}

// ***********************************************************************
//                    Complete IRP
// ***********************************************************************

NTSTATUS CUsbPortToTcp::IoCompletionIrp(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context)
{
    PWORKING_IRP		pWorkSave = (PWORKING_IRP)Context;
    TPtrUsbPortToTcp	pBase = pWorkSave->pBase;

    pBase->DebugPrint (DebugTrace, "IoCompletionIrp - 0x%x - %d", Irp, pWorkSave->pIrpIndef);

    pBase->m_spinUsbIn.Lock ();
    for (int a = 0; a < pBase->m_listWaitCompleted.GetCount (); a++)
    {
        if (pBase->m_listWaitCompleted.GetItem (a) == pWorkSave)
        {
            pBase->m_listWaitCompleted.Remove (a);
            break;
        }
    }
    pBase->m_spinUsbIn.UnLock ();

	#ifdef ISOCH_FAST_MODE
		if (pBase->IsochVerifyDel (pWorkSave))
		{
			return STATUS_MORE_PROCESSING_REQUIRED;
		}
	#endif

	if (pWorkSave->NoAnswer)
	{
		pBase->DeleteIrp (pWorkSave);
		return STATUS_MORE_PROCESSING_REQUIRED;
	}

	if (pWorkSave->Isoch)
	{
		pBase->m_Isoch.PushNextIsoPackedge();
	}

    LONG lTemp = InterlockedExchange(&pWorkSave->lLock, IRPLOCK_COMPLETED);
    if (pBase->m_bIsPresent && !pBase->m_bCancel && lTemp == IRPLOCK_CANCELABLE)
    {
		pBase->IoCompletionIrpThread (Context);
    }
    else
    {
        if (lTemp != IRPLOCK_CANCEL_STARTED)
        {
			pBase->DebugPrint (DebugWarning, "IoCompletionIrp - 0x%x - %d canceled", Irp, pWorkSave->pIrpIndef);

			pBase->DeleteIrp (pWorkSave);
            pWorkSave = NULL;
        }
    }
    return STATUS_MORE_PROCESSING_REQUIRED;
}
// ******************************************************************
//                PnP IRP
// ******************************************************************
LONG CUsbPortToTcp::OutPnpGetSizeBuffer (PIRP Irp, PWORKING_IRP pWorkSave)
{
    LONG lSize = 0;

    PIO_STACK_LOCATION        irpStack;
    
    DebugPrint (DebugTrace, "PnpBufferSize irp 0x%x", pWorkSave->pIrp);
    DebugPrint (DebugSpecial, "%s", DriverBase::CBaseDeviceExtension::PnPMinorFunctionString (pWorkSave->MinorFunction));

    irpStack = IoGetNextIrpStackLocation( Irp );

    switch (pWorkSave->MinorFunction)
    {
    case IRP_MN_QUERY_CAPABILITIES:
        {
			lSize = pWorkSave->lSizeBuffer;
            DebugPrint (DebugInfo, "IRP_MN_QUERY_CAPABILITIES pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, pWorkSave->lSizeBuffer);
        }
        break;
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        if (Irp->IoStatus.Information != 0)
        {
			// Copy device relation
			lSize = sizeof (DEVICE_RELATIONS) + (max (PDEVICE_RELATIONS(Irp->IoStatus.Information)->Count - 1, 0) * sizeof (PDEVICE_OBJECT));
			pWorkSave->lSizeBuffer = lSize;

			pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (lSize, NonPagedPool);
			RtlCopyMemory (pWorkSave->pAllocBuffer, (BYTE*)Irp->IoStatus.Information, lSize);
			// free system buffer
			ExFreePool ((PVOID)Irp->IoStatus.Information);
            DebugPrint (DebugInfo, "IRP_MN_QUERY_DEVICE_RELATIONS pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
        }
        else
        {
            DebugPrint (DebugInfo, "It did not the sub devices");
        }
        break;
    case IRP_MN_QUERY_RESOURCES:
        if (Irp->IoStatus.Information != 0)
        {
            lSize = sizeof (CM_RESOURCE_LIST) + (max (PCM_RESOURCE_LIST(Irp->IoStatus.Information)->Count - 1, 0) * sizeof (CM_FULL_RESOURCE_DESCRIPTOR));
			pWorkSave->lSizeBuffer = lSize;
			pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (lSize, NonPagedPool);
			RtlCopyMemory (pWorkSave->pAllocBuffer, (BYTE*)Irp->IoStatus.Information, lSize);
			// free system buffer
			ExFreePool ((PVOID)Irp->IoStatus.Information);
            DebugPrint (DebugInfo, "IRP_MN_QUERY_RESOURCES pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
        }
        break;
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
        if (Irp->IoStatus.Information != 0)
        {
            lSize = PIO_RESOURCE_REQUIREMENTS_LIST(Irp->IoStatus.Information)->ListSize;
			pWorkSave->lSizeBuffer = lSize;
			pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (lSize, NonPagedPool);
			RtlCopyMemory (pWorkSave->pAllocBuffer, (BYTE*)Irp->IoStatus.Information, lSize);
			// free system buffer
			ExFreePool ((PVOID)Irp->IoStatus.Information);

            DebugPrint (DebugInfo, "IRP_MN_QUERY_RESOURCE_REQUIREMENTS pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
        }
        break;
    case IRP_MN_QUERY_BUS_INFORMATION:
        if (Irp->IoStatus.Information != 0)
        {
            lSize = sizeof (PNP_BUS_INFORMATION);
			pWorkSave->lSizeBuffer = lSize;
			pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (lSize, NonPagedPool);
			RtlCopyMemory (pWorkSave->pAllocBuffer, (BYTE*)Irp->IoStatus.Information, lSize);
			// free system buffer
			ExFreePool ((PVOID)Irp->IoStatus.Information);

            DebugPrint (DebugInfo, "IRP_MN_QUERY_BUS_INFORMATION pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
        }
        break;
    case IRP_MN_QUERY_ID:
        if (Irp->IoStatus.Information != 0)
        {
            switch (pWorkSave->lOther)
            {
			case BusQueryInstanceID:
			case BusQueryDeviceID:
				{
					WCHAR *szTemp = (WCHAR*)Irp->IoStatus.Information;
					lSize = lSize + (wcslen (szTemp) + 1)*sizeof (WCHAR);
				}
				break;
			default:
				{
					WCHAR *szTemp = (WCHAR*)Irp->IoStatus.Information;
					while (wcslen (szTemp))
					{
						lSize = lSize + wcslen (szTemp) + 1;
						szTemp += wcslen (szTemp) + 1;
					}
					lSize = (lSize + 1)*sizeof (WCHAR);
				}
            }
			pWorkSave->lSizeBuffer = lSize;
			pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (lSize, NonPagedPool);
			RtlCopyMemory (pWorkSave->pAllocBuffer, (BYTE*)Irp->IoStatus.Information, lSize);
			// free system buffer
			ExFreePool ((PVOID)Irp->IoStatus.Information);
            DebugPrint (DebugInfo, "IRP_MN_QUERY_ID pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
        }
        break;
    case IRP_MN_QUERY_DEVICE_TEXT:
        if (Irp->IoStatus.Information != 0)
        {
            lSize = (wcslen ((PWCHAR)Irp->IoStatus.Information) + 1);
            lSize *= sizeof (WCHAR);

			pWorkSave->lSizeBuffer = lSize;
			pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (lSize, NonPagedPool);
			RtlCopyMemory (pWorkSave->pAllocBuffer, (BYTE*)Irp->IoStatus.Information, lSize);
			// free system buffer
			ExFreePool ((PVOID)Irp->IoStatus.Information);

            DebugPrint (DebugInfo, "IRP_MN_QUERY_DEVICE_TEXT pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
            DebugPrint (DebugInfo, "IRP_MN_QUERY_DEVICE_TEXT %S", pWorkSave->pAllocBuffer);
        }
        break;
    case IRP_MN_QUERY_INTERFACE:
        if (pWorkSave->pAllocBuffer != 0 && pWorkSave->lSizeBuffer)
        {
            GUID GuidUsb = {0xb1a96a13, 0x3de0, 0x4574, 0x9b, 0x1, 0xc0, 0x8f, 0xea, 0xb3, 0x18, 0xd6};
            GUID GuidUsbCamd = {0x2bcb75c0, 0xb27f, 0x11d1, 0xba, 0x41, 0x0, 0xa0, 0xc9, 0xd, 0x2b, 0x5};
            GUID *GuidCur = (GUID*)pWorkSave->pSecondBuffer;
            PINTERFACE Interface = (PINTERFACE)pWorkSave->pAllocBuffer;

            if (*GuidCur == GuidUsb)
            {
                PUSB_BUS_INTERFACE_USBDI_V3_FULL pInterface = (PUSB_BUS_INTERFACE_USBDI_V3_FULL)pWorkSave->pAllocBuffer;
                RtlCopyMemory (&pWorkSave->pPdo->m_Interface, pWorkSave->pAllocBuffer, min (pInterface->Size, sizeof (USB_BUS_INTERFACE_USBDI_V3_FULL)));
                if (pWorkSave->Is64 == m_bIs64)
                {
                    lSize = lSize + pInterface->Size;
                    DebugPrint (DebugInfo, "IRP_MN_QUERY_INTERFACE pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
                }
                else
                {
                    PUSB_BUS_INTERFACE_USBDI_V3_DIFF pInterfaceNew;
					// allocate buffer for different platform
                    pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (sizeof (USB_BUS_INTERFACE_USBDI_V3_DIFF), NonPagedPool);
					RtlZeroMemory(pWorkSave->pAllocBuffer, sizeof(USB_BUS_INTERFACE_USBDI_V3_DIFF));
                    pInterfaceNew = (PUSB_BUS_INTERFACE_USBDI_V3_DIFF)pWorkSave->pAllocBuffer;
                    pInterfaceNew->Version = pInterface->Version;

                    pInterfaceNew->BusContext = pInterface->BusContext?1:0;
                    if (pInterfaceNew->Version >= USB_BUSIF_USBDI_VERSION_0_FULL)
                    {
                        pInterfaceNew->Size = sizeof (USB_BUS_INTERFACE_USBDI_V0_DIFF);
                        pInterfaceNew->InterfaceReference = pInterface->InterfaceReference?1:0;
                        pInterfaceNew->InterfaceDereference = pInterface->InterfaceDereference?1:0;
                        pInterfaceNew->GetUSBDIVersion = pInterface->GetUSBDIVersion?1:0;
                        pInterfaceNew->QueryBusTime = pInterface->QueryBusTime?1:0;
                        pInterfaceNew->SubmitIsoOutUrb = pInterface->SubmitIsoOutUrb?1:0;
                        pInterfaceNew->QueryBusInformation = pInterface->QueryBusInformation?1:0;
                    }
                    if (pInterfaceNew->Version >= USB_BUSIF_USBDI_VERSION_1_FULL)
                    {
                        pInterfaceNew->Size = sizeof (USB_BUS_INTERFACE_USBDI_V1_DIFF);
                        pInterfaceNew->IsDeviceHighSpeed = pInterface->IsDeviceHighSpeed?1:0;
                    }
                    if (pInterfaceNew->Version >= USB_BUSIF_USBDI_VERSION_2_FULL)
                    {
                        pInterfaceNew->Size = sizeof (USB_BUS_INTERFACE_USBDI_V2_DIFF);
                        pInterfaceNew->EnumLogEntry = pInterface->EnumLogEntry?1:0;
                    }

                    if (pInterfaceNew->Version >= USB_BUSIF_USBDI_VERSION_3_FULL)
                    {
                        pInterfaceNew->Size = sizeof (USB_BUS_INTERFACE_USBDI_V3_DIFF);
                        pInterfaceNew->QueryBusTimeEx = pInterface->QueryBusTimeEx?1:0;
                        pInterfaceNew->QueryControllerType = pInterface->QueryControllerType?1:0;
                    }

					pWorkSave->lSizeBuffer = pInterfaceNew->Size;
                    lSize = lSize + pInterfaceNew->Size;
					// free interface for current platform
                    m_BufferMain.FreeBuffer (pInterface);
                }
            }
            else if (*GuidCur == GuidUsbCamd)
            {
                PUSBCAMD_INTERFACE pInterface = (PUSBCAMD_INTERFACE)pWorkSave->pAllocBuffer;
                RtlCopyMemory (&pWorkSave->pPdo->m_Interface, pWorkSave->pAllocBuffer, min (pInterface->Interface.Size, sizeof (USBCAMD_INTERFACE)));
                if (pWorkSave->Is64 == m_bIs64)
                {
                    lSize = lSize + pInterface->Interface.Size;
                    DebugPrint (DebugInfo, "IRP_MN_QUERY_INTERFACE pBuffer = 0x%x size - %d", pWorkSave->pAllocBuffer, lSize);
                }
                else
                {
                    PUSBCAMD_INTERFACE_DIFF pInterfaceNew;
					// allocate buffer for different platform
                    pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (sizeof (USBCAMD_INTERFACE_DIFF), NonPagedPool);
                    pInterfaceNew = (PUSBCAMD_INTERFACE_DIFF)pWorkSave->pAllocBuffer;

                    pInterfaceNew->Interface.Version = pInterface->Interface.Version;
                    pInterfaceNew->Interface.Size = sizeof (USBCAMD_INTERFACE_DIFF);
                    pInterfaceNew->Interface.Context = pInterface->Interface.Context?1:0;
                    pInterfaceNew->USBCAMD_WaitOnDeviceEvent = pInterface->USBCAMD_WaitOnDeviceEvent?1:0;
                    pInterfaceNew->USBCAMD_BulkReadWrite = pInterface->USBCAMD_BulkReadWrite?1:0;
                    pInterfaceNew->USBCAMD_SetVideoFormat = pInterface->USBCAMD_SetVideoFormat?1:0;
                    pInterfaceNew->USBCAMD_SetIsoPipeState = pInterface->USBCAMD_SetIsoPipeState?1:0;
                    pInterfaceNew->USBCAMD_CancelBulkReadWrite = pInterface->USBCAMD_CancelBulkReadWrite?1:0;
                    pInterfaceNew->Interface.InterfaceReference = pInterface->Interface.InterfaceReference?1:0;
                    pInterfaceNew->Interface.InterfaceDereference = pInterface->Interface.InterfaceDereference?1:0;

                    lSize = lSize + pInterfaceNew->Interface.Size;
					// Free interface for current platform
                    m_BufferMain.FreeBuffer (pInterface);
                }
            }
        }
        break;

    case IRP_MN_READ_CONFIG:
    case IRP_MN_WRITE_CONFIG:

    default:
        break;
    }

	return lSize;
}

void CUsbPortToTcp::InPnpFillIrp (PIRP pnpIrp, PIRP_SAVE irpSave, PWORKING_IRP pWorkSave)
{
    PIO_STACK_LOCATION        irpStack;

    DebugPrint (DebugTrace, "PnpIrpGenerate");
    DebugPrint (DebugTrace, "%s", DriverBase::CBaseDeviceExtension::PnPMinorFunctionString (irpSave->StackLocation.MinorFunction));
    DebugPrint (DebugSpecial, "%s", DriverBase::CBaseDeviceExtension::PnPMinorFunctionString (irpSave->StackLocation.MinorFunction));

    irpStack = IoGetNextIrpStackLocation( pnpIrp );

    switch (irpStack->MinorFunction)
    {
    case IRP_MN_START_DEVICE:
        if (pWorkSave->pPdo)
        {
            pWorkSave->pPdo->SetNewPnpState (DriverBase::Started);
        }
        break;
    case IRP_MN_QUERY_CAPABILITIES:
        if (irpSave->BufferSize >= sizeof (DEVICE_CAPABILITIES))
        {
            irpStack->Parameters.DeviceCapabilities.Capabilities = (PDEVICE_CAPABILITIES)m_BufferMain.AllocBuffer (irpSave->BufferSize, PagedPool);
            RtlCopyMemory ((void*)irpStack->Parameters.DeviceCapabilities.Capabilities, irpSave->Buffer, irpSave->BufferSize);
            pWorkSave->pAllocBuffer = (BYTE*)irpStack->Parameters.DeviceCapabilities.Capabilities;
            pWorkSave->lSizeBuffer = irpSave->BufferSize;
            DebugPrint (DebugInfo, "IRP_MN_QUERY_CAPABILITIES - %d size", irpSave->BufferSize);
        }
		else
		{
            irpStack->Parameters.DeviceCapabilities.Capabilities = (PDEVICE_CAPABILITIES)m_BufferMain.AllocBuffer (sizeof (DEVICE_CAPABILITIES), PagedPool);
            RtlZeroMemory ((void*)irpStack->Parameters.DeviceCapabilities.Capabilities, sizeof (DEVICE_CAPABILITIES));
            pWorkSave->pAllocBuffer = (BYTE*)irpStack->Parameters.DeviceCapabilities.Capabilities;
            pWorkSave->lSizeBuffer = sizeof (DEVICE_CAPABILITIES);
            DebugPrint (DebugInfo, "IRP_MN_QUERY_CAPABILITIES - %d size", irpSave->BufferSize);
		}
        break;

    case IRP_MN_QUERY_DEVICE_TEXT:
        pnpIrp->IoStatus.Information = 0;
        pWorkSave->lSizeBuffer = 0;
        break;

    case IRP_MN_QUERY_ID:
        DebugPrint (DebugInfo, "%s", DriverBase::CBaseDeviceExtension::PnpQueryIdString (irpStack->Parameters.QueryId.IdType));
        pWorkSave->lOther = irpStack->Parameters.QueryId.IdType;
    case IRP_MN_QUERY_DEVICE_RELATIONS:
    case IRP_MN_QUERY_RESOURCES:
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
    case IRP_MN_QUERY_BUS_INFORMATION:
        DebugPrint (DebugInfo, "Current Information %d ,set %d", pnpIrp->IoStatus.Information, 0);
        pnpIrp->IoStatus.Information = 0;
        pWorkSave->lSizeBuffer = 0;
        break;
    case IRP_MN_QUERY_INTERFACE:
        if (irpSave->BufferSize >= sizeof (GUID))
        {
            PVOID lpMem;
            GUID *GuidCur = (GUID*)((BYTE*)irpSave->Buffer);
            
            if (pWorkSave->Is64 != m_bIs64)
            {
                if (*GuidCur == GuidUsb)
                {
                    GUID *GuidCur = (GUID*)((BYTE*)(irpSave->Buffer) + irpStack->Parameters.QueryInterface.Size);

                    switch (irpStack->Parameters.QueryInterface.Version)
                    {
                    case USB_BUSIF_USBDI_VERSION_0_FULL:
                        irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V0_FULL);
                        break;
                    case USB_BUSIF_USBDI_VERSION_1_FULL:
                        irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V1_FULL);
                        break;
                    case USB_BUSIF_USBDI_VERSION_2_FULL:
                        irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V2_FULL);
                        break;
                    case USB_BUSIF_USBDI_VERSION_3_FULL:
                        irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V3_FULL);
                        break;
                    }
                }
                else if (*GuidCur == GuidUsbCamd)
                {
                    irpStack->Parameters.QueryInterface.Size = sizeof (INTERFACE);
                }
            }
			if (irpStack->Parameters.QueryInterface.Size == 0)
			{
                switch (irpStack->Parameters.QueryInterface.Version)
                {
                case USB_BUSIF_USBDI_VERSION_0_FULL:
                    irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V0_FULL);
                    break;
                case USB_BUSIF_USBDI_VERSION_1_FULL:
                    irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V1_FULL);
                    break;
                case USB_BUSIF_USBDI_VERSION_2_FULL:
                    irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V2_FULL);
                    break;
                case USB_BUSIF_USBDI_VERSION_3_FULL:
                    irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V3_FULL);
                    break;
                }
			}
            irpStack->Parameters.QueryInterface.Interface = (PINTERFACE)m_BufferMain.AllocBuffer (irpStack->Parameters.QueryInterface.Size, PagedPool);
			irpStack->Parameters.QueryInterface.InterfaceType = (GUID*)m_BufferSecond.AllocBuffer(irpSave->BufferSize, PagedPool);
            RtlZeroMemory (irpStack->Parameters.QueryInterface.Interface, irpStack->Parameters.QueryInterface.Size);
            RtlCopyMemory ((void*)irpStack->Parameters.QueryInterface.InterfaceType, irpSave->Buffer, irpSave->BufferSize);

            pWorkSave->pAllocBuffer = (BYTE*)(irpStack->Parameters.QueryInterface.Interface);
            pWorkSave->lSizeBuffer = irpSave->BufferSize + irpStack->Parameters.QueryInterface.Size;
            pWorkSave->pSecondBuffer = (BYTE*)irpStack->Parameters.QueryInterface.InterfaceType;
            DebugPrint (DebugInfo, "IRP_MN_QUERY_INTERFACE - %d size", pWorkSave->lSizeBuffer);
        }
        break;
    }
}

// **********************************************************************
//                    USB
// **********************************************************************
void CUsbPortToTcp::InUsbFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave)
{
    PIO_STACK_LOCATION irpStack;

    DebugPrint (DebugTrace, "UsbFillIrp");

    irpStack = IoGetNextIrpStackLocation (pWorkSave->pIrp);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
    {
	case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
		{
			PUSB_IDLE_CALLBACK_INFO_FULL idleCallbackInfo = (PUSB_IDLE_CALLBACK_INFO_FULL)m_BufferMain.AllocBuffer (sizeof(USB_IDLE_CALLBACK_INFO_FULL), NonPagedPool);
			pWorkSave->pAllocBuffer = (BYTE*)idleCallbackInfo;
			idleCallbackInfo->IdleCallback = CPdoStubExtension::UsbIdleCallBack;
			// Put a pointer to the device extension in member IdleContext
			idleCallbackInfo->IdleContext = (PVOID) pWorkSave->pPdo;  
			irpStack->Parameters.DeviceIoControl.Type3InputBuffer = idleCallbackInfo;
			irpStack->Parameters.DeviceIoControl.InputBufferLength = sizeof(USB_IDLE_CALLBACK_INFO_FULL);
		}
		break;
    case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
        irpStack->Parameters.Others.Argument1 = m_BufferMain.AllocBuffer (irpSave->BufferSize, NonPagedPool);
        pWorkSave->pAllocBuffer = (BYTE*)irpStack->Parameters.Others.Argument1;
        pWorkSave->lSizeBuffer = irpSave->BufferSize;
        RtlCopyMemory (irpStack->Parameters.Others.Argument1, irpSave->Buffer, irpSave->BufferSize);
        break;
    case IOCTL_INTERNAL_USB_RESET_PORT:
    case IOCTL_INTERNAL_USB_ENABLE_PORT:
    case IOCTL_INTERNAL_USB_CYCLE_PORT:
        break;
    case IOCTL_INTERNAL_USB_GET_HUB_NAME:
        pWorkSave->pIrp->AssociatedIrp.SystemBuffer = m_BufferMain.AllocBuffer (irpSave->BufferSize, NonPagedPool);
        pWorkSave->pAllocBuffer = (BYTE*)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
        pWorkSave->lSizeBuffer = irpSave->BufferSize;
        RtlCopyMemory (pWorkSave->pIrp->AssociatedIrp.SystemBuffer, irpSave->Buffer, irpSave->BufferSize);
        break;
    case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
		{
			PUSB_HUB_NAME pName;
			irpStack->Parameters.Others.Argument1 = m_BufferMain.AllocBuffer (irpSave->BufferSize, NonPagedPool);
			pName = (PUSB_HUB_NAME)irpStack->Parameters.Others.Argument1;
			if (irpSave->BufferSize == sizeof (USB_HUB_NAME))
			{
				irpStack->Parameters.Others.Argument2 = (PVOID)sizeof (USB_HUB_NAME);
			}
			else
			{
				irpStack->Parameters.Others.Argument2 = &pName->ActualLength;
			}
			
			pWorkSave->pAllocBuffer = (BYTE*)irpStack->Parameters.Others.Argument1;
			pWorkSave->lSizeBuffer = irpSave->BufferSize;
			RtlCopyMemory (pName, irpSave->Buffer, irpSave->BufferSize);
			pName->ActualLength = irpSave->BufferSize;
		}
        break;
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
        irpStack->Parameters.Others.Argument1 = m_BufferMain.AllocBuffer (irpSave->BufferSize, NonPagedPool);
        pWorkSave->pAllocBuffer = (BYTE*)irpStack->Parameters.Others.Argument1;
        pWorkSave->lSizeBuffer = irpSave->BufferSize;
        RtlCopyMemory (irpStack->Parameters.Others.Argument1, irpSave->Buffer, irpSave->BufferSize);
        break;
    }
}

LONG CUsbPortToTcp::OutUsbBufferSize (PWORKING_IRP pWorkSave)
{
    DebugPrint (DebugTrace, "UsbFillIrp");

    switch (pWorkSave->IoControlCode)
    {
    case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
    case IOCTL_INTERNAL_USB_RESET_PORT:
    case IOCTL_INTERNAL_USB_ENABLE_PORT:
    case IOCTL_INTERNAL_USB_CYCLE_PORT:
    case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
        //*pbFreeMem = true;
        break;
    case IOCTL_INTERNAL_USB_GET_HUB_NAME:
        //*pbFreeMem = false;
        break;
    }

	return pWorkSave->lSizeBuffer;
}

void CUsbPortToTcp::OutUsbFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer)
{
    DebugPrint (DebugTrace, "UsbFillBuffer");
    switch (pWorkSave->IoControlCode)
    {
    case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
    case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
    //case IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO:
    //case IOCTL_INTERNAL_USB_GET_HUB_COUNT:
    //case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE:
		ASSERT (pWorkSave->lSizeBuffer > 0 && pWorkSave->pAllocBuffer);
        RtlCopyMemory (pOutBuffer, pWorkSave->pAllocBuffer, pWorkSave->lSizeBuffer);
        break;
    case IOCTL_INTERNAL_USB_RESET_PORT:
    case IOCTL_INTERNAL_USB_ENABLE_PORT:
    case IOCTL_INTERNAL_USB_CYCLE_PORT:
        break;
    case IOCTL_INTERNAL_USB_GET_HUB_NAME:
        RtlCopyMemory (pOutBuffer, pWorkSave->pAllocBuffer, pWorkSave->lSizeBuffer);
        pWorkSave->pIrp->AssociatedIrp.SystemBuffer = NULL;
        break;
    }
}

// **********************************************************************
//                    URB
// **********************************************************************
void CUsbPortToTcp::InUrbFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave)
{
    PURB_FULL               urb;
    BYTE                    *pBufData;
    PMDL                    *ppMdlTemp = NULL;
    PVOID                    *ppBuffer = NULL;
    LONG                    lSizeBuffer = 0;
    BYTE                    *pTemp;
    PIO_STACK_LOCATION        irpStack;
    //bool                    bCopyDate = true;

    DebugPrint (DebugTrace, "UrbFillIrp");

    if (ULONG ((ULONG_PTR)irpSave->StackLocation.Parameters.Others.Argument3) != IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        InUsbFillIrp (irpSave, pWorkSave);
        return;
    }    // Create and fill in IRP

    if ((irpSave->Is64?true:false) != m_bIs64)
    {
        InUrbFillIrpDifferent (irpSave, pWorkSave);
        return;
    }

    DebugPrint (DebugInfo, "%s", CDebugPrintUrb::UrbFunctionString (((PURB)irpSave->Buffer)->UrbHeader.Function));

	urb = (PURB_FULL) m_BufferMain.AllocBuffer (((PURB)irpSave->Buffer)->UrbHeader.Length * 2, NonPagedPool); // 0x10 addition
    RtlCopyMemory (urb, irpSave->Buffer, ((PURB)irpSave->Buffer)->UrbHeader.Length);
    pWorkSave->pAllocBuffer = (BYTE*)urb;
    pWorkSave->lSizeBuffer = urb->UrbHeader.Length;
    pWorkSave->lOther = urb->UrbHeader.Function;
    // Set data Buffer
    pBufData = irpSave->Buffer + urb->UrbHeader.Length;
    // Set new URB
    irpStack = IoGetNextIrpStackLocation( pWorkSave->pIrp );
    irpStack->Parameters.Others.Argument1 = urb;

    // Verify 
    switch (urb->UrbHeader.Function)
    {
    case URB_FUNCTION_SELECT_CONFIGURATION:
        ppBuffer = (PVOID*)&urb->UrbSelectConfiguration.ConfigurationDescriptor;
        lSizeBuffer = irpSave->BufferSize - urb->UrbHeader.Length;
        break;
    case URB_FUNCTION_CONTROL_TRANSFER:
        ppMdlTemp = &urb->UrbControlTransfer.TransferBufferMDL;
        ppBuffer = &urb->UrbControlTransfer.TransferBuffer;
        lSizeBuffer = urb->UrbControlTransfer.TransferBufferLength;
        break;
    case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
        lSizeBuffer = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        ppMdlTemp = &urb->UrbBulkOrInterruptTransfer.TransferBufferMDL;
        ppBuffer = &urb->UrbBulkOrInterruptTransfer.TransferBuffer;

		DebugPrint (DebugTrace, "URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER %d %d", irpSave->Irp, lSizeBuffer);
        break;
    case URB_FUNCTION_ISOCH_TRANSFER:
        lSizeBuffer = urb->UrbIsochronousTransfer.TransferBufferLength;
        ppMdlTemp = &urb->UrbIsochronousTransfer.TransferBufferMDL;
        ppBuffer = &urb->UrbIsochronousTransfer.TransferBuffer;
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
		{
			pWorkSave->Isoch = true;

		}

		if (urb->UrbIsochronousTransfer.StartFrame)
		{
			m_Isoch.BuildStartFrame(urb);
		}
		break;
    case URB_FUNCTION_CONTROL_TRANSFER_EX:
        lSizeBuffer = urb->UrbControlTransferEx.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlTransferEx.TransferBufferMDL;
        ppBuffer = &urb->UrbControlTransferEx.TransferBuffer;
		break;
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:

    case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
        lSizeBuffer = urb->UrbControlDescriptorRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlDescriptorRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlDescriptorRequest.TransferBuffer;
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
        lSizeBuffer = urb->UrbControlGetStatusRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlGetStatusRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlGetStatusRequest.TransferBuffer;
        break;
    case URB_FUNCTION_VENDOR_DEVICE:
    case URB_FUNCTION_VENDOR_INTERFACE:
    case URB_FUNCTION_VENDOR_ENDPOINT:
    case URB_FUNCTION_VENDOR_OTHER:
    case URB_FUNCTION_CLASS_DEVICE:
    case URB_FUNCTION_CLASS_INTERFACE:
    case URB_FUNCTION_CLASS_ENDPOINT:
    case URB_FUNCTION_CLASS_OTHER :
        lSizeBuffer = urb->UrbControlVendorClassRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlVendorClassRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlVendorClassRequest.TransferBuffer;
        break;
    case URB_FUNCTION_GET_CONFIGURATION:
        lSizeBuffer = urb->UrbControlGetConfigurationRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlGetConfigurationRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlGetConfigurationRequest.TransferBuffer;
        break;
    case URB_FUNCTION_GET_INTERFACE:
        lSizeBuffer = urb->UrbControlGetInterfaceRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlGetInterfaceRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlGetInterfaceRequest.TransferBuffer;
        break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        lSizeBuffer = urb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbOSFeatureDescriptorRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbOSFeatureDescriptorRequest.TransferBuffer;
        break;
    case URB_FUNCTION_ABORT_PIPE:
    case URB_FUNCTION_RESET_PIPE:
		// clear query isoch out
		m_Isoch.ClearIsoch ();
		break;
    }

    if ((ppBuffer && *ppBuffer) || (ppMdlTemp && *ppMdlTemp))
    {
		(*ppBuffer) = (BYTE*)m_BufferSecond.AllocBuffer (lSizeBuffer + 0x10, NonPagedPool);

        if (lSizeBuffer <= irpSave->BufferSize - urb->UrbHeader.Length)
        {
            RtlCopyMemory (*ppBuffer, pBufData, lSizeBuffer);
        }
        pWorkSave->pSecondBuffer = *ppBuffer;
        pWorkSave->lSecSizeBuffer = lSizeBuffer;
		if (ppMdlTemp)
		{
			(*ppMdlTemp) = NULL;
		}
    }
}

LONG CUsbPortToTcp::OutUrbBufferSize (PWORKING_IRP pWorkSave)
{
    LONG        OldSize;
	LONG		lSize;
	CBusFilterExtension *pBus = (CBusFilterExtension*)pWorkSave->pPdo->m_pBus;

    DebugPrint (DebugTrace, "UrbBufferSize irp - 0x%x", pWorkSave->pIrp);

    if (pWorkSave->IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        return OutUsbBufferSize (pWorkSave);
    }    // Create and fill in IRP


    PURB_FULL urb = (PURB_FULL)(pWorkSave->pAllocBuffer);
    if ((pWorkSave->Is64?true:false) != m_bIs64)
    {
        lSize = CalcSizeUrbToDiffUrb (urb);
    }
    else
    {
        lSize = urb->UrbHeader.Length;
    }

    DebugPrint (DebugSpecial, "%s 0x%x", CDebugPrintUrb::UrbFunctionString (urb->UrbHeader.Function), urb->UrbHeader.Status);

    OldSize = pWorkSave->lSecSizeBuffer;
    pWorkSave->lSecSizeBuffer = 0;
    // Verify 
    switch (urb->UrbHeader.Function)
    {
    case URB_FUNCTION_CONTROL_TRANSFER:
        {
            lSize += urb->UrbControlTransfer.TransferBufferLength;
            pWorkSave->lSecSizeBuffer = urb->UrbControlTransfer.TransferBufferLength;
        }
        break;
    case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		if (USB_ENDPOINT_DIRECTION_IN (pWorkSave->pPdo->GetPipeEndpoint (ULONG64 ((ULONG_PTR)urb->UrbBulkOrInterruptTransfer.PipeHandle), false)))
        {
            lSize += urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
            pWorkSave->lSecSizeBuffer = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;

        }
        break;
    case URB_FUNCTION_ISOCH_TRANSFER:
        if (USBD_TRANSFER_DIRECTION_IN == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
        {
            if (urb->UrbIsochronousTransfer.ErrorCount == urb->UrbIsochronousTransfer.NumberOfPackets)
            {
                for (ULONG a = 0; a < urb->UrbIsochronousTransfer.NumberOfPackets; a++)
                {
					if (!NT_SUCCESS (urb->UrbIsochronousTransfer.IsoPacket [a].Status))
					{
						DebugPrint (DebugWarning, "0x%x", urb->UrbIsochronousTransfer.IsoPacket [a].Status);
					}
                }
            }

			DebugPrint (DebugSpecial, "%s 0x%x %d %d", CDebugPrintUrb::UrbFunctionString (urb->UrbHeader.Function), urb->UrbHeader.Status, OldSize, pWorkSave->lSecSizeBuffer = urb->UrbIsochronousTransfer.TransferBufferLength);
			if (urb->UrbIsochronousTransfer.TransferBufferLength)
			{
				lSize += urb->UrbIsochronousTransfer.TransferBufferLength;
				pWorkSave->lSecSizeBuffer = urb->UrbIsochronousTransfer.TransferBufferLength;
			}
			else
			{
				pWorkSave->lSecSizeBuffer = 0;
			}
        }
        break;
    case URB_FUNCTION_CONTROL_TRANSFER_EX:
        if (USBD_TRANSFER_DIRECTION_IN == USBD_TRANSFER_DIRECTION(urb->UrbControlTransferEx.TransferFlags))
        {
            lSize += OldSize;
            pWorkSave->lSecSizeBuffer = OldSize;
        }
        break;

    case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
        lSize += urb->UrbControlDescriptorRequest.TransferBufferLength;
        pWorkSave->lSecSizeBuffer = urb->UrbControlDescriptorRequest.TransferBufferLength;
        break;

    case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
    case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
    case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
    case URB_FUNCTION_GET_STATUS_FROM_OTHER:
        lSize += urb->UrbControlGetStatusRequest.TransferBufferLength;
        pWorkSave->lSecSizeBuffer = urb->UrbControlGetStatusRequest.TransferBufferLength;
        break;

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
            lSize += urb->UrbControlVendorClassRequest.TransferBufferLength;
            pWorkSave->lSecSizeBuffer = urb->UrbControlVendorClassRequest.TransferBufferLength;
        }
		pWorkSave->pPdo->GetPipeType ( (ULONG64)(ULONG_PTR (urb->UrbBulkOrInterruptTransfer.PipeHandle)), NULL);
		break;
    case URB_FUNCTION_GET_CONFIGURATION:
        lSize = urb->UrbControlGetConfigurationRequest.TransferBufferLength;
        pWorkSave->lSecSizeBuffer = urb->UrbControlGetConfigurationRequest.TransferBufferLength;
        break;
    case URB_FUNCTION_GET_INTERFACE:
        lSize = urb->UrbControlGetInterfaceRequest.TransferBufferLength;
        pWorkSave->lSecSizeBuffer = urb->UrbControlGetInterfaceRequest.TransferBufferLength;
    case URB_FUNCTION_SELECT_CONFIGURATION:
        pWorkSave->pPdo->m_ConfigurationHandle = urb->UrbSelectConfiguration.ConfigurationHandle;
        pWorkSave->pPdo->InitInterfaceInfo (&urb->UrbSelectConfiguration.Interface, 
                                            urb->UrbSelectConfiguration.ConfigurationDescriptor?urb->UrbSelectConfiguration.ConfigurationDescriptor->bNumInterfaces:0);
        lSize += pWorkSave->lSecSizeBuffer;
        break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        lSize += urb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        pWorkSave->lSecSizeBuffer = urb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        break;
    case URB_FUNCTION_SELECT_INTERFACE:
        pWorkSave->pPdo->SetNewInterface (&urb->UrbSelectInterface.Interface);
    default:
        lSize += pWorkSave->lSecSizeBuffer;
    }

    DebugPrint (DebugInfo, "Urb size - %d buffer- %d", urb->UrbHeader.Length, pWorkSave->lSecSizeBuffer);

	return lSize;
}

void CUsbPortToTcp::OutUrbCopyBufferIsochIn (PURB_FULL pUrb, BYTE *pInBuffer, BYTE *pOutBuffer)
{
	ULONG iOffset = 0;
	ULONG uSize;

	DebugPrint (DebugTrace, "OutUrbCopyBufferIsochIn");


	for (ULONG a = 0; a < pUrb->UrbIsochronousTransfer.NumberOfPackets; a++)
	{
		if (NT_SUCCESS (pUrb->UrbIsochronousTransfer.IsoPacket [a].Status))
		{
			RtlCopyMemory ( pInBuffer + iOffset, pOutBuffer + pUrb->UrbIsochronousTransfer.IsoPacket [a].Offset, pUrb->UrbIsochronousTransfer.IsoPacket [a].Length);
			iOffset += pUrb->UrbIsochronousTransfer.IsoPacket [a].Length;
		}
	}
}

void CUsbPortToTcp::OutUrbFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer)
{
    PURB_FULL    urb;
    BYTE        *pTemp;

    DebugPrint (DebugTrace, "UrbFillBuffer irp");
    DebugPrint (DebugInfo, "%s", CDebugPrintUrb::UrbFunctionString (pWorkSave->lOther));

    if (pWorkSave->IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        OutUsbFillBuffer (pWorkSave, pOutBuffer);
        return;
    }    // Create and fill in IRP

    if ((pWorkSave->Is64?true:false) != m_bIs64)
    {
        OutUrbFillBufferDifferent (pWorkSave, pOutBuffer);
        return;
    }

    urb = (PURB_FULL)pWorkSave->pAllocBuffer;

    // Copy Urb data
    RtlCopyMemory (pOutBuffer, pWorkSave->pAllocBuffer, urb->UrbHeader.Length);
    pTemp = pOutBuffer + urb->UrbHeader.Length;

	// Copy Data
	if (pWorkSave->pSecondBuffer && pWorkSave->lSecSizeBuffer)
	{
		if (urb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
		{
			OutUrbCopyBufferIsochIn (urb, pTemp, (BYTE*)pWorkSave->pSecondBuffer);
		}
		else
		{
			RtlCopyMemory (pTemp, pWorkSave->pSecondBuffer, pWorkSave->lSecSizeBuffer);
		}
	}
}

// ****************************************************************
//            URB different Size
// ****************************************************************
ULONG CUsbPortToTcp::CalcSizeUrbToDiffUrb (PURB_FULL Urb)
{
    LONG uSize;

    DebugPrint (DebugTrace, "CalcSizeUrbToDiffUrb");
    DebugPrint (DebugSpecial, "%s", CDebugPrintUrb::UrbFunctionString (Urb->UrbHeader.Function));

    uSize = Urb->UrbHeader.Length;
    switch (Urb->UrbHeader.Function)
    {
    case URB_FUNCTION_SELECT_CONFIGURATION:
        uSize += sizeof (_URB_SELECT_CONFIGURATION_DIFF) - sizeof (_URB_SELECT_CONFIGURATION);
        {
            PUSBD_INTERFACE_INFORMATION Interface = &Urb->UrbSelectConfiguration.Interface;
            PUSB_CONFIGURATION_DESCRIPTOR Descriptor = (PUSB_CONFIGURATION_DESCRIPTOR)Urb->UrbSelectConfiguration.ConfigurationDescriptor;

            // dec dif struct
            uSize -= sizeof (_USBD_INTERFACE_INFORMATION_DIFF) - sizeof (_USBD_INTERFACE_INFORMATION);

            if (Urb->UrbSelectConfiguration.ConfigurationDescriptor)
            {
                for (int a = 0; a < Urb->UrbSelectConfiguration.ConfigurationDescriptor->bNumInterfaces; a++)
                {
                    uSize += sizeof (_USBD_INTERFACE_INFORMATION_DIFF) - sizeof (_USBD_INTERFACE_INFORMATION);
                    uSize += (sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION)) * (Interface->NumberOfPipes - 1);
                    Interface = (PUSBD_INTERFACE_INFORMATION)((BYTE*)Interface + Interface->Length);
                }
            }
        }
        break;
    case URB_FUNCTION_SELECT_INTERFACE:
        {
            uSize += (sizeof (_URB_SELECT_INTERFACE_DIFF) - sizeof (_USBD_PIPE_INFORMATION_DIFF)) - (sizeof (_URB_SELECT_INTERFACE) - sizeof (_USBD_PIPE_INFORMATION));
            // count additional pipe into interface
            int iCountPipe = (Urb->UrbSelectInterface.Interface.Length - sizeof (_USBD_INTERFACE_INFORMATION) + sizeof (_USBD_PIPE_INFORMATION))/(sizeof (_USBD_PIPE_INFORMATION));
            uSize += (sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION)) * max (iCountPipe, 0);
        }
        break;
    case URB_FUNCTION_ABORT_PIPE:
    case URB_FUNCTION_RESET_PIPE:
    case URB_FUNCTION_SYNC_RESET_PIPE:
    case URB_FUNCTION_SYNC_CLEAR_STALL:
        uSize += sizeof (_URB_PIPE_REQUEST_DIFF) - sizeof (_URB_PIPE_REQUEST);
        break;
    case URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL:
    case URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL:
        uSize += sizeof (_URB_FRAME_LENGTH_CONTROL_DIFF) - sizeof (_URB_FRAME_LENGTH_CONTROL);
        break;
    case URB_FUNCTION_GET_FRAME_LENGTH:
        uSize += sizeof (_URB_GET_FRAME_LENGTH_DIFF) - sizeof (_URB_GET_FRAME_LENGTH);
        break;
    case URB_FUNCTION_SET_FRAME_LENGTH:
        uSize += sizeof (_URB_SET_FRAME_LENGTH_DIFF) - sizeof (_URB_SET_FRAME_LENGTH);
        break;
    case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
        uSize += sizeof (_URB_GET_CURRENT_FRAME_NUMBER_DIFF) - sizeof (_URB_GET_CURRENT_FRAME_NUMBER);
        break;
    case URB_FUNCTION_CONTROL_TRANSFER:
        uSize += sizeof (_URB_CONTROL_TRANSFER_DIFF) - sizeof (_URB_CONTROL_TRANSFER);
        break;
    case URB_FUNCTION_CONTROL_TRANSFER_EX:
        uSize += sizeof (_URB_CONTROL_TRANSFER_EX_DIFF) - sizeof (_URB_CONTROL_TRANSFER_EX_FULL);
        break;
    case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER :
        uSize += sizeof (_URB_BULK_OR_INTERRUPT_TRANSFER_DIFF) - sizeof (_URB_BULK_OR_INTERRUPT_TRANSFER);
        break;
    case URB_FUNCTION_ISOCH_TRANSFER:
        uSize += sizeof (_URB_ISOCH_TRANSFER_DIFF) - sizeof (_URB_ISOCH_TRANSFER);
        break;
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
        uSize += sizeof (_URB_CONTROL_DESCRIPTOR_REQUEST_DIFF) - sizeof (_URB_CONTROL_DESCRIPTOR_REQUEST);
        break;
    case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
    case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_SET_FEATURE_TO_OTHER:
    case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
        uSize += sizeof (_URB_CONTROL_FEATURE_REQUEST_DIFF) - sizeof (_URB_CONTROL_FEATURE_REQUEST);
        break;
    case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
    case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
    case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
    case URB_FUNCTION_GET_STATUS_FROM_OTHER:
        uSize += sizeof (_URB_CONTROL_GET_STATUS_REQUEST_DIFF) - sizeof (_URB_CONTROL_GET_STATUS_REQUEST);
        break;
    case URB_FUNCTION_VENDOR_DEVICE:
    case URB_FUNCTION_VENDOR_INTERFACE:
    case URB_FUNCTION_VENDOR_ENDPOINT:
    case URB_FUNCTION_VENDOR_OTHER:
    case URB_FUNCTION_CLASS_DEVICE:
    case URB_FUNCTION_CLASS_INTERFACE:
    case URB_FUNCTION_CLASS_ENDPOINT:
    case URB_FUNCTION_CLASS_OTHER :
        uSize += sizeof (_URB_CONTROL_VENDOR_OR_CLASS_REQUEST_DIFF) - sizeof (_URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
        break;
    case URB_FUNCTION_GET_CONFIGURATION :
        uSize += sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST_DIFF) - sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST);
        break;
    case URB_FUNCTION_GET_INTERFACE:
        uSize += sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST_DIFF) - sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST);
        break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        uSize += sizeof (_URB_OS_FEATURE_DESCRIPTOR_REQUEST_DIFF) - sizeof (_URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL);
        break;
    case URB_FUNCTION_SET_PIPE_IO_POLICY:
    case URB_FUNCTION_GET_PIPE_IO_POLICY:
        uSize += sizeof (_URB_PIPE_IO_POLICY_DIFF) - sizeof (_URB_PIPE_IO_POLICY_FULL);
        break;
    default:
        DebugPrint (DebugWarning, "CalcSizeUrbToDiffUrb - Did not found URB_FUNCTION");
        break;
    };

    DebugPrint (DebugInfo, "CalcSizeUrbToDiffUrb Cur - %d, new - %d", Urb->UrbHeader.Length, uSize);
    return uSize;
}

ULONG CUsbPortToTcp::CalcSizeDiffUrbToUrb (PURB_DIFF Urb, PVOID SecondBuf)
{
    ULONG uSize;

    DebugPrint (DebugTrace, "CalcSizeDiffUrbToUrb");
    DebugPrint (DebugSpecial, "%s", CDebugPrintUrb::UrbFunctionString (Urb->UrbHeader.Function));

    uSize = Urb->UrbHeader.Length;
    switch (Urb->UrbHeader.Function)
    {
    case URB_FUNCTION_SELECT_CONFIGURATION:
        uSize -= sizeof (_URB_SELECT_CONFIGURATION_DIFF) - sizeof (_URB_SELECT_CONFIGURATION);
        {
            PUSBD_INTERFACE_INFORMATION_DIFF Interface = &Urb->UrbSelectConfiguration.Interface;
            PUSB_CONFIGURATION_DESCRIPTOR_DIFF Descriptor = (PUSB_CONFIGURATION_DESCRIPTOR_DIFF)SecondBuf;

            // dec dif struct
            uSize += sizeof (_USBD_INTERFACE_INFORMATION_DIFF) - sizeof (_USBD_INTERFACE_INFORMATION);

            if (Urb->UrbSelectConfiguration.ConfigurationDescriptor)
            {
                for (int a = 0; a < Descriptor->bNumInterfaces; a++)
                {
                    uSize -= sizeof (_USBD_INTERFACE_INFORMATION_DIFF) - sizeof (_USBD_INTERFACE_INFORMATION);
                    uSize -= (sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION)) * (max (Interface->NumberOfPipes - 1, 0));
                    Interface = (PUSBD_INTERFACE_INFORMATION_DIFF)((BYTE*)Interface + Interface->Length);
                }
            }
        }
        break;
    case URB_FUNCTION_SELECT_INTERFACE:
        {
            uSize -= (sizeof (_URB_SELECT_INTERFACE_DIFF) - sizeof (_USBD_PIPE_INFORMATION_DIFF)) - (sizeof (_URB_SELECT_INTERFACE) - sizeof (_USBD_PIPE_INFORMATION));
            // count additional pipe into interface
            int iCountPipe = (Urb->UrbSelectInterface.Interface.Length - sizeof (_USBD_INTERFACE_INFORMATION_DIFF) + sizeof (_USBD_PIPE_INFORMATION_DIFF))/(sizeof (_USBD_PIPE_INFORMATION_DIFF));
            uSize -= (sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION)) * max (iCountPipe, 0);
        }
        break;
    case URB_FUNCTION_ABORT_PIPE:
    case URB_FUNCTION_RESET_PIPE:
    case URB_FUNCTION_SYNC_RESET_PIPE:
    case URB_FUNCTION_SYNC_CLEAR_STALL:
        uSize -= sizeof (_URB_PIPE_REQUEST_DIFF) - sizeof (_URB_PIPE_REQUEST);
        break;
    case URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL:
    case URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL:
        uSize -= sizeof (_URB_FRAME_LENGTH_CONTROL_DIFF) - sizeof (_URB_FRAME_LENGTH_CONTROL);
        break;
    case URB_FUNCTION_GET_FRAME_LENGTH:
        uSize -= sizeof (_URB_GET_FRAME_LENGTH_DIFF) - sizeof (_URB_GET_FRAME_LENGTH);
        break;
    case URB_FUNCTION_SET_FRAME_LENGTH:
        uSize -= sizeof (_URB_SET_FRAME_LENGTH_DIFF) - sizeof (_URB_SET_FRAME_LENGTH);
        break;
    case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
        uSize -= sizeof (_URB_GET_CURRENT_FRAME_NUMBER_DIFF) - sizeof (_URB_GET_CURRENT_FRAME_NUMBER);
        break;
    case URB_FUNCTION_CONTROL_TRANSFER:
        uSize -= sizeof (_URB_CONTROL_TRANSFER_DIFF) - sizeof (_URB_CONTROL_TRANSFER);
        break;
    case URB_FUNCTION_CONTROL_TRANSFER_EX:
        uSize -= sizeof (_URB_CONTROL_TRANSFER_EX_DIFF) - sizeof (_URB_CONTROL_TRANSFER_EX_FULL);
        break;
    case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER :
        uSize -= sizeof (_URB_BULK_OR_INTERRUPT_TRANSFER_DIFF) - sizeof (_URB_BULK_OR_INTERRUPT_TRANSFER);
        break;
    case URB_FUNCTION_ISOCH_TRANSFER:
        uSize -= sizeof (_URB_BULK_OR_INTERRUPT_TRANSFER_DIFF) - sizeof (_URB_BULK_OR_INTERRUPT_TRANSFER);
        break;

    case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
        uSize -= sizeof (_URB_CONTROL_DESCRIPTOR_REQUEST_DIFF) - sizeof (_URB_CONTROL_DESCRIPTOR_REQUEST);
        break;
    case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
    case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_SET_FEATURE_TO_OTHER:
    case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
        uSize -= sizeof (_URB_CONTROL_FEATURE_REQUEST_DIFF) - sizeof (_URB_CONTROL_FEATURE_REQUEST);
        break;
    case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
    case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
    case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
    case URB_FUNCTION_GET_STATUS_FROM_OTHER:
        uSize -= sizeof (_URB_CONTROL_GET_STATUS_REQUEST_DIFF) - sizeof (_URB_CONTROL_GET_STATUS_REQUEST);
        break;
    case URB_FUNCTION_VENDOR_DEVICE:
    case URB_FUNCTION_VENDOR_INTERFACE:
    case URB_FUNCTION_VENDOR_ENDPOINT:
    case URB_FUNCTION_VENDOR_OTHER:
    case URB_FUNCTION_CLASS_DEVICE:
    case URB_FUNCTION_CLASS_INTERFACE:
    case URB_FUNCTION_CLASS_ENDPOINT:
    case URB_FUNCTION_CLASS_OTHER :
        uSize -= sizeof (_URB_CONTROL_VENDOR_OR_CLASS_REQUEST_DIFF) - sizeof (_URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
        break;
    case URB_FUNCTION_GET_CONFIGURATION :
        uSize -= sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST_DIFF) - sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST);
        break;
    case URB_FUNCTION_GET_INTERFACE:
        uSize -= sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST_DIFF) - sizeof (_URB_CONTROL_GET_CONFIGURATION_REQUEST);
        break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        uSize -= sizeof (_URB_OS_FEATURE_DESCRIPTOR_REQUEST_DIFF) - sizeof (_URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL);
        break;
    case URB_FUNCTION_SET_PIPE_IO_POLICY:
    case URB_FUNCTION_GET_PIPE_IO_POLICY:
        uSize -= sizeof (_URB_PIPE_IO_POLICY_DIFF) - sizeof (_URB_PIPE_IO_POLICY_FULL);
        break;
    default:
        DebugPrint (DebugWarning, "CalcSizeDiffUrbToUrb - Did not found URB_FUNCTION");
        break;
    };

    DebugPrint (DebugInfo, "CalcSizeDiffUrbToUrb Cur - %d, new - %d", Urb->UrbHeader.Length, uSize);
    return uSize;
}

// *******************************************************************
//            URB Interface filling in if cross-platform
// *******************************************************************
void CUsbPortToTcp::OutInterfaceUrbToUrbDiff (PUSBD_INTERFACE_INFORMATION Interface, USHORT uCount, PUSBD_INTERFACE_INFORMATION_DIFF InterfaceDeff)
{
    DebugPrint (DebugTrace, "InterfaceUrbToUrbDiff");
    for (USHORT a = 0; a < uCount; a++)
    {
        InterfaceDeff->Length            = Interface->Length;
        // Interface
        InterfaceDeff->Length += (USHORT) (sizeof (_USBD_INTERFACE_INFORMATION_DIFF) - sizeof (_USBD_INTERFACE_INFORMATION));
        InterfaceDeff->Length -= (USHORT) (sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION));
        // Pipe
        InterfaceDeff->Length += (USHORT) ((sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION)) * Interface->NumberOfPipes);

        InterfaceDeff->InterfaceNumber  = Interface->InterfaceNumber;
        InterfaceDeff->AlternateSetting = Interface->AlternateSetting;
        InterfaceDeff->Class            = Interface->Class;
        InterfaceDeff->SubClass         = Interface->SubClass;
        InterfaceDeff->Protocol         = Interface->Protocol;
        InterfaceDeff->InterfaceHandle  = (USBD_PIPE_HANDLE_DIFF)((ULONG_PTR)Interface->InterfaceHandle);
        InterfaceDeff->Reserved         = Interface->Reserved;
        InterfaceDeff->NumberOfPipes    = Interface->NumberOfPipes;

        for (ULONG b = 0; b < InterfaceDeff->NumberOfPipes; b++)
        {
            InterfaceDeff->Pipes [b].MaximumPacketSize        = Interface->Pipes [b].MaximumPacketSize;
            InterfaceDeff->Pipes [b].MaximumTransferSize      = Interface->Pipes [b].MaximumTransferSize;
            InterfaceDeff->Pipes [b].PipeType                 = USBD_PIPE_TYPE_DEFF((ULONG_PTR)Interface->Pipes [b].PipeType);
            InterfaceDeff->Pipes [b].PipeHandle               = USBD_PIPE_HANDLE_DIFF ((ULONG_PTR)Interface->Pipes [b].PipeHandle);
            InterfaceDeff->Pipes [b].PipeFlags                = Interface->Pipes [b].PipeFlags;
            InterfaceDeff->Pipes [b].EndpointAddress          = Interface->Pipes [b].EndpointAddress;
            InterfaceDeff->Pipes [b].Interval                 = Interface->Pipes [b].Interval;

            DebugPrint (DebugSpecial, "PipeHandle 0x%x 0x%x", InterfaceDeff->Pipes [b].PipeHandle, USBD_PIPE_HANDLE_DIFF ((ULONG_PTR)Interface->Pipes [b].PipeHandle));
            DebugPrint (DebugInfo, "PipeHandle 0x%x", InterfaceDeff->Pipes [b].PipeHandle);
        }
        // get next
        InterfaceDeff = (PUSBD_INTERFACE_INFORMATION_DIFF)((BYTE*)InterfaceDeff + InterfaceDeff->Length);
        Interface = (PUSBD_INTERFACE_INFORMATION)((BYTE*)Interface + Interface->Length);
    }
}

void CUsbPortToTcp::InInterfaceUrbDiffToUrb (PUSBD_INTERFACE_INFORMATION_DIFF InterfaceDeff, USHORT uCount, PUSBD_INTERFACE_INFORMATION Interface)
{
    int iCountPipe = 0;
    DebugPrint (DebugTrace, "InterfaceUrbDiffToUrb");
    for (USHORT a = 0; a < uCount; a++)
    {
        Interface->Length             = InterfaceDeff->Length;

        // Interface
        iCountPipe = (InterfaceDeff->Length - sizeof (_USBD_INTERFACE_INFORMATION_DIFF) + sizeof (_USBD_PIPE_INFORMATION_DIFF))/(sizeof (_USBD_PIPE_INFORMATION_DIFF));

        Interface->Length -= (USHORT)(sizeof (_USBD_INTERFACE_INFORMATION_DIFF) - sizeof (_USBD_INTERFACE_INFORMATION));

        // Pipe
        Interface->Length -= (USHORT)((sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION)) * (iCountPipe - 1));

        Interface->InterfaceNumber  = InterfaceDeff->InterfaceNumber;
        Interface->AlternateSetting = InterfaceDeff->AlternateSetting;

        Interface->NumberOfPipes = InterfaceDeff->NumberOfPipes;

        for (int b = 0; b < iCountPipe; b++)
        {
			Interface->Pipes [b].MaximumPacketSize        = InterfaceDeff->Pipes [b].MaximumPacketSize;
            Interface->Pipes [b].MaximumTransferSize      = InterfaceDeff->Pipes [b].MaximumTransferSize;
            Interface->Pipes [b].PipeFlags                = InterfaceDeff->Pipes [b].PipeFlags;
        }

        // get next
        InterfaceDeff = (PUSBD_INTERFACE_INFORMATION_DIFF)((BYTE*)InterfaceDeff + InterfaceDeff->Length);
        Interface = (PUSBD_INTERFACE_INFORMATION)((BYTE*)Interface + Interface->Length);
    }
}

void CUsbPortToTcp::InInterfaceSelect (PUSBD_INTERFACE_INFORMATION_DIFF InterfaceDeff, PUSBD_INTERFACE_INFORMATION Interface, CPdoStubExtension* pPdo)
{
    PUSBD_INTERFACE_INFORMATION pInterfaceExist;
    int iCount = 0;
    int iSize;
    int iCountPipe;

    DebugPrint (DebugTrace, "InterfaceSelect");

    pPdo->m_spinPdo.Lock ();
    for (iCount = 0; iCount < pPdo->m_listInterface.GetCount (); iCount++)
    {
        pInterfaceExist = (PUSBD_INTERFACE_INFORMATION)pPdo->m_listInterface.GetItem (iCount);
        if (pInterfaceExist->InterfaceNumber == InterfaceDeff->InterfaceNumber)
        {
            iSize = InterfaceDeff->Length;
            iCountPipe = (InterfaceDeff->Length - sizeof (_USBD_INTERFACE_INFORMATION_DIFF) + sizeof (_USBD_PIPE_INFORMATION_DIFF))/(sizeof (_USBD_PIPE_INFORMATION_DIFF));
            iSize -= (int)(sizeof (_USBD_INTERFACE_INFORMATION_DIFF) - sizeof (_USBD_INTERFACE_INFORMATION));
            iSize -= (int)((sizeof (_USBD_PIPE_INFORMATION_DIFF) - sizeof (_USBD_PIPE_INFORMATION)) * (iCountPipe - 1));
            {
                Interface->Length = (USHORT)iSize;

                Interface->InterfaceNumber = InterfaceDeff->InterfaceNumber;
                Interface->AlternateSetting = InterfaceDeff->AlternateSetting;


                for (int b = 0; b < iCountPipe; b++)
                {
                    Interface->Pipes [b].MaximumTransferSize      = InterfaceDeff->Pipes [b].MaximumTransferSize;
                }

                pPdo->m_spinPdo.UnLock ();
                return;
            }
            break;
        }
    }
    pPdo->m_spinPdo.UnLock ();

    if (iCount == pPdo->m_listInterface.GetCount ())
    {
        InInterfaceUrbDiffToUrb (InterfaceDeff, 1, Interface);
    }
}

// *******************************************************
//          IRP filling in if cross-platform
// *******************************************************

void CUsbPortToTcp::InUrbFillIrpDifferent (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave)
{
    PURB_FULL                urb;
    PURB_DIFF                urbOrigina;
    ULONG                    uSize;
    BYTE                    *pBufData;
    PMDL                    *ppMdlTemp = NULL;
    PVOID                    *ppBuffer = NULL;
    LONG                    lSizeBuffer = 0;
    BYTE                    *pTemp;
    PIO_STACK_LOCATION        irpStack;
    //bool                    bCopyDate = true;

    DebugPrint (DebugTrace, "UrbFillIrpDifferent");

    urbOrigina = ((PURB_DIFF)irpSave->Buffer);
    // Set data Buffer
    pBufData = irpSave->Buffer + urbOrigina->UrbHeader.Length;
    // Calc new URB size
    uSize = CalcSizeDiffUrbToUrb (urbOrigina, pBufData);
    urb = (PURB_FULL) m_BufferMain.AllocBuffer (uSize, NonPagedPool);
    RtlZeroMemory (urb, uSize);
    // Set new URB
    irpStack = IoGetNextIrpStackLocation( pWorkSave->pIrp );
    irpStack->Parameters.Others.Argument1 = urb;
    pWorkSave->lOther = urbOrigina->UrbHeader.Function;
    pWorkSave->pAllocBuffer = (BYTE*)urb;
    pWorkSave->lSizeBuffer = uSize;

    // Init Header
    urb->UrbHeader.Function = urbOrigina->UrbHeader.Function;
    urb->UrbHeader.Length = (USHORT)uSize;
    urb->UrbHeader.Status = urbOrigina->UrbHeader.Status;
    urb->UrbHeader.UsbdFlags = urbOrigina->UrbHeader.UsbdFlags;
    // Verify 
    switch (urb->UrbHeader.Function)
    {
    case URB_FUNCTION_SELECT_CONFIGURATION:
        urb->UrbSelectConfiguration.ConfigurationDescriptor = PUSB_CONFIGURATION_DESCRIPTOR (ULONG_PTR (urbOrigina->UrbSelectConfiguration.ConfigurationDescriptor));
        if (urbOrigina->UrbSelectConfiguration.ConfigurationDescriptor)
        {
            InInterfaceUrbDiffToUrb (&urbOrigina->UrbSelectConfiguration.Interface, PUSB_CONFIGURATION_DESCRIPTOR_DIFF(pBufData)->bNumInterfaces,&urb->UrbSelectConfiguration.Interface);
        }

        ppBuffer = (PVOID*)&urb->UrbSelectConfiguration.ConfigurationDescriptor;
        lSizeBuffer = irpSave->BufferSize - urbOrigina->UrbHeader.Length;
        break;
    case URB_FUNCTION_SELECT_INTERFACE:
        urb->UrbSelectInterface.ConfigurationHandle = pWorkSave->pPdo->m_ConfigurationHandle;//urbOrigina->UrbSelectInterface.ConfigurationHandle;
        InInterfaceSelect (&urbOrigina->UrbSelectInterface.Interface, &urb->UrbSelectInterface.Interface, pWorkSave->pPdo);
        break;
    case URB_FUNCTION_ABORT_PIPE:
    case URB_FUNCTION_RESET_PIPE:
		// clear query isoch out
		m_Isoch.ClearIsoch ();
    case URB_FUNCTION_SYNC_RESET_PIPE:
    case URB_FUNCTION_SYNC_CLEAR_STALL:
        urb->UrbPipeRequest.PipeHandle  = pWorkSave->pPdo->GetPipeHandle (urbOrigina->UrbPipeRequest.PipeHandle);
        break;
    case URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL:
    case URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL:
        break;
    case URB_FUNCTION_GET_FRAME_LENGTH:
        urb->UrbGetFrameLength.FrameLength = urbOrigina->UrbGetFrameLength.FrameLength;
        urb->UrbGetFrameLength.FrameNumber = urbOrigina->UrbGetFrameLength.FrameNumber;
        break;
    case URB_FUNCTION_SET_FRAME_LENGTH:
        urb->UrbSetFrameLength.FrameLengthDelta = urbOrigina->UrbSetFrameLength.FrameLengthDelta;
        break;
    case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
        urb->UrbGetCurrentFrameNumber.FrameNumber = urbOrigina->UrbGetCurrentFrameNumber.FrameNumber;
        break;
    case URB_FUNCTION_CONTROL_TRANSFER:
        urb->UrbControlTransfer.PipeHandle              = pWorkSave->pPdo->GetPipeHandle(urbOrigina->UrbControlTransfer.PipeHandle);
        urb->UrbControlTransfer.TransferFlags           = urbOrigina->UrbControlTransfer.TransferFlags;
        urb->UrbControlTransfer.TransferBufferLength    = urbOrigina->UrbControlTransfer.TransferBufferLength;
        urb->UrbControlTransfer.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbControlTransfer.TransferBufferMDL);
        urb->UrbControlTransfer.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbControlTransfer.TransferBuffer);

		RtlCopyMemory (urb->UrbControlTransfer.SetupPacket, urbOrigina->UrbControlTransfer.SetupPacket, 8 * sizeof (UCHAR));

        ppMdlTemp = &urb->UrbControlTransfer.TransferBufferMDL;
        ppBuffer = &urb->UrbControlTransfer.TransferBuffer;
        lSizeBuffer = urb->UrbControlTransfer.TransferBufferLength;
        break;
    case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

        urb->UrbBulkOrInterruptTransfer.PipeHandle              = pWorkSave->pPdo->GetPipeHandle (urbOrigina->UrbBulkOrInterruptTransfer.PipeHandle);
        urb->UrbBulkOrInterruptTransfer.TransferFlags           = urbOrigina->UrbBulkOrInterruptTransfer.TransferFlags;                // note: the direction bit will be set by USBD
        urb->UrbBulkOrInterruptTransfer.TransferBufferLength    = urbOrigina->UrbBulkOrInterruptTransfer.TransferBufferLength;

        urb->UrbBulkOrInterruptTransfer.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbBulkOrInterruptTransfer.TransferBufferMDL);
        urb->UrbBulkOrInterruptTransfer.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbBulkOrInterruptTransfer.TransferBuffer);
        lSizeBuffer = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        ppMdlTemp = &urb->UrbBulkOrInterruptTransfer.TransferBufferMDL;
        ppBuffer = &urb->UrbBulkOrInterruptTransfer.TransferBuffer;
        break;
    case URB_FUNCTION_ISOCH_TRANSFER:
        urb->UrbIsochronousTransfer.PipeHandle              = pWorkSave->pPdo->GetPipeHandle ((USBD_PIPE_HANDLE_DIFF)urbOrigina->UrbIsochronousTransfer.PipeHandle);
        urb->UrbIsochronousTransfer.TransferFlags           = urbOrigina->UrbIsochronousTransfer.TransferFlags;
        urb->UrbIsochronousTransfer.TransferBufferLength    = urbOrigina->UrbIsochronousTransfer.TransferBufferLength;
        urb->UrbIsochronousTransfer.StartFrame              = urbOrigina->UrbIsochronousTransfer.StartFrame;
        urb->UrbIsochronousTransfer.NumberOfPackets         = urbOrigina->UrbIsochronousTransfer.NumberOfPackets;
        urb->UrbIsochronousTransfer.ErrorCount              = urbOrigina->UrbIsochronousTransfer.ErrorCount;

        urb->UrbIsochronousTransfer.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbIsochronousTransfer.TransferBufferMDL);
        urb->UrbIsochronousTransfer.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbIsochronousTransfer.TransferBuffer);
        RtlCopyMemory (urb->UrbIsochronousTransfer.IsoPacket, urbOrigina->UrbIsochronousTransfer.IsoPacket, urb->UrbIsochronousTransfer.NumberOfPackets * sizeof (USBD_ISO_PACKET_DESCRIPTOR));

        lSizeBuffer = urb->UrbIsochronousTransfer.TransferBufferLength;
        ppMdlTemp = &urb->UrbIsochronousTransfer.TransferBufferMDL;
        ppBuffer = &urb->UrbIsochronousTransfer.TransferBuffer;
        if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
		{
			pWorkSave->Isoch = true;
		}

		if (urb->UrbIsochronousTransfer.StartFrame)
		{
			m_Isoch.BuildStartFrame(urb);
		}
        break;
    case URB_FUNCTION_CONTROL_TRANSFER_EX:

        urb->UrbControlTransferEx.PipeHandle                = pWorkSave->pPdo->GetPipeHandle ((USBD_PIPE_HANDLE_DIFF)urbOrigina->UrbControlTransferEx.PipeHandle);;
        urb->UrbControlTransferEx.TransferFlags             = urbOrigina->UrbControlTransferEx.TransferFlags;
        urb->UrbControlTransferEx.TransferBufferLength      = urbOrigina->UrbControlTransferEx.TransferBufferLength;
        urb->UrbControlTransferEx.Timeout                   = urbOrigina->UrbControlTransferEx.Timeout;                      // *optional* timeout in milliseconds

        urb->UrbControlTransferEx.TransferBufferMDL         = PMDL ((LONG_PTR)urbOrigina->UrbControlTransferEx.TransferBufferMDL);
        urb->UrbControlTransferEx.TransferBuffer            = PVOID ((LONG_PTR)urbOrigina->UrbControlTransferEx.TransferBuffer);
        #ifdef WIN64
        urb->UrbControlTransferEx.Pad                     =  urbOrigina->UrbControlTransferEx.Pad;
        #endif
        RtlCopyMemory (urb->UrbControlTransferEx.SetupPacket, urbOrigina->UrbControlTransferEx.SetupPacket, 8 * sizeof (UCHAR));

        lSizeBuffer = urb->UrbControlTransferEx.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlTransferEx.TransferBufferMDL;
        ppBuffer = &urb->UrbControlTransferEx.TransferBuffer;
        break;

    case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
//		pWorkSave->Single = true;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
        urb->UrbControlDescriptorRequest.TransferBufferLength   = urbOrigina->UrbControlDescriptorRequest.TransferBufferLength;
        
        urb->UrbControlDescriptorRequest.Index                  = urbOrigina->UrbControlDescriptorRequest.Index;
        urb->UrbControlDescriptorRequest.DescriptorType         = urbOrigina->UrbControlDescriptorRequest.DescriptorType;
        urb->UrbControlDescriptorRequest.LanguageId             = urbOrigina->UrbControlDescriptorRequest.LanguageId;
        urb->UrbControlDescriptorRequest.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbControlDescriptorRequest.TransferBufferMDL);
        urb->UrbControlDescriptorRequest.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbControlDescriptorRequest.TransferBuffer);

        lSizeBuffer = urb->UrbControlDescriptorRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlDescriptorRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlDescriptorRequest.TransferBuffer;
        break;
    case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
    case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_SET_FEATURE_TO_OTHER:
    case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
        urb->UrbControlFeatureRequest.FeatureSelector   = urbOrigina->UrbControlFeatureRequest.FeatureSelector;
        urb->UrbControlFeatureRequest.Index             = urbOrigina->UrbControlFeatureRequest.Index;                
        break;
    case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
    case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
    case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
    case URB_FUNCTION_GET_STATUS_FROM_OTHER:
        urb->UrbControlGetStatusRequest.TransferBufferLength    = urbOrigina->UrbControlGetStatusRequest.TransferBufferLength;
        urb->UrbControlGetStatusRequest.Index                   = urbOrigina->UrbControlGetStatusRequest.Index;
        urb->UrbControlGetStatusRequest.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbControlGetStatusRequest.TransferBufferMDL);
        urb->UrbControlGetStatusRequest.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbControlGetStatusRequest.TransferBuffer);

        lSizeBuffer = urb->UrbControlVendorClassRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlVendorClassRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlVendorClassRequest.TransferBuffer;
        break;
    case URB_FUNCTION_VENDOR_DEVICE:
    case URB_FUNCTION_VENDOR_INTERFACE:
    case URB_FUNCTION_VENDOR_ENDPOINT:
    case URB_FUNCTION_VENDOR_OTHER:
    case URB_FUNCTION_CLASS_DEVICE:
    case URB_FUNCTION_CLASS_INTERFACE:
    case URB_FUNCTION_CLASS_ENDPOINT:
    case URB_FUNCTION_CLASS_OTHER :
        urb->UrbControlVendorClassRequest.TransferFlags             = urbOrigina->UrbControlVendorClassRequest.TransferFlags;
        urb->UrbControlVendorClassRequest.TransferBufferLength      = urbOrigina->UrbControlVendorClassRequest.TransferBufferLength;
        urb->UrbControlVendorClassRequest.TransferBuffer            = PVOID ((LONG_PTR) urbOrigina->UrbControlVendorClassRequest.TransferBuffer);
        urb->UrbControlVendorClassRequest.TransferBufferMDL         = PMDL ((LONG_PTR) urbOrigina->UrbControlVendorClassRequest.TransferBufferMDL);             // *optional*
        urb->UrbControlVendorClassRequest.RequestTypeReservedBits   = urbOrigina->UrbControlVendorClassRequest.RequestTypeReservedBits;
        urb->UrbControlVendorClassRequest.Request                   = urbOrigina->UrbControlVendorClassRequest.Request;
        urb->UrbControlVendorClassRequest.Value                     = urbOrigina->UrbControlVendorClassRequest.Value;
        urb->UrbControlVendorClassRequest.Index                     = urbOrigina->UrbControlVendorClassRequest.Index;

        lSizeBuffer = urb->UrbControlVendorClassRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlVendorClassRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlVendorClassRequest.TransferBuffer;
        break;
    case URB_FUNCTION_GET_CONFIGURATION:
        urb->UrbControlGetConfigurationRequest.TransferBufferLength = urbOrigina->UrbControlGetConfigurationRequest.TransferBufferLength;
        
        urb->UrbControlGetConfigurationRequest.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbControlGetConfigurationRequest.TransferBufferMDL);
        urb->UrbControlGetConfigurationRequest.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbControlGetConfigurationRequest.TransferBuffer);
        
        lSizeBuffer = urb->UrbControlGetConfigurationRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlGetConfigurationRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlGetConfigurationRequest.TransferBuffer;
        break;
    case URB_FUNCTION_GET_INTERFACE:
        urb->UrbControlGetInterfaceRequest.TransferBufferLength  = urbOrigina->UrbControlGetInterfaceRequest.TransferBufferLength;

        urb->UrbControlGetInterfaceRequest.Interface             = urbOrigina->UrbControlGetInterfaceRequest.Interface;

        urb->UrbControlGetInterfaceRequest.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbControlGetInterfaceRequest.TransferBufferMDL);
        urb->UrbControlGetInterfaceRequest.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbControlGetInterfaceRequest.TransferBuffer);

        lSizeBuffer = urb->UrbControlGetInterfaceRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbControlGetInterfaceRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbControlGetInterfaceRequest.TransferBuffer;
        break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        urb->UrbOSFeatureDescriptorRequest.TransferBufferLength         = urbOrigina->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        urb->UrbOSFeatureDescriptorRequest.Recipient                    = urbOrigina->UrbOSFeatureDescriptorRequest.Recipient;
        urb->UrbOSFeatureDescriptorRequest.InterfaceNumber              = urbOrigina->UrbOSFeatureDescriptorRequest.InterfaceNumber;
        urb->UrbOSFeatureDescriptorRequest.MS_PageIndex                 = urbOrigina->UrbOSFeatureDescriptorRequest.MS_PageIndex;
        urb->UrbOSFeatureDescriptorRequest.MS_FeatureDescriptorIndex    = urbOrigina->UrbOSFeatureDescriptorRequest.MS_FeatureDescriptorIndex;

        urb->UrbOSFeatureDescriptorRequest.TransferBufferMDL       = PMDL ((LONG_PTR)urbOrigina->UrbOSFeatureDescriptorRequest.TransferBufferMDL);
        urb->UrbOSFeatureDescriptorRequest.TransferBuffer          = PVOID ((LONG_PTR)urbOrigina->UrbOSFeatureDescriptorRequest.TransferBuffer);
        lSizeBuffer = urb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        ppMdlTemp = &urb->UrbOSFeatureDescriptorRequest.TransferBufferMDL;
        ppBuffer = &urb->UrbOSFeatureDescriptorRequest.TransferBuffer;
        break;
    case URB_FUNCTION_SET_PIPE_IO_POLICY:
    case URB_FUNCTION_GET_PIPE_IO_POLICY:
        urb->UrbPipeIoPolicy.PipeHandle                                 = pWorkSave->pPdo->GetPipeHandle ((USBD_PIPE_HANDLE_DIFF)urbOrigina->UrbPipeIoPolicy.PipeHandle);
        urb->UrbPipeIoPolicy.UsbIoParamters.UsbIoPriority               = (USB_IO_PRIORITY_FULL)urbOrigina->UrbPipeIoPolicy.UsbIoParamters.UsbIoPriority;
        urb->UrbPipeIoPolicy.UsbIoParamters.UsbMaxControllerTransfer    = urbOrigina->UrbPipeIoPolicy.UsbIoParamters.UsbMaxControllerTransfer;
        urb->UrbPipeIoPolicy.UsbIoParamters.UsbMaxPendingIrps           = urbOrigina->UrbPipeIoPolicy.UsbIoParamters.UsbMaxPendingIrps;
        break;
    default:
        DebugPrint (DebugError, "UrbFillIrpDifferent: Did not find URB function");
        break;
    }

    if ((ppBuffer && *ppBuffer) || (ppMdlTemp && *ppMdlTemp))
    {
		(*ppBuffer) = (BYTE*)m_BufferSecond.AllocBuffer (lSizeBuffer + 0x10, NonPagedPool);
        if (lSizeBuffer <= irpSave->BufferSize - urbOrigina->UrbHeader.Length)
        {
            DebugPrint (DebugSpecial, "UrbFillIrpDifferent - %d", lSizeBuffer);
            RtlCopyMemory (*ppBuffer, pBufData, lSizeBuffer);
        }
        pWorkSave->pSecondBuffer = *ppBuffer;
        pWorkSave->lSecSizeBuffer = lSizeBuffer;
		if (ppMdlTemp)
		{
			(*ppMdlTemp) = NULL;
		}
    }
}

void CUsbPortToTcp::OutUrbFillBufferDifferent (PWORKING_IRP pWorkSave, BYTE *pOutBuffer)
{
    PURB_FULL   urb;
    PURB_DIFF   urbOrigina;
    PMDL        pMdlTemp = NULL;
    BYTE        *pTemp;
    USHORT      uSize;

    DebugPrint (DebugTrace, "UrbFillBufferDifferent irp");

    urb = (PURB_FULL)pWorkSave->pAllocBuffer;
    urbOrigina = (PURB_DIFF)pOutBuffer;

    // Copy Urb data
    uSize = (USHORT)CalcSizeUrbToDiffUrb (urb);
    RtlZeroMemory (urbOrigina, uSize);
    // Init Header
    urbOrigina->UrbHeader.Function = urb->UrbHeader.Function;
    urbOrigina->UrbHeader.Length = uSize;
    urbOrigina->UrbHeader.UsbdDeviceHandle = 0;
    urbOrigina->UrbHeader.Status = urb->UrbHeader.Status;
    urbOrigina->UrbHeader.UsbdFlags = urb->UrbHeader.UsbdFlags;
    // Verify 
    switch (urb->UrbHeader.Function)
    {
    case URB_FUNCTION_SELECT_CONFIGURATION:
        urbOrigina->UrbSelectConfiguration.ConfigurationHandle = USBD_CONFIGURATION_HANDLE_DIFF ((ULONG_PTR) urb->UrbSelectConfiguration.ConfigurationHandle);
        if (urb->UrbSelectConfiguration.ConfigurationDescriptor)
        {
            OutInterfaceUrbToUrbDiff (&urb->UrbSelectConfiguration.Interface, urb->UrbSelectConfiguration.ConfigurationDescriptor->bNumInterfaces,&urbOrigina->UrbSelectConfiguration.Interface);
        }
        break;
    case URB_FUNCTION_SELECT_INTERFACE:
        urbOrigina->UrbSelectInterface.ConfigurationHandle = USBD_CONFIGURATION_HANDLE_DIFF ((ULONG_PTR)pWorkSave->pPdo->m_ConfigurationHandle);//urbOrigina->UrbSelectInterface.ConfigurationHandle;
        OutInterfaceUrbToUrbDiff (&urb->UrbSelectInterface.Interface, 1, &urbOrigina->UrbSelectInterface.Interface);
        break;
    case URB_FUNCTION_ABORT_PIPE:
    case URB_FUNCTION_RESET_PIPE:
    case URB_FUNCTION_SYNC_RESET_PIPE:
    case URB_FUNCTION_SYNC_CLEAR_STALL:
        urbOrigina->UrbPipeRequest.PipeHandle  = pWorkSave->pPdo->GetPipeHandleDiff (urb->UrbPipeRequest.PipeHandle);
        break;
    case URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL:
    case URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL:
        break;
    case URB_FUNCTION_GET_FRAME_LENGTH:
        urbOrigina->UrbGetFrameLength.FrameLength = urb->UrbGetFrameLength.FrameLength;
        urbOrigina->UrbGetFrameLength.FrameNumber = urb->UrbGetFrameLength.FrameNumber;
        break;
    case URB_FUNCTION_SET_FRAME_LENGTH:
        urbOrigina->UrbSetFrameLength.FrameLengthDelta = urb->UrbSetFrameLength.FrameLengthDelta;
        break;
    case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
        urbOrigina->UrbGetCurrentFrameNumber.FrameNumber = urb->UrbGetCurrentFrameNumber.FrameNumber;
        break;
    case URB_FUNCTION_CONTROL_TRANSFER:
        urbOrigina->UrbControlTransfer.PipeHandle              = pWorkSave->pPdo->GetPipeHandleDiff(urb->UrbControlTransfer.PipeHandle);
        urbOrigina->UrbControlTransfer.TransferFlags           = urb->UrbControlTransfer.TransferFlags;
        urbOrigina->UrbControlTransfer.TransferBufferLength    = urb->UrbControlTransfer.TransferBufferLength;

		RtlCopyMemory (urbOrigina->UrbControlTransfer.SetupPacket, urb->UrbControlTransfer.SetupPacket, 8 * sizeof (UCHAR));
        break;
    case URB_FUNCTION_CONTROL_TRANSFER_EX:

        urbOrigina->UrbControlTransferEx.PipeHandle                = pWorkSave->pPdo->GetPipeHandleDiff ( (USBD_PIPE_HANDLE)urb->UrbControlTransferEx.PipeHandle);
        urbOrigina->UrbControlTransferEx.TransferFlags             = urb->UrbControlTransferEx.TransferFlags;
        urbOrigina->UrbControlTransferEx.TransferBufferLength      = urb->UrbControlTransferEx.TransferBufferLength;
        urbOrigina->UrbControlTransferEx.Timeout                   = urb->UrbControlTransferEx.Timeout;                      // *optional* timeout in milliseconds

        #ifdef WIN64
        urbOrigina->UrbControlTransferEx.Pad                        =  urb->UrbControlTransferEx.Pad;
        #endif
        RtlCopyMemory (urbOrigina->UrbControlTransferEx.SetupPacket, urb->UrbControlTransferEx.SetupPacket, 8 * sizeof (UCHAR));

        break;
    case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

        urbOrigina->UrbBulkOrInterruptTransfer.PipeHandle              = pWorkSave->pPdo->GetPipeHandleDiff (urb->UrbBulkOrInterruptTransfer.PipeHandle);
        urbOrigina->UrbBulkOrInterruptTransfer.TransferFlags           = urb->UrbBulkOrInterruptTransfer.TransferFlags;                // note: the direction bit will be set by USBD
        urbOrigina->UrbBulkOrInterruptTransfer.TransferBufferLength    = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        break;
    case URB_FUNCTION_ISOCH_TRANSFER:
        urbOrigina->UrbIsochronousTransfer.PipeHandle              = pWorkSave->pPdo->GetPipeHandleDiff (urb->UrbIsochronousTransfer.PipeHandle);
        urbOrigina->UrbIsochronousTransfer.TransferFlags           = urb->UrbIsochronousTransfer.TransferFlags;
        urbOrigina->UrbIsochronousTransfer.TransferBufferLength    = urb->UrbIsochronousTransfer.TransferBufferLength;
        urbOrigina->UrbIsochronousTransfer.StartFrame              = urb->UrbIsochronousTransfer.StartFrame;
        urbOrigina->UrbIsochronousTransfer.NumberOfPackets         = urb->UrbIsochronousTransfer.NumberOfPackets;
        urbOrigina->UrbIsochronousTransfer.ErrorCount              = urb->UrbIsochronousTransfer.ErrorCount;

        RtlCopyMemory (urbOrigina->UrbIsochronousTransfer.IsoPacket, urb->UrbIsochronousTransfer.IsoPacket, urb->UrbIsochronousTransfer.NumberOfPackets * sizeof (USBD_ISO_PACKET_DESCRIPTOR));
        break;
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
    case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
    case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE:
        urbOrigina->UrbControlDescriptorRequest.TransferBufferLength   = urb->UrbControlDescriptorRequest.TransferBufferLength;
        RtlZeroMemory (&urbOrigina->UrbControlDescriptorRequest.hca, sizeof (_URB_HCD_AREA));
        
        urbOrigina->UrbControlDescriptorRequest.Index                  = urb->UrbControlDescriptorRequest.Index;
        urbOrigina->UrbControlDescriptorRequest.DescriptorType         = urb->UrbControlDescriptorRequest.DescriptorType;
        urbOrigina->UrbControlDescriptorRequest.LanguageId             = urb->UrbControlDescriptorRequest.LanguageId;
        break;
    case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
    case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_SET_FEATURE_TO_OTHER:
    case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
    case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
    case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
        urbOrigina->UrbControlFeatureRequest.FeatureSelector   = urb->UrbControlFeatureRequest.FeatureSelector;
        urbOrigina->UrbControlFeatureRequest.Index             = urb->UrbControlFeatureRequest.Index;                
        break;
    case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
    case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
    case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
    case URB_FUNCTION_GET_STATUS_FROM_OTHER:
        urbOrigina->UrbControlGetStatusRequest.TransferBufferLength    = urb->UrbControlGetStatusRequest.TransferBufferLength;
        urbOrigina->UrbControlGetStatusRequest.Index                   = urb->UrbControlGetStatusRequest.Index;
        break;
    case URB_FUNCTION_VENDOR_DEVICE:
    case URB_FUNCTION_VENDOR_INTERFACE:
    case URB_FUNCTION_VENDOR_ENDPOINT:
    case URB_FUNCTION_VENDOR_OTHER:
    case URB_FUNCTION_CLASS_DEVICE:
    case URB_FUNCTION_CLASS_INTERFACE:
    case URB_FUNCTION_CLASS_ENDPOINT:
    case URB_FUNCTION_CLASS_OTHER:
        urbOrigina->UrbControlVendorClassRequest.TransferFlags              = urb->UrbControlVendorClassRequest.TransferFlags;
        urbOrigina->UrbControlVendorClassRequest.TransferBufferLength       = urb->UrbControlVendorClassRequest.TransferBufferLength;
        urbOrigina->UrbControlVendorClassRequest.RequestTypeReservedBits    = urb->UrbControlVendorClassRequest.RequestTypeReservedBits;
        urbOrigina->UrbControlVendorClassRequest.Request                    = urb->UrbControlVendorClassRequest.Request;
        urbOrigina->UrbControlVendorClassRequest.Value                      = urb->UrbControlVendorClassRequest.Value;
        urbOrigina->UrbControlVendorClassRequest.Index                      = urb->UrbControlVendorClassRequest.Index;
        break;
    case URB_FUNCTION_GET_CONFIGURATION:
        urbOrigina->UrbControlGetConfigurationRequest.TransferBufferLength  = urb->UrbControlGetConfigurationRequest.TransferBufferLength;
        break;
    case URB_FUNCTION_GET_INTERFACE:
        urbOrigina->UrbControlGetInterfaceRequest.TransferBufferLength  = urb->UrbControlGetInterfaceRequest.TransferBufferLength;
        urbOrigina->UrbControlGetInterfaceRequest.Interface             = urb->UrbControlGetInterfaceRequest.Interface;
        break;
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        urbOrigina->UrbOSFeatureDescriptorRequest.TransferBufferLength         = urb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        urbOrigina->UrbOSFeatureDescriptorRequest.Recipient                    = urb->UrbOSFeatureDescriptorRequest.Recipient;
        urbOrigina->UrbOSFeatureDescriptorRequest.InterfaceNumber              = urb->UrbOSFeatureDescriptorRequest.InterfaceNumber;
        urbOrigina->UrbOSFeatureDescriptorRequest.MS_PageIndex                 = urb->UrbOSFeatureDescriptorRequest.MS_PageIndex;
        urbOrigina->UrbOSFeatureDescriptorRequest.MS_FeatureDescriptorIndex    = urb->UrbOSFeatureDescriptorRequest.MS_FeatureDescriptorIndex;
        break;
    case URB_FUNCTION_SET_PIPE_IO_POLICY:
    case URB_FUNCTION_GET_PIPE_IO_POLICY:
        urbOrigina->UrbPipeIoPolicy.PipeHandle                                 = pWorkSave->pPdo->GetPipeHandleDiff (urb->UrbPipeIoPolicy.PipeHandle);
        urbOrigina->UrbPipeIoPolicy.UsbIoParamters.UsbIoPriority               = (USB_IO_PRIORITY_DIFF)urb->UrbPipeIoPolicy.UsbIoParamters.UsbIoPriority;
        urbOrigina->UrbPipeIoPolicy.UsbIoParamters.UsbMaxControllerTransfer    = urb->UrbPipeIoPolicy.UsbIoParamters.UsbMaxControllerTransfer;
        urbOrigina->UrbPipeIoPolicy.UsbIoParamters.UsbMaxPendingIrps           = urb->UrbPipeIoPolicy.UsbIoParamters.UsbMaxPendingIrps;
        break;

    default:
        DebugPrint (DebugError, "UrbFillIrpDifferent: Did not find URB function");
        break;
    }


    pTemp = pOutBuffer + urbOrigina->UrbHeader.Length;
	// copy data
	if (pWorkSave->pSecondBuffer && pWorkSave->lSecSizeBuffer)
	{
		// Copy Data
		if (pWorkSave->pSecondBuffer)
		{
			if (urb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
			{
				OutUrbCopyBufferIsochIn(urb, pTemp, (BYTE*)pWorkSave->pSecondBuffer);
			}
			else
			{
				RtlCopyMemory(pTemp, pWorkSave->pSecondBuffer, pWorkSave->lSecSizeBuffer);
			}
		}
	}
}

void CUsbPortToTcp::IoCompletionIrpThread (PVOID  Context)
{
    PWORKING_IRP            pWorkSave = (PWORKING_IRP)Context;
    TPtrUsbPortToTcp        pBase = pWorkSave->pBase;
    PIRP                    irpWait = NULL;
    CPdoStubExtension       *pPdoStub;

    pBase->DebugPrint (DebugTrace, "IoCompletionIrpThread - 0x%x", pWorkSave->pIrp);

    if (pWorkSave->Cancel)
    {
		#ifdef ISOCH_FAST_MODE
			if (pWorkSave->Isoch)
			{
				IsochStartNew ((PISOCH)pWorkSave->pAddition);
				IsochDeleteSave (pWorkSave);
			}
		#endif
        // Complete device
        DeleteIrp (pWorkSave);
        return;
    }

    m_spinBase.Lock ('CIT');
    pPdoStub = pWorkSave->pPdo;
    if (!pBase->m_bIsPresent)
    {
		m_spinBase.UnLock ();

        pBase->DeleteIrp (pWorkSave);
        return;
    }

	#ifdef ISOCH_FAST_MODE
		if (IsochVerifyDel (pWorkSave))
		{
			return;
		}
	#endif

	m_spinUsbOut.Lock();
	pBase->m_listIrpAnswer.Add (pWorkSave);
	m_spinUsbOut.UnLock();
    if (pBase->m_listIrpControlAnswer.GetCount () && !m_bWorkIrp)
    {
		m_bWorkIrp = true;
        irpWait = (PIRP) pBase->m_listIrpControlAnswer.PopFirst();
    }
	m_spinBase.UnLock ();

	if (irpWait)
    {
		pBase->OutFillAnswer (irpWait);
		m_spinBase.Lock ('CIT2');
		m_bWorkIrp = false;
		m_spinBase.UnLock ();

		// next answer
		pBase->NextAnswer ();
    }
}

void CUsbPortToTcp::NextAnswer ()
{
    PIRP                    irpWait = NULL;
    KIRQL                   OldIrql;

	DebugPrint (DebugTrace, "NextAnswer");
	DebugPrint (DebugSpecial, "NextAnswer");

    m_spinBase.Lock ('NA');
    if (m_listIrpControlAnswer.GetCount () && m_listIrpAnswer.GetCount () && !m_bWorkIrp)
    {
		m_bWorkIrp = true;
        irpWait = (PIRP)m_listIrpControlAnswer.PopFirst();
		DebugPrint (DebugSpecial, "NextAnswer %x", irpWait);
    }
	m_spinBase.UnLock ();

	if (irpWait)
    {
		OutFillAnswer (irpWait);
		m_spinBase.Lock ('NA2');
		m_bWorkIrp = false;
		m_spinBase.UnLock ();

		DebugPrint (DebugSpecial, "NextAnswer Recur");
		// next answer
		NextAnswer ();
    }
}

bool CUsbPortToTcp::GetIrpToAnswerThread (PIRP Irp)
{
    CPdoStubExtension    *pPdoStub;
	bool bRet;

    DebugPrint (DebugTrace, "GetIrpToAnswerThread - 0x%x", Irp);

    m_spinBase.Lock ('IAT');
    if (m_listIrpAnswer.GetCount () == 0 || m_bWorkIrp)
    {
        m_listIrpControlAnswer.Add (Irp);
        DebugPrint (DebugInfo, "ControlAnswer- %d", m_listIrpControlAnswer.GetCount ());
		DebugPrint (DebugSpecial, "ControlAnswer- %d", m_listIrpControlAnswer.GetCount ());
        m_spinBase.UnLock ();
        return false;
    }
	m_bWorkIrp = true;
    m_spinBase.UnLock ();

	#ifdef ISOCH_FAST_MODE
		if (IsochVerifyDel (irpWait))
		{
			GetIrpToAnswerThread (Irp);
			return;
		}
	#endif

	bRet = OutFillAnswer (Irp);

	// free queue
	m_spinBase.Lock ('IAT2');
	m_bWorkIrp = false;
	m_spinBase.UnLock ();

	NextAnswer ();

	return bRet;
}

// ****************************************************************************
//                                interface
// ****************************************************************************
void CUsbPortToTcp::InInterfaceFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave)
{
    PIO_STACK_LOCATION      irpStack;
    PFUNC_INTERFACE         pInterface;

    irpStack = IoGetNextIrpStackLocation( pWorkSave->pIrp );
    // 11
    pInterface = (PFUNC_INTERFACE)m_BufferMain.AllocBuffer (sizeof (FUNC_INTERFACE), PagedPool);
    RtlCopyMemory (pInterface, irpSave->Buffer, sizeof (FUNC_INTERFACE));
    irpStack->Parameters.Others.Argument1 = pInterface;

	pWorkSave->pAllocBuffer = (BYTE*)pInterface;

    switch (pInterface->lNumFunc)
    {
    case FI_InterfaceReference:
        {
            if (pWorkSave->pPdo->m_Interface.InterfaceReference)
            {
                pWorkSave->pPdo->m_Interface.InterfaceReference (pWorkSave->pPdo->m_Interface.BusContext);
                ++pWorkSave->pPdo->m_IncInterface;

            }
        }
        break;
    case FI_InterfaceDereference:
        {
            if (pWorkSave->pPdo->m_Interface.InterfaceDereference)
            {
                pWorkSave->pPdo->m_Interface.InterfaceDereference (pWorkSave->pPdo->m_Interface.BusContext);
                --pWorkSave->pPdo->m_IncInterface;
            }
        }
        break;
    case FI_GetUSBDIVersion:
        {
            if (pWorkSave->pPdo->m_Interface.GetUSBDIVersion)
            {
                USBD_VERSION_INFORMATION    Info;
                ULONG                        lTemp;
                lTemp = ULONG ((ULONG_PTR)pInterface->Param3);
                Info.USBDI_Version = ULONG ((ULONG_PTR)pInterface->Param1);
                Info.Supported_USB_Version = ULONG ((ULONG_PTR)pInterface->Param2);
                pWorkSave->pPdo->m_Interface.GetUSBDIVersion (pWorkSave->pPdo->m_Interface.BusContext, &Info, &lTemp);
                pInterface->Param1 = Info.USBDI_Version;
                pInterface->Param2 = Info.Supported_USB_Version;
                pInterface->Param3 = lTemp;
            }
        }
        break;
    case FI_QueryBusTime:
        {
            if (pWorkSave->pPdo->m_Interface.QueryBusTime)
            {
                ULONG    CurrentFrame = ULONG ((ULONG_PTR)pInterface->Param1);

                pInterface->Param2 = pWorkSave->pPdo->m_Interface.QueryBusTime (pWorkSave->pPdo->m_Interface.BusContext, &CurrentFrame);
                pInterface->Param1 = CurrentFrame;
            }
        }
        break;
    case FI_QueryBusInformation:
        {
            if (pWorkSave->pPdo->m_Interface.QueryBusInformation)
            {
                ULONG BusInformationBufferLength = pInterface->Param3;
                ULONG BusInformationActualLength = pInterface->Param4;
                

                pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (BusInformationBufferLength, NonPagedPool);

                if (irpSave->BufferSize >= LONG(sizeof (FUNC_INTERFACE) + BusInformationBufferLength))
                {
                    RtlCopyMemory (pWorkSave->pAllocBuffer, irpSave->Buffer + sizeof (FUNC_INTERFACE), BusInformationBufferLength);
                }
                else
                {
                    RtlZeroMemory (pWorkSave->pAllocBuffer, BusInformationBufferLength);
                }
                
                pInterface->Param5 = pWorkSave->pPdo->m_Interface.QueryBusInformation (pWorkSave->pPdo->m_Interface.BusContext,
                                                    pInterface->Param1, 
                                                    pWorkSave->pAllocBuffer, &BusInformationBufferLength, &BusInformationActualLength);
                pInterface->Param3 = BusInformationBufferLength;
                pInterface->Param4 = BusInformationActualLength;
            }
        }
        break;
    case FI_IsDeviceHighSpeed:
        {
            if (pWorkSave->pPdo->m_Interface.IsDeviceHighSpeed)
            {
                pInterface->Param1 = pWorkSave->pPdo->m_Interface.IsDeviceHighSpeed (pWorkSave->pPdo->m_Interface.BusContext);
			}

			// get advanced info from linux
			{
				DWORD								dwRet = 0;
				BYTE								pBuff [1024];
				USB_NODE_CONNECTION_INFORMATION_EX	*pInfo = (USB_NODE_CONNECTION_INFORMATION_EX*)pBuff;
				NTSTATUS							status;

				//
				// Initialize 
				//
				RtlZeroMemory (pBuff, sizeof (pBuff));

				// enum ports of usb hub 
				pInfo->ConnectionIndex = pWorkSave->pPdo->m_uDeviceAddress;

				CBusFilterExtension *pBus = (CBusFilterExtension *)pWorkSave->pPdo->m_pBus;
				status = pBus->GetUsbDeviceConnectionInfo (pBus->m_PhysicalDeviceObject, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, pInfo, sizeof (pBuff));

				if (!NT_SUCCESS (status))
				{
					pInterface->Param2 = pInterface->Param1;
				}
				else
				{
					pInterface->Param2 = pInfo->Speed;
				}
			}
        }
        break;
    case FI_EnumLogEntry:
        {
            if (pWorkSave->pPdo->m_Interface.EnumLogEntry)
            {
                pInterface->Param5 = pWorkSave->pPdo->m_Interface.EnumLogEntry (pWorkSave->pPdo->m_Interface.BusContext,
                                                                pInterface->Param1, 
                                                                pInterface->Param2, 
                                                                pInterface->Param3, 
                                                                pInterface->Param4);
            }
        }
        break;
    case FI_QueryBusTimeEx:
        {
            if (pWorkSave->pPdo->m_Interface.QueryBusTimeEx)
            {
                ULONG   HighSpeedFrameCounter;
                pInterface->Param2 = pWorkSave->pPdo->m_Interface.QueryBusTimeEx (pWorkSave->pPdo->m_Interface.BusContext,
                                                                &HighSpeedFrameCounter);
                pInterface->Param1 = HighSpeedFrameCounter;
            }
        }
        break;
    case FI_QueryControllerType:
        {
            if (pWorkSave->pPdo->m_Interface.QueryControllerType)
            {
                ULONG HcdiOptionFlags;
                USHORT PciId [2];
                UCHAR PciOther [4];

                ULONG   HighSpeedFrameCounter;
                pInterface->Param4 = pWorkSave->pPdo->m_Interface.QueryControllerType (pWorkSave->pPdo->m_Interface.BusContext,
                                                                &HcdiOptionFlags,
                                                                &PciId [0],
                                                                &PciId [1],
                                                                &PciOther [0],
                                                                &PciOther [1],
                                                                &PciOther [2],
                                                                &PciOther [3]);
                pInterface->Param1 = HcdiOptionFlags;
                pInterface->Param2 = *((ULONG*)PciId);
                pInterface->Param3 = *((ULONG*)PciOther);
            }
            else
            {
                pInterface->Param4 = STATUS_SUCCESS;
                pInterface->Param1 = 0x2c3;
                pInterface->Param2 = 0x71128086;
                pInterface->Param3 = 0x30c;
            }
        }
        break;
    case FI_USBCAMD_InterfaceReference:
        {
            if (pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.InterfaceReference)
            {
                pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.InterfaceReference (pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.Context);
                ++pWorkSave->pPdo->m_IncInterfaceUsbCamd;
            }
        }
        break;
    case FI_USBCAMD_InterfaceDereference:
        {
            if (pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.InterfaceDereference)
            {
                pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.InterfaceDereference (pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.Context);
                --pWorkSave->pPdo->m_IncInterfaceUsbCamd;
            }
        }
        break;
    case FI_USBCAMD_WaitOnDeviceEvent:
        {
			ASSERT(false);
        }
        break;
    case FI_USBCAMD_BulkReadWrite:
        {
			ASSERT(false);
        }
        break;
    case FI_USBCAMD_SetIsoPipeState:
        {
            if (pWorkSave->pPdo->m_InterfaceUsbCamd.USBCAMD_SetIsoPipeState)
            {
                pInterface->Param1 = pWorkSave->pPdo->m_InterfaceUsbCamd.USBCAMD_SetIsoPipeState (
                                                            pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.Context,
                                                            pInterface->Param1);
            }
        }
        break;
    case FI_USBCAMD_CancelBulkReadWrite:
        {
            if (pWorkSave->pPdo->m_InterfaceUsbCamd.USBCAMD_CancelBulkReadWrite)
            {
                pInterface->Param1 = pWorkSave->pPdo->m_InterfaceUsbCamd.USBCAMD_CancelBulkReadWrite (
                                                            pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.Context,
                                                            pInterface->Param1);
            }
        }
        break;
    case FI_USBCAMD_SetVideoFormat:
        {
            if (pWorkSave->pPdo->m_InterfaceUsbCamd.USBCAMD_SetVideoFormat)
            {
                pWorkSave->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (pInterface->Param2, NonPagedPool);

                RtlCopyMemory (pWorkSave->pAllocBuffer, irpSave->Buffer + sizeof (FUNC_INTERFACE), pInterface->Param2);

                pInterface->Param1 = pWorkSave->pPdo->m_InterfaceUsbCamd.USBCAMD_SetVideoFormat (
                                                            pWorkSave->pPdo->m_InterfaceUsbCamd.Interface.Context,
                                                            (PHW_STREAM_REQUEST_BLOCK)pWorkSave->pAllocBuffer);
            }
        }
        break;
    }

    if (m_bIsPresent && !m_bCancel)
    {
		IoCompletionIrpThread (pWorkSave);
    }
}

NTSTATUS CUsbPortToTcp::CCFunction(PVOID  DeviceContext, PVOID  CommandContext, NTSTATUS  NtStatus)
{
    CUsbPortToTcp *pBase = (CUsbPortToTcp*)CommandContext;
    pBase->DebugPrint (DebugSpecial, "CCFunction - 0x%x", NtStatus);

    return STATUS_SUCCESS;
}

LONG CUsbPortToTcp::OutInterfaceBufferSize (PWORKING_IRP pWorkSave)
{
    PIO_STACK_LOCATION			irpStack;
    PFUNC_INTERFACE				pInterface;
	LONG						lSize = 0;

    irpStack = IoGetNextIrpStackLocation( pWorkSave->pIrp );

    pInterface = (PFUNC_INTERFACE)irpStack->Parameters.Others.Argument1;
    lSize = sizeof (FUNC_INTERFACE);

    if (pInterface->lNumFunc == FI_QueryBusInformation)
    {
        lSize += ULONG ((ULONG_PTR)pInterface->Param3);
    }

	return lSize;
}

void CUsbPortToTcp::OutInterfaceFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer)
{
    PFUNC_INTERFACE			pInterface = (PFUNC_INTERFACE)pWorkSave->pAllocBuffer;        
    BYTE					*pBuff;

    DebugPrint (DebugTrace, "FuncInterfaceFillBuffer");

    RtlCopyMemory (pOutBuffer, pInterface, sizeof (FUNC_INTERFACE));

    if (pInterface->lNumFunc == FI_QueryBusInformation && pWorkSave->pAllocBuffer)
    {
        pBuff = pOutBuffer;
        pBuff += sizeof (FUNC_INTERFACE);
        RtlCopyMemory (pBuff, pWorkSave->pAllocBuffer, pInterface->Param3);
    }
}

void CUsbPortToTcp::SendIdleNotification (CPdoStubExtension *pPdoStub)
{
    PIRP                    newIrp;
    PIO_STACK_LOCATION      irpStack;
    PWORKING_IRP            saveData = NULL;

    DebugPrint (DebugTrace, "SendNewIsochTimer");

    // allocate mem from context /1
    saveData = (PWORKING_IRP)m_AllocListWorkSave.Alloc ();
    // Build IRP
    newIrp = IoBuildAsynchronousFsdRequest(
        IRP_MJ_PNP,
        pPdoStub->m_NextLowerDriver,
        NULL,
        0,
        NULL,
        &saveData->ioStatus
        );

    if (newIrp == NULL) 
    {
        m_AllocListWorkSave.Free (saveData);
        return;
    }

    irpStack = IoGetNextIrpStackLocation( newIrp );
    RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );

    // base param filling in
    newIrp->IoStatus.Status = 0;
    irpStack->MajorFunction = -1;
    irpStack->MinorFunction = 0;
    irpStack->Parameters.Others.Argument1 = 0;
    irpStack->Parameters.Others.Argument2 = 0;
    irpStack->Parameters.Others.Argument3 = 0;
    irpStack->Parameters.Others.Argument4 = 0;

    // save info filling in
    RtlZeroMemory(saveData, sizeof (WORKING_IRP));
    saveData->pIrpIndef = -1;
    saveData->MajorFunction = -1;
    saveData->MinorFunction = 0;
    saveData->IoControlCode = 0;
    saveData->pBase = this;
    saveData->pIrp = newIrp;
    saveData->lLock = IRPLOCK_CANCELABLE;
    saveData->pPdo = pPdoStub;
    saveData->Is64 = 0;
    saveData->Pending = 0;

    saveData->pAllocBuffer = (BYTE*)m_BufferMain.AllocBuffer (sizeof (FUNC_INTERFACE), PagedPool);
	saveData->lSizeBuffer = sizeof (FUNC_INTERFACE);
    RtlZeroMemory (saveData->pAllocBuffer, saveData->lSizeBuffer);

	irpStack->Parameters.Others.Argument1 = saveData->pAllocBuffer;
	++m_irpCounter;

	(PFUNC_INTERFACE (saveData->pAllocBuffer))->lNumFunc = FI_USB_SUBMIT_IDLE_NOTIFICATION;

	IoCompletionIrpThread (saveData);
}

// ********************************************
//			USB Io Control
void CUsbPortToTcp::InUsbIoFillIrp (PIRP_SAVE irpSave, PWORKING_IRP pWorkSave)
{
    PIO_STACK_LOCATION irpStack;

    DebugPrint (DebugTrace, "UsbIoFillIrp");

    irpStack = IoGetNextIrpStackLocation (pWorkSave->pIrp);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
    {
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
		{
			USB_DESCRIPTOR_REQUEST *pReq;

			pWorkSave->pIrp->AssociatedIrp.SystemBuffer = m_BufferMain.AllocBuffer (max ((ULONG)irpSave->BufferSize, irpStack->Parameters.DeviceIoControl.OutputBufferLength), NonPagedPool);
			pWorkSave->pAllocBuffer = (BYTE*)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lSizeBuffer = irpSave->BufferSize;
			RtlCopyMemory (pWorkSave->pIrp->AssociatedIrp.SystemBuffer, irpSave->Buffer, irpSave->BufferSize);
			
			pReq = (USB_DESCRIPTOR_REQUEST*)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lOther = (USHORT)pReq->ConnectionIndex;
			pReq->ConnectionIndex = m_pPdoStub?m_pPdoStub->m_uDeviceAddress:0;
		}
        break;
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
		{
			PUSB_NODE_CONNECTION_INFORMATION pConn;

			pWorkSave->pIrp->AssociatedIrp.SystemBuffer = m_BufferMain.AllocBuffer (max ((ULONG)irpSave->BufferSize, irpStack->Parameters.DeviceIoControl.OutputBufferLength), NonPagedPool);
			pWorkSave->pAllocBuffer = (BYTE*)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lSizeBuffer = irpSave->BufferSize;
			RtlCopyMemory (pWorkSave->pIrp->AssociatedIrp.SystemBuffer, irpSave->Buffer, irpSave->BufferSize);
			
			pConn = (PUSB_NODE_CONNECTION_INFORMATION)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lOther = (USHORT)pConn->ConnectionIndex;
			pConn->ConnectionIndex = m_pPdoStub?m_pPdoStub->m_uDeviceAddress:0;
		}
		break;
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
		{
			PUSB_NODE_CONNECTION_INFORMATION_EX pConn;

			pWorkSave->pIrp->AssociatedIrp.SystemBuffer = m_BufferMain.AllocBuffer (max ((ULONG)irpSave->BufferSize, irpStack->Parameters.DeviceIoControl.OutputBufferLength), NonPagedPool);
			pWorkSave->pAllocBuffer = (BYTE*)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lSizeBuffer = irpSave->BufferSize;
			RtlCopyMemory (pWorkSave->pIrp->AssociatedIrp.SystemBuffer, irpSave->Buffer, irpSave->BufferSize);
			
			pConn = (PUSB_NODE_CONNECTION_INFORMATION_EX)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lOther = (USHORT)pConn->ConnectionIndex;
			pConn->ConnectionIndex = m_pPdoStub?m_pPdoStub->m_uDeviceAddress:0;

		}
		break;
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		{
			PUSB_NODE_CONNECTION_NAME pConn;

			pWorkSave->pIrp->AssociatedIrp.SystemBuffer = m_BufferMain.AllocBuffer (max ((ULONG)irpSave->BufferSize, irpStack->Parameters.DeviceIoControl.OutputBufferLength), NonPagedPool);
			pWorkSave->pAllocBuffer = (BYTE*)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lSizeBuffer = irpSave->BufferSize;
			RtlCopyMemory (pWorkSave->pIrp->AssociatedIrp.SystemBuffer, irpSave->Buffer, irpSave->BufferSize);
			
			pConn = (PUSB_NODE_CONNECTION_NAME)pWorkSave->pIrp->AssociatedIrp.SystemBuffer;
			pWorkSave->lOther = (USHORT)pConn->ConnectionIndex;
			pConn->ConnectionIndex = m_pPdoStub?m_pPdoStub->m_uDeviceAddress:0;

		}
		break;
	}
}

LONG CUsbPortToTcp::OutUsbIoBufferSize (PWORKING_IRP pWorkSave)
{
    DebugPrint (DebugTrace, "UsbIoBufferSize");

    return (LONG)pWorkSave->pIrp->IoStatus.Information;
}

void CUsbPortToTcp::OutUsbIoFillBuffer (PWORKING_IRP pWorkSave, BYTE *pOutBuffer)
{
    DebugPrint (DebugTrace, "UsbIoFillBuffer");

	RtlCopyMemory (pOutBuffer, pWorkSave->pAllocBuffer, pWorkSave->lSizeBuffer);
	pWorkSave->pIrp->AssociatedIrp.SystemBuffer = NULL;

    switch (pWorkSave->IoControlCode)
    {
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
		{
			USB_DESCRIPTOR_REQUEST *pReq;

			pReq = (USB_DESCRIPTOR_REQUEST*)pOutBuffer;
			pReq->ConnectionIndex = pWorkSave->lOther;
		}
        break;
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
		{
			PUSB_NODE_CONNECTION_INFORMATION pConn;

			pConn = (PUSB_NODE_CONNECTION_INFORMATION)pOutBuffer;
			pConn->ConnectionIndex = pWorkSave->lOther;
		}
		break;
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
		{
			PUSB_NODE_CONNECTION_INFORMATION_EX pConn;

			pConn = (PUSB_NODE_CONNECTION_INFORMATION_EX)pOutBuffer;
			pConn->ConnectionIndex = pWorkSave->lOther;
		}
		break;
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		{
			PUSB_NODE_CONNECTION_NAME pConn;

			pConn = (PUSB_NODE_CONNECTION_NAME)pOutBuffer;
			pConn->ConnectionIndex = pWorkSave->lOther;
		}
		break;
	}

}

void CUsbPortToTcp::Delete ()
{
	m_spinBase.Lock ('DEL');
	if (m_pDel)
	{
		m_spinBase.UnLock ();
		return;
	}
	m_pDel = this;
	m_spinBase.UnLock ();

	m_pDel = CWorkItem::Create(CControlDeviceExtension::GetControlExtension()->GetDeviceObject ());
	CWorkItem::AddContext (m_pDel, this);
	CWorkItem::Run (m_pDel, CUsbPortToTcp::WorkItemDelete);
}

// New work with mem pool
VOID CUsbPortToTcp::WorkItemDelete (PDEVICE_OBJECT DeviceObject, PVOID Context)
{
	TPtrUsbPortToTcp pUsbPort = (CUsbPortToTcp*)CWorkItem::GetContext (Context, 0);

	LARGE_INTEGER time;

	pUsbPort->DebugPrint (DebugSpecial, "WorkItmDeleteUsbDev delay");

	pUsbPort->m_poolRead.GetCurNodeWrite();
	pUsbPort->m_poolRead.PushCurNodeWrite();

	time.QuadPart = (LONGLONG)-50000000;
	KeDelayExecutionThread (KernelMode, FALSE, &time);

	Common::intrusive_ptr_release (pUsbPort.get ());
}

VOID CUsbPortToTcp::stItemWriteToSrv (/*PDEVICE_OBJECT DeviceObject, */PVOID Context)
{
	TPtrUsbPortToTcp pBase = (CUsbPortToTcp*)Context;
	pBase->ItemWriteToSrv ();
}

void CUsbPortToTcp::ItemWriteToSrv ()
{
	DebugPrint (DebugTrace, "ItemWriteToSrv");

	m_poolWrite.WaitReady();
	while (m_bWork)
	{
		m_spinUsbOut.Lock ();
		if (m_listIrpAnswer.GetCount () == 0)
		{
			m_spinUsbOut.UnLock ();
			m_listIrpAnswer.WaitAddElemetn ();
			continue;
		}
		m_spinUsbOut.UnLock ();

		if (m_listIrpAnswer.GetCount ())
		{
			OutFillAnswer2();
		}
	}
}

VOID CUsbPortToTcp::stItemReadFromSrv (/*PDEVICE_OBJECT DeviceObject, */PVOID Context)
{
	TPtrUsbPortToTcp pBase = (CUsbPortToTcp*)Context;
	pBase->ItemReadFromSrv ();
}

void CUsbPortToTcp::ItemReadFromSrv ()
{
	char		*pBuff;
	PIRP_SAVE	pSave;

	DebugPrint (DebugTrace, "ItemReadFromSrv");

	m_poolRead.WaitReady();

	while (m_bWork)
	{
		pBuff = m_poolRead.GetCurNodeRead();
		pSave = (PIRP_SAVE)pBuff;

		if (!m_bWork)
		{
			m_poolRead.PopCurNodeRead();
			break;
		}

		if (pSave)
		{
			DebugPrint (DebugSpecial, "ItemReadFromSrv %d", pSave->Irp);
			InPutIrpToDevice((PBYTE)pBuff, pSave->NeedSize);
		}
		else
		{
			//m_poolRead.PopCurNodeRead();
			break;
		}
		m_poolRead.PopCurNodeRead();
	}
}

bool CUsbPortToTcp::OutFillAnswer2 ()
{
	PIRP_SAVE           pIrpSave;
	LONG                lBufferSize = 0;        // Needed data size
	BYTE                *pBuffer = NULL;            // pointer to data
	PWORKING_IRP        pWorkSave = NULL;


	DebugPrint (DebugTrace, "OutFillAnswer irpControl");

	{
		// get irp info
		m_spinUsbOut.Lock();
		if (m_listIrpAnswer.GetCount())
		{
			pWorkSave = (PWORKING_IRP)m_listIrpAnswer.PopFirst();
		}
		m_spinUsbOut.UnLock();

		// if device has been deleted
		if (pWorkSave && m_bAdditionDel)
		{
			DeleteIrp(pWorkSave);
			return true;
		}


		// get buffer for transfer data
		pBuffer = (LPBYTE)m_poolWrite.GetCurNodeWrite();
		// get size of buffer
		lBufferSize = m_poolWrite.GetBlockSize();

		pIrpSave = (PIRP_SAVE)(pBuffer);

		// if pool is stopped
		if (!pIrpSave)
		{
			// free memory for IRP
			if (pWorkSave)
			{
				DeleteIrp(pWorkSave);
			}
			return false;
		}

		pIrpSave->Size = lBufferSize;

		// Send to service what device is removed
		if (pWorkSave == NULL)
		{
			// send error to service
			RtlZeroMemory (pIrpSave, sizeof (IRP_SAVE));
			m_poolWrite.PushCurNodeWrite();

			return true;
		}
		// fill data
		else if (!OutFillIrpSave (pIrpSave, pWorkSave))
		{
			// need more data
			pIrpSave->NeedSize = -1;
			m_spinUsbOut.Lock ();
			m_listIrpAnswer.Add (pWorkSave);
			m_spinUsbOut.UnLock ();

			m_poolWrite.PushCurNodeWrite();
			return false;
		}
		// push data to service
		m_poolWrite.PushCurNodeWrite();
	}


#ifdef ISOCH_FAST_MODE 
	if (pWorkSave->Isoch)
	{
		IsochStartNew ((PISOCH)pWorkSave->pAddition);
		IsochDeleteSave (pWorkSave);
		bRet = true;
	}
#endif
	return true;
}
