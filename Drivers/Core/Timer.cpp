/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Timer.cpp

Environment:

    Kernel mode

--*/

#include "Timer.h"

CTimer::CTimer(LONG lTag)
	: CBaseClass ()
	, m_lTag (lTag)
	, m_pTimer (NULL)
	, m_pKdpc (NULL)
{
	m_pTimer = (PKTIMER)ExAllocatePoolWithTag (NonPagedPool, sizeof(KTIMER), lTag);

	if (m_pTimer)
	{
		KeInitializeTimer(m_pTimer);
	}

}

CTimer::~CTimer(void)
{
	CancelTimer ();

	if (m_pKdpc)
	{
		ExFreePool (m_pKdpc);
		m_pKdpc = NULL;
	}
	if (m_pTimer)
	{
		ExFreePool (m_pTimer);
		m_pTimer = NULL;
	}
}

bool CTimer::InitTimer (PKDEFERRED_ROUTINE Timer, PVOID Context)
{
	if (m_pKdpc)
	{
		CancelTimer();
		ExFreePool (m_pKdpc);
		m_pKdpc = NULL;
	}
	m_pKdpc = (PKDPC)ExAllocatePoolWithTag (NonPagedPool, sizeof(KDPC), m_lTag);
	if (m_pKdpc)
	{
		KeInitializeDpc(m_pKdpc, Timer, Context);
	}

	return true;
}


bool CTimer::SetTimer (int iMSec)
{
	if (!m_pKdpc || !m_pTimer)
	{
		// has already stated
		return false;

	}

	if (m_pKdpc && m_pTimer)
	{
		LARGE_INTEGER	tm = {-iMSec * 1000 * 10, -1};
		return KeSetTimer (m_pTimer, tm, m_pKdpc)?true:false;
	}

	return false;
}

bool CTimer::CancelTimer ()
{
	if (!m_pKdpc || !m_pTimer)
	{
		// has already stated
		return false;

	}
	return KeCancelTimer (m_pTimer)?true:false;
}
