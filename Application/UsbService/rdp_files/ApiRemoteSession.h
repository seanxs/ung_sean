/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ApiRemoteSession.h

Environment:

    User mode

--*/

#pragma once

#include <WtsApi32.h>
#include <wfapi.h>
#include <set>
#include "..\Consts\Lock.h"

typedef HANDLE (WINAPI *FUNCVirtualChannelOpen) (IN HANDLE hServer,
                                   IN DWORD  SessionId,
                                   IN LPSTR  pVirtualName); /* ascii name */


typedef BOOL (WINAPI *PFUNCVirtualChannelQuery)(IN  HANDLE           hChannelHandle,
                        IN  WF_VIRTUAL_CLASS VirtualClass,
                        OUT PVOID*           ppBuffer,
                        OUT DWORD*           pBytesReturned);

typedef VOID (WINAPI *PFUNCFreeMemory)(IN PVOID pMemory);

typedef BOOL (WINAPI *PFUNCVirtualChannelClose) (IN HANDLE hChannelHandle);


class CApiRemoteSession;

class CApiRemoteSession
{
public:
	CApiRemoteSession(void);
	~CApiRemoteSession(void);

	static CApiRemoteSession& impl ()
	{
		static CApiRemoteSession base;
		return base;
	}

	HANDLE VirtualChannelOpen (HANDLE hServer, DWORD  SessionId, LPSTR  pVirtualName);
	BOOL VirtualChannelQuery (HANDLE hChannelHandle, int VirtualClass, PVOID *ppBuffer, DWORD *pBytesReturned);
	VOID FreeMemory (IN PVOID pMemory);
	BOOL VirtualChannelClose (HANDLE hChannelHandle);

protected:
	CLock		m_lock;
	HMODULE		m_hCitrix;
	typedef std::set <HANDLE> SETHANDLE;

	SETHANDLE	setChannelRdp;

	FUNCVirtualChannelOpen m_pVirtualChannelOpen;
	PFUNCVirtualChannelQuery m_pVirtualChannelQuery;
	PFUNCFreeMemory m_pFreeMemory;
	PFUNCVirtualChannelClose m_pVirtualChannelClose;
};

