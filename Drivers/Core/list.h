/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    list.h

Abstract:

   List class. Basic operations are add/remove/get. It is possible to indicate the 
   data repository in NonPagedPool or in PagedPool through a constructor.

Environment:

    Kernel mode

--*/
#ifndef LIST_CONTROL
#define LIST_CONTROL

#include "BaseClass.h"
#include "lock.h"
#include "KEvent.h"
#include "PagedLookasideList.h"

class CList : public CBaseClass
{
// variable 
protected:
	typedef struct _LIST_CONTAINER
	{
		LIST_ENTRY	link;
		void		*ret;
	} LIST_CONTAINER;

	POOL_TYPE				m_PoolType;
	LIST_ENTRY				m_listHeader;
	PLIST_ENTRY				m_plinkCurrent;
	int						m_iCurIndex;
	int						m_iCount;
	TPtrPageedLookList		m_pPagedList;

    CKEvent					m_ClearEvent;
    CKEvent					m_AddEvent;	

	static LONG	m_lTag;

// additional functions
protected:
	void *AllocMem ();
	void FreeMem (void* Mem);
	void SetIndex (int iIndex);

	void TestList();


public:
	CList ();
	CList (POOL_TYPE PoolType);
	virtual ~CList ();
	void InitLookasideList ();
	NEW_ALLOC_DEFINITION ('TSIL')

public:
	// operations
	virtual void* operator [] (int iIndex);
	virtual LIST_ENTRY* Add (void *NewElement);
	virtual bool Remove (int iIndex);
	virtual bool Remove (LIST_ENTRY *link);
	virtual void* GetItem (int iIndex);
	virtual void* PopFirst ();
	virtual int GetCount () const;
	// Wait
	virtual void WaitClearList ();
	virtual void WaitAddElemetn ();

};

typedef boost::intrusive_ptr <CList> TPtrList;

#endif