/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PageedLookasideList.cpp

Abstract:

   Memory assignment for dynamic lists class. Can operate as NonPagedPool or as 
   PagedPool

Environment:

    Kernel mode

--*/
#include "PagedLookasideList.h"

CPagedLookasideList::CPagedLookasideList(POOL_TYPE PoolType, int iSize, LONG lTag)
    : m_pNonLookaside (NULL)
    , m_pLookaside (NULL)
    , m_lTag (lTag)
	, m_lCount ()
{
    if (PoolType == NonPagedPool)
    {
        m_pNonLookaside = (PNPAGED_LOOKASIDE_LIST)ExAllocatePoolWithTag (NonPagedPool, sizeof (NPAGED_LOOKASIDE_LIST), m_lTag);
        ExInitializeNPagedLookasideList (m_pNonLookaside, NULL, NULL, 0, iSize, m_lTag, 0);
    }
    else
    {
        m_pLookaside = (PPAGED_LOOKASIDE_LIST)ExAllocatePoolWithTag (NonPagedPool, sizeof (PAGED_LOOKASIDE_LIST), m_lTag);
        ExInitializePagedLookasideList (m_pLookaside, NULL, NULL, 0, iSize, m_lTag, 0);
    }
}

CPagedLookasideList::~CPagedLookasideList(void)
{
    if (m_pNonLookaside)
    {
        ExDeleteNPagedLookasideList(m_pNonLookaside);
        ExFreePool (m_pNonLookaside);
        m_pNonLookaside = NULL;
    }

    if (m_pLookaside)
    {
        ExDeletePagedLookasideList(m_pLookaside);
        ExFreePool (m_pLookaside);
        m_pLookaside = NULL;
    }

	--m_lCount;
	ASSERT (!m_lCount.GetCurCount());
}

PVOID CPagedLookasideList::Alloc ()
{
    PVOID pRet = NULL;
    if (m_pNonLookaside)
    {
        pRet = ExAllocateFromNPagedLookasideList (m_pNonLookaside);
		++m_lCount;
    }
    if (m_pLookaside)
    {
        pRet = ExAllocateFromPagedLookasideList (m_pLookaside);
		++m_lCount;
    }

    return pRet;
}

void CPagedLookasideList::Free (PVOID pPoint)
{
    if (m_pNonLookaside)
    {
        ExFreeToNPagedLookasideList (m_pNonLookaside, pPoint);
		--m_lCount;
    }
    if (m_pLookaside)
    {
        ExFreeToPagedLookasideList (m_pLookaside, pPoint);
		--m_lCount;
    }
}
