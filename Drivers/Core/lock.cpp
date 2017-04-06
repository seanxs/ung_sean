/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   lock.cpp

Environment:

    Kernel mode

--*/

#include "Lock.h"

// ******************************************************************
//			CEventLock
// ******************************************************************

CEventLock::CEventLock(void)
{
	KeInitializeEvent(&m_hEvent, SynchronizationEvent, TRUE);
}

CEventLock::~CEventLock(void)
{
}

bool CEventLock::Lock (LONG lTag)
{
	if (KeGetCurrentIrql () == PASSIVE_LEVEL)
	{
		KeWaitForSingleObject (&m_hEvent, Executive, KernelMode, FALSE, NULL);
		m_Tag = lTag;
		return true;
	}
	else
	{
		LARGE_INTEGER	time;
		time.QuadPart = 0;
		bool	bRet;

		bRet = (KeWaitForSingleObject (&m_hEvent, Executive, KernelMode, FALSE, &time) == STATUS_SUCCESS);
		if (bRet)
		{
			m_Tag = lTag;
		}
		else
		{
			DebugPrint (DebugWarning, "CEventLock::Lock could not lock Tag=%x", lTag);
		}
		return bRet;

	}
	return false;
}

bool CEventLock::UnLock ()
{
	m_Tag = 0;
	KeSetEvent (&m_hEvent, IO_NO_INCREMENT, FALSE);
	return true;
}


// ******************************************************************
//			CSpinLock
// ******************************************************************
CSpinLock::CSpinLock(void)
	: m_oldIrql (PASSIVE_LEVEL)
{
	KeInitializeSpinLock (&m_SpinLock);
}

CSpinLock::~CSpinLock(void)
{
}

bool CSpinLock::Lock(LONG lTag)
{
	KIRQL OldIrql;
	KeAcquireSpinLock (&m_SpinLock, &OldIrql);

	m_oldIrql = OldIrql;
	m_Tag = lTag;
	return true;

}

bool CSpinLock::UnLock()
{
	m_Tag = 0;
	KeReleaseSpinLock (&m_SpinLock, m_oldIrql);
	return true;
}