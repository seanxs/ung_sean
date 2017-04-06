/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Thread.h

Environment:

    Kernel mode

--*/

#pragma once

#include "BaseClass.h"
#include "List.h"
#include "Lock.h"
#include "KEvent.h"

namespace Thread
{
NTSTATUS Create(PKSTART_ROUTINE StartRoutine, PVOID Context);

typedef VOID(*PKSTART_ROUTINE_EX)(PVOID Context1, PVOID Context2);

NTSTATUS CreateEx(PKSTART_ROUTINE_EX StartRoutine, PVOID Context, PVOID Context2);

bool Sleep(int iMSec);

class CThreadPoolTask;
typedef VOID(*PROUTINE_POOL)(CThreadPoolTask *pTask, PVOID Context);

class CThreadPoolTask : virtual public CBaseClass
{
public:
	CThreadPoolTask();
	~CThreadPoolTask();

	bool Start(PROUTINE_POOL Routine, PVOID Context);
	bool AddThread();

	int GetCountTask() { return m_listQueue.GetCount(); } 
	int GetCountThread() { return m_iCount; } 

	void PushTask(LPVOID);
	PVOID WaitTask();

	void WaitStopThread(int iMSec);
	bool IsRunning();

protected:
	static void stRoutineWork(PVOID pParam);

protected:
	int				m_iCount;
	CList			m_listQueue;
	CSpinLock		m_spin;
	CKEvent			m_Event;

	PROUTINE_POOL	m_Routine;
	PVOID			m_Context;
};

};

