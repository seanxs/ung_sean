/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   fusbhub.cpp

Abstract:

   USB device filter class. It is used for USB devices interception.

Environment:

    Kernel mode

--*/
#include "PdoStubExtension.h"
#include "BusFilterExtension.h"
#include "UsbPortToTcp.h"
#include "ControlDeviceExtension.h"
#include "DebugPrintUrb.h"
#include "UnicIndef.h"

// ***********************************************************************
//						Consts
// ***********************************************************************

WCHAR *CPdoStubExtension::m_szBusQueryDeviceID = L"UsbEStub\\Devices\0";
int CPdoStubExtension::m_iSizeBusQueryDeviceID = sizeof (L"UsbEStub\\Devices\0");
WCHAR *CPdoStubExtension::m_szBusQueryHardwareIDs = L"UsbEStub\0";
int CPdoStubExtension::m_iSizeBusQueryHardwareIDs = sizeof (L"UsbEStub\0");
WCHAR *CPdoStubExtension::m_szBusQueryCompatibleIDs = L"*EltimaCompatibleStub\0";
int CPdoStubExtension::m_iSizeBusQueryCompatibleIDs = sizeof (L"*EltimaCompatibleStub\0");
WCHAR *CPdoStubExtension::m_szDeviceTextDescription = L"Usb Stub (Eltima software)";
int CPdoStubExtension::m_iSizeDeviceTextDescription = sizeof (L"Usb Stub (Eltima software)");
#define ARR_DIM(a) (sizeof(a) / sizeof(a[0]))

static const GUID GUID_ELTIMA_STUB = 
{ 0x1c27b73d, 0x29af, 0x4044, { 0xb9, 0x1c, 0x41, 0x16, 0xd4, 0xef, 0xa6, 0x96 } };

// ***********************************************************************
//						Constructor / destructor
// ***********************************************************************

CPdoStubExtension::CPdoStubExtension (CDeviceRelations *pBus) 
	: CFilterExtension ()
	, m_uTcpPort (0)
	, m_uDeviceAddress (0)
	, m_pParent (NULL)
	, m_bIsFilter (true)
	, m_pBus (pBus)
	, m_uInstance (0)
	, m_bDelete (false)
	, m_bMain (false)
	, m_PhysicalDeviceObject (NULL)
	, m_ConfigurationHandle (NULL)
    , m_IncInterface (0)
	, m_szCompapatible (NULL)
	, m_bRestart (0)
	, CPdoBase (0, pBus)
{
	static TempPort = 0;
	m_uTcpPort = ++TempPort;

	RtlZeroMemory (&m_Interface, sizeof (USB_BUS_INTERFACE_USBDI_V3_FULL));
    RtlZeroMemory (&m_InterfaceUsbCamd, sizeof (USBCAMD_INTERFACE));
	RtlZeroMemory (&m_szDevId, sizeof (m_szDevId));

	ClearDeviceDesc ();

	SetDebugName ("CPdoStubExtension");

	KeInitializeEvent(&m_eventRemove, NotificationEvent, FALSE);
}

CPdoStubExtension::~CPdoStubExtension ()
{
    ClearInterfaceInfo ();
}

// ***********************************************************
//					32 <-> 64
// ***********************************************************
USBD_PIPE_HANDLE_DIFF CPdoStubExtension::GetPipeHandleDiff (USBD_PIPE_HANDLE Handle)
{
	DebugPrint (DebugTrace, "GetPipeHandleDiff");
    return USBD_PIPE_HANDLE_DIFF ((ULONG_PTR)Handle);
}

USBD_PIPE_HANDLE CPdoStubExtension::GetPipeHandle (USBD_PIPE_HANDLE_DIFF Handle)
{
    PUSBD_INTERFACE_INFORMATION pInterface;
    int iCount;
    ULONG iCountPipe;

    DebugPrint (DebugTrace, "GetPipeHandle");

    m_spinPdo.Lock ();
    for (iCount = 0; iCount < m_listInterface.GetCount (); iCount++)
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem (iCount);

		if (pInterface)
		{
			for (iCountPipe = 0; iCountPipe < pInterface->NumberOfPipes; iCountPipe++)
			{
				if (USBD_PIPE_HANDLE_DIFF((ULONG_PTR)pInterface->Pipes[iCountPipe].PipeHandle) == Handle)
				{
					m_spinPdo.UnLock();
					return pInterface->Pipes[iCountPipe].PipeHandle;
				}
			}
		}
		else
		{
			ASSERT(false);
		}
    }

    m_spinPdo.UnLock ();
    return USBD_PIPE_HANDLE ((ULONG_PTR) Handle);
}

UCHAR CPdoStubExtension::GetPipeEndpoint (ULONG64 Handle, bool bLock)
{
    PUSBD_INTERFACE_INFORMATION pInterface;
    int iCount;
    ULONG iCountPipe;

    DebugPrint (DebugTrace, "GetPipeEndpoint");

	if (bLock)
	{
		m_spinPdo.Lock ();
	}
	for (iCount = 0; iCount < m_listInterface.GetCount(); iCount++)
	{
		pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem(iCount);
		if (pInterface)
		{
			for (iCountPipe = 0; iCountPipe < pInterface->NumberOfPipes; iCountPipe++)
			{
				if (ULONG64((ULONG_PTR)pInterface->Pipes[iCountPipe].PipeHandle) == Handle)
				{
					if (bLock)
					{
						m_spinPdo.UnLock();
					}
					return pInterface->Pipes[iCountPipe].EndpointAddress;
				}
			}
		}
		else
		{
			ASSERT(false);
		}
    }

	if (bLock)
	{
		m_spinPdo.UnLock ();
	}
    return 0;
}

LONG CPdoStubExtension::GetPipeType (ULONG64 Handle, UCHAR *Class)
{
    PUSBD_INTERFACE_INFORMATION pInterface;
    int iCount;
    ULONG iCountPipe;

    DebugPrint (DebugTrace, "GetPipeHandle");

    m_spinPdo.Lock ();
    for (iCount = 0; iCount < m_listInterface.GetCount (); iCount++)
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem (iCount);
		if (pInterface)
		{
			for (iCountPipe = 0; iCountPipe < pInterface->NumberOfPipes; iCountPipe++)
			{
				if (ULONG64 ((ULONG_PTR) pInterface->Pipes [iCountPipe].PipeHandle) == Handle)
				{
					m_spinPdo.UnLock ();
					if (Class)
					{
						*Class = pInterface->Class;
					}
					return pInterface->Pipes [iCountPipe].PipeType;
				}
			}
		}
		else
		{
			ASSERT(false);
		}
    }

    m_spinPdo.UnLock ();
    return -1;
}

NTSTATUS CPdoStubExtension::SetNewInterface (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo)
{
    PUSBD_INTERFACE_INFORMATION pInterface;
    int iCount = 0;

    DebugPrint (DebugTrace, "SetNewInterface");

    m_spinPdo.Lock ();
    for (iCount = 0; iCount < m_listInterface.GetCount (); iCount++)
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.GetItem (iCount);
		if (pInterface)
		{
			if (pInterface->InterfaceNumber == pInterfaceInfo->InterfaceNumber)
			{
				m_listInterface.Remove(iCount);
				ExFreePool(pInterface);
				pInterface = NULL;
				pInterface = (PUSBD_INTERFACE_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, pInterfaceInfo->Length, tagUsb);
				if (pInterface)
				{
					m_listInterface.Add(pInterface);
				}
				else
				{
					m_spinPdo.UnLock();
					return STATUS_NO_MEMORY;
				}
				RtlCopyMemory(pInterface, pInterfaceInfo, pInterfaceInfo->Length);

				break;
			}
		}
		else
		{
			ASSERT(false);
		}
    }
    if (iCount == m_listInterface.GetCount ())
    {
        pInterface = (PUSBD_INTERFACE_INFORMATION)ExAllocatePoolWithTag (NonPagedPool, pInterfaceInfo->Length, tagUsb);
        if (pInterface)
        {
            m_listInterface.Add (pInterface);
            RtlCopyMemory (pInterface, pInterfaceInfo, pInterfaceInfo->Length);
        }
        else
        {
            m_spinPdo.UnLock ();
            return STATUS_NO_MEMORY;
        }
    }
    m_spinPdo.UnLock ();
    return STATUS_SUCCESS;
}

void CPdoStubExtension::ClearInterfaceInfo ()
{
    PUSBD_INTERFACE_INFORMATION pInterface;

    DebugPrint (DebugTrace, "ClearInterfaceInfo");

    m_spinPdo.Lock ();
    while (m_listInterface.GetCount ())
    {
		// get interface
        pInterface = (PUSBD_INTERFACE_INFORMATION)m_listInterface.PopFirst();
		m_spinPdo.UnLock ();

		if (!pInterface)
		{
			continue;
		}
		// reset interface
		ResetInterface (pInterface);

		// clear memory
        ExFreePool (pInterface);

		m_spinPdo.Lock ();
    }
    m_spinPdo.UnLock ();
}

NTSTATUS CPdoStubExtension::InitInterfaceInfo (PUSBD_INTERFACE_INFORMATION	pInterfaceInfo, USHORT uCount)
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
			m_spinPdo.Lock();
            m_listInterface.Add (pInterface);
			m_spinPdo.UnLock();
	    }
        else
        {
            return STATUS_NO_MEMORY;
        }
		pInterfaceInfo = (PUSBD_INTERFACE_INFORMATION) (((char*)pInterfaceInfo) + pInterfaceInfo->Length);
    }
    return STATUS_SUCCESS;
}


NTSTATUS CPdoStubExtension::PnpRemove (PIRP Irp)
{
	NTSTATUS            status = STATUS_SUCCESS;
	TPtrUsbPortToTcp	pTcp = m_pParent;


	DebugPrint(DebugTrace,"PnpRemove");

	// Delete child pdo
	DebugPrint (DebugSpecial, "m_eventRemove true");
	KeSetEvent(&m_eventRemove, 0, FALSE);

	if (pTcp.get())	
	{
		pTcp->m_bAdditionDel = true;
		pTcp->m_bCancel = true;
		pTcp->CancelWairIrp ();
	}

    if (m_Interface.InterfaceDereference)
    {
        while (m_IncInterface > 0)
        {
            m_Interface.InterfaceDereference (m_Interface.BusContext);
            --m_IncInterface;
        }
    }
    if (m_InterfaceUsbCamd.Interface.InterfaceDereference)
    {
        while (m_IncInterfaceUsbCamd > 0)
        {
            m_InterfaceUsbCamd.Interface.InterfaceDereference (m_InterfaceUsbCamd.Interface.Context);
            --m_IncInterfaceUsbCamd;
        }
    }

	if (pTcp.get())	
	{
		DebugPrint(DebugSpecial,"DispatchPnp Cancel irp shared decices");
		pTcp->m_bCancel = false;
		pTcp->CancelControlIrp ();

		if (pTcp->m_bDelayedDelete)
		{
			DebugPrint(DebugSpecial,"DispatchPnp Delete UsbToTcp");

			pTcp->StopTcpServer ();
		}
	}

	m_ConfigurationHandle = NULL;
	m_bRestart = 0;

	// for Fritz card
	if (pTcp.get())	{
		ClearInterfaceInfo ();
		if (m_DeviceDescription.bNumConfigurations > 1)
		{
			DeconfigureDevice (m_NextLowerDriver);
		}
	}
	ClearDeviceDesc ();

	// waiting pending irp
	--m_irpCounter;
	m_irpCounter.WaitRemove ();

	// send to parent
	IoSkipCurrentIrpStackLocation(Irp);
	status = IoCallDriver(m_NextLowerDriver, Irp);


	SetNewPnpState (DriverBase::Deleted);

	if (pTcp.get() && pTcp->m_bDelayedDelete)
	{
		DebugPrint (DebugSpecial, "PnpRemove pTcp->m_bDelayedDelete");
		pTcp = m_pParent = NULL;
		m_bIsFilter = true;
	}
	
	if (pTcp.get())	{
		DebugPrint (DebugSpecial, "PnpRemove %d %x", m_uDeviceAddress, pTcp.get ());
		m_bIsFilter = false;
		DebugPrint (DebugSpecial, "PnpRemove m_bIsFilter2 0");

	}
	else
	{
		if (m_uDeviceAddress == 0)
		{
			DebugPrint (DebugSpecial, "PnpRemove m_bIsFilter3 0");
			m_bIsFilter = false;
		}
		else
		{
			DebugPrint (DebugSpecial, "PnpRemove m_bIsFilter4 1");
			m_bIsFilter = true;
		}
	}

	DebugPrint(DebugSpecial,"PnpRemove DispatchPnp flag %d %d %d", m_bDelete, m_pBus, m_pBus && !m_pBus->IsExistPdo (m_PhysicalDeviceObject).get ());
	if (m_bDelete || (m_pBus && !m_pBus->IsExistPdo (m_PhysicalDeviceObject).get()))
	{
		DebugPrint(DebugSpecial,"PnpRemove DispatchPnp remove device");

		if (pTcp.get())
		{
			pTcp->StopTcpServer ();
		}
		DetachFromDevice();
		CUnicId::Instance ()->DelUnicId (m_uInstance);
		DecMyself();
	}
	else
	{
		DebugPrint(DebugSpecial,"PnpRemove DispatchPnp stop device");
		if (m_PhysicalDeviceObject != m_NextLowerDriver)
		{
			DebugPrint (DebugSpecial, "PnpRemove DispatchPnp m_PhysicalDeviceObject != m_NextLowerDriver");
			ULONG ExtensionFlags;

			DetachFromDevice();
			m_irpCounter.Reset ();
			KeClearEvent (&m_eventRemove);

			if (NT_SUCCESS (AttachToDevice (m_DriverObject, m_PhysicalDeviceObject)))
			{
				DebugPrint (DebugSpecial, "PnpRemove AttachToDevice P_%x N_%x A_%x", m_PhysicalDeviceObject, m_NextLowerDriver, m_Self);
			}
			else
			{
				if (m_pBus)
				{
					m_pBus->RemovePdo(m_PhysicalDeviceObject);
					if (pTcp.get())					
					{
						pTcp->m_bNeedDeviceRelations = true;
					}
				}
				DebugPrint(DebugError, "PnpRemove AttachToDevice P_%x", m_PhysicalDeviceObject);
				DecMyself();
				return status;
			}

			if (m_pBus)
			{
				DebugPrint (DebugSpecial, "PnpRemove Removing Pdo");
				{
					DebugPrint (DebugSpecial, "PnpRemove Removed Pdo ok");
					if (pTcp.get())					
					{
						pTcp->m_bNeedDeviceRelations = true;
					}
				}
			}
		}
		else
		{
			m_irpCounter.Reset ();
			KeClearEvent (&m_eventRemove);
		}
	}
	return status;
}

NTSTATUS CPdoStubExtension::PnpStart (PIRP Irp)
{
	NTSTATUS            status = STATUS_SUCCESS;
	KEVENT				startDeviceEvent;
	TPtrUsbPortToTcp	pTcp = m_pParent;

	DebugPrint(DebugTrace,"PnpStart");

	KeInitializeEvent(&startDeviceEvent, NotificationEvent, FALSE);

	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, 
						   IrpCompletionRoutine, 
						   (PVOID)&startDeviceEvent, 
						   TRUE, 
						   TRUE, 
						   TRUE);

	status = IoCallDriver(m_NextLowerDriver, Irp);

	if(status == STATUS_PENDING) 
	{
		KeWaitForSingleObject(&startDeviceEvent, 
							  Executive, 
							  KernelMode, 
							  FALSE, 
							  NULL);

		status = Irp->IoStatus.Status;
	}

	SetNewPnpState (DriverBase::Started);

    status = STATUS_SUCCESS;
    Irp->IoStatus.Status = STATUS_SUCCESS;

	{
		InitDeviceDesc ();
	}

	ReadandSelectDescriptors ();

		if (pTcp.get())		
		{
			m_pParent->m_spinBase.Lock ('SPNP');

			// Read USB description
			pTcp->m_bCancelControl = false;
			pTcp->m_bAdditionDel = false;
			pTcp->StartTcpServer (this);
			if (pTcp->m_irpWait)
			{
				PIRP	irpWait = pTcp->m_irpWait;

				pTcp->m_bCancelControl = false;
				pTcp->m_irpWait = NULL;
				pTcp->m_spinBase.UnLock ();

				DebugPrint (DebugTrace, "IoComplitionWorkItmWaitIrp");
				pTcp->CompleteWaitIrp (irpWait, !m_bRestart);
				m_bRestart = 0;

				pTcp->m_spinBase.Lock ('SPN2');
			}
			pTcp->m_spinBase.UnLock ();
		}
		if (pTcp.get())		{
			DebugPrint (DebugError, "m_uDeviceAddress == 0");
		}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounter;
	return STATUS_SUCCESS;
}
// ***********************************************************************
//						virtual major functions
// ***********************************************************************
NTSTATUS CPdoStubExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	DebugPrint (DebugTrace, "%s", PnPMinorFunctionString (Irp));
	DebugPrint (DebugSpecial, "%s", PnPMinorFunctionString (Irp));
    
    NTSTATUS                status = STATUS_SUCCESS;
	PIO_STACK_LOCATION		irpStack;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	DebugPrint (DebugTrace, "DispatchPnp");
	++m_irpCounter;

	// get device description
	DebugPrint (DebugSpecial, "%s %d %x %d", PnPMinorFunctionString (Irp), m_uDeviceAddress, m_pParent, m_bIsFilter);

	// Remove device. 
	if (irpStack->MinorFunction == IRP_MN_REMOVE_DEVICE)
	{
		return PnpRemove (Irp);
	}
	else if (irpStack->MinorFunction == IRP_MN_START_DEVICE)
	{
		return PnpStart (Irp);
	}
	else if (m_bIsFilter)
	{
		--m_irpCounter;

		return CFilterExtension::DispatchPnp (DeviceObject, Irp);
	}

	switch (irpStack->MinorFunction)
    {
    case IRP_MN_QUERY_DEVICE_TEXT:
		if ((DeviceObject->AttachedDevice == NULL) ||
			(DeviceObject->AttachedDevice->Flags & DO_POWER_PAGABLE)) 
		{

			DeviceObject->Flags |= DO_POWER_PAGABLE;
		}

		IoCopyCurrentIrpStackLocationToNext(Irp);

		IoSetCompletionRoutine(
			Irp,
			CompletionRoutineDeviceText,
			NULL,
			TRUE,
			TRUE,
			TRUE
			);

		DebugPrint(DebugSpecial, "end %s", PnPMinorFunctionString(Irp));

		return IoCallDriver(m_NextLowerDriver, Irp);

	case IRP_MN_QUERY_ID:
		if ((DeviceObject->AttachedDevice == NULL) ||
			(DeviceObject->AttachedDevice->Flags & DO_POWER_PAGABLE)) 
		{

			DeviceObject->Flags |= DO_POWER_PAGABLE;
		}

		IoCopyCurrentIrpStackLocationToNext(Irp);

		    if (m_pBus)
		    {
				ApplyXPSilentInstallHack (m_uInstance);
		    }

		IoSetCompletionRoutine(
			Irp,
			CompletionRoutineDeviceId,
			NULL,
			TRUE,
			TRUE,
			TRUE
			);

		DebugPrint(DebugSpecial, "end %s", PnPMinorFunctionString(Irp));

		return IoCallDriver(m_NextLowerDriver, Irp);
	case IRP_MN_QUERY_CAPABILITIES:
		DebugPrint(DebugSpecial, "end %s", PnPMinorFunctionString(Irp));

        return PnpQueryDeviceCaps (DeviceObject, Irp);		
	}

	
	DebugPrint(DebugSpecial, "end %s", PnPMinorFunctionString(Irp));
	--m_irpCounter;
	NTSTATUS ret = CFilterExtension::DispatchPnp (DeviceObject, Irp);

	DebugPrint(DebugSpecial, "end PNP");
	return ret;
}

NTSTATUS CPdoStubExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "DispatchDefault");
	if (m_bIsFilter)
	{
		return CFilterExtension::DispatchDefault (DeviceObject, Irp);
	}
	else
	{
		++m_irpCounter;
		Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--m_irpCounter;
		return STATUS_NOT_SUPPORTED;
	}
}

NTSTATUS CPdoStubExtension::DispatchSysCtrl	(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	return CFilterExtension::DispatchDefault (DeviceObject, Irp);
}

NTSTATUS CPdoStubExtension::DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    return CFilterExtension::DispatchDefault (DeviceObject, Irp);
}

// ***********************************************************************
//						Attach to Device
// ***********************************************************************
NTSTATUS CPdoStubExtension::AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
	NTSTATUS			status;

	DebugPrint (DebugTrace, "AttachToDevice");

	if (NT_SUCCESS (status = CFilterExtension::AttachToDevice (DriverObject, PhysicalDeviceObject)))
	{
		m_PhysicalDeviceObject = PhysicalDeviceObject;
	}

	BuildDevId ();

	return status;
}

void CPdoStubExtension::BuildDevId ()
{
	NTSTATUS	status;

	DebugPrint (DebugTrace, "BuildDevId");

	if (m_uDeviceAddress)
	{
		BYTE								pBuff [1024];
		USB_NODE_CONNECTION_INFORMATION		*pInfo = (USB_NODE_CONNECTION_INFORMATION*)pBuff;

		CBusFilterExtension *pBase = (CBusFilterExtension*)m_pBus;

		pInfo->ConnectionIndex = m_uDeviceAddress;

		status = pBase->GetUsbDeviceConnectionInfo (pBase->m_PhysicalDeviceObject, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, pInfo, sizeof (pBuff));

		if (NT_SUCCESS (status))
		{
			if (pInfo->ConnectionStatus != NoDeviceConnected)
			{
				RtlStringCbPrintfW(m_szDevId, sizeof(m_szDevId)
										, L"USB\\VID_%04x&PID_%04x&REV_%04x" 
										, pInfo->DeviceDescriptor.idVendor
										, pInfo->DeviceDescriptor.idProduct
										, pInfo->DeviceDescriptor.bcdDevice);

				if (pInfo->DeviceDescriptor.iSerialNumber)
				{
					PUSB_DESCRIPTOR_REQUEST stringDescReq;
					PUSB_STRING_DESCRIPTOR  stringDesc;

					stringDescReq = (PUSB_DESCRIPTOR_REQUEST)pBuff;
					stringDesc = (PUSB_STRING_DESCRIPTOR)(stringDescReq+1);

					stringDescReq->ConnectionIndex = m_uDeviceAddress;
					stringDescReq->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8) | pInfo->DeviceDescriptor.iSerialNumber;
					stringDescReq->SetupPacket.wIndex = 0x0409;
					stringDescReq->SetupPacket.wLength = (USHORT)(1024 - sizeof(USB_DESCRIPTOR_REQUEST));

					status = pBase->GetUsbDeviceConnectionInfo (pBase->m_PhysicalDeviceObject, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, stringDescReq, sizeof (pBuff));

					if (NT_SUCCESS (status))
					{
						if (stringDesc->bDescriptorType == USB_STRING_DESCRIPTOR_TYPE && stringDesc->bLength % 2 == 0)
						{
							RtlStringCbCatW(m_szDevId, sizeof(m_szDevId), L"\\");
							RtlStringCbCatNW(m_szDevId, sizeof(m_szDevId), stringDesc->bString, ((stringDesc->bLength - sizeof(UCHAR) * 2) / 2));
						}
					}
				}
			}
		}
	}
}

NTSTATUS CPdoStubExtension::CompletionRoutineDeviceText (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    CPdoStubExtension		*pBase = (CPdoStubExtension*)GetExtensionClass (DeviceObject);

    pBase->DebugPrint(DebugTrace, "CompletionRoutineDeviceText");

    if (Irp->PendingReturned) 
	{
        IoMarkIrpPending(Irp);
    }

	if (NT_SUCCESS (Irp->IoStatus.Status))
	{
		if (Irp->IoStatus.Information != 0)
		{
            pBase->DebugPrint(DebugInfo, "IRP_MN_QUERY_DEVICE_TEXT - %S", (WCHAR*)Irp->IoStatus.Information);
		}
	}

    //
    // On the way up, pagable might become clear. Mimic the driver below us.
    //
    if (!(pBase->m_NextLowerDriver->Flags & DO_POWER_PAGABLE)) 
	{

        DeviceObject->Flags &= ~DO_POWER_PAGABLE;
    }
	--pBase->m_irpCounter;
    return STATUS_SUCCESS;
}

NTSTATUS CPdoStubExtension::CompletionRoutineDeviceId (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    CPdoStubExtension		*pBase = (CPdoStubExtension*)GetExtensionClass (DeviceObject);
	PIO_STACK_LOCATION      stack;

    pBase->DebugPrint(DebugTrace, "CompletionRoutineDeviceId");

    if (Irp->PendingReturned) 
	{
        IoMarkIrpPending(Irp);
    }

	if (NT_SUCCESS (Irp->IoStatus.Status))
	{
		if (Irp->IoStatus.Information != 0)
		{
			stack = IoGetCurrentIrpStackLocation (Irp);

			if (stack->Parameters.QueryId.IdType == BusQueryHardwareIDs)
			{
				if (Irp->IoStatus.Information)
				{
					pBase->m_szCompapatible = (PCWSTR)Irp->IoStatus.Information;
				}
			}

			ExFreePool ((PVOID)Irp->IoStatus.Information);
			Irp->IoStatus.Information = 0;
		}
		pBase->PnpQueryDeviceId (Irp);
	}

    //
    // On the way up, pagable might become clear. Mimic the driver below us.
    //
    if (!(pBase->m_NextLowerDriver->Flags & DO_POWER_PAGABLE)) 
	{

        DeviceObject->Flags &= ~DO_POWER_PAGABLE;
    }
	--pBase->m_irpCounter;
    return STATUS_SUCCESS;
}

NTSTATUS CPdoStubExtension::PnpQueryDeviceId (PIRP Irp)
{
    NTSTATUS                status = STATUS_SUCCESS;
    PIO_STACK_LOCATION      stack;
    PWCHAR                  buffer;
	PWCHAR					temp;
    ULONG                   length;
    
    stack = IoGetCurrentIrpStackLocation (Irp);

	DebugPrint (DebugTrace, "PnpQueryDeviceId");

	switch (stack->Parameters.QueryId.IdType) 
	{
    case BusQueryDeviceID:

        //
        // DeviceID is unique string to identify a device.
        // This can be the same as the hardware ids (which requires a multi
        // sz).
        //
        
        buffer = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_iSizeBusQueryDeviceID, tagUsb);

        if (!buffer) 
		{
           status = STATUS_INSUFFICIENT_RESOURCES;
           break;
        }

        RtlCopyMemory (buffer, m_szBusQueryDeviceID, m_iSizeBusQueryDeviceID);
        Irp->IoStatus.Information = (ULONG_PTR) buffer;

		DebugPrint (DebugInfo, "BusQueryDeviceID - %S %d", buffer, m_iSizeBusQueryDeviceID);
        break;

    case BusQueryInstanceID:
        //
        // total length = number (10 digits to be safe (2^32)) + null wide char
        //
        length = 11 * sizeof(WCHAR);
        
        buffer = (PWCHAR)ExAllocatePoolWithTag (PagedPool, length, tagUsb);
        if (!buffer) 
		{
           status = STATUS_INSUFFICIENT_RESOURCES;
           break;
        }
        //swprintf (buffer, L"%04d", m_uInstance + (ULONG(m_pBus->m_wId)<<16));
		RtlStringCbPrintfW(buffer, length, L"%04d", m_uInstance);
        Irp->IoStatus.Information = (ULONG_PTR) buffer;
		DebugPrint (DebugInfo, "BusQueryInstanceID - %S %d", buffer, length);
		break;

    case BusQueryHardwareIDs:

        //
        // A device has at least one hardware id.
        // In a list of hardware IDs (multi_sz string) for a device, 
        // DeviceId is the most specific and should be first in the list.
        //
        buffer = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_iSizeBusQueryHardwareIDs, tagUsb);
        if (!buffer) 
		{
           status = STATUS_INSUFFICIENT_RESOURCES;
           break;
        }

        RtlCopyMemory (buffer, m_szBusQueryHardwareIDs, m_iSizeBusQueryHardwareIDs);
        Irp->IoStatus.Information = (ULONG_PTR) buffer;
		DebugPrint (DebugInfo, "BusQueryHardwareIDs - %S %d", buffer, m_iSizeBusQueryHardwareIDs);
        break;


    case BusQueryCompatibleIDs:

        //
        // The generic ids for installation of this PDO.
        //
        buffer = (PWCHAR)ExAllocatePoolWithTag (PagedPool, m_iSizeBusQueryCompatibleIDs, tagUsb);
        if (!buffer) 
		{
           status = STATUS_INSUFFICIENT_RESOURCES;
           break;
        }
        RtlCopyMemory (buffer, m_szBusQueryCompatibleIDs, m_iSizeBusQueryCompatibleIDs);

        Irp->IoStatus.Information = (ULONG_PTR) buffer;
		DebugPrint (DebugInfo, "BusQueryCompatibleIDs - %S %d", buffer, m_iSizeBusQueryCompatibleIDs);
        break;
        
    default:

        status = Irp->IoStatus.Status;

    }
    return status;
}

NTSTATUS CPdoStubExtension::PnpQueryDeviceRelations (PIRP Irp)
{
    PIO_STACK_LOCATION   stack;
    PDEVICE_RELATIONS deviceRelations;
    NTSTATUS status = Irp->IoStatus.Status;

    DebugPrint (DebugTrace, "PnpQueryDeviceRelations");

    stack = IoGetCurrentIrpStackLocation (Irp);

    switch (stack->Parameters.QueryDeviceRelations.Type) 
	{
    case BusRelations: // Not handled by PDO
        deviceRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information; 
        if (deviceRelations) 
		{
            //
            // Only PDO can handle this request. Somebody above
            // is not playing by rules.
            //
            DebugPrint(DebugError, "Someone above is handling BusRelations");      

			ExFreePool (deviceRelations);

        }

        deviceRelations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag (PagedPool, sizeof(DEVICE_RELATIONS), tagUsb);
        if (!deviceRelations) 
		{
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
        }

        // 
        // There is only one PDO pointer in the structure 
        // for this relation type. The PnP Manager removes 
        // the reference to the PDO when the driver or application 
        // un-registers for notification on the device. 
        //

		{
			deviceRelations->Count = 0;
		}

        status = STATUS_SUCCESS;
		Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = (ULONG_PTR) deviceRelations;
        break;
    }

    return status;

}

NTSTATUS CPdoStubExtension::PnpQueryDeviceCaps (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "PnpQueryDeviceCaps");
	//
	// On the way down, pagable might become set. Mimic the driver
	// above us. If no one is above us, just set pagable.
	//
	if ((DeviceObject->AttachedDevice == NULL) ||
		(DeviceObject->AttachedDevice->Flags & DO_POWER_PAGABLE)) 
	{

		DeviceObject->Flags |= DO_POWER_PAGABLE;
	}

	IoCopyCurrentIrpStackLocationToNext(Irp);

	IoSetCompletionRoutine(
		Irp,
		CompletionRoutineDeviceCaps,
		NULL,
		TRUE,
		TRUE,
		TRUE
		);

	return IoCallDriver(m_NextLowerDriver, Irp);
}

// ***********************************************************************
//						completion routine
// ***********************************************************************

NTSTATUS CPdoStubExtension::CompletionRoutineDeviceCaps(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    CPdoStubExtension		*pBase = (CPdoStubExtension*)GetExtensionClass (DeviceObject);

    pBase->DebugPrint(DebugTrace, "CompletionRoutineDeviceCaps");

    if (Irp->PendingReturned) 
	{

        IoMarkIrpPending(Irp);
    }

	if (NT_SUCCESS (Irp->IoStatus.Status))
	{
		PIO_STACK_LOCATION      stack;
		PDEVICE_CAPABILITIES    deviceCapabilities;
		NTSTATUS                status;
	    
		stack = IoGetCurrentIrpStackLocation (Irp);

		//
		// Get the packet.
		//
		deviceCapabilities=stack->Parameters.DeviceCapabilities.Capabilities;

		//
		// Set the capabilities.
		//

		if(deviceCapabilities->Version == 1 && deviceCapabilities->Size >= sizeof(DEVICE_CAPABILITIES))
		{
			//
			// Adjust the caps to what your device supports.
			// Our device just supports D0 and D3.
			//

			deviceCapabilities->DeviceState[PowerSystemWorking] = PowerDeviceD0;

			if(deviceCapabilities->DeviceState[PowerSystemSleeping1] != PowerDeviceD0)
				deviceCapabilities->DeviceState[PowerSystemSleeping1] = PowerDeviceD1;
		        
			if(deviceCapabilities->DeviceState[PowerSystemSleeping2] != PowerDeviceD0)
				deviceCapabilities->DeviceState[PowerSystemSleeping2] = PowerDeviceD3;

			if(deviceCapabilities->DeviceState[PowerSystemSleeping3] != PowerDeviceD0)
				deviceCapabilities->DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
		        
			// We can wake the system from D1   
			deviceCapabilities->DeviceWake = PowerDeviceD1;

			
			 //Specifies whether the device hardware supports the D1 and D2
			 //power state. Set these bits explicitly.
			

			deviceCapabilities->DeviceD1 = FALSE; // Yes we can
			deviceCapabilities->DeviceD2 = FALSE;

			
			 //Specifies whether the device can respond to an external wake 
			 //signal while in the D0, D1, D2, and D3 state. 
			 //Set these bits explicitly.
			
		    
			deviceCapabilities->WakeFromD0 = FALSE;
			deviceCapabilities->WakeFromD1 = FALSE; //Yes we can
			deviceCapabilities->WakeFromD2 = FALSE;
			deviceCapabilities->WakeFromD3 = FALSE;


			 //We have no latencies
		    
			deviceCapabilities->D1Latency = 0;
			deviceCapabilities->D2Latency = 0;
			deviceCapabilities->D3Latency = 0;

			 //Ejection supported
		    
			deviceCapabilities->EjectSupported = FALSE;
		    
			//
			// This flag specifies whether the device's hardware is disabled.
			// The PnP Manager only checks this bit right after the device is
			// enumerated. Once the device is started, this bit is ignored. 
			//
			deviceCapabilities->HardwareDisabled = FALSE;
		    
			//
			// Out simulated device can be physically removed.
			//    
			deviceCapabilities->Removable = TRUE;
			//
			// Setting it to TRUE prevents the warning dialog from appearing 
			// whenever the device is surprise removed.
			//
			deviceCapabilities->SurpriseRemovalOK = TRUE;

			// We don't support system-wide unique IDs. 

			deviceCapabilities->UniqueID = TRUE;

			//
			// Specify whether the Device Manager should suppress all 
			// installation pop-ups except required pop-ups such as 
			// "no compatible drivers found." 
			//

			deviceCapabilities->SilentInstall = TRUE;

			//
			// Specifies an address indicating where the device is located 
			// on its underlying bus. The interpretation of this number is 
			// bus-specific. If the address is unknown or the bus driver 
			// does not support an address, the bus driver leaves this 
			// member at its default value of 0xFFFFFFFF. In this example
			// the location address is same as instance id.
			//

			deviceCapabilities->Address = 0xFFFFFFFF;

			//
			// UINumber specifies a number associated with the device that can 
			// be displayed in the user interface. 
			//
			deviceCapabilities->UINumber = pBase->m_uInstance;
		}
	}
    //
    // On the way up, pagable might become clear. Mimic the driver below us.
    //
    if (!(pBase->m_NextLowerDriver->Flags & DO_POWER_PAGABLE)) 
	{

        DeviceObject->Flags &= ~DO_POWER_PAGABLE;
    }
	--pBase->m_irpCounter;
    return STATUS_SUCCESS;
}


void CPdoStubExtension::ApplyXPSilentInstallHack (ULONG uInstance)
{
	PWCHAR path, data;
	NTSTATUS status;
	ULONG i;
	PWCHAR szGiudClass = L"{BA60D326-EE12-4957-B702-7F5EDBE9ECE5}";
	LONG	lSize;
	LONG	lLen;

	static struct 
	{
		PWCHAR	ValueName;
		PWCHAR	ValueData;
	} ClassValues[] = {
		NULL,	L"Eltima Usb Stub",
		L"Class",	L"EltimaUsbStub",
		L"Icon",	L"-20",
		L"NoInstallClass",	L"1",
		L"SilentInstall",	L"1"
	}, ClassInstanceValues[] = {
		L"ProviderName",	L"ELTIMA Software",
		L"DriverDate",	L"11-23-2010",
		L"DriverVersion",	L"3.0.383",
		L"MatchingDeviceId",	L"UsbEStub",
		L"DriverDesc",	L"Usb Stub (Eltima Software)"
	}, EnumValues[] = {
		L"DeviceDesc",	L"Usb Stub (Eltima Software)",
		L"LocationInformation",	L"Usb Stub (Eltima Software)",
		L"ClassGUID", szGiudClass,
		L"Class",	L"EltimaUsbStub",
		L"Mfg",	L"ELTIMA Software",
		L"Service",	L"eustub"
	};

	static ULONG	   uVersion = 0;

	if (uVersion == 0)
	{
		ULONG	   uMinor;
		PsGetVersion (&uVersion, &uMinor, NULL, NULL);
		uVersion *= 100;
		uVersion += uMinor;
	}

	if (uVersion >= 600)
	{
		return;
	}

	lSize = 512;
	path = (PWCHAR)ExAllocatePoolWithTag(PagedPool, lSize * sizeof (WCHAR), 'CHUF');
	do 
	{
		if (!path) 
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		data = path + 256;

		RtlStringCbPrintfW(path, lSize, L"Class");
		status = RtlCreateRegistryKey(RTL_REGISTRY_CONTROL, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}
		RtlStringCbCatW(path, lSize, L"\\");
		RtlStringCbCatW(path, lSize, szGiudClass);
		status = RtlCreateRegistryKey(RTL_REGISTRY_CONTROL, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		for (i = 0; i < ARR_DIM(ClassValues); i++) 
		{
			status = RtlWriteRegistryValue(RTL_REGISTRY_CONTROL, path, ClassValues[i].ValueName, REG_SZ,
					(PVOID)ClassValues[i].ValueData, (wcslen(ClassValues[i].ValueData) + 1) * sizeof(WCHAR));
			if (!NT_SUCCESS(status))
			{
				break;
			}
		}
		if (!NT_SUCCESS(status))
		{
			break;
		}

		lLen = wcslen(path);
		RtlStringCbPrintfW(path + lLen, lSize - lLen * sizeof (WCHAR), L"\\%.4d", uInstance);
		status = RtlCreateRegistryKey(RTL_REGISTRY_CONTROL, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		for (i = 0; i < ARR_DIM(ClassInstanceValues); i++) 
		{
			status = RtlWriteRegistryValue(RTL_REGISTRY_CONTROL, path, ClassInstanceValues[i].ValueName, REG_SZ,
					(PVOID)ClassInstanceValues[i].ValueData, (wcslen(ClassInstanceValues[i].ValueData) + 1) * sizeof(WCHAR));
			if (!NT_SUCCESS(status))
			{
				break;
			}
		}
		if (!NT_SUCCESS(status))
		{
			break;
		}

		RtlStringCbPrintfW(path, lSize, L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum\\UsbEStub");
		status = RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		RtlStringCbCatW(path, lSize, L"\\Devices");
		status = RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}
		lLen = wcslen(path);
		RtlStringCbPrintfW(path + lLen, lSize - lLen * sizeof(WCHAR), L"\\%.4d", uInstance);
		status = RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		for (i = 0; i < ARR_DIM(EnumValues); i++) 
		{
			status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, EnumValues[i].ValueName, REG_SZ,
					(PVOID)EnumValues[i].ValueData, (wcslen(EnumValues[i].ValueData) + 1) * sizeof(WCHAR));
			if (!NT_SUCCESS(status))
			{
				break;
			}
		}
		if (!NT_SUCCESS(status))
		{
			break;
		}

		RtlStringCbPrintfW(data, lSize, L"%s\\%.4d", szGiudClass, uInstance);
		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"Driver", REG_SZ,
			(PVOID)data, (wcslen(data) + 1) * sizeof(WCHAR));
		if (!NT_SUCCESS(status))
		{
			break;
		}

		RtlStringCbCopyW(data, lSize, L"Eltima Usb Stub");
		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"FriendlyName", REG_SZ,
			(PVOID)data, (wcslen(data) + 1) * sizeof(WCHAR));
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"CompatibleIDs", REG_MULTI_SZ,
			(PVOID)m_szBusQueryCompatibleIDs, m_iSizeBusQueryCompatibleIDs);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"HardwareID", REG_MULTI_SZ,
			m_szBusQueryHardwareIDs, m_iSizeBusQueryHardwareIDs);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		i = 0xB4;
		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"Capabilities", REG_DWORD,
			(PVOID)&i, sizeof(ULONG));
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"UINumber", REG_DWORD,
			(PVOID)&uInstance, sizeof(ULONG));
		if (!NT_SUCCESS(status))
		{
			break;
		}

		LONG i = 0;
		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"ConfigFlags", REG_DWORD,
			(PVOID)&i, sizeof(ULONG));
		if (!NT_SUCCESS(status))
		{
			break;
		}
	}while (false);

	{
		if (path)
		{
			ExFreePool(path);
		}
	}
}


NTSTATUS CPdoStubExtension::IrpCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PKEVENT event = (PKEVENT)Context;
    KeSetEvent(event, 0, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS CPdoStubExtension::CallUSBD(IN PURB Urb)
{
    PIRP               irp;
    KEVENT             event;
    NTSTATUS           ntStatus;
    IO_STATUS_BLOCK    ioStatus;
    PIO_STACK_LOCATION nextStack;

    //
    // initialize the variables
    //
	DebugPrint (DebugTrace, "CallUSBD");

    irp = NULL;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_SUBMIT_URB, 
                                        m_PhysicalDeviceObject,
                                        NULL, 
                                        0, 
                                        NULL, 
                                        0, 
                                        TRUE, 
                                        &event, 
                                        &ioStatus);

    if(!irp) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    nextStack = IoGetNextIrpStackLocation(irp);
    ASSERT(nextStack != NULL);
    nextStack->Parameters.Others.Argument1 = Urb;

    ntStatus = IoCallDriver(m_PhysicalDeviceObject, irp);

    if(ntStatus == STATUS_PENDING) {

        KeWaitForSingleObject(&event, 
                              Executive, 
                              KernelMode, 
                              FALSE, 
                              NULL);

        ntStatus = ioStatus.Status;
    }
    
    return ntStatus;
}

NTSTATUS CPdoStubExtension::ReadandSelectDescriptors()
{
    return 0;

    PURB                   urb;
    ULONG                  siz;
    NTSTATUS               ntStatus;
    PUSB_DEVICE_DESCRIPTOR deviceDescriptor;

	DebugPrint (DebugTrace, "ReadandSelectDescriptors");
    //
    // initialize variables
    //

    urb = NULL;
    deviceDescriptor = NULL;

    //
    // 1. Read the device descriptor
    //

    urb = (PURB)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), 'Usb');

    if(urb) 
    {
        siz = sizeof(USB_DEVICE_DESCRIPTOR);
        deviceDescriptor = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPool, siz, 'Usb');

        if(deviceDescriptor) {

            UsbBuildGetDescriptorRequest(
                    urb, 
                    (USHORT) sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                    USB_DEVICE_DESCRIPTOR_TYPE, 
                    0, 
                    0, 
                    deviceDescriptor, 
                    NULL, 
                    siz, 
                    NULL);

            ntStatus = CallUSBD(urb);

            if(NT_SUCCESS(ntStatus)) 
            {

                ASSERT(deviceDescriptor->bNumConfigurations);
                ntStatus = ConfigureDevice();    
            }
                            
            ExFreePool(urb);                
            ExFreePool(deviceDescriptor);
        }
        else {
            ExFreePool(urb);
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else {

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

NTSTATUS CPdoStubExtension::ConfigureDevice()
{
    PURB                          urb;
    ULONG                         siz;
    NTSTATUS                      ntStatus;
    PUSB_CONFIGURATION_DESCRIPTOR configurationDescriptor;

	DebugPrint (DebugTrace, "ConfigureDevice");
    //
    // initialize the variables
    //

    urb = NULL;
    configurationDescriptor = NULL;

    //
    // Read the first configuration descriptor
    // This requires two steps:
    // 1. Read the fixed sized configuration desciptor (CD)
    // 2. Read the CD with all embedded interface and endpoint descriptors
    //

    urb = (PURB)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), 'Usb');

    if(urb) 
	{
        siz = sizeof(USB_CONFIGURATION_DESCRIPTOR);
        configurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPool, siz, 'Usb');

        if(configurationDescriptor) 
		{
			UsbBuildGetDescriptorRequest(
                    urb, 
                    (USHORT) sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                    USB_CONFIGURATION_DESCRIPTOR_TYPE, 
                    0, 
                    0, 
                    configurationDescriptor,
                    NULL, 
                    sizeof(USB_CONFIGURATION_DESCRIPTOR), 
                    NULL);

            ntStatus = CallUSBD(urb);

            if(!NT_SUCCESS(ntStatus)) 
			{

                goto ConfigureDevice_Exit;
            }
        }
        else 
		{

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto ConfigureDevice_Exit;
        }

        siz = configurationDescriptor->wTotalLength;

        ExFreePool(configurationDescriptor);

        configurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPool, siz, 'Usb');

        if(configurationDescriptor) 
		{
            UsbBuildGetDescriptorRequest(
                    urb, 
                    (USHORT)sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                    USB_CONFIGURATION_DESCRIPTOR_TYPE,
                    0, 
                    0, 
                    configurationDescriptor, 
                    NULL, 
                    siz, 
                    NULL);

            ntStatus = CallUSBD(urb);

            if(!NT_SUCCESS(ntStatus)) 
			{
                goto ConfigureDevice_Exit;
            }
        }
        else 
		{
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto ConfigureDevice_Exit;
        }
    }
    else 
	{
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto ConfigureDevice_Exit;
    }

ConfigureDevice_Exit:

    if(urb) 
	{
        ExFreePool(urb);
    }

    if(configurationDescriptor) 
	{
        ExFreePool(configurationDescriptor);
    }

    return ntStatus;
}

NTSTATUS CPdoStubExtension::IoCompletionSync(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context)
{
    PKEVENT event = (PKEVENT)Context;

    *Irp->UserIosb = Irp->IoStatus;
    KeSetEvent(event, 0, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

//////////////////////////////////////////////////////////
//					reset Interface
void CPdoStubExtension::ResetInterface (PUSBD_INTERFACE_INFORMATION	pInterface)
{
    int iCount;
    ULONG iCountPipe;

	DebugPrint (DebugTrace, "ResetInterface");

	for (iCountPipe = 0; iCountPipe < pInterface->NumberOfPipes; iCountPipe++)
	{
		if (m_PhysicalDeviceObject)
		{
			RequestPipe ( m_NextLowerDriver, pInterface->Pipes [iCountPipe].PipeHandle, URB_FUNCTION_ABORT_PIPE );
			RequestPipe ( m_NextLowerDriver, pInterface->Pipes [iCountPipe].PipeHandle, URB_FUNCTION_RESET_PIPE );
		}
	}

	if (pInterface->Class == 14 && m_ConfigurationHandle) // USB_DEVICE_CLASS_VIDEO
	{
		if (pInterface->AlternateSetting != 0)
		{
			USHORT                          urbSize;
			PURB                            urb;
			PUSBD_INTERFACE_INFORMATION     interfaceInfoUrb;
			NTSTATUS                        ntStatus;

			urbSize = (USHORT)GET_SELECT_INTERFACE_REQUEST_SIZE(pInterface->NumberOfPipes);
			urb = (PURB)ExAllocatePoolWithTag(NonPagedPool, urbSize, tagUsb);

			if (urb != NULL)
			{
				// Initialize the URB
				//
				RtlZeroMemory(urb, urbSize);

				urb->UrbHeader.Length   = urbSize;
				urb->UrbHeader.Function = URB_FUNCTION_SELECT_INTERFACE;
				urb->UrbSelectInterface.ConfigurationHandle = m_ConfigurationHandle;
				interfaceInfoUrb = &urb->UrbSelectInterface.Interface;

				RtlCopyMemory (interfaceInfoUrb, pInterface, GET_USBD_INTERFACE_SIZE(pInterface->NumberOfPipes));
				interfaceInfoUrb->AlternateSetting = 0;
				ntStatus = SendUrb (m_PhysicalDeviceObject, urb);
			}
			urbSize = 0;
		}
	}

}

bool CPdoStubExtension::RequestPipe (PDEVICE_OBJECT  DeviceObject, USBD_PIPE_HANDLE PipeHandle, USHORT iFunction)
{
    PURB        urb;
    NTSTATUS    ntStatus;

	DebugPrint (DebugTrace, "RequestPipe");
    // Allocate URB for ABORT_PIPE / RESET_PIPE request
    //
    urb = (PURB)ExAllocatePoolWithTag (NonPagedPool, sizeof(struct _URB_PIPE_REQUEST), tagUsb);

    if (urb != NULL)
    {
        // Initialize ABORT_PIPE / RESET_PIPE request URB
        //
        urb->UrbHeader.Length   = sizeof(struct _URB_PIPE_REQUEST);
        urb->UrbHeader.Function = iFunction;
        urb->UrbPipeRequest.PipeHandle = PipeHandle;

        // Synchronously submit ABORT_PIPE / RESET_PIPE request URB
        //
        ntStatus = SendUrb(DeviceObject, urb);

        // Done with URB for ABORT_PIPE / RESET_PIPE request, free it
        //
        ExFreePool(urb);

		return NT_SUCCESS (ntStatus);
    }

	return false;
}

NTSTATUS CPdoStubExtension::SendUrb (PDEVICE_OBJECT  DeviceObject, PURB urb, LARGE_INTEGER &TimeOut)
{
    KEVENT              localevent;
    PIRP                irp;
    PIO_STACK_LOCATION  nextStack;
    NTSTATUS            ntStatus;

    // Initialize the event we'll wait on
    //
    KeInitializeEvent(&localevent,
                      SynchronizationEvent,
                      FALSE);

    // Allocate the IRP
    //
    irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

    if (irp == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Set the IRP parameters
    //
    nextStack = IoGetNextIrpStackLocation(irp);

    nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

    nextStack->Parameters.DeviceIoControl.IoControlCode =
        IOCTL_INTERNAL_USB_SUBMIT_URB;

    nextStack->Parameters.Others.Argument1 = urb;

    // Set the completion routine, which will signal the event
    
    IoSetCompletionRoutine(irp,
                           CPdoStubExtension::IrpCompletionRoutine,
                           &localevent,
                           TRUE,    // InvokeOnSuccess
                           TRUE,    // InvokeOnError
                           TRUE);   // InvokeOnCancel



    // Pass the IRP & URB down the stack
    //
    ntStatus = IoCallDriver(DeviceObject, irp);

    // If the request is pending, block until it completes
    //
    if (ntStatus == STATUS_PENDING)
    {
        //LARGE_INTEGER timeout;
        // Specify a timeout of 5 seconds to wait for this call to complete.
        //

        ntStatus = KeWaitForSingleObject(&localevent,
                                         Executive,
                                         KernelMode,
                                         FALSE,
                                         &TimeOut);

        if (ntStatus == STATUS_TIMEOUT)
        {
            ntStatus = STATUS_IO_TIMEOUT;

            // Cancel the IRP we just sent.
            //
            IoCancelIrp(irp);

            // And wait until the cancel completes
            //
            KeWaitForSingleObject(&localevent,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);
        }
        else
        {
            ntStatus = irp->IoStatus.Status;
        }
    }

    // Done with the IRP, now free it.
    //
    IoFreeIrp(irp);

    return ntStatus;
}

NTSTATUS CPdoStubExtension::SendUrb (PDEVICE_OBJECT  DeviceObject, PURB urb)
{
    KEVENT              localevent;
    PIRP                irp;
    PIO_STACK_LOCATION  nextStack;
    NTSTATUS            ntStatus;

    // Initialize the event we'll wait on
    //
    KeInitializeEvent(&localevent,
                      SynchronizationEvent,
                      FALSE);

    // Allocate the IRP
    //
    irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

    if (irp == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Set the IRP parameters
    //
    nextStack = IoGetNextIrpStackLocation(irp);

    nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

    nextStack->Parameters.DeviceIoControl.IoControlCode =
        IOCTL_INTERNAL_USB_SUBMIT_URB;

    nextStack->Parameters.Others.Argument1 = urb;

    // Set the completion routine, which will signal the event
    
    IoSetCompletionRoutine(irp,
                           CPdoStubExtension::IrpCompletionRoutine,
                           &localevent,
                           TRUE,    // InvokeOnSuccess
                           TRUE,    // InvokeOnError
                           TRUE);   // InvokeOnCancel



    // Pass the IRP & URB down the stack
    //
    ntStatus = IoCallDriver(DeviceObject, irp);

    // If the request is pending, block until it completes
    //
    if (ntStatus == STATUS_PENDING)
    {
        LARGE_INTEGER timeout;

        // Specify a timeout of 5 seconds to wait for this call to complete.
        //
        timeout.QuadPart = -10000 * 5000;

        ntStatus = KeWaitForSingleObject(&localevent,
                                         Executive,
                                         KernelMode,
                                         FALSE,
                                         &timeout);

        if (ntStatus == STATUS_TIMEOUT)
        {
            ntStatus = STATUS_IO_TIMEOUT;

            // Cancel the IRP we just sent.
            //
            IoCancelIrp(irp);

            // And wait until the cancel completes
            //
            KeWaitForSingleObject(&localevent,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);
        }
        else
        {
            ntStatus = irp->IoStatus.Status;
        }
    }

    // Done with the IRP, now free it.
    //
    IoFreeIrp(irp);

    return ntStatus;
}

NTSTATUS CPdoStubExtension::ResetUsbConfig ()
{
    NTSTATUS ntStatus = STATUS_SUCCESS; 

    PURB urb; 
    ULONG size; 

	DebugPrint (DebugTrace, "ResetUsbConfig");

	if (m_ConfigurationHandle)
	{
		size= sizeof(struct _URB_SELECT_CONFIGURATION); 
		urb = (PURB)ExAllocatePoolWithTag(NonPagedPool, size, tagUsb); 
		if (urb) 
		{ 
			UsbBuildSelectConfigurationRequest(urb, (USHORT) size, NULL); 
			ntStatus = SendUrb(m_PhysicalDeviceObject, urb); 
			ExFreePool(urb); 
		} 
		else 
		{ 
			ntStatus = STATUS_NO_MEMORY; 
		} 
	}

    return ntStatus; 
}

WCHAR* CPdoStubExtension::GetQueryId (PDEVICE_OBJECT PhysicalDeviceObject, BUS_QUERY_ID_TYPE Type)
{
    IO_STATUS_BLOCK                     ioStatus;
    KEVENT                              pnpEvent;
    NTSTATUS                            status;
    PIO_STACK_LOCATION					irpStack;
    PIRP                                pnpIrp;
    PDEVICE_OBJECT                      targetObject;


    //
    // Initialize the event
    //
    KeInitializeEvent( &pnpEvent, NotificationEvent, FALSE );
	RtlZeroMemory (&ioStatus, sizeof (IO_STATUS_BLOCK));

	targetObject = IoGetAttachedDeviceReference( PhysicalDeviceObject );
    // Build an IRP
    //
    pnpIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP,
        targetObject,
        NULL,
        0,
        NULL,
        &pnpEvent,
        &ioStatus
        );

    if (pnpIrp == NULL) 
    {

        status = STATUS_INSUFFICIENT_RESOURCES;
        return NULL;
    }

    //
    // Pnp IRPs all begin life as STATUS_NOT_SUPPORTED;
    //
    pnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    //
    // Get the top of stack
    //
    irpStack = IoGetNextIrpStackLocation( pnpIrp );

    //
    // Set the top of stack
    //
    RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );
    irpStack->MajorFunction = IRP_MJ_PNP;
    irpStack->MinorFunction = IRP_MN_QUERY_ID;
	irpStack->Parameters.QueryId.IdType = Type;

    //
    // Call the driver
    //
    status = IoCallDriver( targetObject, pnpIrp );
    if (status == STATUS_PENDING) 
    {
        //
        // Block until the IRP comes back.
        // Important thing to note here is when you allocate 
        // the memory for an event in the stack you must do a  
        // KernelMode wait instead of UserMode to prevent 
        // the stack from getting paged out.
        //

        KeWaitForSingleObject(
            &pnpEvent,
            Executive,
            KernelMode,
            FALSE,
            NULL
            );
        status = ioStatus.Status;
    }
	else
	{
		ioStatus = pnpIrp->IoStatus;
	}

	if (NT_SUCCESS (status))
	{
		if (ioStatus.Information )
		{
			return ( (WCHAR*)ioStatus.Information );
		}
	}

	return NULL;
}

// get usb device description
bool CPdoStubExtension::GetUsbDeviceDesc (PDEVICE_OBJECT PhysicalDeviceObject, USB_DEVICE_DESCRIPTOR *pDevDesk)
{
    PURB                   urb;
    ULONG                  siz;
    NTSTATUS               ntStatus;
    

    //
    // initialize variables
    //
    urb = NULL;

    //
    // 1. Read the device descriptor
    //
    urb = (PURB)ExAllocatePoolWithTag(NonPagedPool, 
                         sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), 'DD');

    if(urb) 
	{
        LARGE_INTEGER timeout;
        // Specify a timeout of 5 seconds to wait for this call to complete.
        //
        timeout.QuadPart = -10000 * 100;

		RtlZeroMemory (urb, sizeof (struct _URB_CONTROL_DESCRIPTOR_REQUEST));
        UsbBuildGetDescriptorRequest(
                urb, 
                (USHORT) sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                USB_DEVICE_DESCRIPTOR_TYPE, 
                0, 
                0, 
                pDevDesk, 
                NULL, 
                sizeof(USB_DEVICE_DESCRIPTOR), 
                NULL);

		ntStatus = CPdoStubExtension::SendUrb (PhysicalDeviceObject, urb, timeout);
                        
        ExFreePool(urb);                

		return NT_SUCCESS (ntStatus);
    }

    return false;
}

bool CPdoStubExtension::InitDeviceDesc ()
{
	bool bRet = false;
	DebugPrint (DebugTrace, "InitDeviceDesc");
	CBusFilterExtension *pBus;

	if (m_DeviceDescription.bLength)
	{
		return true;
	}

	if (!m_pBus)
	{
		return false;
	}
	pBus = (CBusFilterExtension*)m_pBus;

	if (m_pBus->GetList().GetCount () != pBus->GetCountUsbDevs ())
	{
		return false;
	}

	DebugPrint (DebugSpecial, "InitDeviceDesc1");
	bRet = GetUsbDeviceDesc (m_PhysicalDeviceObject, &m_DeviceDescription);
	DebugPrint (DebugSpecial, "InitDeviceDesc2 %d", bRet);
	return bRet;
}

void CPdoStubExtension::ClearDeviceDesc ()
{
	DebugPrint (DebugTrace, "ClearDeviceDesc");
	RtlZeroMemory (&m_DeviceDescription, sizeof (USB_DEVICE_DESCRIPTOR));
}

void CPdoStubExtension::InitUsbInterace ()
{
    IO_STATUS_BLOCK                     ioStatus;
    KEVENT                              pnpEvent;
    NTSTATUS                            status;
    PIO_STACK_LOCATION					irpStack;
    PIRP                                pnpIrp;
    PDEVICE_OBJECT                      targetObject;

	DebugPrint (DebugTrace, "InitUsbInterace");
    //
    // Initialize the event
    //
    KeInitializeEvent( &pnpEvent, NotificationEvent, FALSE );
	RtlZeroMemory (&ioStatus, sizeof (IO_STATUS_BLOCK));

	targetObject = IoGetAttachedDeviceReference( m_PhysicalDeviceObject );
    // Build an IRP
    //
    pnpIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP,
        targetObject,
        NULL,
        0,
        NULL,
        &pnpEvent,
        &ioStatus
        );

    if (pnpIrp == NULL) 
    {

        status = STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Pnp IRPs all begin life as STATUS_NOT_SUPPORTED;
    //
    pnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    //
    // Get the top of stack
    //
    irpStack = IoGetNextIrpStackLocation( pnpIrp );

    //
    // Set the top of stack
    //
	GUID GuidUsb = {0xb1a96a13, 0x3de0, 0x4574, 0x9b, 0x1, 0xc0, 0x8f, 0xea, 0xb3, 0x18, 0xd6};

    RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );
	RtlZeroMemory( &m_Interface, sizeof( USB_BUS_INTERFACE_USBDI_V3_FULL ) );
    irpStack->MajorFunction = IRP_MJ_PNP;
    irpStack->MinorFunction = IRP_MN_QUERY_INTERFACE;
	irpStack->Parameters.QueryInterface.InterfaceType = &GuidUsb;
	irpStack->Parameters.QueryInterface.Size = sizeof (USB_BUS_INTERFACE_USBDI_V2_FULL);
	irpStack->Parameters.QueryInterface.Version = 2;
	irpStack->Parameters.QueryInterface.Interface = (PINTERFACE)&m_Interface;

    //
    // Call the driver
    //
    status = IoCallDriver( targetObject, pnpIrp );
    if (status == STATUS_PENDING) 
    {
        //
        // Block until the IRP comes back.
        // Important thing to note here is when you allocate 
        // the memory for an event in the stack you must do a  
        // KernelMode wait instead of UserMode to prevent 
        // the stack from getting paged out.
        //

        KeWaitForSingleObject(
            &pnpEvent,
            Executive,
            KernelMode,
            FALSE,
            NULL
            );
        status = ioStatus.Status;
    }
}

NTSTATUS CPdoStubExtension::DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS            status;
    PIO_STACK_LOCATION  stack;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

	DebugPrint (DebugTrace, "DispatchPower");

	++m_irpCounter;

    stack = IoGetCurrentIrpStackLocation (Irp);
    powerType = stack->Parameters.Power.Type;
    powerState = stack->Parameters.Power.State;

	DebugPrint (DebugTrace, "%s", PowerMinorFunctionString (stack->MinorFunction));
	DebugPrint (DebugSpecial, "%s", PowerMinorFunctionString (stack->MinorFunction));

    switch (stack->MinorFunction) 
	{
    case IRP_MN_SET_POWER:
    
        DebugPrint (DebugTrace, "Setting %s power state to %s",
                     ((powerType == SystemPowerState) ?  "System" : "Device"),
                     ((powerType == SystemPowerState) ? \
                        SystemPowerString(powerState.SystemState) : \
                        DevicePowerString(powerState.DeviceState)));
        break;

    case IRP_MN_QUERY_POWER:
        break;

    case IRP_MN_WAIT_WAKE:
    case IRP_MN_POWER_SEQUENCE:
    default:
        break;
    }
	--m_irpCounter;

	return CFilterExtension::DispatchPower (DeviceObject, Irp);
}

NTSTATUS CPdoStubExtension::DeconfigureDevice(PDEVICE_OBJECT DeviceObject)
////*++
// 
//Routine Description:
//
//    This routine is invoked when the device is removed or stopped.
//    This routine de-configures the usb device.
//
//Arguments:
//
//    DeviceObject - pointer to device object
//
//Return Value:
//
//    NT status value
//
//--*//
{
    PURB     urb;
    ULONG    siz;
    NTSTATUS ntStatus;
	USB_CONFIGURATION_DESCRIPTOR	Desc;
    
    //
    // initialize variables
    //

	DebugPrint (DebugTrace, "DeconfigureDevice");

	RtlZeroMemory (&Desc, sizeof (USB_CONFIGURATION_DESCRIPTOR));
    siz = sizeof(struct _URB_SELECT_CONFIGURATION);
    urb = (PURB)ExAllocatePoolWithTag (NonPagedPool, siz, tagUsb);
	//Desc.wTotalLength = sizeof (USB_CONFIGURATION_DESCRIPTOR);
	Desc.MaxPower = 253;

    if(urb) 
	{

        UsbBuildSelectConfigurationRequest(urb, (USHORT)siz, NULL);

        ntStatus = SendUrb (DeviceObject, urb);

        if(!NT_SUCCESS(ntStatus)) 
		{
			DebugPrint (DebugInfo, "Failed to deconfigure device");
        }

        ExFreePool(urb);
    }
    else 
	{
		DebugPrint (DebugInfo, "Failed to allocate urb");
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}


NTSTATUS CPdoStubExtension::GetInstanceId (WCHAR *szId, LONG lSize)
{
	HANDLE						hReg;
	PVOID						ObjReg;
	OBJECT_NAME_INFORMATION		*pObjectNameInfo;
	DWORD						NeedSize = 0;
	NTSTATUS					status;

	DebugPrint (DebugTrace, "GetInstanceId");

	if (m_PhysicalDeviceObject)
	{
		if (!NT_SUCCESS (status = IoOpenDeviceRegistryKey (m_PhysicalDeviceObject, PLUGPLAY_REGKEY_DEVICE, GENERIC_READ, &hReg)))
		{
			return STATUS_UNSUCCESSFUL;
		}

		if (!NT_SUCCESS (ObReferenceObjectByHandle (hReg, THREAD_ALL_ACCESS, NULL, KernelMode, &ObjReg, NULL)))
		{
			ZwClose(hReg);
			return STATUS_UNSUCCESSFUL;
		}
		ZwClose(hReg);

		szId[0] = 0;  
		pObjectNameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag (PagedPool, 1024, tagUsb);
		pObjectNameInfo->Name.MaximumLength = 1000;

		if (!NT_SUCCESS (status = ObQueryNameString (ObjReg, pObjectNameInfo, 1000, &NeedSize)))
		{
			ExFreePool (pObjectNameInfo);
			ObDereferenceObject (ObjReg);
			return STATUS_UNSUCCESSFUL;
		}
		
		RtlStringCbCopyW(szId, lSize, pObjectNameInfo->Name.Buffer);
		ExFreePool (pObjectNameInfo);
		ObDereferenceObject (ObjReg);			
	}
	
	return STATUS_SUCCESS;
}

VOID CPdoStubExtension::UsbIdleCallBack(PVOID Context)
{
	CPdoStubExtension *pBase = (CPdoStubExtension*)Context;
	TPtrUsbPortToTcp pTcp = pBase->m_pParent;

	pBase->DebugPrint (DebugTrace, "UsbIdleCallBack");

	if (pTcp.get ())
	{
		pTcp->SendIdleNotification (pBase);
		//pBase->m_pParent
	}
}

bool CPdoStubExtension::WaitRemoveDevice (int iMSec)
{
	LARGE_INTEGER	time;
	time.QuadPart = (LONGLONG)-10000 * iMSec;				// 100 msec

	DebugPrint (DebugTrace, "WaitRemoveDevice");

	if (KeWaitForSingleObject(&m_eventRemove, 
						  Executive, 
						  KernelMode, 
						  FALSE, 
						  &time) == STATUS_SUCCESS)
	{
		return true;
	}

	return false;
}