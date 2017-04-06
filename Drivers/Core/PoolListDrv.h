/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   PoolListDrv.h

Environment:

    Kernel mode

--*/

#pragma once
#include "BaseClass.h"

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
	ULONG		Offests [1];
};

#pragma pack(pop)

class CPoolListDrv : public CBaseClass
{
public:
	CPoolListDrv();
	~CPoolListDrv(void);

	PVOID Init (PVOID pList);
	void Clear ();

	void Lock ();
	void UnLock ();

	char* GetCurNodeWrite ();
	void PushCurNodeWrite ();
	void FreeNodeWrite();

	char* GetCurNodeRead ();
	void PopCurNodeRead ();
	void FreeCurNodeRead();

	char* GetRawData () {return m_pBuff;}

	ULONG GetBlockSize(){return m_pList->uSizeBlock;}

	void WaitReady ();

protected:
	LONG		m_WaitCount;
	PVOID		m_User;
	char		*m_pBuff;
	POOL_LIST	*m_pList;
	PMDL		m_pMdl;

	KEVENT		m_EventReady;

	PKEVENT		m_EventAdd;
	PKEVENT		m_EventDel;
	PKEVENT		m_EventWork;

};

