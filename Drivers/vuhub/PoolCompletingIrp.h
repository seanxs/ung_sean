/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PoolCompletingIrp.h

Environment:

    Kernel mode

--*/

#pragma once
#include "evuh.h"
#include "Core\BaseClass.h"
#include "Core\Thread.h"
#include "Core\CounterIrp.h"
#include "private.h"

class CPoolCompletingIrp;

class CPoolCompletingIrp : public CBaseClass
{
public:
	CPoolCompletingIrp();
	~CPoolCompletingIrp();

public:
	void AddIrpToCompleting(PIRP_NUMBER);
	static CPoolCompletingIrp* Instance()
	{
		return m_stBase;
	}

	static void Initialization();
	static void Shutdown();
private:
	CCounterIrp					m_Count;
	static CPoolCompletingIrp*	m_stBase;
	Thread::CThreadPoolTask		m_threads;
	static VOID ThreadCompleted(Thread::CThreadPoolTask *pTask, PVOID Context);
};

