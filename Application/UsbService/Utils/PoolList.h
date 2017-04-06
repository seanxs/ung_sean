/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    PoolList.h

Environment:

    User mode

--*/

#pragma once
#include "stdafx.h"
#include <windows.h>
#include <Winsvc.h>

#pragma pack(push, 1)

struct POOL_LIST
{
	LONG		Lock;
	HANDLE		EventAdd;
	HANDLE		EventDel;
	HANDLE		EventWork;
	ULONG		uSizeAll;
	ULONG		uSizeBlock;
	ULONG		uCount;
	ULONG		uCur;
	ULONG		uUsed;
	char		bReadBusy:1;
	char		bWriteBusy:1;
	char		bStopped:1;
	ULONG		Offests[1];
};

#pragma pack(pop)

class CPoolList
{
public:
	CPoolList();
	~CPoolList(void);

	void Init (ULONG uCount, ULONG uSize, ULONG uReserv/*, LPCTSTR Tag*/);

	void Lock ();
	void UnLock ();

	bool IsEmpty () {return m_pListInit?false:true;}

	BYTE* GetCurNodeWrite ();
	void PushCurNodeWrite ();
	void FreeNodeWrite();

	BYTE* GetCurNodeRead ();
	void PopCurNodeRead ();
	void FreeNodeRead();

	BYTE* GetFullBuffer (BYTE*);

	ULONG GetBlockSize () {return m_pList->uSizeBlock;}
	ULONG GetBlockCount () {return m_pList->uCount;}

	POOL_LIST* GetInitData () {return m_pListInit;}
	ULONG GetInitSize () {return sizeof (POOL_LIST) + sizeof (ULONG) * m_pListInit->uCount;}

	void InitBuffer (BYTE *pBuff);

	void Clear ();
	void Stop();		// stop adding data
	void Resume();		// resume adding data

protected:
	LONG		m_WaitCount;
	LONG		m_MoreThan;
	POOL_LIST	*m_pListInit;
	ULONG		m_uReserv;
	BYTE		*m_pBuff;
	POOL_LIST	*m_pList;
};
