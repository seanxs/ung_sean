/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PoolCompletingIrp.cpp

Environment:

    Kernel mode

--*/

#include "PoolCompletingIrp.h"
#include "UsbDevToTcp.h"

const int iNumOfThreads = 5;
const int iMaxOfThreads = 20;

CPoolCompletingIrp* CPoolCompletingIrp::m_stBase = NULL;

CPoolCompletingIrp::CPoolCompletingIrp()
{
	SetDebugName("CPoolCompletingIrp");
	m_threads.Start(ThreadCompleted, this);
	for (int a = 1; a < iNumOfThreads; a++)
	{
		m_threads.AddThread();
	}
}

CPoolCompletingIrp::~CPoolCompletingIrp()
{
}

void CPoolCompletingIrp::AddIrpToCompleting(PIRP_NUMBER irpNumber)
{
	m_threads.PushTask(irpNumber);
	if ((m_Count.GetCurCount () + m_threads.GetCountTask() - 1) > m_threads.GetCountThread())
	{
		if (iMaxOfThreads < m_threads.GetCountThread())
		{
			DebugPrint(DebugWarning, "raise max of thread %d", iMaxOfThreads);
			return;
		}
		m_threads.AddThread();
	}
}

VOID CPoolCompletingIrp::ThreadCompleted(Thread::CThreadPoolTask *pTask, PVOID Context)
{
	PIRP_NUMBER			irpNumber = NULL;

	while (irpNumber = (PIRP_NUMBER)pTask->WaitTask())
	{
		++Instance ()->m_Count;
		irpNumber->pTcp->CompleteIrp(irpNumber);
		--Instance()->m_Count;
	}
}

void CPoolCompletingIrp::Initialization()
{
	if (m_stBase == NULL)
	{
		m_stBase = new CPoolCompletingIrp;
	}
}

void CPoolCompletingIrp::Shutdown()
{
	if (m_stBase != NULL)
	{
		delete (CPoolCompletingIrp*)m_stBase;
		m_stBase = NULL;
	}
}
