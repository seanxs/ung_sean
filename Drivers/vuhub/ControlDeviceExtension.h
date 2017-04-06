/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ControlDeviceExtension.h

Abstract:

   Device class that third ring interacts with. 
     - add virtual USB device
     - delete virtual USB device
     - data transfer between real and virtual devices

Environment:

    Kernel mode

--*/
#pragma once
#include "Core\BaseDeviceExtension.h"
#include "Core\CounterIrp.h"
#include "Core\list.h"
#include "Common\public.h"
#include "HubStub.h"
#include "Core\Thread.h"

namespace evuh
{

class CControlDeviceExtension  : public CHubStubExtension
{
public:
    CControlDeviceExtension(PVOID pVHub);
    ~CControlDeviceExtension(void);

	NEW_ALLOC_DEFINITION ('EDCC')

    virtual NTSTATUS DispatchDevCtrl (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS DispatchCleanup (PDEVICE_OBJECT DeviceObject, PIRP Irp);

	static VOID CancelControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);


protected:
    CBaseDeviceExtension *m_pHcd;

	LONG PauseAddRemove();
	Thread::CThreadPoolTask m_poolCommands;
	static VOID stThreadCommand(Thread::CThreadPoolTask *pTask, PVOID Context);
};

};