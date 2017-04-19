/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Service.cpp

Abstract:

	Service class

Environment:

    User mode

--*/

#pragma once

#include "stdafx.h"
#include "SflBase.h"
#include "Manager.h"
#include <set>
#include "Dbt.h"
#include <DbgHelp.h>

class CService: public CServiceBaseT<CService, SERVICE_ACCEPT_STOP | SERVICE_CONTROL_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE>
{
	friend class CUsbServiceApp;
	CLog m_Log;

	CService(LPCTSTR pszName);
	~CService ();

#if(_MSC_VER < 1300)	
	friend class CServiceBaseT<CMyService, SERVICE_ACCEPT_STOP | SERVICE_CONTROL_SHUTDOWN >;
#endif
	SFL_DECLARE_SERVICECLASS_FRIENDS()

	DWORD OnSessionChange ( DWORD& dwState, 
							DWORD& dwWin32Err, 
							DWORD& dwSpecificErr, 
							BOOL&  bHandled, 
							DWORD  dwEventType, 
							LPVOID lpEventData, 
							LPVOID lpContext
							);

	DWORD OnDeviceChange(DWORD& dwState,
							DWORD& dwWin32Err,
							DWORD& dwSpecificErr,
							BOOL&  bHandled,
							DWORD  dwEventType,
							LPVOID lpEventData,
							LPVOID lpContext);


	SFL_BEGIN_CONTROL_MAP_EX (CService)
		SFL_HANDLE_CONTROL_STOP()
		SFL_HANDLE_CONTROL_SHUTDOWN ()
		SFL_HANDLE_CONTROL_EX(SERVICE_CONTROL_SESSIONCHANGE, &CService::OnSessionChange)
		SFL_HANDLE_CONTROL_EX( SERVICE_CONTROL_DEVICEEVENT, &CService::OnDeviceChange )
	SFL_END_CONTROL_MAP_EX()


	DWORD OnStop (DWORD& /*dwWin32Err*/, DWORD& /*dwSpecificErr*/, BOOL& bHandled );
	DWORD OnShutdown (DWORD& /*dwWin32Err*/, DWORD& /*dwSpecificErr*/, BOOL& bHandled );
    BOOL InitInstance( DWORD dwArgc, LPTSTR* lpszArgv, DWORD& dwSpecific );
public:
    void CheckWait (int iTime) {CheckPoint (iTime);}

	static CService*	GetService () {return m_pService;}
	static CManager*	GetManager () {return &m_pService->m_manager;}

	void ResetTimer ();
	bool IsEnableTimer () {return (m_hTimer != NULL);}

	static void	UpdateUsbTree () 
	{
		m_pService->m_iCount = 0;
		m_pService->WaitOrTimerCallback (m_pService, true);
	}

protected:
    CManager	m_manager;
	std::set <LONG>	m_setNotUsedRdp;
	static LONG MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *pExceptionPointers);
protected:
	CRITICAL_SECTION	m_cs;
	HDEVNOTIFY			m_hDevNotify;
	HANDLE				m_hTimer; 
	HANDLE				m_Queue; 
	int					m_iCount;
	static CService*	m_pService;
	long				m_lLastUpdate;
	bool				m_bUpdateTree;

	static VOID CALLBACK WaitOrTimerCallback (PVOID lpParameter, BOOLEAN TimerOrWaitFired);
	static void CALLBACK OnChangeDeviceManger ();
};

typedef enum _ACTION_TYPE 
{
	AddSessionAction,
	RemoveSessionAction
} ACTION_TYPE, *PACTION_TYPE;

typedef struct _PARAMS_TO_WRAP 
{
	ACTION_TYPE ActionType;
	DWORD dwSessionId;
} PARAMS_TO_WRAP, *PPARAMS_TO_WRAP;

