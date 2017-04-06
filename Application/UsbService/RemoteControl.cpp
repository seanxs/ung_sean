/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RemoteControl.cpp

Abstract:

	Class that waits for connection for service management

Environment:

    User mode

--*/

#include "stdafx.h"
#include <vector>
#include <WinSock2.h>
#include "Consts\consts.h"
#include "Consts\UngNetSettings.h"
#include "Manager.h"
#include "RemoteControl.h"


CRemoteControl::CRemoteControl(void)
	: m_sock (INVALID_SOCKET)
{
	m_Log.SetLogDest(Nirvana);
	//m_Log.SetLogDest(FileDest);
}

CRemoteControl::~CRemoteControl(void)
{
	CloseServer ();
}

// **************************************************
//			 Work with socket
// **************************************************

bool CRemoteControl::StartServer (short int sPort)
{
	DWORD dwFuncLogArea = LOG_INIT;
	bool bFuncResult = false;

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): ENTER - sPort = %d", sPort);

	do 
	{
		int rVal;
		SOCKADDR_IN sin;

		m_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_sock != INVALID_SOCKET)
		{
			sin.sin_family = PF_INET;
			sin.sin_port = htons(sPort);

			sin.sin_addr.s_addr = INADDR_ANY;
			//bind the socket
			rVal = bind(m_sock, (LPSOCKADDR)&sin, sizeof(sin));
			if (rVal != SOCKET_ERROR)
			{
				rVal = listen(m_sock, 10);
				if (rVal != SOCKET_ERROR)
				{
					DWORD	dw;
					CloseHandle (CreateThread (NULL, 0, ThreadAccept, this, 0, &dw));
					bFuncResult = true;
					break;
				}
			}
			else
			{
#ifdef LOG_FULL
				CManager::AddLogFile (_T("CService: error bind port 5475"));
#endif
			}
		}
	} while (false);

 
	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): EXIT - bFuncResult = \"%s\"", bFuncResult?"true":"false");
	return bFuncResult;
}

void CRemoteControl::CloseServer ()
{
	closesocket (m_sock);
	m_sock = INVALID_SOCKET;
}


////*******************************************************************************
////					Work with pipe
////*******************************************************************************
DWORD WINAPI CRemoteControl::ThreadAccept (LPVOID lpParam)
{
	return ((CRemoteControl*)lpParam)->ThreadAcceptRoutine();
}

DWORD CRemoteControl::ThreadAcceptRoutine ()
{
	DWORD dwFuncLogArea = LOG_THREAD;
	DWORD dwFuncResult = 0;

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	SOCKET			sockTemp = 0; 

	do
	{
		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Call accept(). Waiting for GUI connection...");
		sockTemp = accept (m_sock, NULL, NULL);

		if (sockTemp == INVALID_SOCKET)
		{
			m_Log.LogString(dwFuncLogArea, LogWarning, __FUNCTION__"(): accept() returns INVALID_SOCKET. Stop.");
			dwFuncResult = 1;
			break;
		}

		// start new accept
		{
			m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): launching another thread for next accept");
			DWORD	dw;
			CloseHandle (CreateThread (NULL, 0, ThreadAccept, this, 0, &dw));
		}

		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): accept() success.");

		std::vector<char> vBuff;
		vBuff.reserve (BUFSIZE_1K);	// minimal capacity 1K should be enough for most commands

		while (1) 
		{ 
			m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Reading GUI command...");

			vBuff.clear();
			bool bSocketError = false;
			bool bCompleteCommand = false;

			for (size_t a = 0; a < BUFSIZE_16K; ++a)
			{
				char x;

				// read byte-by-byte
				const int iBytesRead = recv ( 
					sockTemp,		// handle to pipe (socket)
					&x,				// buffer to receive data 
					1,				// size of buffer (1 byte)
					0);				// not overlapped I/O 

				if (iBytesRead == 0 || iBytesRead == SOCKET_ERROR) 
				{
					bSocketError = true;
					break; 
				}

				if (x == CUngNetSettings::GetEnd ())
				{
					// normal end of recv command
					bCompleteCommand = true;
					break;
				}

				vBuff.push_back(x);
			}

			if (!bSocketError)
			{
				// try to process command
				vBuff.push_back(0);
				m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Parsing and executing command...");
				ParseCommand (&vBuff[0], sockTemp);
				if (bCompleteCommand)
					continue;
				m_Log.LogString(dwFuncLogArea, LogWarning, __FUNCTION__"(): end marker wasn't received yet...");
			}

			while (!bSocketError && !bCompleteCommand)
			{
				// receive tail of the too long command
				char x;

				// read byte-by-byte
				const int iBytesRead = recv ( 
					sockTemp,		// handle to pipe (socket)
					&x,				// buffer to receive data 
					1,				// size of buffer (1 byte)
					0);				// not overlapped I/O 

				if (iBytesRead == 0 || iBytesRead == SOCKET_ERROR) 
				{
					bSocketError = true;
					break; 
				}

				if (x == CUngNetSettings::GetEnd ())
				{
					// normal end of recv command
					bCompleteCommand = true;
					break;
				}
			}

			if (bSocketError) 
			{
				break;
			}
		} 

		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Closing socket.");
		closesocket (sockTemp);
	} 
	while (0);

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): EXIT - return dwFuncResult = %d", dwFuncResult);
	return dwFuncResult;
}
