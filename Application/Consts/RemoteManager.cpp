/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RemoteManager.cpp

Abstract:
	
	Class that waits for incoming connections. It is used for the output of the info about
	shared devices

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "remotemanager.h"
#include "consts.h"

CRemoteManager::CRemoteManager(void)
	: m_hThread (INVALID_HANDLE_VALUE)
{
	m_hEvent = CreateEvent (NULL, true, true, NULL);

	WORD sockVersion;
	WSADATA wsaData;

	sockVersion = MAKEWORD(1,1);
	WSAStartup(sockVersion, &wsaData);
}

CRemoteManager::~CRemoteManager(void)
{
	WSACleanup ();
	CloseHandle (m_hEvent);
}

void CRemoteManager::Start (USHORT uTcpPort)
{
	DWORD	dwId;
	SetEvent (m_hEvent);
	m_uTcpPort = uTcpPort;
	m_hThread = CreateThread (NULL, 0, ThreadReport, this, 0, &dwId);
}

void CRemoteManager::Stop ()
{
	ResetEvent (m_hEvent);
	WaitForSingleObject (m_hThread, INFINITE);
}

DWORD CRemoteManager::ThreadReport (LPVOID lpParam)
{
	CRemoteManager *pBase = (CRemoteManager*)lpParam;
	SOCKET			sockListen, sockTemp;
	int rVal;
	SOCKADDR_IN sin;

	sockListen = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sockListen != INVALID_SOCKET)
	{
		sin.sin_family = PF_INET;
		sin.sin_port = htons(pBase->m_uTcpPort);

		sin.sin_addr.s_addr = INADDR_ANY;

		//bind the socket
		rVal = bind(sockListen, (LPSOCKADDR)&sin, sizeof(sin));

		if (rVal != SOCKET_ERROR)
		{
			rVal = listen(sockListen, 10);
			if (rVal == SOCKET_ERROR)
			{
				return 1;
			}

            fd_set fdAccept;
			struct timeval wait = {1,0};
			FD_ZERO (&fdAccept);
			while (WaitForSingleObject (pBase->m_hEvent, 0) == WAIT_OBJECT_0)
			{
				FD_SET (sockListen, &fdAccept);
				if (select (0, &fdAccept, NULL, NULL, &wait))
				{
                    rVal = sizeof (SOCKADDR_IN);
					sockTemp = accept (sockListen, (sockaddr*)&sin, &rVal);
					if (sockTemp != INVALID_SOCKET)
					{
						pBase->ExistConnect (sockTemp, sin);
						closesocket (sockTemp);
					}
				}
			}
			closesocket (sockListen);
		}
		else
		{
		}
	}
 
	return 0;
}
