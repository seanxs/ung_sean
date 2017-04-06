/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Lock.cpp

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "Lock.h"

CLock::CLock(void)
{
	InitializeCriticalSection (&m_cs);
}

CLock::~CLock(void)
{
	DeleteCriticalSection (&m_cs);
}

void CLock::Lock ()
{
	EnterCriticalSection (&m_cs);
}

void CLock::UnLock ()
{
	LeaveCriticalSection (&m_cs);
}

BOOL CLock::TryLock ()
{
	return TryEnterCriticalSection (&m_cs);
}
