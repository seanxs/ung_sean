/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RemoteManager.h

Abstract:

	Class that waits for incoming connections. It is used for the output of the info about
	shared devices

Environment:

    User mode

--*/

#pragma once

#include <windows.h>
#include <atlstr.h>

class CRemoteManager {

protected:
	HANDLE	m_hThread;
	HANDLE	m_hEvent;
    USHORT	m_uTcpPort;

public:
	CRemoteManager(void);
	~CRemoteManager(void);
	virtual void Start (USHORT uTcpPort);
	virtual void Stop ();
	virtual void ExistConnect (SOCKET sock, const SOCKADDR_IN &ip) = 0;

protected:  
	static DWORD WINAPI ThreadReport (LPVOID lpParam);

};
