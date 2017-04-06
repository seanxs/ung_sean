/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbInterface.h

Environment:

    Kernel mode

--*/

#pragma once
#include "evuh.h"

#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)
#include <strmini.h>
#include <ksmedia.h>
#include "usbdi.h"
#include "usbcamdi.h"
#pragma pack(pop)
}

class CUsbInterface
{
private:
	CUsbInterface(){}
	~CUsbInterface() {}

	// *************************
	// usb interface 
public:
	static void InitUsbInterface(PUSB_BUS_INTERFACE_USBDI_V3_FULL pInterface, CPdoExtension *pPdo);
	// version 0
	static VOID InterfaceReferenceSend(PVOID Context, int iFunc);
	static VOID InterfaceReference(PVOID Context);
	static VOID InterfaceDereference(PVOID Context);
	static VOID GetUSBDIVersion(PVOID Context, PUSBD_VERSION_INFORMATION VersionInformation, PULONG HcdCapabilities);
	static NTSTATUS QueryBusTime(PVOID  BusContext, PULONG  CurrentFrame);
	static NTSTATUS SubmitIsoOutUrb(PVOID  BusContext, PURB  Urb);
	static NTSTATUS QueryBusInformation(PVOID  BusContext, ULONG  Level, PVOID  BusInformationBuffer, PULONG  BusInformationBufferLength, PULONG  BusInformationActualLength);
	// version 1
	static BOOLEAN IsDeviceHighSpeed(PVOID  BusContext);
	// verison 2
	static NTSTATUS EnumLogEntry(PVOID  BusContext, ULONG  DriverTag, ULONG  EnumTag, ULONG  P1, ULONG  P2);
	// version 3
	static NTSTATUS QueryBusTimeEx(PVOID BusContext, PULONG HighSpeedFrameCounter);
	static NTSTATUS QueryControllerType(PVOID BusContext, PULONG HcdiOptionFlags, PUSHORT PciVendorId, PUSHORT PciDeviceId, PUCHAR PciClass, PUCHAR PciSubClass, PUCHAR PciRevisionId, PUCHAR PciProgIf);

	// *************************
	// usb Camd
	static VOID InterfaceReferenceUsbCamd(PVOID Context);
	static VOID InterfaceDereferenceUsbCamd(PVOID Context);
	static NTSTATUS USBCAMD_WaitOnDeviceEvent(PVOID  DeviceContext, ULONG  PipeIndex, PVOID  Buffer, ULONG  BufferLength
											, PCOMMAND_COMPLETE_FUNCTION  EventComplete, PVOID  EventContext, BOOLEAN  LoopBack);
	static NTSTATUS USBCAMD_BulkReadWrite(PVOID  DeviceContext, USHORT  PipeIndex, PVOID  Buffer, ULONG  BufferLength
										, PCOMMAND_COMPLETE_FUNCTION  CommandComplete, PVOID  CommandContext);

	static NTSTATUS USBCAMD_SetVideoFormat(PVOID  DeviceContext, PHW_STREAM_REQUEST_BLOCK  pSrb);
	static NTSTATUS USBCAMD_SetIsoPipeState(PVOID  DeviceContext, ULONG  PipeStateFlags);
	static NTSTATUS USBBCAMD_CancelBulkReadWrite(PVOID  DeviceContext, ULONG  PipeIndex);
	static bool FillInterfaceUsbCamd(PINTERFACE pInterface, CPdoExtension *pPdo);

};


