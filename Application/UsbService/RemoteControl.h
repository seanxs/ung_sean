/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RemoteControl.h

Abstract:

	Class that waits for connection for service management

Environment:

    User mode

--*/

#pragma once

#include "Consts\log.h"

class CRemoteControl
{
	CLog m_Log;

protected:
	SOCKET	m_sock;

public:
	CRemoteControl(void);
	~CRemoteControl(void);

	bool StartServer (short int sPort);
	void CloseServer ();
	static DWORD WINAPI ThreadAccept (LPVOID lpParam);
	DWORD ThreadAcceptRoutine ();

public:
	virtual bool ParseCommand (LPCSTR pBuff, SOCKET sock) = 0;
};
