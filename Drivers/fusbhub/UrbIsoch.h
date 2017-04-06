/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UrbIsoch.h

Environment:

    Kernel mode

--*/

#pragma once

#include "Core\BaseClass.h"
#include "private.h"
#include "Common\public.h"
#include "Common\common.h"
#include "urbDiff.h"
#include "Core\Lock.h"
#include "Core\List.h"
#include "Core\Thread.h"

class CPdoStubExtension;
class CUsbPortToTcp;

typedef struct _ISOCH
{
	USBD_PIPE_HANDLE    hPipe;
	LONG                lSizeBuf;
	PIRP_SAVE           irpSave;
	CPdoStubExtension   *pPdo;
	CList               *listIrp;
	PLIST_ENTRY         pList;
}ISOCH, *PISOCH;

extern const int iMaxIsoch;

class CUrbIsoch : public CBaseClass
{
public:
	CUrbIsoch();
	~CUrbIsoch(void);

	void SetBase(CUsbPortToTcp *pBase);

protected:
	// sync
	CSpinLock	m_spinUsbIsoch;

	// base class 
	CUsbPortToTcp *m_pBase;

	// 
	long	m_lIsochCount;
	static long	m_lIsochFew;
	static long	m_lIsochMid;
	static long	m_lIsochMany;
	//int		m_maxWait;
	CList	m_listIsoch;

	ULONG		m_StartFrameOut;
	ULONG		m_StartFrameIn;

	// auto restart irp
	Thread::CThreadPoolTask		m_RestartIsochOut;

protected:
	static VOID stRestartIschoOut(Thread::CThreadPoolTask *pTask, PVOID Context);
public:
	void ClearIsoch ();
	bool IsochCompleted (/*PWORKING_IRP pWork*/);
	void SendNewIsochTimer (CPdoStubExtension *pPdoStub, long iTime); // ms
	void BuildStartFrame (PURB_FULL pUrb);
	void PushNextIsoPackedge();

#ifdef ISOCH_FAST_MODE
	bool IsochVerify (PIRP_SAVE irpSave, CPdoStubExtension *pPdo);
	bool IsochStart (PIRP_SAVE irpSave, CPdoStubExtension *pPdo);
	void IsochStartNew (PISOCH       pIsoch);
	void IsochDeleteSave (PWORKING_IRP saveWork);
	void IsochClear (PISOCH pIsoch);
	PISOCH IsochFind (USBD_PIPE_HANDLE    hPipe);
	void IsochDeleteWorkSave (PWORKING_IRP saveWork);
	bool IsochVerifyDel (PWORKING_IRP saveWork);
	void IsochComplitingWorkSaveWorkIrp (PWORKING_IRP CurWorkSave);
#endif
	bool IsochVerifyOut (PWORKING_IRP pWorkSave);
public:

	NTSTATUS PutIrpToDevice (PWORKING_IRP pWorkSave, CPdoStubExtension *pPdoStub);

};

