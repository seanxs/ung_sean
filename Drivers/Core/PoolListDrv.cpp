/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   PoolListDrv.cpp

Environment:

    Kernel mode

--*/
#include "PoolListDrv.h"


CPoolListDrv::CPoolListDrv()
	: m_EventAdd (NULL)
	, m_EventDel (NULL)
	, m_pBuff (NULL)
	, m_pList (NULL)
	, m_pMdl (NULL)
	, m_User (NULL)
	, m_WaitCount (0)
{
	KeInitializeEvent(&m_EventReady,
		NotificationEvent,
		FALSE);
}

CPoolListDrv::~CPoolListDrv(void)
{
	Clear ();
}

PVOID CPoolListDrv::Init (PVOID pList)
{
	NTSTATUS status;
	PVOID	 pTemp;

	POOL_LIST *pListPool = (POOL_LIST*)pList;

	if (m_pMdl)
	{
		return NULL;
	}
	
	pTemp = ExAllocatePoolWithTag(PagedPool, pListPool->uSizeAll, 'LP');
	if (!pTemp)
	{
		return NULL;
	}

	RtlCopyMemory (pTemp, pList, sizeof (POOL_LIST) + sizeof (ULONG) * pListPool->uCount);

	// get event
	status = ObReferenceObjectByHandle (pListPool->EventAdd, NULL, *ExEventObjectType, KernelMode, (PVOID*)&m_EventAdd, NULL);
	status = ObReferenceObjectByHandle (pListPool->EventDel, NULL, *ExEventObjectType, KernelMode, (PVOID*)&m_EventDel, NULL);
	status = ObReferenceObjectByHandle (pListPool->EventWork, NULL, *ExEventObjectType, KernelMode, (PVOID*)&m_EventWork, NULL);

	if (!m_EventAdd || !m_EventDel )
	{
		Clear();
		return NULL;
	}

	m_pMdl = IoAllocateMdl ( pTemp, pListPool->uSizeAll, FALSE, FALSE, NULL);

	if (m_pMdl)
	__try 
	{ 
		MmProbeAndLockPages(m_pMdl, KernelMode, IoModifyAccess);
	} 
	__except (EXCEPTION_EXECUTE_HANDLER) 
	{ 
		IoFreeMdl (m_pMdl); 
		m_pMdl = NULL; 
		Clear();
		return NULL;
	}

	__try
	{
		m_User = (char*)MmMapLockedPagesSpecifyCache(m_pMdl, UserMode, MmNonCached, NULL, FALSE, NormalPagePriority);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		IoFreeMdl(m_pMdl);
		m_pMdl = NULL;
		Clear();
		return NULL;
	}

	if (!m_User)
	{
		MmUnmapLockedPages (pTemp , m_pMdl);

		IoFreeMdl (m_pMdl); 
		m_pMdl = NULL; 
		Clear();
		return m_User;
	}

	m_pList = (POOL_LIST*)pTemp;
	m_pBuff = (char*)pTemp;

	KeSetEvent (&m_EventReady,
		IO_NO_INCREMENT,
		FALSE);

	KeSetEvent (m_EventWork,
		IO_NO_INCREMENT,
		FALSE);

	return m_User;
}

void CPoolListDrv::Clear ()
{
	PVOID	pTemp = m_pBuff;

	if (m_pBuff)
	{
		m_pBuff = NULL;
	}
	else
	{
		return;
	}

	if (m_EventWork)
	{
		KeClearEvent (m_EventWork);
		ObDereferenceObject (m_EventWork);
		m_EventWork = NULL;
	}

	while (m_pList->bWriteBusy)
	{
		KeWaitForSingleObject (m_EventAdd, Executive, KernelMode, FALSE, NULL);
	}
	while (m_pList->bReadBusy)
	{
		KeWaitForSingleObject (m_EventDel, Executive, KernelMode, FALSE, NULL);
	}


	if (m_pMdl) 
	{ 
		if (pTemp) 
		{ 
			MmUnmapLockedPages ( m_User, m_pMdl ); 
		} 
		MmUnlockPages (m_pMdl); 
		IoFreeMdl (m_pMdl); 
		m_pMdl = NULL;

		m_User = NULL;
	}

	if (m_EventAdd)
	{
		KeSetEvent (m_EventAdd, IO_NO_INCREMENT, FALSE);
		ObDereferenceObject (m_EventAdd);
		m_EventAdd = NULL;
	}
	if (m_EventDel)
	{
		KeSetEvent (m_EventDel, IO_NO_INCREMENT, FALSE);
		ObDereferenceObject (m_EventDel);
		m_EventDel = NULL;
	}

	KeSetEvent (&m_EventReady,
		IO_NO_INCREMENT,
		FALSE);


	if (pTemp)
	{
		ExFreePool (pTemp);
	}
}

void CPoolListDrv::Lock ()
{
	if (!m_pBuff)
	{
		return;			
	}
	while (InterlockedCompareExchange (&m_pList->Lock, 1, 0));
}

void CPoolListDrv::UnLock ()
{
	if (!m_pBuff)
	{
		return;			
	}
	InterlockedCompareExchange (&m_pList->Lock, 0, 1);
}

char* CPoolListDrv::GetCurNodeWrite ()
{
	while (true)
	{
		if (!m_pBuff)
		{
			return NULL;			
		}

		if (m_pList->bStopped)
		{
			return NULL;
		}

		if (m_pList->uUsed == m_pList->uCount)
		{
			NTSTATUS status = KeWaitForSingleObject (m_EventDel, Executive, KernelMode, FALSE, NULL);
			InterlockedIncrement (&m_WaitCount);
			continue;
		}
		break;
	}

	Lock ();
	if (m_pList->bStopped)
	{
		UnLock();
		return NULL;
	}
	m_pList->bWriteBusy = true;
	UnLock ();

	return m_pBuff + m_pList->Offests [m_pList->uCur];
}

void CPoolListDrv::PushCurNodeWrite ()
{
	if (!m_pBuff)
	{
		return;			
	}
	Lock ();
	if (!m_pList->bStopped)
	{
		m_pList->uCur = (m_pList->uCur + 1) % m_pList->uCount;
		++m_pList->uUsed;
	}
	m_pList->bWriteBusy = false; 
	UnLock ();
	KeSetEvent(m_EventAdd, IO_NO_INCREMENT, FALSE);
}

void CPoolListDrv::FreeNodeWrite()
{
	if (!m_pBuff)
	{
		return;
	}
	Lock();
	m_pList->bWriteBusy = false;
	UnLock();
	KeSetEvent(m_EventAdd, IO_NO_INCREMENT, FALSE);
}

char* CPoolListDrv::GetCurNodeRead ()
{
	ULONG uNum = 0;

	while (true)
	{
		if (!m_pBuff)
		{
			return NULL;			
		}

		if (m_pList->bStopped)
		{
			return false;
		}

		if (m_pList->uUsed == 0)
		{
			NTSTATUS status = KeWaitForSingleObject (m_EventAdd, Executive, KernelMode, FALSE, NULL);
			InterlockedIncrement (&m_WaitCount);
			continue;
		}
		break;
	}

	Lock ();
	if (m_pList->bStopped)
	{
		UnLock();
		return NULL;
	}
	uNum = (m_pList->uCount + m_pList->uCur - m_pList->uUsed) % m_pList->uCount;
	m_pList->bReadBusy = true;
	UnLock();

	return m_pBuff + m_pList->Offests [uNum];
}

void CPoolListDrv::PopCurNodeRead ()
{
	if (!m_pBuff)
	{
		return;			
	}

	Lock ();
	if (!m_pList->bStopped)
	{
		--m_pList->uUsed;
	}
	m_pList->bReadBusy = false;
	UnLock ();
	KeSetEvent(m_EventDel, IO_NO_INCREMENT, FALSE);
}

void CPoolListDrv::FreeCurNodeRead()
{
	if (!m_pBuff)
	{
		return;
	}

	Lock();
	m_pList->bReadBusy = false;
	UnLock();
	KeSetEvent(m_EventDel, IO_NO_INCREMENT, FALSE);
}

void CPoolListDrv::WaitReady ()
{
	KeWaitForSingleObject (
		&m_EventReady,
		Executive,
		KernelMode,
		FALSE,
		NULL);
}