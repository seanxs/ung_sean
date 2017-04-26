/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BusFilterExtension.cpp

Abstract:

  USB hub filter class. Is used for USB devices interception

Environment:

    Kernel mode

--*/
#include "BusFilterExtension.h"
#include "ControlDeviceExtension.h"
#include "PdoStubExtension.h"
#include "UnicIndef.h"

#pragma warning( disable : 4200 )

CBusFilterExtension::CBusFilterExtension (BOOLEAN bHubDecect) 
    : CFilterExtension()
	, m_bHubDecect (bHubDecect)
    , m_pControl (NULL)
    , m_pListControl (NULL)
    , m_PhysicalDeviceObject (NULL)
	, m_PdoHub (NULL)
{
    SetDebugName ("CBusFilterExtension");
	RtlStringCbCopyW(m_szDriverKeyName, sizeof(m_szDriverKeyName), L"");
}

CBusFilterExtension::~CBusFilterExtension ()
{
	ClearEvent ();
}
//***************************************************************************************
//                              Dispatch function
//***************************************************************************************
//      Pnp
NTSTATUS CBusFilterExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION          irpStack;
    NTSTATUS                                status;

    DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
    DebugPrint (DebugTrace, "%s", PnPMinorFunctionString (Irp));
    DebugPrint (DebugSpecial, "%s", PnPMinorFunctionString (Irp));

	++m_irpCounter;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->MinorFunction)
    {
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        {
            if ((DeviceObject->AttachedDevice == NULL) ||
                (DeviceObject->AttachedDevice->Flags & DO_POWER_PAGABLE)) 
            {

                DeviceObject->Flags |= DO_POWER_PAGABLE;
            }

			if (irpStack->Parameters.QueryDeviceRelations.Type == BusRelations)
			{
	            IoCopyCurrentIrpStackLocationToNext(Irp);
				IoSetCompletionRoutine(
					Irp,
					FilterCompletionRoutineRelations,
					NULL,
					TRUE,
					TRUE,
					TRUE
					);
			}
			else
			{
				IoSkipCurrentIrpStackLocation(Irp);
				--m_irpCounter;
			}

            return IoCallDriver(m_NextLowerDriver, Irp);
        }
    case IRP_MN_REMOVE_DEVICE:
        {
            CControlDeviceExtension         *pControl;

            DebugPrint(DebugInfo,"Waiting for outstanding requests");

            IoSkipCurrentIrpStackLocation(Irp);
            status = IoCallDriver(m_NextLowerDriver, Irp);

			--m_irpCounter;
			m_irpCounter.WaitRemove ();

            SetNewPnpState (DriverBase::Deleted);

            IoDetachDevice(m_NextLowerDriver);
            IoDeleteDevice(DeviceObject);

            pControl = CControlDeviceExtension::GetControlExtension ();
            if (pControl && m_pListControl)
            {
				pControl->m_spinUsb.Lock();
                pControl->m_listUsbHub.Remove (m_pListControl);
				pControl->m_spinUsb.UnLock();
            }
            delete this;
            return status;
        }
    case IRP_MN_QUERY_BUS_INFORMATION:
        DebugPrint (DebugInfo, "IRP_MN_QUERY_BUS_INFORMATION");
        break;
    }

	--m_irpCounter;
    return CFilterExtension::DispatchPnp (DeviceObject, Irp);
}

//***************************************************************************************
//                              completion routine
//***************************************************************************************

NTSTATUS CBusFilterExtension::FilterCompletionRoutineRelations (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    CBusFilterExtension* pBase = (CBusFilterExtension*) GetExtensionClass (DeviceObject);
    pBase->DebugPrint(DebugTrace, "FilterStartCompletionRoutineRelations");

    if (Irp->PendingReturned) 
    {
        IoMarkIrpPending(Irp);
    }

    if ( NT_SUCCESS (Irp->IoStatus.Status))
    {
        PDEVICE_RELATIONS                       Relations;
        PIO_STACK_LOCATION                      irpStack;

        irpStack = IoGetCurrentIrpStackLocation(Irp);
        Relations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
        if (Relations)
        {
			#if _FEATURE_ENABLE_DEBUGPRINT
				pBase->ShowDeviceRelation (Relations);
			#endif

            pBase->QueryRelations (Relations, NULL);
        }
    }

    //
    // On the way up, Pagable might become clear. Mimic the driver below us.
    //
    if (!(pBase->m_NextLowerDriver->Flags & DO_POWER_PAGABLE)) 
    {

        DeviceObject->Flags &= ~DO_POWER_PAGABLE;
    }

	--pBase->m_irpCounter;
    return STATUS_SUCCESS;

}

#if _FEATURE_ENABLE_DEBUGPRINT
void CBusFilterExtension::ShowDeviceRelation (PDEVICE_RELATIONS Relations)
{
	IO_STATUS_BLOCK	ioStatus;
	NTSTATUS	status;

	DebugPrint (DebugSpecial, "ShowDeviceRelation %d", Relations->Count);

	for (UINT a = 0; a < Relations->Count; a++)
	{
		status = GetDeviceId (Relations->Objects [a], BusQueryHardwareIDs, &ioStatus);
		if (ioStatus.Information)
		{
			DebugPrint (DebugSpecial, "\t\t%d %x %S", a, Relations->Objects [a], (WCHAR*)ioStatus.Information);
			ExFreePool ((PVOID)ioStatus.Information);
			ioStatus.Information = NULL;
		}
	}
}
#endif

void CBusFilterExtension::InitName (PDEVICE_OBJECT PhysicalDeviceObject)
{
    ULONG       pRes;

	DebugPrint (DebugTrace, "InitName");
	if (PhysicalDeviceObject && wcslen (m_szDriverKeyName) == 0)
	{
		IoGetDeviceProperty (PhysicalDeviceObject, 
			DevicePropertyDriverKeyName,
			200,
			m_szDriverKeyName,
			&pRes);         

		InitUnicId (PhysicalDeviceObject);
	}
}

void CBusFilterExtension::InitUnicId (PDEVICE_OBJECT PhysicalDeviceObject)
{
	WCHAR           szName [200];
	ULONG           pRes;
	NTSTATUS        status;

	DebugPrint (DebugTrace, "InitUnicId");

	status = IoGetDeviceProperty (PhysicalDeviceObject, 
		DevicePropertyDriverKeyName,
		200,
		szName,
		&pRes);

	DebugPrint (DebugInfo, "Unic Id Bus - %S", szName);

	if (NT_SUCCESS (status))
	{
		m_wId = 0;
		WCHAR *pTemp = szName;
		while (*pTemp)
		{
			m_wId ^= *pTemp;
			pTemp++;
		}
	}
}

NTSTATUS CBusFilterExtension::AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS                                        status;
    CControlDeviceExtension         *pControl;

	DebugPrint (DebugTrace, "AttachToDevice");

    status = CFilterExtension::AttachToDevice (DriverObject, PhysicalDeviceObject);
    if (NT_SUCCESS (status))
    {
        // init param
		InitName (PhysicalDeviceObject);

        pControl = CControlDeviceExtension::GetControlExtension ();
        if (pControl)
        {
			pControl->m_spinUsb.Lock();
            m_pListControl = pControl->m_listUsbHub.Add (this);
			pControl->m_spinUsb.UnLock();
            m_pControl = pControl;
        }
        m_PhysicalDeviceObject = PhysicalDeviceObject;
		m_PdoHub.m_PhysicalDeviceObject = PhysicalDeviceObject;
		m_PdoHub.m_NextLowerDriver = m_NextLowerDriver;

        #if _WIN32_WINNT >= _WIN32_WINNT_LONGHORN
			for (int a = 0; a < 16; a++)
			{
				CPdoStubExtension::ApplyXPSilentInstallHack ((ULONG(m_wId)<<16) + a);
			}
		#endif
    }
    return status;
}

TPtrPdoBase CBusFilterExtension::AddPdo (PDEVICE_OBJECT PhysicalDeviceObject)
{
    TPtrPdoStubExtension	pPdo = NULL;
    ULONG           pRes;
    NTSTATUS        status;

    DebugPrint (DebugTrace, "AddPdo");

	pPdo = new CPdoStubExtension (this);

	if (pPdo.get())
	{
		pPdo->m_bMain = true;
		VerifyShared (pPdo, PhysicalDeviceObject);

		pPdo->m_uInstance = CUnicId::Instance()->BuildUnicId ();

		if (pPdo.get())
		{
			status = pPdo->AttachToDevice (CBaseDeviceExtension::m_DriverObject, PhysicalDeviceObject);
			pPdo->SetDevIndef (PhysicalDeviceObject);
			if (NT_SUCCESS (status))
			{
				TPtrPdoBase pPdoBase (pPdo.get());
				SpinDr().Lock();
				pPdoBase->IncMyself ();
				m_arrPdo.Add (pPdoBase);
				SpinDr ().UnLock();

				return pPdo;
			}
			//else
			//{
			//	delete pPdo;
			//}
		}
	}
	return pPdo.get();
}   


TPtrPdoBase CBusFilterExtension::IsExistPdo (PDEVICE_OBJECT PhysicalDeviceObject)
{
	TPtrPdoBase pPdoBase = CDeviceRelations::IsExistPdo (PhysicalDeviceObject);
	TPtrPdoStubExtension pPdo = (CPdoStubExtension*)pPdoBase.get();

	DebugPrint (DebugTrace, "IsExistPdo");

	if (pPdo.get() && pPdo->m_uDeviceAddress == 0)
	{
		VerifyShared (pPdo, PhysicalDeviceObject);
	}
	return pPdoBase;
}

bool CBusFilterExtension::VerifyShared (TPtrPdoStubExtension pPdo, PDEVICE_OBJECT PhysicalDeviceObject)
{
    TPtrUsbPortToTcp				pUsbPort;
	BOOLEAN							bHub;

	DebugPrint (DebugTrace, "VerifyShared");
	
	if (pPdo->m_uDeviceAddress == 0)
	{
		InitName (m_PhysicalDeviceObject);
		{
			pPdo->m_uDeviceAddress = GetUsbDeviceAddressFromHub (PhysicalDeviceObject);
			if ((pPdo->m_uDeviceAddress > 0) && (pPdo->m_uDeviceAddress < 129))
			{
				DebugPrint (DebugInfo, "VerifyShared2 - %d", pPdo->m_uDeviceAddress);
				DebugPrint (DebugSpecial, "VerifyShared2 - %d", pPdo->m_uDeviceAddress);
				//pPdo->m_uDeviceAddress = UsbDev.DeviceAddress;
			}
			else
			{
				//pPdo->m_uDeviceAddress = 0;
				pPdo->m_uDeviceAddress = GetUsbDeviceAddress(PhysicalDeviceObject);

				if (pPdo->m_uDeviceAddress == 0)
				{
					pPdo->m_pParent = NULL;
					pPdo->m_bIsFilter = true;//DeviceIsHubPnp (m_DriverObject,  PhysicalDeviceObject, bHub)?true:false;
					DebugPrint(DebugInfo, "VerifyShared3 - %d", pPdo->m_uDeviceAddress);
					DebugPrint(DebugSpecial, "VerifyShared3 - %d", pPdo->m_uDeviceAddress);
				}
			}
			pPdo->BuildDevId();
		}
	}

	// didn't work with hub 
	if (DeviceIsHubPnp (m_DriverObject,  PhysicalDeviceObject, bHub))
	{
		DebugPrint (DebugInfo, "VerifyShared: device is hub", pPdo->m_uDeviceAddress);
		DebugPrint (DebugSpecial, "VerifyShared: device is hub", pPdo->m_uDeviceAddress);
		pPdo->m_pParent = NULL;
		pPdo->m_bIsFilter = true;
	}                                                   
	else if (PhysicalDeviceObject && m_pControl && !pPdo->m_pParent.get() && (pUsbPort = m_pControl->GetUsbPortToTcp (pPdo.get()).get ()).get ())
	{
		CAutoLock lockRemove (&pUsbPort->m_lockRemove, 'VrSr');
        pPdo->m_pParent = pUsbPort;
        pPdo->m_bIsFilter = false;

		DebugPrint (DebugInfo, "VerifyShared: device shared %d", pPdo->m_uDeviceAddress);
		DebugPrint (DebugSpecial, "VerifyShared: device shared %d", pPdo->m_uDeviceAddress);

		if (pUsbPort->m_pPdoStub)
        {
			pUsbPort->StopTcpServer (pUsbPort->m_pPdoStub);
        }
		if (pUsbPort->m_bDelayedDelete)
		{
			pPdo->m_pParent = NULL;
			pPdo->m_bIsFilter = true;
		}
		else
		{
			SpinDr().Lock();
			pUsbPort->m_pPdoStub = pPdo.get ();
			SpinDr().UnLock();
		}
        DebugPrint (DebugInfo, "Add Stub Pdo");

		return true;
    }

	return false;
}

ULONG CBusFilterExtension::GetCountUsbDevs ()
{
	DWORD								dwRet = 0;
    PDEVICE_OBJECT						targetObject;
    PIO_STACK_LOCATION					irpStack;
    PIRP								pIrp;
	BYTE								pBuff [1024];
	USB_NODE_CONNECTION_INFORMATION		*pInfo = (USB_NODE_CONNECTION_INFORMATION*)pBuff;
	PUSB_NODE_INFORMATION				pHub = (PUSB_NODE_INFORMATION)pBuff;
    KEVENT								Event;
    NTSTATUS							status;
    IO_STATUS_BLOCK						ioStatus;
	int									iCountUsbPort;

	DebugPrint (DebugTrace, "GetCountUsbDevs");
	//
	// Initialize 
	//
	KeInitializeEvent( &Event, NotificationEvent, FALSE );
	targetObject = IoGetAttachedDeviceReference( m_PhysicalDeviceObject );

	// get count usb ports
	RtlZeroMemory (pBuff, sizeof (pBuff));
	GetUsbDeviceConnectionInfo (m_PhysicalDeviceObject,  IOCTL_USB_GET_NODE_INFORMATION, pInfo, 1024);
	iCountUsbPort = pHub->u.HubInformation.HubDescriptor.bNumberOfPorts;

	RtlZeroMemory (pBuff, sizeof (pBuff));

	// enum ports of usb hub 
	for (int a = 0; a < iCountUsbPort; a++)
	{
		pInfo->ConnectionIndex++;

		status = GetUsbDeviceConnectionInfo (m_PhysicalDeviceObject, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, pInfo, 1024);
		if (!NT_SUCCESS (status))
		{
			break;
		}
		else
		{
			if ((pInfo->ConnectionStatus !=  NoDeviceConnected))
			{
				dwRet++;
			}
		}

		//
		// Done with reference
		//
	}
	ObDereferenceObject( targetObject );

	return dwRet;
}

bool CBusFilterExtension::IsUsbDeviceHub (PDEVICE_OBJECT PhysicalDeviceObject, int iUsbPort)
{
	bool bRet = true;
	BYTE								pBuff [1024];
	USB_NODE_CONNECTION_INFORMATION		*pInfo = (USB_NODE_CONNECTION_INFORMATION*)pBuff;
    NTSTATUS							status;

	pInfo->ConnectionIndex++;

	DebugPrint (DebugTrace, "IsUsbDeviceHub");

	status = GetUsbDeviceConnectionInfo (PhysicalDeviceObject, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, pInfo, sizeof (pBuff));

	if (NT_SUCCESS (status))
	{
		DebugPrint  (DebugSpecial, "IsUsbDeviceHub %d", pInfo->DeviceIsHub);
		bRet = pInfo->DeviceIsHub?true:false;
	}
	else
	{
		DebugPrint  (DebugError, "IsUsbDeviceHub CONNECTION_INFORMATION");
	}

	return bRet;
}

NTSTATUS CBusFilterExtension::GetUsbDeviceConnectionInfo (PDEVICE_OBJECT PhysicalDeviceObject, ULONG  IoControlCode, PVOID pInfo, int iSize)
{
    PDEVICE_OBJECT						targetObject;
    PIO_STACK_LOCATION					irpStack;
    PIRP								pIrp;
    KEVENT								Event;
    IO_STATUS_BLOCK						ioStatus;
	NTSTATUS							status;

	KeInitializeEvent( &Event, NotificationEvent, FALSE );
	targetObject = IoGetAttachedDeviceReference( PhysicalDeviceObject );
	//
	// Build an IRP
	//
	pIrp = IoBuildDeviceIoControlRequest(
		IoControlCode,
		targetObject,
		pInfo,
		iSize,
		pInfo,
		iSize,
		false,
		&Event,
		&ioStatus
		);

	if (pIrp == NULL) 
	{

		status = STATUS_INSUFFICIENT_RESOURCES;
		ObDereferenceObject( targetObject );
		return status;
	}

	//
	// Pnp IRPs all begin life as STATUS_NOT_SUPPORTED;
	//
	pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

	//
	// Call the driver
	//
	status = IoCallDriver( targetObject, pIrp );
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
			&Event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
		status = ioStatus.Status;
	}

	ObDereferenceObject( targetObject );

	return status;
}

ULONG CBusFilterExtension::GetUsbDeviceAddress (PDEVICE_OBJECT PhysicalDeviceObject)
{
	DEVICE_CAPABILITIES         DeviceCapabilities;
	IO_STATUS_BLOCK             ioStatus;
	KEVENT                      pnpEvent;
	NTSTATUS                    status;
	PDEVICE_OBJECT              targetObject;
	PIO_STACK_LOCATION          irpStack;
	PIRP                        pnpIrp;

	DebugPrint (DebugTrace, "GetUsbDeviceAddress");
	//
	// Initialize the capabilities that we will send down
	//
	RtlZeroMemory( &DeviceCapabilities, sizeof(DEVICE_CAPABILITIES) );
	DeviceCapabilities.Size = sizeof(DEVICE_CAPABILITIES);
	DeviceCapabilities.Version = 1;
	DeviceCapabilities.Address = (ULONG)-1;
	DeviceCapabilities.UINumber = (ULONG)-1;

	//
	// Initialize the event
	//
	KeInitializeEvent( &pnpEvent, NotificationEvent, FALSE );

	targetObject = PhysicalDeviceObject;

	//
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
		ObDereferenceObject( targetObject );
		return (ULONG)-1;
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
	irpStack->MinorFunction = IRP_MN_QUERY_CAPABILITIES;
	irpStack->Parameters.DeviceCapabilities.Capabilities = (PDEVICE_CAPABILITIES)&DeviceCapabilities;

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

	//
	// Done
	//
	DebugPrint(DebugInfo, "AddPdo UsbDeviceAddress %d", DeviceCapabilities.Address);

	return DeviceCapabilities.Address;
}

ULONG CBusFilterExtension::GetUsbDeviceAddressFromHub (PDEVICE_OBJECT PhysicalDeviceObject, bool bLock)
{
	DWORD								dwRet = 0;
	BYTE								pBuff [1024];
	USB_NODE_CONNECTION_INFORMATION		*pInfo = (USB_NODE_CONNECTION_INFORMATION*)pBuff;
    NTSTATUS							status;
	WCHAR								szTemp [1024];
	WCHAR								*szHardwareId = NULL;

	DebugPrint (DebugTrace, "GetUsbDeviceAddressFromHub");
	//
	// Initialize 
	//
	RtlZeroMemory (pBuff, sizeof (pBuff));

	// Get hardware id
	{
		IO_STATUS_BLOCK	ioStatus;
		status = GetDeviceId (PhysicalDeviceObject, BusQueryHardwareIDs, &ioStatus);

		if (NT_SUCCESS (status))
		{
			szHardwareId = (WCHAR*)ioStatus.Information;
		}
	}

	if (!szHardwareId)
	{
		DebugPrint (DebugInfo, "GetUsbDeviceAddressFromHub2 - HdId - %S", szTemp);
		return dwRet;
	}

	DebugPrint (DebugInfo, "GetUsbDeviceAddressFromHub - HdId - %S", szHardwareId);

	// enum ports of usb hub 
	for (int a = 0; a < 128; a++)
	{
		pInfo->ConnectionIndex++;

		status = GetUsbDeviceConnectionInfo (m_PhysicalDeviceObject, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, pInfo, sizeof (pBuff));

		if (!NT_SUCCESS (status))
		{
			break;
		}
		else
		{
			if (pInfo->ConnectionStatus !=  NoDeviceConnected)
			{
				RtlStringCbPrintfW(szTemp, sizeof(szTemp), L"USB\\VID_%04x&PID_%04x&REV_%04x"
									 , pInfo->DeviceDescriptor.idVendor
									 , pInfo->DeviceDescriptor.idProduct
									 , pInfo->DeviceDescriptor.bcdDevice);

				DebugPrint (DebugInfo, "GetUsbDeviceAddressFromHub2 - HdId - %S", szTemp);

				if (_wcsnicmp (szTemp, szHardwareId, wcslen (szTemp)) == 0)
				{
					dwRet = pInfo->ConnectionIndex;
					if (FindDevice (pInfo->ConnectionIndex, bLock).get())
					{
						DebugPrint (DebugError, "GetUsbDeviceAddressFromHub - HdId - %S already exist", szTemp);
					}
				}
			}
		}

		//
		// Done with reference
		//
	}

	if (szHardwareId)
	{
		ExFreePool (szHardwareId);
	}

	DebugPrint  (DebugInfo, "GetUsbDeviceAddressFromHub %d", dwRet);
	return dwRet;
}

TPtrPdoStubExtension CBusFilterExtension::FindDevice (DWORD DeviceAddress, bool bLock)
{
	TPtrPdoStubExtension	pRet = NULL;
	TPtrPdoStubExtension	pPdo = NULL;

	DebugPrint (DebugTrace, "FindDevice");

	if (bLock)
	{
		SpinDr().Lock ();
	}

	for (int b = 0; b < GetList().GetCount ();b++)
	{
		pPdo = (CPdoStubExtension*)GetChild (b).get();
		if (pPdo->m_uDeviceAddress == DeviceAddress)
		{
			pRet = pPdo;
			break;
		}
	}

	if (bLock)
	{
		SpinDr ().UnLock ();
	}
	
	return pRet;
}

void CBusFilterExtension::QueryRelations (PDEVICE_RELATIONS Relations, PDEVICE_OBJECT PhysicalDeviceObject)
{
	DebugPrint (DebugTrace, "QueryRelations");

	CDeviceRelations::QueryRelations (Relations);
	ClearEvent ();
}

void CBusFilterExtension::ClearEvent ()
{
	DebugPrint (DebugTrace, "ClearEvent");

	m_spinDr.Lock ();
	while (m_listEventDevRelations.GetCount ())
	{
		PKEVENT pEevent = (PKEVENT)m_listEventDevRelations.GetItem (0);
		KeSetEvent(pEevent, 0, FALSE);
		m_listEventDevRelations.Remove (0);
	}
	m_spinDr.UnLock ();
}

bool CBusFilterExtension::DelEvent (PKEVENT pEvent)
{
	bool bRet = false;

	DebugPrint (DebugTrace, "DelEvent");

	m_spinDr.Lock ();
	for (int a =0; a < m_listEventDevRelations.GetCount (); a++)
	{
		PKEVENT pItem = (PKEVENT)m_listEventDevRelations.GetItem (a);
		if (pItem == pEvent)
		{
			KeSetEvent(pEvent, 0, FALSE);
			m_listEventDevRelations.Remove (a);
			bRet = true;
			break;
		}
	}
	m_spinDr.UnLock ();

	return bRet;
}

bool CBusFilterExtension::WaitEventRelations (int iMSec)
{
	LARGE_INTEGER	time;
	KEVENT	eventDeviceRelations;

	DebugPrint (DebugTrace, "WaitEventRelations");

	KeInitializeEvent(&eventDeviceRelations, NotificationEvent, FALSE);

	m_spinDr.Lock();
	m_listEventDevRelations.Add (&eventDeviceRelations);
	m_spinDr.UnLock ();

	time.QuadPart = (LONGLONG)-10000 * iMSec;				// 100 msec
	if (KeWaitForSingleObject(&eventDeviceRelations, 
						  Executive, 
						  KernelMode, 
						  FALSE, 
						  &time) == STATUS_TIMEOUT)
	{
		DelEvent (&eventDeviceRelations);
		return false;

	}
	return true;

}

NTSTATUS CBusFilterExtension::DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	return CFilterExtension::DispatchIntrenalDevCtrl ( DeviceObject, Irp );
}