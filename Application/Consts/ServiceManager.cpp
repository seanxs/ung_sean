/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ServiceManager.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "..\Consts\log.h"
#include ".\servicemanager.h"
#include "..\consts\consts.h"
#include "sddl.h"

CServiceManager::CServiceManager(void)
	: m_sock (INVALID_SOCKET)
	, m_chEnd ('!')
	, m_strHost (_T("LOCALHOST"))
	, m_uPort (Consts::uTcpControl)
	, m_bAlreadyMes (FALSE)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD( 2, 2 );
	err = WSAStartup( wVersionRequested, &wsaData );

	DWORD dwFuncLogArea = LOG_CONSTR;

	m_Log.SetLogLevel(LogWarning);

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Try to open mutex \"%ws\".", Consts::wsRemoteControlMutexName);

	m_hRemoteControlMutex = OpenMutex(SYNCHRONIZE, FALSE, Consts::wsRemoteControlMutexName);

	if (m_hRemoteControlMutex == NULL)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Try to CREATE mutex \"%ws\".", Consts::wsRemoteControlMutexName);
			m_hRemoteControlMutex = CreateRemoteControlMutex(Consts::wsRemoteControlMutexName);
			
			if(m_hRemoteControlMutex)
			{
				m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Mutex \"%ws\" created successful!", Consts::wsRemoteControlMutexName);
			}
			else
			{
				m_Log.LogString(dwFuncLogArea, LogError, __FUNCTION__"(): Cannot CREATE mutex (GLE=%d)", GetLastError());
			}

		}
		else
		{
			m_Log.LogString(dwFuncLogArea, LogError, __FUNCTION__"(): Cannot OPEN mutex && GLE !=  ERROR_FILE_NOT_FOUND (GLE=%d)", GetLastError());
		}
	}
	else 
	{
		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Mutex \"%ws\" opened successful!", Consts::wsRemoteControlMutexName);
	}

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

CServiceManager::~CServiceManager(void)
{
	CloseHandle(m_hRemoteControlMutex);
}

void CServiceManager::SetCharEnd (TCHAR chEnd)
{
	m_chEnd = chEnd;
}

bool CServiceManager::OpenConnection (LPCTSTR szHost, USHORT uPort)
{
	DWORD dwFuncLogArea = LOG_CONNECT;
	bool bFuncResult = false;

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): ENTER - szHost = \"%ws\", uPort = %d", szHost, uPort);

	do 
	{
		if(m_hRemoteControlMutex)
		{
			m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): WATING for mutex...");
			DWORD dwWaitResult = WaitForSingleObject(m_hRemoteControlMutex, INFINITE);

			LPCTSTR strTextResult = NULL;

			switch(dwWaitResult)
			{
			case WAIT_OBJECT_0:
				{
					strTextResult = _T("WAIT_OBJECT_0");
					break;
				}
			case WAIT_ABANDONED:
				{
					strTextResult = _T("WAIT_ABANDONED");
					break;
				}
			case WAIT_TIMEOUT:
				{
					strTextResult = _T("WAIT_TIMEOUT");
					break;
				}
			case WAIT_FAILED:
				{
					strTextResult = _T("WAIT_FAILED");
					break;
				}

				if(strTextResult != NULL)
				{
					m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Wait result = \"%ws\"", strTextResult);
				}
				else
				{
					m_Log.LogString(dwFuncLogArea, LogWarning, __FUNCTION__"(): UNKNOWN wait result = %d", dwWaitResult);
				}

			}
		}
		else
		{
			m_Log.LogString(dwFuncLogArea, LogError, __FUNCTION__"(): Cannot open connection without mutex handle!");
			break;
		}

		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Mutex wait success. Try to create connection to service...");

		LPHOSTENT hostEntry;
		int rVal;
		SOCKADDR_IN serverInfo;
		hostEntry = gethostbyname(CStringA(szHost));

		serverInfo.sin_port = htons(uPort);

		if (!hostEntry)
		{
			break;
		}

		serverInfo.sin_family = PF_INET;
		serverInfo.sin_addr = *((LPIN_ADDR)*hostEntry->h_addr_list);

		m_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

		rVal = connect(m_sock, (LPSOCKADDR)&serverInfo, sizeof(serverInfo));

		if (rVal != INVALID_SOCKET)
		{
			m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Connection to service successful established.");
			bFuncResult = true;
		}
		else
		{
			int iErr = WSAGetLastError ();
			iErr = iErr;
		}

	} while (false);

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): EXIT - return \"%s\"", bFuncResult?"true":"false");
	return bFuncResult;
}

void CServiceManager::CloseConnection ()
{
	DWORD dwFuncLogArea = LOG_CONNECT;
	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	if (m_sock != INVALID_SOCKET)
	{
		closesocket (m_sock);
		m_sock = INVALID_SOCKET;
	}

	if (m_hRemoteControlMutex)
	{
		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Releasing mutex...");
		ReleaseMutex(m_hRemoteControlMutex);
	}

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

bool CServiceManager::SendCommand (LPCSTR szCommand, int iSize, CStringA *strAnswer)
{
	bool	bRet = false;

	DWORD dwFuncLogArea = LOG_UTIL;

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): ENTER - szCommand = \"%s\"", szCommand);

	do 
	{
		if (!OpenConnection (m_strHost, m_uPort))
		{
			if (WSAGetLastError() != WSAEADDRINUSE)
			{
				if (m_bAlreadyMes == FALSE) 
				{
					m_bAlreadyMes = TRUE;
				}
			}
			CloseConnection ();
			break;
		}

		DWORD	dwCount = 0;
		OVERLAPPED	over;
		CStringA	strAnsw;
		char		ch;

		m_bAlreadyMes = FALSE;

		ZeroMemory (&over, sizeof (over));
		over.hEvent = CreateEvent (NULL, true, false, NULL);
		if (send (m_sock, szCommand, iSize, 0) == iSize)
		{
			while (true)
			{
				dwCount = recv (m_sock, &ch, 1, 0);
				if (dwCount == 0 || ch == char (m_chEnd) || dwCount == INVALID_SOCKET)
				{
					break;
				}
				strAnsw += ch;
			}
			bRet = ParseAnsver (strAnsw);
		}
		if (strAnswer)
		{
			*strAnswer = strAnsw;
		}
		CloseHandle (over.hEvent);

	} while (0);

	CloseConnection();

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): EXIT - return \"%s\"", bRet?"true":"false");
	return bRet;
}

bool CServiceManager::ParseAnsver (LPCSTR szAnswer)
{
	return true;
}

HANDLE CServiceManager::CreateRemoteControlMutex(LPCWSTR wsMutexName)
{
	DWORD dwFuncLogArea = LOG_UTIL;
	HANDLE hFuncResult = NULL;

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	PSECURITY_DESCRIPTOR pSD;
	SECURITY_ATTRIBUTES sa;
	wchar_t * wzSecurityDescriptor =
		L"D:"                  
		L"(A;OICI;GA;;;WD)"     // Deny access to network service
		L"(A;OICI;GA;;;CO)"		// Allow read/write/execute to creator/owner
		L"(A;OICI;GA;;;BA)";    // Allow full control to administrators

	sa.bInheritHandle = TRUE; 
	sa.nLength = sizeof(sa);   

	ConvertStringSecurityDescriptorToSecurityDescriptorW (
		wzSecurityDescriptor,
		SDDL_REVISION_1,
		&sa.lpSecurityDescriptor, // <-- sa = SECURITY_ATTRIBUTES
		NULL
		); 
	pSD = (PSECURITY_DESCRIPTOR) LocalAlloc( LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH );
	InitializeSecurityDescriptor( pSD, SECURITY_DESCRIPTOR_REVISION );

	// Add a NUL DACL to the security descriptor...

	SetSecurityDescriptorDacl( pSD, TRUE, (PACL) NULL, FALSE );

	sa.nLength = sizeof( sa );
	sa.lpSecurityDescriptor = pSD;
	sa.bInheritHandle = TRUE;

	hFuncResult = CreateMutex( &sa, false, wsMutexName);

	LocalFree (sa.lpSecurityDescriptor);

	if (hFuncResult)
	{
		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Mutex \"%ws\" created successful!", wsMutexName);
	}
	else
	{
		int nError = GetLastError();
		m_Log.LogString(dwFuncLogArea, LogInfo, __FUNCTION__"(): Cannot create mutex \"%ws\" (GLE=%d)", wsMutexName, nError);
	}	

	m_Log.LogString(dwFuncLogArea, LogTrace, __FUNCTION__"(): EXIT - return hFuncResult = 0x%.8x", hFuncResult);
	return hFuncResult;
}
