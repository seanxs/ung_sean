/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   KEvent.h

Environment:

    Kernel mode

--*/
#pragma once

#include "BaseClass.h"

class CKEvent : public CBaseClass
{
public:
	CKEvent(EVENT_TYPE  Type, BOOLEAN  State);			// NotificationEvent or SynchronizationEvent.
	virtual ~CKEvent(void);

public:
	void SetEvent () const;
	void ClearEvent () const;
	bool Wait (int iMSec) const;		// 0 - verify status, -1 - wait singal

protected:
	mutable KEVENT	m_Event;
};

typedef boost::intrusive_ptr <CKEvent> TPtrKEvent;