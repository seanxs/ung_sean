/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   DebugPrintUrb.h

Abstract:

   URB additional info output class

Environment:

    Kernel mode

--*/
#ifndef DebugPrintUrb_H
#define DebugPrintUrb_H

#include "Core\BaseClass.h"

#pragma warning( disable : 4200 )
extern "C" {
#pragma pack(push, 8)
#include "usbdi.h"
#include "usbdlib.h"
#include "usbioctl.h"
#pragma pack(pop)
}

class CDebugPrintUrb : public CBaseClass
{
public:
	static void DebugUrb (CBaseClass *pBase, PIRP Irp);
	static void DebugUrb (CBaseClass *pBase, PURB urb);
	static int DebugUrbInterfaceDesc (CBaseClass *pBase, PUSB_INTERFACE_DESCRIPTOR pDesc);
	static void DebugUrbInterfaceDesc (CBaseClass *pBase, PUSBD_INTERFACE_INFORMATION pDesc);
	static void DebugUrbEndpointDesc (CBaseClass *pBase, PUSBD_PIPE_INFORMATION pDesc);
	static int DebugUrbEndpointDesc (CBaseClass *pBase, PUSB_ENDPOINT_DESCRIPTOR pDesc);
	static void DebugUrbControl (CBaseClass *pBase, PURB urb);
	static PCHAR UrbFunctionString(USHORT Func);
	static void DebugBuffer (CBaseClass *pBase, char* lpBuffer, LONG lSize);
	static void Debug_URB_HCD_AREA (CBaseClass *pBase, _URB_HCD_AREA *hcdArea);
    static PCHAR UsbIoControl (LONG lFunc);
	static PCHAR PipeType (UCHAR lFunc);
};
	
#endif