/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PageedLookasideList.h

Abstract:

   Memory assignment for dynamic lists class. Can operate as NonPagedPool or as 
   PagedPool

Environment:

    Kernel mode

--*/
#pragma once

#ifndef PAGE_CONTROL_H
#define PAGE_CONTROL_H

#include "BaseClass.h"
#include "CounterIrp.h"

class CPagedLookasideList : public CBaseClass
{
public:
    CPagedLookasideList(POOL_TYPE PoolType = NonPagedPool, int iSize = sizeof (PVOID), LONG lTag = 'PLSL');
    ~CPagedLookasideList(void);

	NEW_ALLOC_DEFINITION ('LLPC')

    PVOID Alloc ();
    void Free (PVOID pPoint);

protected:
    PNPAGED_LOOKASIDE_LIST  m_pNonLookaside;
    PPAGED_LOOKASIDE_LIST   m_pLookaside;
    long                    m_lTag;
	CCounterIrp				m_lCount;
};

typedef boost::intrusive_ptr <CPagedLookasideList> TPtrPageedLookList;

#endif