/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   usb4rdpstub.h

Environment:

    User mode

--*/

#include "stdafx.h"
#include "cchannel.h"
#include "Context.h"

UINT VCAPITYPE VirtualChannelInit(LPVOID FAR *ppInitHandle, PCHANNEL_DEF pChannel, INT channelCount, ULONG versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc);
UINT VCAPITYPE VirtualChannelOpen(LPVOID pInitHandle, LPDWORD pOpenHandle, PCHAR pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc);
UINT VCAPITYPE VirtualChannelClose(DWORD openHandle);
UINT VCAPITYPE VirtualChannelWrite(DWORD openHandle, LPVOID pData, ULONG dataLength, LPVOID pUserData);

// load usb5rdp
extern HMODULE	hUsb4rdp;
typedef BOOL ( VCAPITYPE *VirtualChannelEntry)(PCHANNEL_ENTRY_POINTS pEntryPoints);
extern VirtualChannelEntry pfVirtualChannelEntry;
CHANNEL_INIT_EVENT_FN	pfEventStub;

int SendDataHpc (TPtrEventBuffer pBuffer);

