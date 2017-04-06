/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Insulation.h

Environment:

    Kernel mode

--*/

#pragma once
#include "Core\BaseClass.h"
#include "Core\DeviceRelations.h"
#include "Core\SystemEnum.h"
#include "Core\Thread.h"

namespace Insulation
{
	bool FindAllChildGlobaleSymbolicLinks(PDEVICE_OBJECT pDev, CDeviceRelations *Relation, CSystemEnum::FEnunSymbolicLink funcEnum, PVOID pContext);
	NTSTATUS Detect_NewStorage(PDEVICE_INTERFACE_CHANGE_NOTIFICATION NotificationStruct, PVOID Context);
	VOID ItemSymbolLynk(Thread::CThreadPoolTask *pTask, PVOID Context);
	VOID FindSymbolLynk(struct _KDPC *Dpc, PVOID  DeferredContext, PVOID  SystemArgument1, PVOID  SystemArgument2);
	bool EnunSymbolicLink(PVOID pContext, PDEVICE_OBJECT pDev, PUNICODE_STRING strDev, PUNICODE_STRING strSymboleLink);
};

