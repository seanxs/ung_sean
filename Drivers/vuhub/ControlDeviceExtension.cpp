/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ControlDeviceExtension.cpp

Abstract:

   Device class that third ring interacts with. 
     - add virtual USB device
     - delete virtual USB device
     - data transfer between real and virtual devices

Environment:

    Kernel mode

--*/
#include "ControlDeviceExtension.h"
#include "VirtualUsbHub.h"
#include "PdoExtension.h"
#include "UsbDevToTcp.h"
#include "Core\SystemEnum.h"
#include "Core\Registry.h"
#include "Common\common.h"

evuh::CControlDeviceExtension::CControlDeviceExtension(PVOID pVHub)
    : CHubStubExtension (pVHub)
{
    SetDebugName ("CControlDeviceExtension");
    m_uInstance = 4;
	RtlInitUnicodeString (&m_LinkName, L"\\DosDevices\\VUHControl");
	RtlInitUnicodeString (&m_DeviceName, L"\\Device\\VUHControl");

	m_poolCommands.Start(stThreadCommand, this);
}

evuh::CControlDeviceExtension::~CControlDeviceExtension(void)
{
}

CHAR*  DevCtrlToString (ULONG uCommand)
{
	switch (uCommand) 
	{
	case IOCTL_VUHUB_ADD:
		return "IOCTL_VUHUB_ADD";
	case IOCTL_VUHUB_REMOVE:
		return "IOCTL_VUHUB_REMOVE";
	case IOCTL_VUHUB_IRP_TO_TCP:
		return "IOCTL_VUHUB_IRP_TO_TCP";
	case IOCTL_VUHUB_IRP_ANSWER_TCP:
		return "IOCTL_VUHUB_IRP_ANSWER_TCP";
	case IOCTL_VUHUB_SET_USER_SID:
		return "IOCTL_VUHUB_SET_USER_SID";
	default:
		break;
	}	
	return "unknown";
}

NTSTATUS evuh::CControlDeviceExtension::DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;

	PIO_STACK_LOCATION  irpStack;
	NTSTATUS            status = STATUS_INVALID_PARAMETER;
	ULONG				uCommand;

	++m_irpCounter;
	irpStack = IoGetCurrentIrpStackLocation (Irp);
	uCommand = irpStack->Parameters.DeviceIoControl.IoControlCode;

	DebugPrint (DebugSpecial, "DispatchDevCtrl %s", DevCtrlToString (uCommand));


	IoSetCancelRoutine (Irp, CancelControl);

	switch (uCommand) 
	{
	case IOCTL_VUHUB_ADD:
	case IOCTL_VUHUB_REMOVE:
		status = STATUS_PENDING;
		IoMarkIrpPending(Irp);
		m_poolCommands.PushTask(Irp);
		break;
	case IOCTL_VUHUB_IRP_TO_TCP:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof (IRP_SAVE))
		{
            status = pHub->GetIrpToTcp (Irp);
			return status;
		}
		break;
	case IOCTL_VUHUB_IRP_ANSWER_TCP:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof (IRP_SAVE))
		{
            status = pHub->AnswerTcpToIrp (Irp);
		}
		break;
	case IOCTL_VUHUB_SET_DOSDEV_PREF:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
			status = pHub->SetDosDevPref (Irp, (PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer);
		}
		break;
	case IOCTL_VUHUB_SET_USER_SID:
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof (USB_DEV_SHARE))
		{
			status = pHub->SetUserSid (Irp, (PUSB_DEV_SHARE)Irp->AssociatedIrp.SystemBuffer);
		}
		break;
	case IOCTL_VUHUB_IRP_TO_TCP_SET:
	case IOCTL_VUHUB_IRP_ANSWER_SET:
		{
			status = pHub->SetPoolTcp(Irp);
		}
		break;
	case IOCTL_VUHUB_IRP_TO_TCP_CLR:
	case IOCTL_VUHUB_IRP_ANSWER_CLR:
		{
			status = pHub->ClrPoolTcp(Irp);
		}
		break;
	case IOCTL_VUHUB_VERSION:
		if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (ULONG))
		{
			PULONG lVer = (PULONG)Irp->AssociatedIrp.SystemBuffer;
			*lVer = uDriverVerison;
			Irp->IoStatus.Information = sizeof(ULONG);
			status = STATUS_SUCCESS;
		}

	default:
        DebugPrint (DebugWarning, "not support irp");
		break;
	}

	if (STATUS_PENDING != status)
	{
		Irp->IoStatus.Status = status;

		if (!CUsbDevToTcp::VerifyMultiCompleted (Irp))
		{
			IoSetCancelRoutine (Irp, NULL);
			IoCompleteRequest (Irp, IO_NO_INCREMENT);
			DebugPrint (DebugInfo, "DispatchDevCtrl completed %d", status);
		}
		--m_irpCounter;
	}
	else
	{
		DebugPrint (DebugInfo, "DispatchDevCtrl STATUS_PENDING");
	}
	return status;
}


VOID evuh::CControlDeviceExtension::CancelControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    PIRP	pIrpControl;
	PIO_STACK_LOCATION  irpStack;

	if (!DeviceObject)
	{
		IoReleaseCancelSpinLock ( Irp->CancelIrql );
		return;
	}

	CControlDeviceExtension		*pControl = (CControlDeviceExtension*)CBaseDeviceExtension::GetExtensionClass (DeviceObject);
	CPdoExtension	*pPdo;
    evuh::CUsbDevToTcp    *pTcp;
	PIRP			pIrpSave; 

	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo && pPdo->GetPnpState () != DriverBase::Deleted)
	{
        pTcp = pPdo->m_pTcp;
        if (pTcp)
        {
            ++pTcp->m_WaitUse;

			pTcp->m_spinUsbOut.Lock ();
			for (int a =0;a < pTcp->m_listGetIrpToTcp.GetCount (); a++)
			{
				// Get first element 
				pIrpSave = (PIRP)pTcp->m_listGetIrpToTcp.GetItem (a);

				if (Irp == pIrpSave)
				{
					pTcp->m_listGetIrpToTcp.Remove (a);
					if (pTcp->VerifyMultiCompleted (Irp))
					{
    					PIRP_SAVE pSave = (PIRP_SAVE)Irp->AssociatedIrp.SystemBuffer;
						pSave->NeedSize = 0;
						Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;//STATUS_IO_TIMEOUT;
						Irp->IoStatus.Information = sizeof (IRP_SAVE);

    					// Complete control device
						IoCompleteRequest (Irp, IO_NO_INCREMENT);
						--pControl->m_irpCounter;
						break;
					}
				}
			}
			pTcp->m_spinUsbOut.UnLock ();

            --pTcp->m_WaitUse;
        }
	}

	irpStack = IoGetCurrentIrpStackLocation (Irp);
	pControl->DebugPrint (DebugError, "CanctlControl irp - 0x%x", Irp);
	IoReleaseCancelSpinLock( Irp->CancelIrql );

    return;

}

NTSTATUS evuh::CControlDeviceExtension::DispatchCleanup (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	USB_DEV	usbDev;
	CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;

	DebugPrint (DebugTrace, "DispatchClose");

	++m_irpCounter;

	CPdoExtension	*pPdo;
	pPdo = (CPdoExtension*)Irp->Tail.Overlay.OriginalFileObject->FsContext;
	if (pPdo)
	{
		usbDev.DeviceAddress = pPdo->m_uAddressDevice;
		usbDev.HostIp = pPdo->m_HostIp;
		usbDev.ServerIpPort = pPdo->m_uServerPort;
		pHub->RemovePdo (Irp, &usbDev);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounter;
	return STATUS_SUCCESS;
}

LONG evuh::CControlDeviceExtension::PauseAddRemove()
{
	LONG	lRet = 1000;
	CRegistry reg;
	CUnicodeString str(*DriverBase::CBaseDeviceExtension::GetRegistryPath(), NonPagedPool);

	str += L"\\Parameters";

	if (NT_SUCCESS(reg.Open(str)))
	{
		reg.GetNumber(CUnicodeString(L"PauseAddDel"), lRet);
	}

	return lRet;
}

VOID evuh::CControlDeviceExtension::stThreadCommand(Thread::CThreadPoolTask *pTask, PVOID Context)
{
	PIRP	pIrp;
	PIO_STACK_LOCATION  irpStack;
	NTSTATUS            status = STATUS_INVALID_PARAMETER;
	DWORD				uCommand;
	evuh::CControlDeviceExtension *pControl = (evuh::CControlDeviceExtension*)Context;
	CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)pControl->m_pVHub;

	while (true)
	{
		pIrp = (PIRP)pTask->WaitTask();
		if (NULL == pIrp)
		{
			break;
		}

		irpStack = IoGetCurrentIrpStackLocation(pIrp);
		uCommand = irpStack->Parameters.DeviceIoControl.IoControlCode;
		switch (uCommand)
		{
		case IOCTL_VUHUB_ADD:
			if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof(USB_DEV))
			{
				status = pHub->AddNewPdo(pIrp, (PUSB_DEV)pIrp->AssociatedIrp.SystemBuffer);
			}
			break;
		case IOCTL_VUHUB_REMOVE:
			if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof(USB_DEV))
			{
				status = pHub->RemovePdo(pIrp, (PUSB_DEV)pIrp->AssociatedIrp.SystemBuffer);
			}
			break;
		default:
			ASSERT(false);
		}
		pIrp->IoStatus.Status = status;
		IoSetCancelRoutine(pIrp, NULL);
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		--pControl->m_irpCounter;
		Thread::Sleep(pControl->PauseAddRemove());
	}
}
