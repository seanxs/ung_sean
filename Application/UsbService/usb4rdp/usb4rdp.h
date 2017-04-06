/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   usb4rdp.h

Environment:

    User mode

--*/

#include "stdafx.h"
#include "cchannel.h"
#include "Sddl.h"
#include "..\..\Consts\log.h"
#include "..\..\Consts\consts.h"
#include "..\..\Consts\ServiceManager.h"

// types
typedef struct _CLIENT_DATA 
{
	UINT Reserved;
	CHAR DataBuffer[1];
} CLIENT_DATA, *PCLIENT_DATA;

// usb4rdp.cpp
extern CLog logger;
extern CHANNEL_ENTRY_POINTS SavedEntryPoints;
extern HANDLE ClientHandle;
extern DWORD  ChannelHandle;
extern HANDLE hDataPipe;
extern wchar_t * g_wsPipeName;

 extern "C" 
 {
 	__declspec(dllexport) BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints);
 };


VOID VCAPITYPE VirtualChannelInitEvent(LPVOID pInitHandle, UINT Event, LPVOID pData, UINT dataLength);
VOID VCAPITYPE VirtualChannelOpenEvent(DWORD openHandle, UINT Event, LPVOID pData, 
									   UINT32 dataLength, UINT32 totalLength,  UINT32 dataFlag);
// utils.cpp
HANDLE CreateDataPipe(LPCWSTR wsPipeName);

// threads.cpp
DWORD WINAPI ThreadReadServicePipe(LPVOID lParam);
DWORD WINAPI ThreadReadPipe(LPVOID lParam);
