/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   CounterIrp.h

Abstract:

   Counter class. It is used to count those IRPs quantity, that are currently 
   being processed.

Environment:

    Kernel mode

--*/
#ifndef Counter_irp
#define Counter_irp

#include "baseclass.h"

class CCounterIrp : public CBaseClass
{
public:
	CCounterIrp ();
	virtual ~CCounterIrp ();
	NEW_ALLOC_DEFINITION ('CIRP')

// variable
private:
	static LONG	m_lTag;
    //
    // RemoveEvent represents the kernel event that synchronizes the function
    // driver's processing of IRP_MN_REMOVE_DEVICE. RemoveEvent is only signaled
    // when OutstandingIO equals 0.
    //
    KEVENT              m_RemoveEvent;

    //
    // StopEvent represents the kernel event the synchronizes the function driver's
    // processing of IRP_MN_QUERY_STOP_DEVICE and IRP_MN_QUERY_REMOVE_DEVICE.
    // StopEvent is only signaled when OutstandingIO equals 1.
    //
    KEVENT              m_StopEvent;

    //
    // OutstandingIO represents a 1 based count of unprocessed IRPs the function
    // driver has received. When OutstandingIO equals 1 the function driver has no
    // unprocessed IRPs and the hardware can be stopped. When OutstandingIO equals
    // 0, the hardware instance is being removed.
    //
    LONG               m_OutstandingIO;

// operation
public:

	LONG&	operator++ ();
	LONG	operator++ (int);
	LONG&	operator-- ();
	LONG	operator-- (int);

// methods
public:
	LONG GetCurCount () {return m_OutstandingIO;}
	bool WaitStop(int iMSec = -1);
	void WaitRemove () ;
	void Reset ();
};

#endif