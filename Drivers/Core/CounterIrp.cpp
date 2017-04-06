/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   CounterIrp.cpp

Abstract:

   Counter class. It is used to count those IRPs quantity, that are currently
   being processed.

Environment:

    Kernel mode

--*/
#include "CounterIrp.h"

LONG CCounterIrp::m_lTag = 'CIRP';
// **********************************************************************
//						Constructor
// **********************************************************************
CCounterIrp::CCounterIrp ()
	: CBaseClass ()
{
    // Initialize RemoveEvent to an unsignaled state. ToasterIoDecrement later
    // signals RemoveEvent when OutstandingIO transitions from 1 to 0.
    //
    KeInitializeEvent(&m_RemoveEvent,
                      SynchronizationEvent,
                      FALSE);

    //
    // Initialize StopEvent to a signaled state. ToasterIoIncrement unsignals
    // StopEvent when OutstandingIO transitions from 1 to 2. When StopEvent is in an
    // unsignaled state, ToasterDispatchPnP is blocked from continuing to process
    // IRP_MN_QUERY_STOP_DEVICE or IRP_MN_QUERY_REMOVE_DEVICE until a call to
    // ToasterIoDecrement later signals StopEvent when OutstandingIO transitions
    // from 2 to 1.
    //
    KeInitializeEvent(&m_StopEvent,
                      SynchronizationEvent,
                      TRUE);

    //
    // Initialize OutstandingIO to 1. The function driver must make an equal number
    // of calls to ToasterIoIncrement and ToasterIoDecrement in order for
    // ToasterIoDecrement to properly signal StopEvent or RemoveEvent. The only time
    // OutstandingIO equals 0 is when ToasterDispatchPnP processes
    // IRP_MN_REMOVE_DEVICE and calls ToasterIoDecrement twice. Otherwise, if there
    // are not an equal number of calls to ToasterIoIncrement and ToasterIoDecrement
    // then StopEvent and RemoveEvent may be prematurely signaled, resulting in
    // unpredictable behavior in the function driver.
    //
    m_OutstandingIO = 1;
}

CCounterIrp::~CCounterIrp ()
{
}

// **********************************************************************
//						Operations
// **********************************************************************

LONG& CCounterIrp::operator++ ()
{
    LONG            result;
    //
    // Increment the count of uncompleted IRPs. The InterlockedIncrement function
    // increments the count in a thread-safe manner. Thus, the function driver does
    // not need to define a separate spin lock in the device extension to protect
    // the count from being simultaneously manipulated by multiple threads.
    //
    result = InterlockedIncrement(&m_OutstandingIO);

    //ToasterDebugPrint(TRACE, "ToasterIoIncrement %d\n", result);

    ASSERT(result > 1);

    //
    // Determine if StopEvent should be unsignaled (cleared). StopEvent must only be
    // unsignaled when the count of uncompleted IRPs increments from 1 to 2.
    // Unsignaling StopEvent prevents ToasterDispatchPnP from prematurely passing
    // IRP_MN_QUERY_STOP_DEVICE or IRP_MN_QUERY_REMOVE_DEVICE down the device stack
    // to be processed by the underlying bus driver.
    //
    if (2 == result)
    {
        KeClearEvent(&m_StopEvent);
    }

    return m_OutstandingIO;
}

LONG CCounterIrp::operator++ (int)
{
	LONG result = m_OutstandingIO;
	++*this;
	return result;
}

LONG& CCounterIrp::operator-- ()
{
    LONG            result;

    //
    // Decrement the count of uncompleted IRPs. The InterlockedDecrement function
    // decrements the count in a thread-safe manner. Thus, the function driver does
    // not need to define a separate spin lock in its device extension to protect
    // the count from being simultaneously manipulated by multiple threads.
    //
    result = InterlockedDecrement(&m_OutstandingIO);

    //ToasterDebugPrint(TRACE, "ToasterIoDecrement %d\n", result);

    ASSERT(result >= 0);

    //
    // Determine if StopEvent should be signaled. StopEvent must only be signaled
    // when the count of uncompleted IRPs decrements from 2 to 1. A count of 1
    // indicates that there are no more uncompleted IRPs.
    // 
    // Signaling StopEvent allows ToasterDispatchPnP to pass IRP_MN_QUERY_STOP_DEVICE
    // or IRP_MN_QUERY_REMOVE_DEVICE down the device stack to be processed by the
    // underlying bus driver.
    //
    if (1 == result)
    {
        KeSetEvent (&m_StopEvent,
                    IO_NO_INCREMENT,
                    FALSE);
    }

    //
    // Determine if RemoveEvent should be signaled. RemoveEvent must only be signaled
    // after StopEvent has been signaled, when the count of uncompleted IRPs
    // decrements from 1 to 0. A count of 0 indicates that ToasterDispatchPnP is
    // waiting to send IRP_MN_REMOVE_DEVICE down the device stack to be processed by
    // the bus driver.
    //
    if (0 == result)
    {
        //
        // Test the assumption that the toaster instance“s hardware state equals
        // Deleted. RemoveEvent should not be signaled if the hardware instance is
        // still connected to the computer.
        //
        KeSetEvent (&m_RemoveEvent,
                    IO_NO_INCREMENT,
                    FALSE);
    }

    return m_OutstandingIO;
}

LONG CCounterIrp::operator-- (int)
{
	ULONG result = m_OutstandingIO;
	--*this;
	return result;
}

// **********************************************************************
//						Wait methods
// **********************************************************************
bool CCounterIrp::WaitStop(int iMSec)
{
	LARGE_INTEGER	time;
	LARGE_INTEGER	*pTime = &time;

	switch (iMSec)
	{
	case -1:
		pTime = NULL;
		break;
	default:
		time.QuadPart = iMSec * 1000 * 10 * -1;
		break;
	}

	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
	{
		time.QuadPart = 0;
	}
	return (KeWaitForSingleObject(&m_StopEvent, Executive, KernelMode, FALSE, pTime) == STATUS_SUCCESS);
}

void CCounterIrp::WaitRemove ()
{
	--(*this);
	if (KeGetCurrentIrql () == PASSIVE_LEVEL)
	{
		LARGE_INTEGER	time;
		time.QuadPart = (LONGLONG)-100000000;		// 10 sec
		if (KeWaitForSingleObject (
				&m_RemoveEvent,
				Executive,
				KernelMode,
				FALSE,
				&time) == STATUS_SUCCESS)
		{
			return;
		}
	}
    KeWaitForSingleObject (
            &m_RemoveEvent,
            Executive,
            KernelMode,
            FALSE,
            NULL);
}


void CCounterIrp::Reset ()
{
    KeSetEvent (&m_StopEvent,
                IO_NO_INCREMENT,
                FALSE);

    m_OutstandingIO = 1;
}