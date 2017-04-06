/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Timeouts.cpp

Abstract:

	Windows timer to run specified callback after some period.

Environment:

    User mode

--*/

#include "stdafx.h"
#include "Timeouts.h"
#include <boost/make_shared.hpp>

namespace eltima {
namespace utils {

#define UNUSED_VARIABLE(x)	(void)(x)

class CWinTimeout: public ITimeout
{
public:
	CWinTimeout();
	virtual ~CWinTimeout();
	virtual void Start(const unsigned timeout_ms, ITimeout::TimeoutCallback callback);
	virtual void StartPeriodic(const unsigned timeout_ms, ITimeout::TimeoutCallback callback);
	virtual void Cancel();

private:
	HANDLE m_hTimer;
	ITimeout::TimeoutCallback m_callback;

	static void CALLBACK OnTimer(PVOID lpParam, BOOLEAN TimerOrWaitFired);
};

//----------------------------------------------------------------------

ShptrTimeout ITimeout::newTimeout()
{
	// platform-specific implementation
	return boost::make_shared<CWinTimeout>();
}


//-------------------------------------------------------------

CWinTimeout::CWinTimeout(): m_hTimer(NULL), m_callback(NULL)
{
}

CWinTimeout::~CWinTimeout()
{
	Cancel();
}

void CWinTimeout::Start(const unsigned timeout_ms, ITimeout::TimeoutCallback callback)
{
	Cancel();

	m_callback = callback;
	::CreateTimerQueueTimer(&m_hTimer, NULL, CWinTimeout::OnTimer, this, timeout_ms, 0, WT_EXECUTEDEFAULT);
}

void CWinTimeout::StartPeriodic(const unsigned timeout_ms, ITimeout::TimeoutCallback callback)
{
	Cancel();

	m_callback = callback;
	::CreateTimerQueueTimer(&m_hTimer, NULL, CWinTimeout::OnTimer, this, timeout_ms, timeout_ms, WT_EXECUTEDEFAULT);
}

void CWinTimeout::Cancel()
{
	m_callback = NULL;
	if (m_hTimer && m_hTimer != INVALID_HANDLE_VALUE) {
		::DeleteTimerQueueTimer(NULL, m_hTimer, NULL);
		m_hTimer = NULL;
	}
}

void CALLBACK CWinTimeout::OnTimer(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	UNUSED_VARIABLE(TimerOrWaitFired);
	if (lpParam) {
		CWinTimeout *pTimeout = (CWinTimeout*)lpParam;
		ITimeout::TimeoutCallback callback = pTimeout->m_callback;
		if (callback)
			callback();
	}
}

} // namespace utils
} // namespace eltima
