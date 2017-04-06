/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   WorkItem.h

Environment:

    Kernel mode

--*/

#pragma once

#include "BaseClass.h"

class CWorkItem
{
	struct WIContext
	{
		PIO_WORKITEM			WorkItem;
		PIO_WORKITEM_ROUTINE	Routine;
		LONG					Count;
		PVOID					Context [1];
	};
public:
	static PVOID Create (PDEVICE_OBJECT pDevObj);
	static void AddContext (PVOID WorkItem, PVOID pContext);
	static PVOID GetContext (PVOID WorkItem, int iIndex);
	static void Run (PVOID WorkItem, PIO_WORKITEM_ROUTINE  WorkerRoutine);

	static VOID WorkItem (PDEVICE_OBJECT DeviceObject, PVOID Context);
private:
	static void Del (PVOID WorkItem);
};
