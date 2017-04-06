/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   WorkItem.cpp

Environment:

    Kernel mode

--*/

#include "WorkItem.h"

PVOID CWorkItem::Create (PDEVICE_OBJECT pDevObj)
{
	WIContext *lpContext;
	lpContext = (WIContext*)ExAllocatePoolWithTag (NonPagedPool, sizeof (WIContext), 'IW');

	if (lpContext)
	{
		lpContext->WorkItem = IoAllocateWorkItem (pDevObj);
		lpContext->Count = 0;
	}

	return lpContext;
}

void CWorkItem::AddContext (PVOID WorkItem, PVOID pContext)
{
	WIContext *lpContext = (WIContext *)WorkItem;
	WIContext *lpContextTemp;

	if (lpContext->Count > 0)
	{
		lpContextTemp = (WIContext*)ExAllocatePoolWithTag (NonPagedPool, sizeof (WIContext) + (lpContext->Count * sizeof (PVOID)), 'IW');
		RtlCopyMemory (lpContextTemp, lpContext, sizeof (WIContext) + ((lpContext->Count -1) * sizeof (PVOID)));
		ExFreePool (lpContext);
		lpContext = lpContextTemp;
	}

	lpContext->Context [lpContext->Count] = pContext;
	lpContext->Count++;
}

PVOID CWorkItem::GetContext (PVOID WorkItem, int iIndex)
{
	WIContext *lpContext = (WIContext *)WorkItem;
	PVOID pRet = NULL;
	if (iIndex < lpContext->Count)
	{
		pRet = lpContext->Context [iIndex];
	}
	return pRet;
}

void CWorkItem::Run (PVOID WorkItem, PIO_WORKITEM_ROUTINE  WorkerRoutine)
{
	WIContext *lpContext = (WIContext *)WorkItem;
	lpContext->Routine = WorkerRoutine;

	IoQueueWorkItem (lpContext->WorkItem, CWorkItem::WorkItem, DelayedWorkQueue, lpContext);
}

void CWorkItem::Del (PVOID WorkItem)
{
	WIContext *lpContext = (WIContext *)WorkItem;
	IoFreeWorkItem (lpContext->WorkItem);
	ExFreePool (lpContext);
}

VOID CWorkItem::WorkItem (PDEVICE_OBJECT DeviceObject, PVOID Context)
{
	WIContext *lpContext = (WIContext *)Context;

	lpContext->Routine (DeviceObject, Context);
	Del (Context);
}
