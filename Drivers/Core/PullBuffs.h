/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   PullBuffs.h

Environment:

    Kernel mode

--*/
#pragma once

#ifndef BUFFER_H
#define BUFFER_H

#include "list.h"
#include "lock.h"
#include "PagedLookasideList.h"
#include "BaseClass.h"

typedef struct _SAVE_BUFFER
{
	LONG		lSize;
	POOL_TYPE	PoolType;
	LONG		lUsed;
	PVOID		pBuffer;
}SAVE_BUFFER, *PSAVE_BUFFER;

class CPullBuffer : public CBaseClass
{
public:
	CPullBuffer(void);
	~CPullBuffer(void);
	NEW_ALLOC_DEFINITION ('BPE')

protected:
	// list buffer
	CPagedLookasideList			m_AllocInfo;
	CList					m_listBufferPagedPool;
	CList					m_listBufferNonPagedPool;
	CSpinLock				m_lock;
	CCounterIrp				m_CountUsing;

	// debug
	LONG					m_lCountNP;
	LONG					m_lSizeNP;					
	LONG					m_lCountP;
	LONG					m_lSizeP;					

public:
	// add buffer
	bool FreeBuffer (PVOID Buffer);
	PVOID AllocBuffer (LONG lSize, POOL_TYPE	PoolType);
	void ClearBuffer ();

	static LONG GetUsedSize (PVOID pBuff);
	static void SetUsedSize (PVOID pBuff, LONG lSize);
};

#endif