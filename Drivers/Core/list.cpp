/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    list.cpp

Abstract:

   List class. Basic operations are add/remove/get. It is possible to indicate the 
   data repository in NonPagedPool or in PagedPool through a constructor.

Environment:

    Kernel mode

--*/
#include "list.h"

LONG CList::m_lTag = 'LIST';

CList::CList ()
	: m_PoolType (NonPagedPool)
	, m_iCurIndex (-1)
	, m_plinkCurrent (NULL)
	, m_iCount (0)
	, CBaseClass ()
	, m_AddEvent (SynchronizationEvent, FALSE)
	, m_ClearEvent (SynchronizationEvent, TRUE)
{
	SetDebugName ("CList");
	InitializeListHead (&m_listHeader);
	InitLookasideList ();
}

CList::CList (POOL_TYPE PoolType)
	: CBaseClass ()
	, m_AddEvent (SynchronizationEvent, FALSE)
	, m_ClearEvent (SynchronizationEvent, TRUE)
{
	SetDebugName ("CList");
	m_PoolType = PoolType;
	InitializeListHead (&m_listHeader);
	InitLookasideList ();
}

CList::~CList ()
{
	while (m_iCount)
	{
		PopFirst();
	}
	m_pPagedList = NULL;
}

void CList::InitLookasideList ()
{
	m_pPagedList = new CPagedLookasideList (m_PoolType, sizeof (NPAGED_LOOKASIDE_LIST), m_lTag);
	m_lTag++;
}

// **********************************************************************
//						additional functions
// **********************************************************************
void* CList::AllocMem ()
{
	return m_pPagedList->Alloc();
}

void CList::FreeMem (void *Mem)
{
	if (!Mem)
	{
		return;
	}

	m_pPagedList->Free(Mem);
}

void CList::SetIndex (int iIndex)
{
	PLIST_ENTRY listStart, nextEntry;

	TestList();

	if (iIndex < 0)
	{
        m_iCurIndex = -1;
		m_plinkCurrent = NULL;
		return;
	}

	if (m_iCurIndex <= iIndex && m_plinkCurrent)
	{
		listStart = m_plinkCurrent;
	}
	else
	{
		listStart = m_listHeader.Flink;
        m_iCurIndex = 0;
	}

	if (m_iCurIndex == -1)
	{
		m_iCurIndex = 0;
	}

    for(nextEntry = listStart->Flink;
        listStart != &m_listHeader;
        listStart = nextEntry,nextEntry = listStart->Flink) 
	{
		if (iIndex == m_iCurIndex)
		{
            m_plinkCurrent = listStart;
			return;
		}
        m_iCurIndex++;        
    }

	m_iCurIndex = -1;
	m_plinkCurrent = NULL;

	TestList();
}
// **********************************************************************
//						operations
// **********************************************************************
void* CList::operator [] (int iIndex)
{
	TestList();
	return GetItem (iIndex);
}

void* CList::PopFirst ()
{
	void *pRet = NULL;
	
	TestList();
	if (GetCount())
	{
		pRet = GetItem(0);
		Remove(0);
	}
	TestList();
	return pRet;
}

void* CList::GetItem (int iIndex)
{
	void				*ret = NULL;
	KIRQL				oldIrql;
	LIST_CONTAINER		*pContainer;

	TestList();

	SetIndex (iIndex);

	if (m_iCurIndex != -1 && m_plinkCurrent)
	{
        pContainer = CONTAINING_RECORD (m_plinkCurrent, LIST_CONTAINER, link);
		ret = pContainer->ret;
	}

	TestList();
	return ret;
}

void CList::TestList()
{
#ifdef DBG
	PLIST_ENTRY nextEntry;
	int iCount = 0;
	for(nextEntry = m_listHeader.Flink;
		nextEntry != &m_listHeader;
		nextEntry = nextEntry->Flink)
	{
		iCount++;
	}
	ASSERT(iCount == m_iCount);
#endif
}

LIST_ENTRY*  CList::Add (void *NewElement)
{
	KIRQL				oldIrql;
	LIST_CONTAINER		*pContainer;

	pContainer = (LIST_CONTAINER*)AllocMem ();

	if (pContainer)
	{
		TestList();
		//
		InsertTailList (&m_listHeader, &pContainer->link);
		m_iCount++;

		TestList();

		pContainer->ret = NewElement;
		// Clear event false
		m_ClearEvent.ClearEvent();
		// Add event true
		m_AddEvent.SetEvent();
		return &pContainer->link;

	}
	return NULL;
}

bool CList::Remove (int iIndex)
{
	KIRQL				oldIrql;
	LIST_CONTAINER		*pContainer;

	SetIndex (iIndex);

	if (m_iCurIndex != -1 && m_plinkCurrent)
	{
        pContainer = CONTAINING_RECORD (m_plinkCurrent, LIST_CONTAINER, link);

		TestList();
        RemoveEntryList (&pContainer->link);
		m_iCurIndex = -1;
		m_plinkCurrent = NULL;
        FreeMem (pContainer);
		m_iCount--;
		TestList();

		if (!m_iCount)
		{
			// Clear event true
			m_ClearEvent.SetEvent();
		}
	}
	return true;
}

bool CList::Remove (LIST_ENTRY *link)
{
	KIRQL				oldIrql;
	LIST_CONTAINER		*pContainer;

	TestList();

    pContainer = CONTAINING_RECORD (link, LIST_CONTAINER, link);
    RemoveEntryList (&pContainer->link);
	m_iCurIndex = -1;
	m_plinkCurrent = NULL;
    FreeMem (pContainer);
	m_iCount--;

	TestList();

	if (!m_iCount)
	{
		// Clear event true
		m_ClearEvent.SetEvent();
	}

	return true;
}

int CList::GetCount () const
{
	return m_iCount;
}

// **********************************************************************
//						Wait
// **********************************************************************
void CList::WaitClearList ()
{
	if (GetCount ())
	{
		m_ClearEvent.Wait(-1);
	}
}

void CList::WaitAddElemetn ()
{
	m_AddEvent.Wait(-1);
	m_AddEvent.ClearEvent();
}