/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Thread.cpp

Environment:

    Kernel mode

--*/

#include "Thread.h"

// *******************************************************************************************
//			Create thread
// *******************************************************************************************
NTSTATUS Thread::Create(PKSTART_ROUTINE StartRoutine, PVOID StartContext)
{
	HANDLE              threadHandle;
	NTSTATUS			status;

	status = PsCreateSystemThread(&threadHandle, (ACCESS_MASK)0L, NULL, 0L, NULL, StartRoutine, StartContext);

	if (!NT_SUCCESS(status))
	{
		
	}
	else
	{
		ZwClose(threadHandle);
	}

	return status;
}

bool Thread::Sleep(int iMSec)
{
	LARGE_INTEGER time;

	if (KeGetCurrentIrql() == PASSIVE_LEVEL)
	{
		time.QuadPart = iMSec * 1000 * 10 * -1;
		KeDelayExecutionThread(KernelMode, FALSE, &time);

		return true;
	}

	return false;
}

// *******************************************************************************************
//			CreateEx thread with two parameters
// *******************************************************************************************

struct TThreadContext
{
	Thread::PKSTART_ROUTINE_EX StartRoutine;
	LPVOID Context1;
	LPVOID Context2;
};

VOID ThreadEx(LPVOID Param)
{
	TThreadContext *pContext = (TThreadContext *)Param;

	pContext->StartRoutine(pContext->Context1, pContext->Context2);
	ExFreePool(pContext);
}


NTSTATUS Thread::CreateEx(Thread::PKSTART_ROUTINE_EX StartRoutine, PVOID Context, PVOID Context2)
{
	TThreadContext *pContext = (TThreadContext *)ExAllocatePoolWithTag(NonPagedPool, sizeof(TThreadContext), 'DRHT');

	pContext->StartRoutine = StartRoutine;
	pContext->Context1 = Context;
	pContext->Context2 = Context2;

	return Create(ThreadEx, pContext);

}

// *******************************************************************************************
//			CThreadPoolTask
// *******************************************************************************************
Thread::CThreadPoolTask::CThreadPoolTask()
	: m_Event(NotificationEvent, true)
	, m_iCount(0)
{
}

bool Thread::CThreadPoolTask::Start(PROUTINE_POOL Routine, PVOID Context)
{
	m_spin.Lock();
	if (!IsRunning ())
	{
		m_Event.ClearEvent();
		m_iCount = 1;
		m_spin.UnLock();

		m_Routine = Routine;
		m_Context = Context;

		Thread::Create(Thread::CThreadPoolTask::stRoutineWork, this);
		return true;
	}
	m_spin.UnLock();

	return false;
}

Thread::CThreadPoolTask::~CThreadPoolTask()
{
	// stop thread
	PushTask(NULL);
	// wait stop thread
	WaitStopThread (-1);
	// time to complete thread
	Thread::Sleep(0);
}

void Thread::CThreadPoolTask::PushTask(LPVOID pParam)
{
	m_spin.Lock();
	m_listQueue.Add(pParam);
	m_spin.UnLock();
}

PVOID Thread::CThreadPoolTask::WaitTask()
{
	PVOID lpParam = NULL;

	while (true)
	{
		m_spin.Lock();
		if (m_listQueue.GetCount())
		{
			lpParam = m_listQueue.PopFirst();
		}
		else
		{
			m_spin.UnLock();
			m_listQueue.WaitAddElemetn();
			continue;
		}
		m_spin.UnLock();

		break;
	}

	return lpParam;
}

void Thread::CThreadPoolTask::stRoutineWork(PVOID pParam)
{
	Thread::CThreadPoolTask *pBase = (Thread::CThreadPoolTask*)pParam;

	pBase->m_Routine(pBase, pBase->m_Context);
	pBase->PushTask(NULL);
	pBase->m_spin.Lock();
	--pBase->m_iCount;
	if (pBase->m_iCount == 0)
	{
		pBase->m_spin.UnLock();
		pBase->m_Event.SetEvent();
		return;
	}
	pBase->m_spin.UnLock();

}

void Thread::CThreadPoolTask::WaitStopThread(int iMSec)
{
	m_Event.Wait(iMSec);
}

bool Thread::CThreadPoolTask::IsRunning()
{
	return m_Event.Wait(0) ? false : true;
}

bool Thread::CThreadPoolTask::AddThread()
{
	m_spin.Lock();
	if (IsRunning())
	{
		++m_iCount;
		m_spin.UnLock();

		Thread::Create(Thread::CThreadPoolTask::stRoutineWork, this);
		return true;
	}
	m_spin.UnLock();

	return false;
}