/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Lock.h

Environment:

    User mode

--*/

#pragma once

class CLock
{
public:
	CLock(void);
	~CLock(void);

	void Lock ();
	void UnLock ();
	BOOL TryLock ();

private:
	CRITICAL_SECTION	m_cs;
};

class CAutoLock
{
public:
	CAutoLock (CLock *pLock) : m_lock (pLock) {m_lock->Lock ();}
	~CAutoLock () 
	{
		if (m_lock) 
		{
			m_lock->UnLock ();
			m_lock = NULL;
		}
	}

	bool UnAttach () 
	{
		if (m_lock)
		{
			m_lock->UnLock ();
			m_lock = NULL;
			return true;
		}
		return false;
	}

private:
	CLock *m_lock;
};
