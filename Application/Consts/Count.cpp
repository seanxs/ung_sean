/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Count.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "Count.h"

CCount::CCount (void)
	: m_iCount (0)
{
	m_hEvent = CreateEvent (NULL, true, true, NULL);
	InitializeCriticalSection (&m_cs);
}

CCount::~CCount (void)
{
	CloseHandle (m_hEvent);
	DeleteCriticalSection (&m_cs);
}

long CCount::operator++ ()
{
	EnterCriticalSection (&m_cs);
	++m_iCount;
	ResetEvent (m_hEvent);
	LeaveCriticalSection (&m_cs);
	return m_iCount;
}

long CCount::operator++ (int)
{
	long result = m_iCount;
	++*this;
	return result;
}

long CCount::operator-- ()
{
	EnterCriticalSection (&m_cs);
	--m_iCount;

	//AtlAssert (m_iCount >= 0);

	if (m_iCount == 0)
	{
		SetEvent (m_hEvent);
	}
	LeaveCriticalSection (&m_cs);

	return m_iCount;
}

long CCount::operator-- (int)
{
	long result = m_iCount;
	--*this;
	return result;
}

bool CCount::WaitClear (DWORD dwMilliseconds)
{
	bool bRet = false;

	if (WaitForSingleObject (m_hEvent, dwMilliseconds) == WAIT_OBJECT_0)
	{
		bRet = true;
	}

	return bRet;
}
