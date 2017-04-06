/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbInterface.cpp

Environment:

    Kernel mode

--*/

#include "UsbInterface.h"
#include "UsbDevToTcp.h"
#include "PdoExtension.h"

// *******************************************
//				BUS_INTERFACE_USBDI
// *******************************************

void CUsbInterface::InitUsbInterface(PUSB_BUS_INTERFACE_USBDI_V3_FULL pInterface, CPdoExtension *pPdo)
{
	// version 0
	if (pInterface->BusContext)
	{
		pInterface->BusContext = pPdo->GetDeviceObject();
	}
	if (pInterface->InterfaceReference)
	{
		pInterface->InterfaceReference = InterfaceReference;
	}
	if (pInterface->InterfaceDereference)
	{
		pInterface->InterfaceDereference = InterfaceDereference;
	}
	if (pInterface->GetUSBDIVersion)
	{
		pInterface->GetUSBDIVersion = GetUSBDIVersion;
	}
	if (pInterface->QueryBusTime)
	{
		pInterface->QueryBusTime = QueryBusTime;
	}
	if (pInterface->SubmitIsoOutUrb)
	{
		pInterface->SubmitIsoOutUrb = SubmitIsoOutUrb;
	}
	if (pInterface->QueryBusInformation)
	{
		pInterface->QueryBusInformation = QueryBusInformation;
	}
	if (pInterface->Version > 0 && pInterface->IsDeviceHighSpeed)
	{
		pInterface->IsDeviceHighSpeed = IsDeviceHighSpeed;
	}
	if (pInterface->Version > 1 && pInterface->EnumLogEntry)
	{
		pInterface->EnumLogEntry = EnumLogEntry;
	}
	if (pInterface->Version > 2 && pInterface->QueryBusTimeEx)
	{
		pInterface->QueryBusTimeEx = QueryBusTimeEx;
	}
	if (pInterface->Version > 2)
	{
		pInterface->QueryControllerType = QueryControllerType;
	}
}


VOID CUsbInterface::InterfaceReferenceSend(PVOID Context, int iFunc)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)Context);
	evuh::CUsbDevToTcp	*pTcp;
	KIRQL				CurIrql;

	if (!pBase)
	{
		return;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "GetUSBDIVersion");
	CurIrql = KeGetCurrentIrql();
	pTcp = pBase->m_pTcp;

	if (!pTcp || (CurIrql != PASSIVE_LEVEL))
	{
		return;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		return;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));
	pInterface->lNumFunc = iFunc;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	ExFreePool(pInterface);
}

VOID CUsbInterface::InterfaceReference(PVOID Context)
{
	return;
	InterfaceReferenceSend(Context, FI_InterfaceReference);
}

VOID CUsbInterface::InterfaceDereference(PVOID Context)
{
	return;
	InterfaceReferenceSend(Context, FI_InterfaceDereference);
}

VOID CUsbInterface::GetUSBDIVersion(PVOID Context, PUSBD_VERSION_INFORMATION VersionInformation, PULONG HcdCapabilities)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)Context);
	evuh::CUsbDevToTcp        *pTcp;
	KIRQL				CurIrql;

	if (!pBase)
	{
		return;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "GetUSBDIVersion");
	CurIrql = KeGetCurrentIrql();
	pTcp = pBase->m_pTcp;

	if (!pTcp || (CurIrql != PASSIVE_LEVEL))
	{
		return;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));

	pInterface->lNumFunc = FI_GetUSBDIVersion;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	VersionInformation->USBDI_Version = pInterface->Param1;
	VersionInformation->Supported_USB_Version = pInterface->Param2;
	*HcdCapabilities = pInterface->Param3;

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;
}

NTSTATUS CUsbInterface::QueryBusTime(PVOID  BusContext, PULONG  CurrentFrame)
{
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)BusContext);
	evuh::CUsbDevToTcp	*pTcp;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}

	pTcp = pBase->m_pTcp;

	if (!pTcp)
	{
		return STATUS_NOT_SUPPORTED;
	}

	return pTcp->m_Isoch.QueryBusTime(BusContext, CurrentFrame);
}

NTSTATUS CUsbInterface::SubmitIsoOutUrb(PVOID  BusContext, PURB  Urb)
{
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)BusContext);
	if (pBase)
	{
		pBase->DebugPrint(CBaseClass::DebugTrace, "SubmitIsoOutUrb");
	}
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS CUsbInterface::QueryBusInformation(PVOID  BusContext, ULONG  Level, PVOID  BusInformationBuffer, PULONG  BusInformationBufferLength, PULONG  BusInformationActualLength)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)BusContext);
	evuh::CUsbDevToTcp        *pTcp;
	NTSTATUS			status;
	KIRQL CurIrql;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "QueryBusInformation");
	CurIrql = KeGetCurrentIrql();
	pTcp = pBase->m_pTcp;

	if (!pTcp || (CurIrql != PASSIVE_LEVEL))
	{
		return STATUS_NOT_SUPPORTED;
	}
	++pTcp->m_WaitUse;

	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		*BusInformationActualLength = 0;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));
	pInterface->lNumFunc = FI_QueryBusInformation;
	pInterface->Param1 = Level;
	pInterface->Param2 = LONG((ULONG_PTR)BusInformationBuffer);
	pInterface->Param3 = *BusInformationBufferLength;

	pTcp->SetInterfaceFunction(pInterface, pBase, BusInformationBuffer);

	*BusInformationBufferLength = pInterface->Param3;
	*BusInformationActualLength = (ULONG)pInterface->Param4;
	status = NTSTATUS(pInterface->Param5);

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;
	return status;
}

// version 1
BOOLEAN CUsbInterface::IsDeviceHighSpeed(PVOID  BusContext)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)BusContext);
	evuh::CUsbDevToTcp        *pTcp;
	BOOLEAN				bRet;
	KIRQL CurIrql;

	if (!pBase)
	{
		return false;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "IsDeviceHighSpeed");
	CurIrql = KeGetCurrentIrql();
	pTcp = pBase->m_pTcp;

	if (!pTcp || (CurIrql != PASSIVE_LEVEL))
	{
		return false;
	}
	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return false;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));
	pInterface->lNumFunc = FI_IsDeviceHighSpeed;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	bRet = pInterface->Param1 ? TRUE : FALSE;

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;
	return bRet;
}

// version 2
NTSTATUS CUsbInterface::EnumLogEntry(PVOID  BusContext, ULONG  DriverTag, ULONG  EnumTag, ULONG  P1, ULONG  P2)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)BusContext);
	evuh::CUsbDevToTcp        *pTcp;
	NTSTATUS			status;
	KIRQL CurIrql;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "EnumLogEntry");
	CurIrql = KeGetCurrentIrql();
	pTcp = pBase->m_pTcp;

	if (!pTcp || (CurIrql != PASSIVE_LEVEL))
	{
		return STATUS_NOT_SUPPORTED;
	}
	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));
	pInterface->lNumFunc = FI_EnumLogEntry;
	pInterface->Param1 = DriverTag;
	pInterface->Param2 = EnumTag;
	pInterface->Param3 = P1;
	pInterface->Param4 = P2;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	status = NTSTATUS(pInterface->Param5);
	ExFreePool(pInterface);

	--pTcp->m_WaitUse;
	return status;
}

// version 3
NTSTATUS CUsbInterface::QueryBusTimeEx(PVOID BusContext, PULONG HighSpeedFrameCounter)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)BusContext);
	evuh::CUsbDevToTcp	*pTcp;
	NTSTATUS			status;
	KIRQL CurIrql;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "QueryBusTimeEx");
	CurIrql = KeGetCurrentIrql();
	pTcp = pBase->m_pTcp;

	if (!pTcp || (CurIrql != PASSIVE_LEVEL))
	{
		return STATUS_NOT_SUPPORTED;
	}

	++pTcp->m_WaitUse;

	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));
	pInterface->lNumFunc = FI_QueryBusTimeEx;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	*HighSpeedFrameCounter = pInterface->Param1;
	status = NTSTATUS(pInterface->Param2);
	ExFreePool(pInterface);

	--pTcp->m_WaitUse;
	return status;
}

NTSTATUS CUsbInterface::QueryControllerType(PVOID BusContext, PULONG HcdiOptionFlags, PUSHORT PciVendorId, PUSHORT PciDeviceId, PUCHAR PciClass, PUCHAR PciSubClass, PUCHAR PciRevisionId, PUCHAR PciProgIf)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)BusContext);
	evuh::CUsbDevToTcp	*pTcp;
	NTSTATUS			status;
	KIRQL CurIrql;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}
	pTcp = pBase->m_pTcp;
	pBase->DebugPrint(CBaseClass::DebugTrace, "QueryControllerType");
	CurIrql = KeGetCurrentIrql();

	if (!pTcp || (CurIrql != PASSIVE_LEVEL))
	{
		return STATUS_NOT_SUPPORTED;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));
	pInterface->lNumFunc = FI_QueryControllerType;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	if (HcdiOptionFlags)
	{
		*HcdiOptionFlags = pInterface->Param1;
	}
	USHORT *PciVer = (USHORT*)&pInterface->Param2;
	if (PciVendorId)
	{
		*PciVendorId = PciVer[0];
	}

	if (PciDeviceId)
	{
		*PciDeviceId = PciVer[1];
	}
	UCHAR   *PciOther = (UCHAR*)&pInterface->Param3;

	if (PciClass)
	{
		*PciClass = PciOther[0];
	}
	if (PciSubClass)
	{
		*PciSubClass = PciOther[1];
	}
	if (PciRevisionId)
	{
		*PciRevisionId = PciOther[2];
	}
	if (PciProgIf)
	{
		*PciProgIf = PciOther[3];
	}
	status = NTSTATUS(pInterface->Param4);
	ExFreePool(pInterface);
	--pTcp->m_WaitUse;
	return status;
}


// ***************************************************************************
//			Usb Camd

VOID CUsbInterface::InterfaceReferenceUsbCamd(PVOID Context)
{
	InterfaceReferenceSend(Context, FI_USBCAMD_InterfaceReference);
}

VOID CUsbInterface::InterfaceDereferenceUsbCamd(PVOID Context)
{
	InterfaceReferenceSend(Context, FI_USBCAMD_InterfaceDereference);
}

bool CUsbInterface::FillInterfaceUsbCamd(PINTERFACE pInterface, CPdoExtension *pPdo)
{
	PUSBCAMD_INTERFACE pUsbCamd;
	if ((USBCAMD_VERSION_200 != pInterface->Version)
		&& (pInterface->Size < sizeof(USBCAMD_INTERFACE)))
	{
		return false;
	}
	pUsbCamd = (PUSBCAMD_INTERFACE)pInterface;

	if (pUsbCamd->Interface.Context)
	{
		pUsbCamd->Interface.Context = pPdo->GetDeviceObject();
	}
	if (pUsbCamd->Interface.InterfaceReference)
	{
		pUsbCamd->Interface.InterfaceReference = InterfaceReferenceUsbCamd;
	}
	if (pUsbCamd->Interface.InterfaceDereference)
	{
		pUsbCamd->Interface.InterfaceDereference = InterfaceDereferenceUsbCamd;
	}
	if (pUsbCamd->USBCAMD_WaitOnDeviceEvent)
	{
		pUsbCamd->USBCAMD_WaitOnDeviceEvent = USBCAMD_WaitOnDeviceEvent;
	}
	if (pUsbCamd->USBCAMD_BulkReadWrite)
	{
		pUsbCamd->USBCAMD_BulkReadWrite = USBCAMD_BulkReadWrite;
	}
	if (pUsbCamd->USBCAMD_SetVideoFormat)
	{
		pUsbCamd->USBCAMD_SetVideoFormat = USBCAMD_SetVideoFormat;
	}
	if (pUsbCamd->USBCAMD_SetIsoPipeState)
	{
		pUsbCamd->USBCAMD_SetIsoPipeState = USBCAMD_SetIsoPipeState;
	}
	if (pUsbCamd->USBCAMD_CancelBulkReadWrite)
	{
		pUsbCamd->USBCAMD_CancelBulkReadWrite = USBBCAMD_CancelBulkReadWrite;
	}

	return true;
}

NTSTATUS CUsbInterface::USBCAMD_WaitOnDeviceEvent(PVOID  DeviceContext, ULONG  PipeIndex, PVOID  Buffer, ULONG  BufferLength
												 ,PCOMMAND_COMPLETE_FUNCTION  EventComplete, PVOID  EventContext, BOOLEAN  LoopBack)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)DeviceContext);
	evuh::CUsbDevToTcp	*pTcp;
	NTSTATUS			status;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "GetUSBDIVersion");
	pTcp = pBase->m_pTcp;
	if (!pTcp)
	{
		return STATUS_NOT_SUPPORTED;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));

	pInterface->lNumFunc = FI_USBCAMD_WaitOnDeviceEvent;
	pInterface->Param1 = PipeIndex;
	pInterface->Param2 = BufferLength;
	pInterface->Param6 = (LONG64)EventComplete;
	pInterface->Param7 = (LONG64)EventContext;
	pInterface->Param5 = LoopBack;

	pTcp->SetInterfaceFunction(pInterface, pBase, Buffer);

	status = NTSTATUS(pInterface->Param1);

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;

	return status;
}

NTSTATUS CUsbInterface::USBCAMD_BulkReadWrite(PVOID  DeviceContext, USHORT  PipeIndex, PVOID  Buffer, ULONG  BufferLength
											, PCOMMAND_COMPLETE_FUNCTION  CommandComplete, PVOID  CommandContext)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)DeviceContext);
	evuh::CUsbDevToTcp	*pTcp;
	NTSTATUS			status;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}
	pBase->DebugPrint(CBaseClass::DebugTrace, "GetUSBDIVersion");
	pTcp = pBase->m_pTcp;
	if (!pTcp)
	{
		return STATUS_NOT_SUPPORTED;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));

	pInterface->lNumFunc = FI_USBCAMD_BulkReadWrite;
	pInterface->Param1 = PipeIndex;
	pInterface->Param2 = BufferLength;
	pInterface->Param6 = (LONG64)CommandComplete;
	pInterface->Param7 = (LONG64)CommandContext;

	pTcp->SetInterfaceFunction(pInterface, pBase, Buffer);

	status = NTSTATUS(pInterface->Param1);

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;

	return status;
}

NTSTATUS CUsbInterface::USBCAMD_SetVideoFormat(PVOID  DeviceContext, PHW_STREAM_REQUEST_BLOCK  pSrb)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)DeviceContext);
	evuh::CUsbDevToTcp	*pTcp;
	NTSTATUS			status;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}
	pBase->DebugPrint(CBaseClass::DebugTrace, "GetUSBDIVersion");
	pTcp = pBase->m_pTcp;

	if (!pTcp)
	{
		return STATUS_NOT_SUPPORTED;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));

	pInterface->lNumFunc = FI_USBCAMD_SetVideoFormat;
	pInterface->Param1 = pSrb->SizeOfThisPacket;

	pTcp->SetInterfaceFunction(pInterface, pBase, pSrb);

	status = NTSTATUS(pInterface->Param1);

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;

	return status;
}

NTSTATUS CUsbInterface::USBCAMD_SetIsoPipeState(PVOID  DeviceContext, ULONG  PipeStateFlags)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)DeviceContext);
	evuh::CUsbDevToTcp	*pTcp;
	NTSTATUS			status;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "GetUSBDIVersion");
	pTcp = pBase->m_pTcp;

	if (!pTcp)
	{
		return STATUS_NOT_SUPPORTED;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));

	pInterface->lNumFunc = FI_USBCAMD_SetIsoPipeState;
	pInterface->Param1 = PipeStateFlags;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	status = NTSTATUS(pInterface->Param1);

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;

	return status;
}

NTSTATUS CUsbInterface::USBBCAMD_CancelBulkReadWrite(PVOID  DeviceContext, ULONG  PipeIndex)
{
	PFUNC_INTERFACE		pInterface;
	CPdoExtension		*pBase = (CPdoExtension*)DriverBase::CBaseDeviceExtension::GetExtensionClass((PDEVICE_OBJECT)DeviceContext);
	evuh::CUsbDevToTcp	*pTcp;
	NTSTATUS			status;

	if (!pBase)
	{
		return STATUS_NOT_SUPPORTED;
	}

	pBase->DebugPrint(CBaseClass::DebugTrace, "GetUSBDIVersion");
	pTcp = pBase->m_pTcp;

	if (!pTcp)
	{
		return STATUS_NOT_SUPPORTED;
	}

	++pTcp->m_WaitUse;
	pInterface = (PFUNC_INTERFACE)ExAllocatePoolWithTag(PagedPool, sizeof(FUNC_INTERFACE), tagUsb);
	if (!pInterface)
	{
		--pTcp->m_WaitUse;
		return STATUS_NOT_SUPPORTED;
	}

	RtlZeroMemory(pInterface, sizeof(FUNC_INTERFACE));

	pInterface->lNumFunc = FI_USBCAMD_CancelBulkReadWrite;
	pInterface->Param1 = PipeIndex;

	pTcp->SetInterfaceFunction(pInterface, pBase, NULL);

	status = NTSTATUS(pInterface->Param1);

	ExFreePool(pInterface);
	--pTcp->m_WaitUse;

	return status;
}