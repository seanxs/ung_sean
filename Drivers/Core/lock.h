/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   lock.h

Environment:

    Kernel mode

--*/

#pragma once

#include "BaseClass.h"

class CCoreLock : public CBaseClass
{
public:
	CCoreLock () : m_Tag (0) {}
	virtual ~CCoreLock () {};

	virtual bool Lock (LONG lTag = 0) = 0;
	virtual bool UnLock () = 0;

protected:
	LONG	m_Tag;
};

class CAutoLock : public CBaseClass
{
public:
	CAutoLock (CCoreLock *Event, LONG lTag = 0) : m_event (Event) 
	{
		m_event->Lock (lTag);
	}

	~CAutoLock ()
	{
		m_event->UnLock ();
	}

private:
	CCoreLock  *m_event;
};

class CEventLock : public CCoreLock
{
public:
	CEventLock(void);
	~CEventLock(void);

	virtual bool Lock (LONG lTag = 0);
	virtual bool UnLock ();

private:
	KEVENT	m_hEvent;
};

class CSpinLock : public CCoreLock
{
public:
	CSpinLock(void);
	~CSpinLock(void);

	virtual bool Lock (LONG lTag = 0);
	virtual bool UnLock ();

private:
	KSPIN_LOCK			m_SpinLock;
	KIRQL				m_oldIrql;
};

