/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Service.cpp

Abstract:

	Service class

Environment:

    User mode

--*/

#include "StdAfx.h"
#include <WtsApi32.h>
#include "rdp_files\common\filetransport.h"
#include "rdp_files\RemoteSession.h"
#include ".\service.h"
#include "consts.h"
#include "assert.h"
#include "Usb\UsbServer.h"

#define U2EC_EXPORTS_STATIC
#include "..\u2ec\dllmain.h"

CService*	CService::m_pService = NULL;

CService::CService(LPCTSTR pszName)
	: CServiceBaseClass(pszName)
	, m_hDevNotify (NULL)
	, m_hTimer (NULL)
	, m_Queue (NULL)
	, m_iCount (6)
	, m_lLastUpdate (0)
	, m_bUpdateTree (false)
{
	m_pService = this;
	m_Queue = CreateTimerQueue();
	CreateTimerQueueTimer(&m_hTimer, m_Queue, CService::WaitOrTimerCallback, 
							  this, 60000, 0, WT_EXECUTEDEFAULT);

	InitializeCriticalSection (&m_cs);
	SetCallBackOnChangeDevList (OnChangeDeviceManger);
}

CService::~CService ()
{
	SetCallBackOnChangeDevList (NULL);
	if (m_hTimer)
	{
		DeleteTimerQueueTimer (m_Queue, m_hTimer, NULL);
		m_hTimer = NULL;
	}
	if (m_Queue)
	{
		DeleteTimerQueue (m_Queue);
		m_Queue = NULL;
	}
	DeleteCriticalSection (&m_cs);
}

DWORD CService::OnShutdown ( DWORD& /*dwWin32Err*/, DWORD& /*dwSpecificErr*/, BOOL& bHandled )
{
    SetServiceStatus (SERVICE_STOP_PENDING);
    CheckPoint (300000);

    m_manager.Stop (this);
    bHandled = TRUE; 
    m_manager.AddLogFile (_T("------------------------------------"));
    m_manager.AddLogFile (_T("          Shutdown service             "));
    m_manager.AddLogFile (_T("------------------------------------"));
    return SERVICE_STOPPED;
}

DWORD CService::OnStop( DWORD& /*dwWin32Err*/, DWORD& /*dwSpecificErr*/, BOOL& bHandled )
{
    SetServiceStatus (SERVICE_STOP_PENDING);
    CheckPoint (300000);

    m_manager.Stop (this);
    bHandled = TRUE; 
    m_manager.AddLogFile (_T("------------------------------------"));
    m_manager.AddLogFile (_T("          Stop service             "));
    m_manager.AddLogFile (_T("------------------------------------"));
    return SERVICE_STOPPED;
}

BOOL CService::InitInstance( DWORD dwArgc, LPTSTR* lpszArgv, DWORD& dwSpecific )
{
	m_manager.AddLogFile (_T("------------------------------------"));
	m_manager.AddLogFile (_T("          Start service             "));
	m_manager.AddLogFile (_T("------------------------------------"));
    
	if (!m_manager.Start (Consts::uTcpPortConfig, Consts::uTcpUdp))
    {
		m_manager.AddLogFile (_T("Failed to start service"));
        return false;
    }

	m_manager.UpdateUsbTree ();
	// get new devices
	DEV_BROADCAST_DEVICEINTERFACE nf = {0};
    GUID const CLASS_GUID = {0xA5DCBF10L, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};

	ZeroMemory( &nf, sizeof(nf) );

	nf.dbcc_size = sizeof(nf);
	nf.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	nf.dbcc_classguid = CLASS_GUID;
	m_hDevNotify = RegisterDeviceNotification( m_status.GetHandle(), 
		                                       (LPVOID)&nf,
											   DEVICE_NOTIFY_SERVICE_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
	if (!m_hDevNotify)
	{
		m_hDevNotify = (HDEVNOTIFY)GetLastError ();
	}

    return true;
}

DWORD CService::OnSessionChange (DWORD& dwState, 
								 DWORD& dwWin32Err, 
								 DWORD& dwSpecificErr, 
								 BOOL&  bHandled, 
								 DWORD  dwEventType, 
								 LPVOID lpEventData, 
								 LPVOID lpContext)
{
	ULONG FuncLogArea = LOG_GENERAL;
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER %d", dwEventType);

	switch (dwEventType) 
	{

		case WTS_CONSOLE_CONNECT: 
			m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): WTS_CONSOLE_CONNECT");
			break;

		case WTS_CONSOLE_DISCONNECT: 
			m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): WTS_CONSOLE_DISCONNECT");
			break;

		case WTS_REMOTE_DISCONNECT: 
		case WTS_SESSION_LOGOFF: 
		case WTS_SESSION_LOCK:
			{
				m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): WTS_REMOTE_DISCONNECT");

				PWTSSESSION_NOTIFICATION pSessionInfo = (PWTSSESSION_NOTIFICATION)lpEventData;
				m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Removing session with ID = %d", 
					pSessionInfo->dwSessionId);


				PPARAMS_TO_WRAP pParams = new PARAMS_TO_WRAP;

				pParams->ActionType = RemoveSessionAction;
				pParams->dwSessionId = pSessionInfo->dwSessionId;

				m_manager.CreateWrapThread (pParams);

				// Session delete from not used rdp list
				std::set <LONG>::iterator item = m_setNotUsedRdp.find (pSessionInfo->dwSessionId);
				if (item != m_setNotUsedRdp.end ())
				{
					m_setNotUsedRdp.erase (item);
				}

				break;
			}

		case WTS_SESSION_LOGON: 
		case WTS_REMOTE_CONNECT: 
		case WTS_SESSION_UNLOCK: 
			{

 				PWTSSESSION_NOTIFICATION pSessionInfo = (PWTSSESSION_NOTIFICATION)lpEventData;
				LPTSTR szName = NULL;
				DWORD	dwSize = 0;

				WTSQuerySessionInformation ( WTS_CURRENT_SERVER_HANDLE, pSessionInfo->dwSessionId, WTSUserName, 
											&szName, &dwSize );

				if (szName && _tcslen (szName))
				{
					std::set <LONG>::iterator item = m_setNotUsedRdp.find (pSessionInfo->dwSessionId);
					if (item != m_setNotUsedRdp.end ())
					{
						m_setNotUsedRdp.erase (item);
					}

					// start rdp session
					PPARAMS_TO_WRAP pParams = new PARAMS_TO_WRAP;

					pParams->ActionType = AddSessionAction;
					pParams->dwSessionId = pSessionInfo->dwSessionId;

					m_manager.CreateWrapThread (pParams);
				}
				else
				{
					m_setNotUsedRdp.insert (pSessionInfo->dwSessionId);
				}

				if (szName)
				{
					WTSFreeMemory (szName);
				}
				m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): WTS_SESSION_LOGON");
				break;
			}

		case WTS_SESSION_REMOTE_CONTROL: 
			m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): WTS_SESSION_REMOTE_CONTROL");
			break;

		default:
			m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): UNSUPPORTED event type - %d", dwEventType);
	}

	bHandled = TRUE; 

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
	return NO_ERROR;
}


DWORD CService::OnDeviceChange(DWORD& dwState,
						DWORD& dwWin32Err,
						DWORD& dwSpecificErr,
						BOOL&  bHandled,
						DWORD  dwEventType,
						LPVOID lpEventData,
						LPVOID lpContext)
{
	switch (dwEventType)
	{
	case DBT_DEVICEARRIVAL:
	case DBT_DEVICEREMOVECOMPLETE:
		ResetTimer ();
		break;
	}

	return NO_ERROR;
}

void CService::ResetTimer ()
{
	EnterCriticalSection (&m_cs);
	if (m_hTimer)
	{
		DeleteTimerQueueTimer (m_Queue, m_hTimer, NULL);
		m_hTimer = NULL;
	}
	CreateTimerQueueTimer(&m_hTimer, m_Queue, CService::WaitOrTimerCallback, 
								this, 1000, 0, WT_EXECUTEDEFAULT);

	m_iCount = 0;
	LeaveCriticalSection (&m_cs);
}

VOID CService::WaitOrTimerCallback (PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	CService *pBase = (CService*)lpParameter;
	long lCurTick;

	EnterCriticalSection (&pBase->m_cs);

	if (pBase->m_bUpdateTree)
	{
		// now already updating 
		LeaveCriticalSection (&pBase->m_cs);
		return;
	}
	lCurTick = GetTickCount ();
	if (pBase->m_lLastUpdate + 1000 > lCurTick)
	{
		// Updating was made recently
		LeaveCriticalSection (&pBase->m_cs);
		return;
	}
	if (CUsbServer::m_cRestart.m_iCount != 0)
	{
		// Now is restarting device
		if (pBase->m_hTimer)
		{
			DeleteTimerQueueTimer (pBase->m_Queue, pBase->m_hTimer, NULL);
			pBase->m_hTimer = NULL;
		}
		CreateTimerQueueTimer(&pBase->m_hTimer, pBase->m_Queue, CService::WaitOrTimerCallback, 
								  pBase, 1000, 0, WT_EXECUTEDEFAULT);

		pBase->m_iCount = 0;
		LeaveCriticalSection (&pBase->m_cs);
		return;
	}
	pBase->m_bUpdateTree = true;
	LeaveCriticalSection (&pBase->m_cs);

	pBase->m_manager.UpdateUsbTree ();

	EnterCriticalSection (&pBase->m_cs);
	pBase->m_bUpdateTree = false;
	pBase->m_lLastUpdate = GetTickCount ();
	LeaveCriticalSection (&pBase->m_cs);

	EnterCriticalSection (&pBase->m_cs);
	DeleteTimerQueueTimer (pBase->m_Queue, pBase->m_hTimer, NULL);
	pBase->m_hTimer = NULL;

	if (pBase->m_iCount < 6)
	{
		++pBase->m_iCount;
	}
	CreateTimerQueueTimer(&pBase->m_hTimer, pBase->m_Queue, CService::WaitOrTimerCallback, 
						   pBase, 10000 * pBase->m_iCount, 0, WT_EXECUTEDEFAULT);

	LeaveCriticalSection (&pBase->m_cs);
}

void CService::OnChangeDeviceManger ()
{
	m_pService->WaitOrTimerCallback (m_pService, true);
}