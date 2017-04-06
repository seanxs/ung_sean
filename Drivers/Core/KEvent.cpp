/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   KEvent.cpp

Environment:

    Kernel mode

--*/

#include "KEvent.h"


CKEvent::CKEvent(EVENT_TYPE  Type, BOOLEAN  State)
{
	KeInitializeEvent(&m_Event, Type, State);
}

CKEvent::~CKEvent(void)
{
}

void CKEvent::SetEvent () const
{
	KeSetEvent (&m_Event, IO_NO_INCREMENT, FALSE);
}

void CKEvent::ClearEvent () const
{
	KeClearEvent(&m_Event);
}

bool CKEvent::Wait (int iMSec) const
{
	LARGE_INTEGER	time;
	LARGE_INTEGER	*pTime = &time;

	switch (iMSec)
	{
	case -1:
		pTime = NULL;
		break;
	default:
		time.QuadPart = iMSec * 1000 * 10 * -1;
		break;
	}

	if (KeGetCurrentIrql () != PASSIVE_LEVEL)
	{
		time.QuadPart = 0;
	}
	return (KeWaitForSingleObject (&m_Event, Executive, KernelMode, FALSE, pTime) == STATUS_SUCCESS);
}
