/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   cUrbIsoch.cpp

Environment:

    Kernel mode

--*/
#include "cUrbIsoch.h"
#include "UsbDevToTcp.h"
#include "PdoExtension.h"

const long c_lCurrentFrame = 2000;

CcUrbIsoch::CcUrbIsoch()
	: m_lCurrentFrame(c_lCurrentFrame)
	, m_lDelta(0)
	, m_lLastTime(0)
	, m_pBase(NULL)
{
	m_DueTimeIsoch = 5;
	m_timerIsoch.InitTimer(CcUrbIsoch::IsochDpc, this);
}

CcUrbIsoch::~CcUrbIsoch()
{
	ULONGLONG uCur = KeQueryInterruptTime();
}

void CcUrbIsoch::SetBase(evuh::CUsbDevToTcp *pBase)
{
	m_pBase = pBase;
	// start isoch compete
}

// ******************************************************************
//              Isoch
// ******************************************************************
PISOCH_SAVE CcUrbIsoch::IsochFind(USBD_PIPE_HANDLE hPipe)
{
	PISOCH_SAVE pIsoch = NULL;

	for (int a = 0; a < m_listIsoch.GetCount(); a++)
	{
		pIsoch = (PISOCH_SAVE)m_listIsoch.GetItem(a);
		if (hPipe == pIsoch->hPipe)
		{
			return pIsoch;
		}
	}

	return NULL;
}

PISOCH_SAVE CcUrbIsoch::IsochFind(LONG iNumIrp)
{
	PISOCH_SAVE pIsoch = NULL;

	for (int a = 0; a < m_listIsoch.GetCount(); a++)
	{
		pIsoch = (PISOCH_SAVE)m_listIsoch.GetItem(a);
		if (iNumIrp == pIsoch->lPnpNum)
		{
			return pIsoch;
		}
	}

	return NULL;
}

bool CcUrbIsoch::IsochVerify(PIRP Irp, CPdoExtension *pPdo)
{
	PIO_STACK_LOCATION  stack;
	PISOCH_SAVE     pIsoch;
	bool            bCreateNew = true;

	DebugPrint(DebugTrace, "IsochVerify");

#ifndef ISOCH_FAST_MODE
	return false;
#endif

	stack = IoGetCurrentIrpStackLocation(Irp);
	if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
		stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		PURB urb = (PURB)stack->Parameters.Others.Argument1;
		switch (urb->UrbHeader.Function)
		{
		case URB_FUNCTION_ISOCH_TRANSFER:
			if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
			{
				break;
			}
			m_spinIsoch.Lock();
			pIsoch = IsochFind(urb->UrbIsochronousTransfer.PipeHandle);
			if (pIsoch)
			{
				IsochFire(Irp, pIsoch, pPdo);
				m_spinIsoch.UnLock();
				return true;
			}

			{
				bCreateNew = IsochStart(Irp, pPdo);
			}
			m_spinIsoch.UnLock();

			return bCreateNew;
		case URB_FUNCTION_ABORT_PIPE:
		case URB_FUNCTION_RESET_PIPE:
			m_spinIsoch.Lock();
			for (int a = 0; a < m_listIsoch.GetCount(); a++)
			{
				pIsoch = (PISOCH_SAVE)m_listIsoch.GetItem(a);
				if (urb->UrbIsochronousTransfer.PipeHandle == pIsoch->hPipe)
				{
					IsochDelete(pIsoch);
					break;
				}
			}
			m_spinIsoch.UnLock();
			break;
		}
	}
	return false;
}

bool CcUrbIsoch::IsochStart(PIRP Irp, CPdoExtension *pPdo)
{
	PISOCH_SAVE         pIsoch;
	PIRP_NUMBER			irpNumber;
	PIO_STACK_LOCATION  stack;
	PURB                urb;
	KIRQL               OldIrql;

	DebugPrint(DebugTrace, "IsochStart");

	pIsoch = (PISOCH_SAVE)ExAllocatePoolWithTag(NonPagedPool, sizeof(ISOCH_SAVE), tagUsb);
	if (pIsoch)
	{
		stack = IoGetCurrentIrpStackLocation(Irp);
		urb = (PURB)stack->Parameters.Others.Argument1;
		// Create standard 
		irpNumber = m_pBase->NewIrpNumber(Irp, pPdo);

		// save number
		pIsoch->lPnpNum = irpNumber->Count;
		pIsoch->lSizeBuf = urb->UrbIsochronousTransfer.TransferBufferLength;
		pIsoch->hPipe = urb->UrbIsochronousTransfer.PipeHandle;

		pIsoch->pList = m_listIsoch.Add(pIsoch);

		// Fire standard 
		m_pBase->IrpToQuery(irpNumber);

		return true;
	}

	return false;
}

void CcUrbIsoch::IsochFire(PIRP Irp, PISOCH_SAVE pIsoch, CPdoExtension *pPdo)
{
	PIRP_NUMBER			irpNumber;
	PIRP_SAVE           pIrpSave;

	DebugPrint(DebugTrace, "IsochFire");

	// Create save number
	irpNumber = (PIRP_NUMBER)m_pBase->m_AllocNumber.Alloc();

	RtlZeroMemory(irpNumber, sizeof(IRP_NUMBER));
	irpNumber->Count = pIsoch->lPnpNum;
	irpNumber->Lock = 0;
	irpNumber->Irp = Irp;
	irpNumber->pPdo = pPdo;
	irpNumber->Cancel = false;
	KeInitializeEvent(&irpNumber->Event, NotificationEvent, FALSE);

	m_pBase->WiatCompleted(irpNumber);
}

bool CcUrbIsoch::IsochDelete(PISOCH_SAVE pIsoch)
{
	PIRP_SAVE pIrpSave;

	DebugPrint(DebugTrace, "IsochDelete");

	m_listIsoch.Remove(pIsoch->pList);

	ExFreePool(pIsoch);
	return false;
}


VOID CcUrbIsoch::IsochDpc(struct _KDPC *Dpc, PVOID  DeferredContext, PVOID  SystemArgument1, PVOID  SystemArgument2)
{
	PIRP_NUMBER			irpNumber = NULL;
	CcUrbIsoch			*pBase = (CcUrbIsoch*)DeferredContext;
	PIRP_NUMBER			irpCompleted = NULL;
	evuh::CUsbDevToTcp	*pTcp = pBase->m_pBase;
	int					iCompleted = 0;

	++pTcp->m_WaitUse;

	pTcp->m_spinUsbIn.Lock();

	for (int a = 0; a < pTcp->m_listIrpAnswer.GetCount(); a++)
	{
		irpNumber = (PIRP_NUMBER)pTcp->m_listIrpAnswer.GetItem(a);
		if (pBase->IsochVerifyOut(irpNumber) && (!irpCompleted || (irpCompleted->Count > irpNumber->Count)))
		{
			irpCompleted = irpNumber;
			iCompleted = a;
		}
	}

	if (irpCompleted)
	{
		pTcp->m_listIrpAnswer.Remove(iCompleted);
		pTcp->m_spinUsbIn.UnLock();
		pBase->IsochCompleted(irpCompleted);
	}
	else
	{
		pTcp->m_spinUsbIn.UnLock();
	}

	if (irpCompleted)
	{
		pBase->m_timerIsoch.SetTimer(pBase->m_DueTimeIsoch);
	}

	--pTcp->m_WaitUse;
}

bool CcUrbIsoch::IsochVerifyOut(PIRP_NUMBER stDevice)
{
	PIO_STACK_LOCATION  stack;
	PISOCH_SAVE     pIsoch;
	bool            bCreateNew = true;
	KIRQL           OldIrql;

	DebugPrint(DebugTrace, "IsochVerifyOut");

	stack = IoGetCurrentIrpStackLocation(stDevice->Irp);
	if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
		stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		PURB urb = (PURB)stack->Parameters.Others.Argument1;
		switch (urb->UrbHeader.Function)
		{
		case URB_FUNCTION_ISOCH_TRANSFER:
			if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
			{
				return true;
			}
			break;
		}
	}
	return false;
}


bool CcUrbIsoch::IsochCompleted(PIRP_NUMBER stDevice)
{
	PIO_STACK_LOCATION  stack;
	PISOCH_SAVE     pIsoch;
	bool            bCreateNew = true;
	KIRQL           OldIrql;

	DebugPrint(DebugTrace, "IsochCompleted");

	stack = IoGetCurrentIrpStackLocation(stDevice->Irp);
	if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
		stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		PURB urb = (PURB)stack->Parameters.Others.Argument1;
		switch (urb->UrbHeader.Function)
		{
		case URB_FUNCTION_ISOCH_TRANSFER:
			if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(urb->UrbIsochronousTransfer.TransferFlags))
			{
				DebugPrint(DebugError, "StartFrame %d %d", m_lCurrentFrame, urb->UrbIsochronousTransfer.NumberOfPackets);

				urb->UrbIsochronousTransfer.Hdr.Status = 0;
				stDevice->Irp->IoStatus.Status = 0;
				m_IsochCompleting.PushTask(stDevice);
			}
			break;
		}
	}
	return false;
}

// isoch completing
VOID CcUrbIsoch::stIsochCompleting(Thread::CThreadPoolTask *pTask, PVOID Context)
{
	CcUrbIsoch *pBase = (CcUrbIsoch*)Context;

	while (pBase->m_pBase && pBase->m_pBase->m_bWork)
	{
		PIRP_NUMBER irpNumber = (PIRP_NUMBER)pTask->WaitTask();
		if (irpNumber == NULL)
		{
			break;
		}
		pBase->m_pBase->CompleteIrp(irpNumber);
	}
}

// Cacl current frame
NTSTATUS CcUrbIsoch::QueryBusTime(PVOID  BusContext, PULONG  CurrentFrame)
{
	*CurrentFrame = GetCurrentFrame ();
	return STATUS_SUCCESS;
}

// restart timer for auto completed isoch out
void CcUrbIsoch::RestartTimer()
{
	m_timerIsoch.CancelTimer();
	m_timerIsoch.SetTimer(m_DueTimeIsoch);
}

// clear waiting irp
void CcUrbIsoch::ClearIsochIrp()
{
	PISOCH_SAVE     pIsoch;
	PIRP_NUMBER		stDevice;

	DebugPrint(DebugInfo, "Clearing isoch irp");
	m_spinIsoch.Lock();
	while (m_listIsoch.GetCount())
	{
		pIsoch = (PISOCH_SAVE)m_listIsoch.GetItem(0);
		IsochDelete(pIsoch);
	}
	m_spinIsoch.UnLock();

	// cancel isoch waiting completed
	m_IsochCompleting.PushTask(NULL);
	m_IsochCompleting.PushTask(NULL);
	while (stDevice = (PIRP_NUMBER)m_IsochCompleting.WaitTask())
	{
		m_pBase->CompleteIrp(stDevice);
	}
}


ULONG CcUrbIsoch::GetCurrentFrame()
{
	m_spinIsoch.Lock();
	if (m_lDelta == 0 && m_lLastTime != 0)
	{
		ULONG lCur = KeQueryTimeIncrement();
		lCur -= m_lLastTime;
		lCur /= 10000;
		m_lDelta = lCur;
	}
	m_lCurrentFrame += m_lDelta;
	m_lDelta = 0;
	m_lLastTime = KeQueryTimeIncrement();
	m_spinIsoch.UnLock();

	return m_lCurrentFrame;
}

void CcUrbIsoch::BuildIsochStartFrame(URB_FULL *pUrb)
{
}

void CcUrbIsoch::CompletingIrp(PIRP_NUMBER stDevice)
{
	if (!m_IsochCompleting.IsRunning ())
		m_IsochCompleting.Start(stIsochCompleting, this);

	m_IsochCompleting.PushTask(stDevice);
}