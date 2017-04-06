/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Count.h

Environment:

    User mode

--*/

#pragma once

class CCount
{
public:
	CCount(void);
	~CCount(void);

	long	operator++ ();
	long	operator++ (int);
	long	operator-- ();
	long	operator-- (int);

	bool WaitClear (DWORD dwMilliseconds);

	void Lock ()
	{
		EnterCriticalSection (&m_cs);
	}
	void UnLock ()
	{
		LeaveCriticalSection (&m_cs);
	}

public:
	long	m_iCount;

private:
	HANDLE	m_hEvent;
	CRITICAL_SECTION m_cs;
};