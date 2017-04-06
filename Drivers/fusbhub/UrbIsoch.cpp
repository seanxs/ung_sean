/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UrbIsoch.cpp

Environment:

    Kernel mode

--*/

#include "UrbIsoch.h"
#include "PdoStubExtension.h"

long	CUrbIsoch::m_lIsochFew = 4;
long	CUrbIsoch::m_lIsochMid = 10;
long	CUrbIsoch::m_lIsochMany = 20;


CUrbIsoch::CUrbIsoch()
	: m_lIsochCount (0)
	, m_pBase (NULL)
	, m_StartFrameOut(0)
	, m_StartFrameIn(0)
{
}

CUrbIsoch::~CUrbIsoch(void)
{
	ClearIsoch ();
}

void CUrbIsoch::SetBase(CUsbPortToTcp *pBase)
{
	m_pBase = pBase;
	m_RestartIsochOut.Start(stRestartIschoOut, this);
}

NTSTATUS CUrbIsoch::PutIrpToDevice (PWORKING_IRP saveData, CPdoStubExtension *pPdoStub)
{
	NTSTATUS status = STATUS_SUCCESS;
	int	iCount = 0;

	// verify query isoch out
	m_spinUsbIsoch.Lock ();
	if (m_lIsochCount > 3 || m_listIsoch.GetCount ())
	{
		m_listIsoch.Add (saveData);
		iCount = m_listIsoch.GetCount ();

		m_spinUsbIsoch.UnLock ();

		// if buffer is big to increment timer
		if (iCount == m_lIsochMid)
		{
			SendNewIsochTimer (pPdoStub, 10);
		}
		else if (iCount == m_lIsochMany)
		{
			SendNewIsochTimer (pPdoStub, 15);
		}
	}
	else
	{
		m_lIsochCount++;
		m_spinUsbIsoch.UnLock ();

		m_pBase->m_spinUsbIn.Lock();
		m_pBase->m_listWaitCompleted.Add (saveData);
		m_pBase->m_spinUsbIn.UnLock();

		status = IoCallDriver( pPdoStub->m_NextLowerDriver, saveData->pIrp );
	}

	return status;
}

bool CUrbIsoch::IsochVerifyOut (PWORKING_IRP pWorkSave)
{
	LONG        OldSize;

	DebugPrint (DebugTrace, "IsochVerifyOut irp - 0x%x", pWorkSave->pIrp);

	if (pWorkSave->IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		return false;
	}    // Create and fill in IRP

	PURB_FULL urb = (PURB_FULL)(pWorkSave->pAllocBuffer);

	// Verify 
	if (urb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
	{
		if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
		{
			return true;
		}
	}

	return false;
}

// Clear query of isoch out 
void CUrbIsoch::ClearIsoch ()
{
	PWORKING_IRP	pSave;
	PVOID			pRet = NULL;

	DebugPrint (DebugTrace, "ClearBuffer");

	m_spinUsbIsoch.Lock ();
	while (m_listIsoch.GetCount ())
	{
		pSave = (PWORKING_IRP)m_listIsoch.GetItem (0);

		if (pSave)
		{
			m_pBase->DeleteIrp (pSave);
			m_listIsoch.Remove (0);
		}
	}
	m_spinUsbIsoch.UnLock ();
}

bool CUrbIsoch::IsochCompleted ()
{
	PWORKING_IRP	pSave = NULL;
	int iCount;

	m_spinUsbIsoch.Lock ();

	m_lIsochCount--; // decrement counter
	iCount = m_listIsoch.GetCount ();
	if (iCount)
	{
		pSave = (PWORKING_IRP)m_listIsoch.PopFirst();
		m_lIsochCount++;
	}
	m_spinUsbIsoch.UnLock ();

	// if quyry is containing little irp to decrenent timer
	if (m_lIsochFew == iCount)
	{
		SendNewIsochTimer (m_pBase->m_pPdoStub, 4); // 5 ms
	}

	if (pSave)
	{
		m_pBase->m_spinUsbIn.Lock();
		m_pBase->m_listWaitCompleted.Add (pSave);
		m_pBase->m_spinUsbIn.UnLock();

		IoCallDriver( pSave->pPdo->m_NextLowerDriver, pSave->pIrp );
	}
	return false;
}

VOID CUrbIsoch::stRestartIschoOut(Thread::CThreadPoolTask *pTask, PVOID Context)
{
	CUrbIsoch	*pBase = (CUrbIsoch*)Context;
	bool		bTask = false;

	while (pBase->m_pBase->m_bWork)
	{
		bTask = pTask->WaitTask()?true:false;

		if (bTask)
		{
			pBase->IsochCompleted();
		}
		else
		{
			break;
		}
	}
}

void CUrbIsoch::PushNextIsoPackedge()
{
	m_RestartIsochOut.PushTask((PVOID)true);
}

void CUrbIsoch::SendNewIsochTimer (CPdoStubExtension *pPdoStub, long iTime)
{
	PIRP                    newIrp;
	PIO_STACK_LOCATION      irpStack;
	PWORKING_IRP            saveData = NULL;

	DebugPrint (DebugTrace, "SendNewIsochTimer");

	// allocate mem from context /1
	saveData = (PWORKING_IRP)m_pBase->m_AllocListWorkSave.Alloc ();
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
		m_pBase->m_AllocListWorkSave.Free (saveData);
		return;
	}

	irpStack = IoGetNextIrpStackLocation( newIrp );
	RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );

	// base param filling in
	newIrp->IoStatus.Status = 0;
	newIrp->IoStatus.Information = iTime;
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
	saveData->pBase = m_pBase;
	saveData->pIrp = newIrp;
	saveData->lLock = IRPLOCK_CANCELABLE;
	saveData->pPdo = pPdoStub;
	saveData->Is64 = 0;
	saveData->Pending = 0;

	saveData->pAllocBuffer = (BYTE*)m_pBase->m_BufferMain.AllocBuffer (sizeof (FUNC_INTERFACE), PagedPool);
	saveData->lSizeBuffer = sizeof (FUNC_INTERFACE);
	RtlZeroMemory (saveData->pAllocBuffer, saveData->lSizeBuffer);

	irpStack->Parameters.Others.Argument1 = saveData->pAllocBuffer;
	++m_pBase->m_irpCounter;

	(PFUNC_INTERFACE (saveData->pAllocBuffer))->lNumFunc = FI_ISOCH_OUT_TIMER;

	m_pBase->IoCompletionIrpThread (saveData);
}

// ****************************************************
//      URB_FUNCTION_ISOCH_TRANSFER addition 
// ****************************************************
#ifdef ISOCH_FAST_MODE
bool CUrbIsoch::IsochVerifyDel (PWORKING_IRP saveWork)
{
	DebugPrint (DebugTrace, "IsochVerifyDel");

	if (saveWork->Isoch)
	{
		switch (saveWork->lLockIsoch)
		{
		case IRPLOCKI_DELETED:
			IsochDeleteWorkSave (saveWork);
			--m_irpCounter;
			return true;
		}
	}
	return false;
}

PISOCH CUrbIsoch::IsochFind (USBD_PIPE_HANDLE    hPipe)
{
	PISOCH pIsoch = NULL;

	DebugPrint (DebugTrace, "IsochFind 0x%x", hPipe);

	for (int a = 0; a < m_listIsoch.GetCount (); a++)
	{
		pIsoch = (PISOCH)m_listIsoch.GetItem (a, false);
		if (pIsoch->hPipe == hPipe)
		{
			DebugPrint (DebugInfo, "IsochFind - exist 0x%x", hPipe);
			return pIsoch;
		}
		pIsoch = NULL;
	}
	return pIsoch;
}

bool CUrbIsoch::IsochVerify (PIRP_SAVE irpSave, CPdoStubExtension *pPdo)
{
	PISOCH  pIsoch;
	bool    bIsoDetect = false;
	KIRQL   OldIrql;

#ifndef ISOCH_FAST_MODE
	return bIsoDetect;
#endif

	DebugPrint (DebugTrace, "IsochVerify");

	// Detect ISOCH function
	if (irpSave->StackLocation.MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL)
	{
		if (irpSave->Is64 == (BYTE)m_bIs64)
		{
			PURB urb = ((PURB)irpSave->Buffer);
			switch (urb->UrbHeader.Function)
			{
			case URB_FUNCTION_ISOCH_TRANSFER:
				{
					if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
					{
						break;
					}
					OldIrql = m_listIsoch.Lock ();
					pIsoch = IsochFind (urb->UrbIsochronousTransfer.PipeHandle);
					if (pIsoch)
					{
						IsochClear (pIsoch);
						bIsoDetect = IsochStart (irpSave, pPdo);
					}
					else
					{
						bIsoDetect = IsochStart (irpSave, pPdo);
					}

					m_listIsoch.UnLock (OldIrql);
				}
				break;
			case URB_FUNCTION_ABORT_PIPE:
			case URB_FUNCTION_RESET_PIPE:
			case URB_FUNCTION_SYNC_RESET_PIPE:
				{
					OldIrql = m_listIsoch.Lock ();
					pIsoch = IsochFind (urb->UrbPipeRequest.PipeHandle);
					if (pIsoch)
					{
						IsochClear (pIsoch);
					}
					m_listIsoch.UnLock (OldIrql);
				}
				break;
			}
		}
		else
		{
			PURB_DIFF urb = ((PURB_DIFF)irpSave->Buffer);
			switch (urb->UrbHeader.Function)
			{
			case URB_FUNCTION_ISOCH_TRANSFER:
				if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
				{
					break;
				}
				OldIrql = m_listIsoch.Lock ();
				pIsoch = IsochFind (pPdo->GetPipeHandle ((USBD_PIPE_HANDLE_DIFF)urb->UrbIsochronousTransfer.PipeHandle));
				if (pIsoch)
				{
					IsochClear (pIsoch);
					bIsoDetect = IsochStart (irpSave, pPdo);
				}
				else
				{
					bIsoDetect = IsochStart (irpSave, pPdo);
				}

				m_listIsoch.UnLock (OldIrql);
				break;
			case URB_FUNCTION_ABORT_PIPE:
			case URB_FUNCTION_RESET_PIPE:
			case URB_FUNCTION_SYNC_RESET_PIPE:
				{
					OldIrql = m_listIsoch.Lock ();
					pIsoch = IsochFind (pPdo->GetPipeHandle (urb->UrbPipeRequest.PipeHandle));
					if (pIsoch)
					{
						IsochClear (pIsoch);
					}
					m_listIsoch.UnLock (OldIrql);
				}
				break;
			}
		}
	}

	return bIsoDetect;
}

bool CUrbIsoch::IsochStart (PIRP_SAVE irpSave, CPdoStubExtension *pPdo)
{
	PISOCH  pIsoch;
	PURB urb = ((PURB)irpSave->Buffer);
	PWORKING_IRP saveWork;

	DebugPrint (DebugTrace, "IsochStart 0x%x", urb->UrbIsochronousTransfer.PipeHandle);

	pIsoch = (PISOCH)ExAllocatePoolWithTag (NonPagedPool, sizeof (ISOCH), tagUsb);
	if (!pIsoch)
	{
		return false;
	}

	// Copy irpsave struct
	pIsoch->irpSave = (PIRP_SAVE)ExAllocatePoolWithTag (NonPagedPool, irpSave->NeedSize, tagUsb);
	if (!pIsoch->irpSave)
	{
		ExFreePool (pIsoch);
		return false;
	}
	RtlCopyMemory (pIsoch->irpSave, irpSave, irpSave->NeedSize);

	// Init list
	pIsoch->listIrp = new CList;
	// Init struct
	pIsoch->pPdo = pPdo;
	pIsoch->lSizeBuf = urb->UrbIsochronousTransfer.TransferBufferLength;
	pIsoch->hPipe = urb->UrbIsochronousTransfer.PipeHandle;

	pIsoch->pList = m_listIsoch.Add (pIsoch, false);

	// Start stream
	for (int a = 0; a < iMaxIsoch; a++)
	{
		IsochStartNew (pIsoch);
	}

	return true;
}

void CUrbIsoch::IsochDeleteSave (PWORKING_IRP saveWork)
{
	PWORKING_IRP saveWorkNew;
	PISOCH       pIsoch =  (PISOCH)saveWork->pAddition;

	DebugPrint (DebugTrace, "IsochRestart");

	// if not delete
	if (pIsoch)
	{
		KIRQL OldIrql = pIsoch->listIrp->Lock ();
		// delete from list
		for (int a = 0; a < pIsoch->listIrp->GetCount (); a++)
		{
			saveWorkNew = (PWORKING_IRP)pIsoch->listIrp->GetItem (a, false);
			if (saveWork == saveWorkNew)
			{
				pIsoch->listIrp->Remove (a, false);
				pIsoch->listIrp->UnLock (OldIrql);
				return;
			}
		}
		DebugPrint (DebugSpecial, "IsochRestart - not complare first element");
		pIsoch->listIrp->UnLock (OldIrql);
	}
}

void CUrbIsoch::IsochStartNew (PISOCH       pIsoch)
{
	PWORKING_IRP saveWorkNew;

	DebugPrint (DebugTrace, "IsochRestart");

	// if not delete
	if (pIsoch)
	{
		KIRQL OldIrql = pIsoch->listIrp->Lock ();
		DebugPrint (DebugSpecial, "IsochRestart - %d", pIsoch->listIrp->GetCount ());

		if (pIsoch->listIrp->GetCount () < 20)
		{
			// Create new
			saveWorkNew = InitWorkIrp (pIsoch->irpSave, pIsoch->pPdo);
			if (saveWorkNew)
			{
				saveWorkNew->Isoch = true;
				saveWorkNew->lLockIsoch = IRPLOCKI_SET;
				saveWorkNew->pAddition = pIsoch;
				pIsoch->listIrp->Add (saveWorkNew, false);
				IsochComplitingWorkSaveWorkIrp (saveWorkNew);
				pIsoch->listIrp->UnLock (OldIrql);
				return;
			}
		}
		pIsoch->listIrp->UnLock (OldIrql);
	}
}

void CUrbIsoch::IsochClear (PISOCH pIsoch)
{
	PWORKING_IRP    saveIrp;

	DebugPrint (DebugTrace, "IsochClear");

	// cancel irp
	KIRQL OldIrql = pIsoch->listIrp->Lock ();
	while (pIsoch->listIrp->GetCount ())
	{
		saveIrp = (PWORKING_IRP)pIsoch->listIrp->GetItem (0, false);
		pIsoch->listIrp->Remove (0, false);
		saveIrp->pAddition = NULL;
		saveIrp->lLockIsoch = IRPLOCKI_DELETED;
	}
	pIsoch->listIrp->UnLock (OldIrql);

	// delete 
	m_listIsoch.Remove (pIsoch->pList, false);

	// Free mem
	ExFreePool (pIsoch->irpSave);
	delete pIsoch->listIrp;
	ExFreePool (pIsoch);
}

void CUrbIsoch::IsochComplitingWorkSaveWorkIrp (PWORKING_IRP CurWorkSave)
{
	PWORKING_IRP WorkIrp = NULL;
	PWORKING_IRP WorkIrpWait = NULL;

	CurWorkSave->lLockIsoch = IRPLOCKI_USED;
	++m_irpCounter;
	m_listWaitCompleted.Add (CurWorkSave);
	IoCallDriver( CurWorkSave->pPdo->m_PhysicalDeviceObject, CurWorkSave->pIrp );
}

void CUrbIsoch::IsochDeleteWorkSave (PWORKING_IRP saveWork)
{
	DebugPrint (DebugTrace, "DeleteWorkSave");

	if (saveWork->pIrp)
	{
		IoFreeIrp(saveWork->pIrp);
		saveWork->pIrp = NULL;
	}
	if (saveWork->pBuffer)
	{
		ExFreePool (saveWork->pBuffer);
		saveWork->pBuffer = NULL;
	}
	if (saveWork->pSecondBuffer)
	{
		ExFreePool (saveWork->pSecondBuffer);
		saveWork->pSecondBuffer = NULL;
	}

	m_AllocListWorkSave.Free (saveWork);
	saveWork = NULL;
}
#endif

void CUrbIsoch::BuildStartFrame(PURB_FULL pUrb)
{
	pUrb->UrbIsochronousTransfer.StartFrame = 0;
	pUrb->UrbIsochronousTransfer.TransferFlags |= USBD_START_ISO_TRANSFER_ASAP;
}
