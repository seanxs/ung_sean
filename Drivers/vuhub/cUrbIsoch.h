/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   cUrbIsoch.h

Environment:

    Kernel mode

--*/

#include "evuh.h"
#include "Core\BaseClass.h"
#include "Core\Thread.h"
#include "Core\Timer.h"

#pragma once

namespace evuh
{
	class CUsbDevToTcp;
};
class CPdoExtension;

typedef struct
{
	USBD_PIPE_HANDLE    hPipe;
	LONG                lSizeBuf;
	LONG                lPnpNum;
	PLIST_ENTRY         pList;
} ISOCH_SAVE, *PISOCH_SAVE;


class CcUrbIsoch : public CBaseClass
{
public:
	CcUrbIsoch();
	~CcUrbIsoch();

	void SetBase(evuh::CUsbDevToTcp *pBase);
protected:
	CSpinLock			m_spinIsoch;
	// ISOCH
	CList				m_listIsoch;
	CTimer				m_timerIsoch;
	int					m_DueTimeIsoch;
	evuh::CUsbDevToTcp*	m_pBase;

	// Isoch in
	ULONG				m_lCurrentFrame;
	ULONG				m_lDelta;
	ULONG				m_lLastTime;

public:
	bool IsochVerify(PIRP Irp, CPdoExtension *pPdo);
	bool IsochStart(PIRP Irp, CPdoExtension *pPdo);
	void IsochFire(PIRP Irp, PISOCH_SAVE pIsoch, CPdoExtension *pPdo);
	bool IsochDelete(PISOCH_SAVE pIsoch);
	PISOCH_SAVE IsochFind(USBD_PIPE_HANDLE hPipe);
	PISOCH_SAVE IsochFind(LONG iNumIrp);
	bool IsochVerifyOut(PIRP_NUMBER stDevice);
	bool IsochCompleted(PIRP_NUMBER stDevice);  // need to rewrite with SetCompleteIrpWorkItm
	// Cacl current frame
	NTSTATUS QueryBusTime(PVOID  BusContext, PULONG  CurrentFrame);

	// set timer period to out isoch
	void SetTimerPeriod(int iValue) { m_DueTimeIsoch = iValue; }
	// set new current frame
	ULONG GetCurrentFrame();
	// restart timer for auto completed isoch out
	void RestartTimer();
	// clear waiting irp
	void ClearIsochIrp();

	void BuildIsochStartFrame(URB_FULL *pUrb);

private:
	// auto completed isoch out
	static VOID IsochDpc(struct _KDPC *Dpc, PVOID  DeferredContext, PVOID  SystemArgument1, PVOID  SystemArgument2);

public:
	void CompletingIrp(PIRP_NUMBER stDevice);// { m_IsochCompleting.PushTask(stDevice); }
protected:
	Thread::CThreadPoolTask		m_IsochCompleting;
	static VOID stIsochCompleting(Thread::CThreadPoolTask *pTask, PVOID Context);
};

