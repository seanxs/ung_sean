/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    HubStub.cpp

Abstract:

   Virtual USB hub class. Virtual USB devices appear on this hub.

Environment:

    Kernel mode

--*/

#include "HubStub.h"
#include "VirtualUsbHub.h"
#include "PdoExtension.h"
#include "UsbDevToTcp.h"
#include "Core\Utils.h"

#pragma warning(disable : 4995)

WCHAR *CHubStubExtension::m_szBusQueryDeviceID = L"UsbEStub\\Devices\0";
int CHubStubExtension::m_iSizeBusQueryDeviceID = sizeof (L"UsbEStub\\Devices\0");
WCHAR *CHubStubExtension::m_szBusQueryHardwareIDs = L"UsbEStub\0";
int CHubStubExtension::m_iSizeBusQueryHardwareIDs = sizeof (L"UsbEStub\0");
WCHAR *CHubStubExtension::m_szBusQueryCompatibleIDs = L"*EltimaCompatibleStub\0";
int CHubStubExtension::m_iSizeBusQueryCompatibleIDs = sizeof (L"*EltimaCompatibleStub\0");
WCHAR *CHubStubExtension::m_szDeviceTextDescription = L"Usb Stub (Eltima software)";
int CHubStubExtension::m_iSizeDeviceTextDescription = sizeof (L"Usb Stub (Eltima software)");
#define ARR_DIM(a) (sizeof(a) / sizeof(a[0]))

static const GUID GUID_ELTIMA_STUB = 
{ 0x1c27b73d, 0x29af, 0x4044, { 0xb9, 0x1c, 0x41, 0x16, 0xd4, 0xef, 0xa6, 0x96 } };

CHubStubExtension::CHubStubExtension(PVOID pVHub) 
    : CBaseDeviceExtension ()
    , m_pVHub (pVHub)
    , m_uInstance (0)
	, m_bDel (false)
{
    SetDebugName ("CHubStubExtension");

	RtlInitUnicodeString (&m_LinkName, L"\\DosDevices\\VHubStub");
	RtlInitUnicodeString (&m_DeviceName, L"\\Device\\VHubStub");

    KeInitializeTimer(&m_pTimerDeviceRelations);
    KeInitializeDpc(&m_pKdpcDeviceRelations, &CHubStubExtension::DeviceRelationsDpc, this);

}

CHubStubExtension::~CHubStubExtension()
{
}

TPtrPdoExtension CHubStubExtension::GetDevByPort (ULONG uPort)
{
	TPtrPdoExtension			pPdo = NULL;
	CVirtualUsbHubExtension		*pHub = (CVirtualUsbHubExtension*)m_pVHub;

	DebugPrint (DebugTrace, "GetDevByPort");

	pHub->m_spinPdo.Lock();
	for (int a = 0; a < pHub->m_arrPdo.GetCount(); a++ )
	{
		pPdo = pHub->m_arrPdo [a];
		if (pPdo.get () && pPdo->m_uNumberPort == uPort)
		{
			break;
		}
	}
	pHub->m_spinPdo.UnLock();

	return pPdo;
}

NTSTATUS CHubStubExtension::DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "DispatchDevCtrl");

	PIO_STACK_LOCATION  irpStack;
	NTSTATUS            status = STATUS_INVALID_PARAMETER;

	++m_irpCounter;
	irpStack = IoGetCurrentIrpStackLocation (Irp);

    DebugPrint (DebugSpecial, "DispatchDevCtrl %s", evuh::CUsbDevToTcp::UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode));

	switch (irpStack->Parameters.DeviceIoControl.IoControlCode) 
	{
    case IOCTL_USB_GET_NODE_INFORMATION:
        {
            PUSB_NODE_INFORMATION pNode;
            CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;

			if (pHub && irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(USB_NODE_INFORMATION))
            {
                pNode = (PUSB_NODE_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
                pNode->NodeType = UsbHub;
                pNode->u.HubInformation.HubIsBusPowered = true;
                pNode->u.HubInformation.HubDescriptor.bDescriptorLength = sizeof (USB_HUB_DESCRIPTOR);
                pNode->u.HubInformation.HubDescriptor.bDescriptorType = 0x29;
                pNode->u.HubInformation.HubDescriptor.bNumberOfPorts = (char)(min (127, pHub->m_uMaxPort));
                pNode->u.HubInformation.HubDescriptor.wHubCharacteristics = 0;
                pNode->u.HubInformation.HubDescriptor.bPowerOnToPowerGood = 0;
                pNode->u.HubInformation.HubDescriptor.bHubControlCurrent = 0;
                pNode->u.HubInformation.HubDescriptor.bRemoveAndPowerMask [0] = 0;

                Irp->IoStatus.Information = sizeof (USB_NODE_INFORMATION);
                Irp->IoStatus.Status = STATUS_SUCCESS;
                status = STATUS_SUCCESS;
            }
        }
        break;
    case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
        {
            PUSB_NODE_CONNECTION_INFORMATION pNode;
            CVirtualUsbHubExtension			*pHub = (CVirtualUsbHubExtension*)m_pVHub;
			TPtrPdoExtension				pPdo;

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (USB_NODE_CONNECTION_INFORMATION))
            {
                pNode = (PUSB_NODE_CONNECTION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
				pPdo = GetDevByPort (pNode->ConnectionIndex);
                if (pNode->ConnectionIndex < (ULONG)pHub->m_uMaxPort + 1)
                {
					pPdo = GetDevByPort (pNode->ConnectionIndex);
                    if (pPdo.get () && pPdo->m_bPresent)
                    {
						evuh::CUsbDevToTcp        *pTcp;
						pTcp = pPdo->m_pTcp; 

						++pTcp->m_WaitUse;
						status = pPdo->m_pTcp->OutIrpToTcp (&m_PdoHub, Irp);
						--pTcp->m_WaitUse;
						--m_irpCounter;

						IoSetCancelRoutine (Irp, CHubStubExtension::CancelRoutine);

						return status;
                    }
					else
					{
						ULONG ConnectionIndex = pNode->ConnectionIndex;
						RtlZeroMemory (pNode, sizeof (USB_NODE_CONNECTION_INFORMATION));
						pNode->ConnectionIndex = ConnectionIndex;
						pNode->ConnectionIndex = ConnectionIndex;
						Irp->IoStatus.Information = sizeof (USB_NODE_CONNECTION_INFORMATION);
						status = STATUS_SUCCESS;
					}
                }
            }
        }
        break;
    case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
        {
            PUSB_NODE_CONNECTION_INFORMATION_EX pNode;
            CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;
            TPtrPdoExtension		pPdo;

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (USB_NODE_CONNECTION_INFORMATION_EX))
            {
                pNode = (PUSB_NODE_CONNECTION_INFORMATION_EX)Irp->AssociatedIrp.SystemBuffer;

				if (pNode && pNode->ConnectionIndex < (ULONG)pHub->m_uMaxPort + 1)
                {
					pPdo = GetDevByPort (pNode->ConnectionIndex);
                    if (pPdo.get () && pPdo->m_bPresent)
                    {
						evuh::CUsbDevToTcp        *pTcp;

						pTcp = pPdo->m_pTcp; 

						++pTcp->m_WaitUse;
						status = pPdo->m_pTcp->OutIrpToTcp (&m_PdoHub, Irp);
						--pTcp->m_WaitUse;
						--m_irpCounter;

						IoSetCancelRoutine (Irp, CHubStubExtension::CancelRoutine);

						return status;
                    }
					else
					{
						ULONG ConnectionIndex = pNode->ConnectionIndex;
						RtlZeroMemory (pNode, sizeof (USB_NODE_CONNECTION_INFORMATION_EX));
						pNode->ConnectionIndex = ConnectionIndex;
						Irp->IoStatus.Information = sizeof (USB_NODE_CONNECTION_INFORMATION_EX);
						status = STATUS_SUCCESS;
					}
                }
            }
        }
        break;
    case IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME:
        {
            ULONG NeedSize = 0;
            ULONG NeedSizeStruct = 0;
            PUSB_NODE_CONNECTION_DRIVERKEY_NAME pNode;

            CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;
            TPtrPdoExtension pPdo;

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (USB_NODE_CONNECTION_DRIVERKEY_NAME))
            {
                pNode = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;

				pPdo = GetDevByPort (pNode->ConnectionIndex);

				if (pNode && pNode->ConnectionIndex < (ULONG)pHub->m_uMaxPort + 1)
                {
					pPdo = GetDevByPort (pNode->ConnectionIndex);

					if (pPdo.get () && pPdo->m_bPresent)
					{
						IoGetDeviceProperty(pPdo->GetDeviceObject (), DevicePropertyDriverKeyName, 0, NULL, &NeedSize);

						NeedSizeStruct = NeedSize + sizeof (USB_NODE_CONNECTION_DRIVERKEY_NAME);
						if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < NeedSizeStruct && 
							irpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof (USB_NODE_CONNECTION_DRIVERKEY_NAME))
						{
							pNode->ActualLength = NeedSizeStruct;
							pNode->DriverKeyName [0] = 0;
							status = STATUS_SUCCESS;
							Irp->IoStatus.Information = sizeof (USB_NODE_CONNECTION_DRIVERKEY_NAME);
						}
						else if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >= NeedSizeStruct)
						{
							pNode->ActualLength = NeedSizeStruct;
							IoGetDeviceProperty(pPdo->GetDeviceObject (), DevicePropertyDriverKeyName, NeedSize, pNode->DriverKeyName, &NeedSize);
							status = STATUS_SUCCESS;
							Irp->IoStatus.Information = NeedSizeStruct;
						}
					}
                }
            }
        }
        break;  
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
		{
            CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;
			TPtrPdoExtension		pPdo;
			USB_DESCRIPTOR_REQUEST *pRequest;

            if (irpStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof (USB_DESCRIPTOR_REQUEST))
			{
				pRequest = (USB_DESCRIPTOR_REQUEST*)Irp->AssociatedIrp.SystemBuffer;

				if (pRequest && pRequest->ConnectionIndex < (ULONG)pHub->m_uMaxPort + 1)
				{
					evuh::CUsbDevToTcp        *pTcp;

					pPdo = GetDevByPort (pRequest->ConnectionIndex);
					if (pPdo.get () && pPdo->m_bPresent)
					{
						pTcp = pPdo->m_pTcp; 

						++pTcp->m_WaitUse;
						status = pPdo->m_pTcp->OutIrpToTcp (&m_PdoHub, Irp);
						--pTcp->m_WaitUse;
						--m_irpCounter;

						IoSetCancelRoutine (Irp, CHubStubExtension::CancelRoutine);

						return status;
					}
				}
			}

		}
		break;
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		{
            CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;
			TPtrPdoExtension pPdo;
			USB_NODE_CONNECTION_NAME *pRequest;

            if (irpStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof (USB_NODE_CONNECTION_NAME))
			{
				pRequest = (USB_NODE_CONNECTION_NAME*)Irp->AssociatedIrp.SystemBuffer;

				if (pRequest && pRequest->ConnectionIndex < (ULONG)pHub->m_uMaxPort + 1)
				{
					evuh::CUsbDevToTcp        *pTcp;

					pPdo = GetDevByPort (pRequest->ConnectionIndex);
					if (pPdo.get ())
					{
						pTcp = pPdo->m_pTcp; 

						++pTcp->m_WaitUse;
						status = pPdo->m_pTcp->OutIrpToTcp (&m_PdoHub, Irp);
						--pTcp->m_WaitUse;
						--m_irpCounter;

						IoSetCancelRoutine (Irp, CHubStubExtension::CancelRoutine);
						return status;
					}
				}
			}

		}
		break;

	default:
        DebugPrint (DebugSpecial, "DispatchDevCtrl %s", evuh::CUsbDevToTcp::UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode));
        DebugPrint (DebugWarning, "not support irp");
		break;
	}

	if (STATUS_PENDING != status)
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		--m_irpCounter;
	}
	return status;
}

NTSTATUS CHubStubExtension::DispatchIntrenalDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION  irpStack;
	NTSTATUS            status = STATUS_INVALID_PARAMETER;

	irpStack = IoGetCurrentIrpStackLocation (Irp);

    DebugPrint (DebugSpecial, "DispatchIntrenalDevCtrl %s", evuh::CUsbDevToTcp::UsbIoControl (irpStack->Parameters.DeviceIoControl.IoControlCode));

	return DispatchDefault (DeviceObject, Irp);
}

NTSTATUS CHubStubExtension::DispatchCreate	(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "DispatchCreate");
	++m_irpCounter;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounter;

    return Irp->IoStatus.Status;
}

NTSTATUS CHubStubExtension::DispatchClose	(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugPrint (DebugTrace, "DispatchClose");
	++m_irpCounter;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	--m_irpCounter;

    return Irp->IoStatus.Status;
}

// ********************************************************************
//					virtual major function
// ********************************************************************

NTSTATUS CHubStubExtension::DispatchPower (PDEVICE_OBJECT DeviceObject, PIRP Irp)
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

    switch (stack->MinorFunction) {
    case IRP_MN_SET_POWER:

        switch (powerType) {
            case DevicePowerState:
                PoSetPowerState (m_Self, powerType, powerState);
                status = STATUS_SUCCESS;
                break;

            case SystemPowerState:
                status = STATUS_SUCCESS;
                break;

            default:
                status = STATUS_NOT_SUPPORTED;
                break;
        }
        break;

    case IRP_MN_QUERY_POWER:
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_WAIT_WAKE:
    case IRP_MN_POWER_SEQUENCE:
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (status != STATUS_NOT_SUPPORTED) {

        Irp->IoStatus.Status = status;
    }

    PoStartNextPowerIrp(Irp);
    status = Irp->IoStatus.Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

	--m_irpCounter;

    return status;
}


NTSTATUS CHubStubExtension::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS                status = STATUS_NOT_SUPPORTED;
	PIO_STACK_LOCATION		irpStack;


	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	DebugPrint (DebugTrace, "%s", PnPMinorFunctionString (Irp));

	irpStack = IoGetCurrentIrpStackLocation(Irp);

    ++m_irpCounter;

	status = Irp->IoStatus.Status;

    switch (irpStack->MinorFunction) 
    {

    case IRP_MN_START_DEVICE:
        status = STATUS_SUCCESS;
        {
            GUID hubInterface = {0xf18a0e88, 0xc30c, 0x11d0, 0x88, 0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8};
            status = IoRegisterDeviceInterface (
                        m_Self,
                        (LPGUID) &hubInterface,
                        NULL,
                        &m_LinkInterface);

            if (NT_SUCCESS (status))
            {
                IoSetDeviceInterfaceState (&m_LinkInterface, true);
            }
        }

        break;

    case IRP_MN_STOP_DEVICE:
        SetNewPnpState (DriverBase::Stopped);
        status = STATUS_SUCCESS;
        break;


    case IRP_MN_QUERY_STOP_DEVICE:
        SetNewPnpState (DriverBase::StopPending);
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_STOP_DEVICE:
        if (DriverBase::StopPending == m_DevicePnPState)
        {
            RestorePnpState ();
        }
        status = STATUS_SUCCESS;// We must not fail this IRP.
        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:
        SetNewPnpState (DriverBase::RemovePending);
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:
        if (DriverBase::RemovePending == m_DevicePnPState)
        {
            RestorePnpState ();
        }
        status = STATUS_SUCCESS; // We must not fail this IRP.
        break;

    case IRP_MN_SURPRISE_REMOVAL:
        SetNewPnpState (DriverBase::SurpriseRemovePending);
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_REMOVE_DEVICE:
        {
            CVirtualUsbHubExtension  *pHub = (CVirtualUsbHubExtension*)m_pVHub;

            SetNewPnpState (DriverBase::Deleted);

			if (m_bDel)
			{
				--m_irpCounter;
				--m_irpCounter;
				m_irpCounter.WaitRemove ();

				IoDeleteSymbolicLink (&m_LinkName);
				m_Self->DeviceExtension = 0;
				IoDeleteDevice (m_Self);
				m_Self = NULL;

				delete this;
			}
			else
			{
				--m_irpCounter;
				m_irpCounter.WaitStop ();
			}

			Irp->IoStatus.Status = STATUS_SUCCESS;
			IoCompleteRequest (Irp, IO_NO_INCREMENT);

            return STATUS_SUCCESS;
        }

    case IRP_MN_QUERY_CAPABILITIES:

        //
        // Return the capabilities of a device, such as whether the device
        // can be locked or ejected..etc
        //

        status = QueryDeviceCaps (Irp);

        break;

    case IRP_MN_QUERY_ID:
        status = QueryDeviceId (Irp);
        break;

    case IRP_MN_QUERY_INTERFACE:
        DebugPrint (DebugWarning, "DispatchPnp - no support");
        break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
        if (BusRelations != irpStack->Parameters.QueryDeviceRelations.Type) 
		{
            break;
        }
		status = RelationsPdos (Irp);
        break;

    case IRP_MN_QUERY_BUS_INFORMATION:
        status = QueryBusInformation (Irp);
        break;


    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
    default:

        status = Irp->IoStatus.Status;

        break;
    }

    --m_irpCounter;

	Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return status;

}

NTSTATUS CHubStubExtension::DispatchDefault (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS CHubStubExtension::QueryBusInformation(PIRP   Irp)
{
    PPNP_BUS_INFORMATION busInfo;
    GUID    InterfaceGUID = {0xf18a0e88, 0xc30c, 0x11d0, 0x88, 0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8};

	DebugPrint (DebugTrace, "QueryBusInformation");

    busInfo = (PPNP_BUS_INFORMATION)ExAllocatePoolWithTag (PagedPool, sizeof(PNP_BUS_INFORMATION),'HUB');

    if (busInfo == NULL) 
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    busInfo->BusTypeGuid = InterfaceGUID;
    busInfo->LegacyBusType = PNPBus;
    busInfo->BusNumber = 0;
    Irp->IoStatus.Information = (ULONG_PTR)busInfo;

    return STATUS_SUCCESS;

}

NTSTATUS CHubStubExtension::QueryResourceRequirements(PIRP   Irp)
{
    PIO_RESOURCE_REQUIREMENTS_LIST  resourceList;
    ULONG resourceListSize;
    NTSTATUS status;

	DebugPrint (DebugTrace, "QueryResourceRequirements");

    resourceListSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST);

    resourceList =  (PIO_RESOURCE_REQUIREMENTS_LIST)ExAllocatePoolWithTag (PagedPool, resourceListSize, 'hub');

    if (resourceList == NULL ) 
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    RtlZeroMemory( resourceList, resourceListSize );

    resourceList->ListSize = resourceListSize;

    //
    // Initialize the list header.
    //
    resourceList->AlternativeLists = 0;//1;
    resourceList->InterfaceType = InterfaceTypeUndefined;
    resourceList->BusNumber = 0;
    resourceList->List[0].Version = 1;
    resourceList->List[0].Revision = 1;
    resourceList->List[0].Count = 1;

    Irp->IoStatus.Information = (ULONG_PTR)resourceList;
    status = STATUS_SUCCESS;
    
    return status;
}

NTSTATUS CHubStubExtension::QueryDeviceId (PIRP Irp)
{
    NTSTATUS                status = STATUS_SUCCESS;
    PIO_STACK_LOCATION      stack;
    PWCHAR                  buffer;
	PWCHAR					temp;
    ULONG                   length;
    
    stack = IoGetCurrentIrpStackLocation (Irp);

	DebugPrint (DebugTrace, "QueryDeviceId");

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
        swprintf (buffer, L"%04d", m_uInstance);
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
        // The generic ids for installation of this pdo.
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

NTSTATUS CHubStubExtension::QueryDeviceCaps(PIRP   Irp)
{

    PIO_STACK_LOCATION      stack;
    PDEVICE_CAPABILITIES    deviceCapabilities;
    DEVICE_CAPABILITIES     parentCapabilities;
    NTSTATUS                status;
    CVirtualUsbHubExtension  *pHub = (CVirtualUsbHubExtension*)m_pVHub;
    
	DebugPrint (DebugTrace, "QueryDeviceCaps");

    stack = IoGetCurrentIrpStackLocation (Irp);

    //
    // Get the packet.
    //
    deviceCapabilities = (PDEVICE_CAPABILITIES) stack->Parameters.DeviceCapabilities.Capabilities;

    //
    // Set the capabilities.
    //

    if(deviceCapabilities->Version != 1 || deviceCapabilities->Size < sizeof(DEVICE_CAPABILITIES))
    {
       return STATUS_UNSUCCESSFUL; 
    }
    
    //
    // Get the device capabilities of the parent
    //
    status = GetDeviceCapabilities (pHub->m_NextLowerDriver, &parentCapabilities);
    if (!NT_SUCCESS(status)) 
    {
        return status;
    }

    //
    // The entries in the DeviceState array are based on the capabilities 
    // of the parent devnode. These entries signify the highest-powered 
    // state that the device can support for the corresponding system 
    // state. A driver can specify a lower (less-powered) state than the 
    // bus driver.  For eg: Suppose the virtual serial bus controller supports 
    // D0, D2, and D3; and the Virtual Serial Port supports D0, D1, D2, and D3. 
    // Following the above rule, the device cannot specify D1 as one of 
    // it's power state. A driver can make the rules more restrictive 
    // but cannot loosen them.
    // First copy the parent's S to D state mapping
    //

    RtlCopyMemory(  deviceCapabilities->DeviceState,
                    parentCapabilities.DeviceState,
                    (PowerSystemShutdown + 1) * sizeof(DEVICE_POWER_STATE));

    //
    // Adjust the caps to what your device supports.
    // Our device just supports D0 and D3.
    //

    deviceCapabilities->DeviceState[PowerSystemWorking] = PowerDeviceD0;

    if(deviceCapabilities->DeviceState[PowerSystemSleeping1] != PowerDeviceD0)
        deviceCapabilities->DeviceState[PowerSystemSleeping1] = PowerDeviceD3;
        
    if(deviceCapabilities->DeviceState[PowerSystemSleeping2] != PowerDeviceD0)
        deviceCapabilities->DeviceState[PowerSystemSleeping2] = PowerDeviceD3;

    if(deviceCapabilities->DeviceState[PowerSystemSleeping3] != PowerDeviceD0)
        deviceCapabilities->DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
        
    // We cannot wake the system.
    
    deviceCapabilities->SystemWake = PowerSystemUnspecified;
    deviceCapabilities->DeviceWake = PowerDeviceUnspecified;

    //
    // Specifies whether the device hardware supports the D1 and D2
    // power state. Set these bits explicitly.
    //

    deviceCapabilities->DeviceD1 = FALSE;
    deviceCapabilities->DeviceD2 = FALSE;

    //
    // Specifies whether the device can respond to an external wake 
    // signal while in the D0, D1, D2, and D3 state. 
    // Set these bits explicitly.
    //
    
    deviceCapabilities->WakeFromD0 = FALSE;
    deviceCapabilities->WakeFromD1 = FALSE;
    deviceCapabilities->WakeFromD2 = FALSE;
    deviceCapabilities->WakeFromD3 = FALSE;


    // We have no latencies
    
    deviceCapabilities->D1Latency = 0;
    deviceCapabilities->D2Latency = 0;
    deviceCapabilities->D3Latency = 0;

    // Ejection supported
    
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
    deviceCapabilities->Address = 0xFFFFFFFF;//(ULONG)((ULONG_PTR)&DeviceData->VirtualUart);//SerialNo;

    //#if _WIN32_WINNT >= _WIN32_WINNT_LONGHORN
		ApplyXPSilentInstallHack (m_uInstance);
    //#endif

    //
    // UINumber specifies a number associated with the device that can 
    // be displayed in the user interface. 
    //
    deviceCapabilities->UINumber = m_uInstance;


    return STATUS_SUCCESS;
}

NTSTATUS CHubStubExtension::GetDeviceCapabilities(PDEVICE_OBJECT DeviceObject, PDEVICE_CAPABILITIES DeviceCapabilities)
{
    IO_STATUS_BLOCK     ioStatus;
    KEVENT              pnpEvent;
    NTSTATUS            status;
    PDEVICE_OBJECT      targetObject;
    PIO_STACK_LOCATION  irpStack;
    PIRP                pnpIrp;

	DebugPrint (DebugTrace, "GetDeviceCapabilities");
    //
    // Initialize the capabilities that we will send down
    //
    RtlZeroMemory( DeviceCapabilities, sizeof(DEVICE_CAPABILITIES) );
    DeviceCapabilities->Size = sizeof(DEVICE_CAPABILITIES);
    DeviceCapabilities->Version = 1;
    DeviceCapabilities->Address = (ULONG)-1;
    DeviceCapabilities->UINumber = (ULONG)-1;

    //
    // Initialize the event
    //
    KeInitializeEvent( &pnpEvent, NotificationEvent, FALSE );

    targetObject = IoGetAttachedDeviceReference( DeviceObject );

    //
    // Build an Irp
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
    else
    {
        pnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        irpStack = IoGetNextIrpStackLocation( pnpIrp );

        RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );
        irpStack->MajorFunction = IRP_MJ_PNP;
        irpStack->MinorFunction = IRP_MN_QUERY_CAPABILITIES;
        irpStack->Parameters.DeviceCapabilities.Capabilities = (PDEVICE_CAPABILITIES)DeviceCapabilities;

        //
        // Call the driver
        //
        status = IoCallDriver( targetObject, pnpIrp );
        if (status == STATUS_PENDING) 
        {
            KeWaitForSingleObject(&pnpEvent, Executive, KernelMode, FALSE, NULL);
            status = ioStatus.Status;
        }
    }

    //
    // Done with reference
    //
    ObDereferenceObject( targetObject );

    //
    // Done
    //
    return status;
}


void CHubStubExtension::ApplyXPSilentInstallHack (ULONG uInstance)
{
	PWCHAR path, data;
	NTSTATUS status;
	ULONG i;
	PWCHAR szGiudClass = L"{BA60D326-EE12-4957-B702-7F5EDBE9ECE5}";

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
		L"DriverVersion",	L"3.0.380",
		L"MatchingDeviceId",	L"UsbEStub",
		L"DriverDesc",	L"Usb Stub (Eltima software)"
	}, EnumValues[] = {
		L"DeviceDesc",	L"Usb Stub (Eltima software)",
		L"LocationInformation",	L"Usb Stub (Eltima software)",
		L"ClassGUID", szGiudClass,
		L"Class",	L"EltimaUsbStub",
		L"Mfg",	L"ELTIMA Software",
		L"Service",	L"eustub"
	};

	if (GetOsVersion () >= 600)
	{
		return;
	}

	DebugPrint (DebugTrace, "ApplyXPSilentInstallHack");


	path = (PWCHAR)ExAllocatePoolWithTag (NonPagedPool, 512 * sizeof(WCHAR), 'hack');
	do 
	{
		if (!path) 
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		data = path + 256;

		swprintf (path, L"Class");
		status = RtlCreateRegistryKey(RTL_REGISTRY_CONTROL, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}
		RtlStringCbCatW (path, 200, L"\\");
		RtlStringCbCatW (path, 200, szGiudClass);
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

		swprintf (path + wcslen(path), L"\\%.4d", uInstance);
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

		swprintf (path, L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum\\UsbEStub");
		status = RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		RtlStringCbCatW (path, 200, L"\\Devices");
		status = RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE, path);
		if (!NT_SUCCESS(status))
		{
			break;
		}
		swprintf (path + wcslen(path), L"\\%.4d", uInstance);
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

		swprintf (data, L"%s\\%.4d", szGiudClass, uInstance);
		status = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, path, L"Driver", REG_SZ,
			(PVOID)data, (wcslen(data) + 1) * sizeof(WCHAR));
		if (!NT_SUCCESS(status))
		{
			break;
		}

		RtlStringCchCopyW (data, 200, L"Eltima Usb Stub");
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

	if (path)	
	{
		ExFreePool(path);
	}
}


NTSTATUS CHubStubExtension::CreateDevice ()
{
    NTSTATUS            status;
	//
    // Create a filter device object.
    //
	DebugPrint (DebugTrace, "CreateDevice");

    status = IoCreateDevice (m_DriverObject,
                             sizeof (CHubStubExtension*),
                             &m_DeviceName,  // No Name
                             FILE_DEVICE_BUS_EXTENDER,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &m_Self);

    if (!NT_SUCCESS (status)) 
	{
        //
        // Returning failure here prevents the entire stack from functioning,
        // but most likely the rest of the stack will not be able to create
        // device objects either, so it is still OK.
        //
		DebugPrint (DebugError, "Did not create to FDO");

        return status;
    }
    *((CHubStubExtension**)m_Self->DeviceExtension) = this;
    //
    // Failure for attachment is an indication of a broken plug & play system.
    //

	if (!NT_SUCCESS (IoCreateSymbolicLink (&m_LinkName, &m_DeviceName)))
	{
        IoDeleteDevice(m_Self);
		m_Self = NULL;
        return STATUS_UNSUCCESSFUL;
	}

    //
    // Set the initial state of the Filter DO
    //
    m_DevicePnPState =  DriverBase::NotStarted;
    m_PreviousPnPState = DriverBase::NotStarted;

    m_Self->Flags &= ~DO_DEVICE_INITIALIZING;

	m_PdoHub.m_Self = GetDeviceObject ();
	m_PdoHub.m_DeviceIndef = 0;
	m_PdoHub.m_bPresent = true;
    return STATUS_SUCCESS;
}

NTSTATUS CHubStubExtension::RelationsPdos (PIRP Irp)
{
    PIO_STACK_LOCATION              irpStack;
    NTSTATUS                        status = STATUS_SUCCESS;;
    PDEVICE_RELATIONS		        relations, oldRelations;
    ULONG					        length, prevCount, numPdosPresent;
	TPtrPdoExtension			     pPdo;

	DebugPrint (DebugTrace, "RelationsPdos");

    CVirtualUsbHubExtension *pHub = (CVirtualUsbHubExtension*)m_pVHub;

    irpStack = IoGetCurrentIrpStackLocation (Irp);

	pHub->m_spinPdo.Lock ();

    oldRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information;
    if (oldRelations) 
	{
        prevCount = oldRelations->Count; 
        if (!pHub->m_arrPdo.GetCount ()) 
		{
            //
            // There is a device relations struct already present and we have
            // nothing to add to it, so just call IoSkip and IoCall
            //
            pHub->m_spinPdo.UnLock ();
            return status;
        }
    }
    else  
	{
        prevCount = 0;
    }

    // 
    // Calculate the number of PDOs actually present on the bus
    // 
    numPdosPresent = 0;
	for (int a = 0; a < pHub->m_arrPdo.GetCount (); a++)
	{
		pPdo = pHub->m_arrPdo [a];
		if (pPdo.get() && pPdo->IsPnpReady ())
		{
			numPdosPresent++;
		}
	}

	pHub->m_spinPdo.UnLock ();
    //
    // Need to allocate a new relations structure and add our
    // PDOs to it.
    //
    
    length = sizeof(DEVICE_RELATIONS) +
            ((numPdosPresent + prevCount) * sizeof (PDEVICE_OBJECT));

    relations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag (NonPagedPool, length, 'Rela');

    if (NULL == relations) 
	{
        //
        // Fail the IRP
        //

        Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;

    }

    //
    // Copy in the device objects so far
    //
    if (prevCount) 
	{
        RtlCopyMemory (relations->Objects, oldRelations->Objects,
                                  prevCount * sizeof (PDEVICE_OBJECT));
    }
    
    relations->Count = prevCount + numPdosPresent;

    //
    // For each PDO present on this bus add a pointer to the device relations
    // buffer, being sure to take out a reference to that object.
    // The Plug & Play system will dereference the object when it is done 
    // with it and free the device relations buffer.
    //

	pHub->m_spinPdo.Lock ();
	for (int a = 0; a < pHub->m_arrPdo.GetCount (); a++)
	{
        pPdo = pHub->m_arrPdo[a];
        if(pPdo->IsPnpReady ()) 
		{
			if (pPdo->GetDeviceObject ())
			{
				relations->Objects[prevCount] = pPdo->GetDeviceObject ();
				ObReferenceObject(pPdo->GetDeviceObject ());
			}
            prevCount++;
        } 
    }
	pHub->m_spinPdo.UnLock ();
	DebugPrint (DebugInfo, "PDOs present - %d, PDOs reported - %d", pHub->m_arrPdo.GetCount (), relations->Count);
	DebugPrint (DebugSpecial, "PDOs present - %d, PDOs reported - %d", pHub->m_arrPdo.GetCount (), relations->Count);
    
    //
    // Replace the relations structure in the IRP with the new
    // one.
    //
    if (oldRelations) 
	{
        ExFreePool (oldRelations);
    }
    Irp->IoStatus.Information = (ULONG_PTR) relations;


    Irp->IoStatus.Status = STATUS_SUCCESS;
	return status;
}

void CHubStubExtension::SetDeviceRelationsTimer ()
{
	LARGE_INTEGER Timer = {-500 * 1000 * 10, -1}; // 100 ms

	DebugPrint (DebugTrace, "SetDeviceRelationsTimer");

	KeCancelTimer (&m_pTimerDeviceRelations);
	KeSetTimer (&m_pTimerDeviceRelations, Timer, &m_pKdpcDeviceRelations);
}

VOID CHubStubExtension::DeviceRelationsDpc(struct _KDPC *Dpc,PVOID  DeferredContext,PVOID  SystemArgument1,PVOID  SystemArgument2)
{
	CHubStubExtension *pBase = (CHubStubExtension*)DeferredContext;

	pBase->DebugPrint (DebugTrace, "DeviceRelationsDpc");

	IoInvalidateDeviceRelations (pBase->m_Self, BusRelations);
}

VOID CHubStubExtension::CancelRoutine( PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	evuh::CUsbDevToTcp			*pBase = NULL;
	CHubStubExtension		*pPdo = (CHubStubExtension*)CBaseDeviceExtension::GetExtensionClass (DeviceObject);
	PIRP_NUMBER				irpNumber = NULL;

	pBase->DebugPrint (DebugTrace, "CancelRoutine");

	pBase = ((CVirtualUsbHubExtension*) (pPdo->m_pVHub))->FindTcp(Irp);

	if (pBase)
	{
		pBase->DebugPrint (DebugTrace, "CancelRoutine irp- 0x%x", Irp);
		++pBase->m_WaitUse;

		IoReleaseCancelSpinLock( Irp->CancelIrql );

		pBase->m_spinUsbIn.Lock ();
		for (int a = 0; a < pBase->m_listIrpAnswer.GetCount (); a++)
		{
			irpNumber = (PIRP_NUMBER)pBase->m_listIrpAnswer.GetItem (a);
			if (irpNumber->Irp == Irp)
			{
				pBase->m_listIrpAnswer.Remove (a);

				pBase->m_spinUsbIn.UnLock ();

				irpNumber->Cancel = true;

				pBase->UrbCancel (Irp);

				pBase->CompleteIrp (irpNumber);

				--pBase->m_WaitUse;
				return;

			}
		}
		pBase->m_spinUsbIn.UnLock ();
		--pBase->m_WaitUse;
	}
	else
	{
		IoReleaseCancelSpinLock( Irp->CancelIrql );
	}
}