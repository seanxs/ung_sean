/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Timer.h

Environment:

    Kernel mode

--*/
#pragma once
#include "baseclass.h"

class CTimer : public CBaseClass
{
public:
	CTimer(LONG lTag = 'rmt');
	~CTimer(void);

private:
	PKTIMER		m_pTimer;
	PKDPC		m_pKdpc;
	LONG		m_lTag;

public:
	bool InitTimer (PKDEFERRED_ROUTINE Timer, PVOID Context);
	bool SetTimer (int iMSec);
	bool CancelTimer ();
};
