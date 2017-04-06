/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PoolList.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "PoolList.h"
#include <atlstr.h>


CPoolList::CPoolList()
	: m_uReserv (0)
	, m_pListInit (NULL)
	, m_pBuff (NULL)
	, m_pList (NULL)
	, m_WaitCount (0)
	, m_MoreThan (0)
{
}

void CPoolList::Init (ULONG uCount, ULONG uSize, ULONG uReserv/*, LPCTSTR Tag*/)
{
	ULONG uSizeStruct = sizeof (POOL_LIST) + uCount * sizeof (ULONG);
	m_pListInit = (POOL_LIST*)new BYTE [uSizeStruct];

	FillMemory (m_pListInit, uSizeStruct, 0);

	m_uReserv = uReserv;
	m_pListInit->Lock = 0;
	m_pListInit->uCur = 0;
	m_pListInit->uUsed = 0;
	m_pListInit->uCount = uCount;
	m_pListInit->uSizeBlock = uSize;
	m_pListInit->uSizeAll = (uSize + uReserv) * uCount + sizeof (POOL_LIST) + uCount * sizeof (ULONG);
	m_pListInit->bReadBusy = 0;
	m_pListInit->bWriteBusy = 0;

	m_pListInit->EventAdd = CreateEvent (NULL, false, false, NULL);
	m_pListInit->EventDel = CreateEvent (NULL, false, false, NULL);
	m_pListInit->EventWork = CreateEvent (NULL, true, false, NULL);

	ULONG uOffset = sizeof (POOL_LIST) + sizeof (ULONG) * uCount;
	for (ULONG a = 0; a < uCount; a++)
	{
		uOffset += uReserv;
		m_pListInit->Offests [a] = uOffset;
		uOffset += uSize;
	}
}

CPoolList::~CPoolList(void)
{
	Clear ();
}

void CPoolList::Clear ()
{
	if (m_pBuff)
	{
		Stop();

		CloseHandle(m_pListInit->EventAdd);
		m_pListInit->EventAdd = NULL;
		CloseHandle(m_pListInit->EventDel);
		m_pListInit->EventDel = NULL;
		CloseHandle(m_pListInit->EventWork);
		m_pListInit->EventWork = NULL;

		delete [] (LPBYTE)m_pListInit;
		m_pListInit = NULL;
	}
	m_pBuff = NULL;
	m_pList = NULL;

}

void CPoolList::InitBuffer (BYTE *pBuff)
{
	m_pBuff = pBuff;
	m_pList  = (POOL_LIST *)m_pBuff;
}

void CPoolList::Lock ()
{
	if (!m_pBuff)
	{
		return;
	}
	while (InterlockedCompareExchange (&m_pList->Lock, 1, 0));
}

void CPoolList::UnLock ()
{
	if (!m_pBuff)
	{
		return;
	}
	InterlockedCompareExchange (&m_pList->Lock, 0, 1);
}

BYTE* CPoolList::GetFullBuffer (BYTE* pBuff)
{
	return pBuff - m_uReserv;
}

BYTE* CPoolList::GetCurNodeWrite ()
{
	while (true)
	{
		if (!m_pListInit)
		{
			return NULL;
		}

		if (WaitForSingleObject(m_pListInit->EventWork, 0) != WAIT_OBJECT_0)
		{
			return NULL;
		}
		if (m_pList->uUsed == m_pList->uCount)
		{
			WaitForSingleObject(m_pList->EventDel, -1);
			InterlockedIncrement (&m_WaitCount);
			continue;
		}
		break;
	}

	Lock();
	if (m_pList->bStopped)
	{
		UnLock();
		return NULL;
	}
	m_pList->bWriteBusy = 1;
	UnLock();
	return m_pBuff + m_pList->Offests [m_pList->uCur];
}

void CPoolList::PushCurNodeWrite ()
{
	if (!m_pListInit)
	{
		return;
	}

	if (WaitForSingleObject(m_pListInit->EventWork, 0) != WAIT_OBJECT_0)
	{
		return;
	}

	Lock ();
	m_pList->bWriteBusy = 0;
	if (!m_pList->bStopped)
	{
		m_pList->uCur = (m_pList->uCur + 1) % m_pList->uCount;
		++m_pList->uUsed;
	}
	if (m_pList->uUsed > 2)
	{
		++m_MoreThan;
	}
	ATLASSERT(m_pList->uUsed <= m_pList->uCount);
	UnLock ();
	SetEvent(m_pList->EventAdd);
}

void CPoolList::FreeNodeWrite()
{
	if (!m_pListInit)
	{
		return;
	}

	if (WaitForSingleObject(m_pListInit->EventWork, 0) != WAIT_OBJECT_0)
	{
		return;
	}

	Lock();
	m_pList->bWriteBusy = 0;
	ATLASSERT(m_pList->uUsed <= m_pList->uCount);
	UnLock();
	SetEvent(m_pList->EventAdd);
}

BYTE* CPoolList::GetCurNodeRead ()
{
	ULONG uNum = 0;

	while (m_pListInit)
	{
		// list is not initialized or stopped
		if (!m_pListInit)
		{
			return NULL;
		}

		if (WaitForSingleObject(m_pListInit->EventWork, 0) != WAIT_OBJECT_0)
		{
			return NULL;
		}

		// pool is stopped
		if (m_pList->bStopped)
		{
			return NULL;
		}

		if (m_pList->uUsed == 0)
		{
			WaitForSingleObject(m_pList->EventAdd, -1);
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
	m_pList->bReadBusy = 1;
	UnLock();

	return m_pBuff + m_pList->Offests [uNum];
}

void CPoolList::PopCurNodeRead ()
{
	if (!m_pListInit)
	{
		return;
	}

	if (WaitForSingleObject(m_pListInit->EventWork, 0) != WAIT_OBJECT_0)
	{
		return;
	}

	Lock ();
	ATLASSERT (m_pList->uUsed != 0);
	if (!m_pList->bStopped)
	{
		--m_pList->uUsed;
	}
	m_pList->bReadBusy = 0;
	if (m_pList->uUsed > 0)
	{
		++m_MoreThan;
	}

	UnLock ();
	SetEvent(m_pList->EventDel);
}

void CPoolList::FreeNodeRead()
{
	if (!m_pListInit)
	{
		return;
	}

	if (WaitForSingleObject(m_pListInit->EventWork, 0) != WAIT_OBJECT_0)
	{
		return;
	}

	Lock();
	m_pList->bReadBusy = 0;
	UnLock();
	SetEvent(m_pList->EventDel);
}

void CPoolList::Stop()
{
	if (m_pList)
	{
		Lock();
		m_pList->uUsed = 0;
		m_pList->uCur = 0;
		m_pList->bStopped = true;
		UnLock();
		SetEvent(m_pList->EventDel);
		SetEvent(m_pList->EventAdd);
	}
}

void CPoolList::Resume()
{
	if (m_pList)
	{
		Lock();
		m_pList->bStopped = false;
		UnLock();

		SetEvent(m_pList->EventDel);
		SetEvent(m_pList->EventAdd);
	}
}
