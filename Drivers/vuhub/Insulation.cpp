/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Insulation.cpp

Environment:

    Kernel mode

--*/

#include "Core\BaseClass.h"
#include "Ntddstor.h"
#include "Insulation.h"
#include "Core\SystemEnum.h"
#include "Core\Buffer.h"
#include "FilterStub.h"
#include "VirtualUsbHub.h"

bool Insulation::FindAllChildGlobaleSymbolicLinks(PDEVICE_OBJECT pDev, CDeviceRelations *Relation, CSystemEnum::FEnunSymbolicLink funcEnum, PVOID pContext)
{
	CBuffer					pBuff;
	UNICODE_STRING			strPath;
	TCONTEXT				Context;
	bool					bRet = false;

	pBuff.SetSize(NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR));

	{
		int a = 0;
		TPtrPdoBase		pPdo;
		CFilterStub		*pStub;
		while ((pPdo = Relation->GetChild(a)).get ())
		{
			ULONG	uDisk;
			pStub = (CFilterStub*)pPdo.get();
			if (pStub->GetPnpState() == DriverBase::Started && pStub->GetDeviceObject ())
			{
				// get path device
				if (CSystemEnum::GetDiskNumber(pStub->GetDeviceObject(), uDisk))
				{
					RtlZeroMemory(pBuff.GetData(), pBuff.GetBufferSize());

					RtlStringCbPrintfW((WCHAR*)pBuff.GetData(), pBuff.GetBufferSize(), L"\\Device\\Harddisk%d", uDisk);
					RtlInitUnicodeString(&strPath, (PWCHAR)pBuff.GetData());
					strPath.MaximumLength = (USHORT)pBuff.GetBufferSize();

					Context.pCont1 = funcEnum;
					Context.pCont2 = &strPath;
					Context.pCont3 = pStub->GetDeviceObject();
					Context.pCont4 = pContext;
					CSystemEnum::EnumDir(&strPath, FindSlCallback1, &Context);
					bRet = true;
				}
				else
				{
					bRet |= FindAllChildGlobaleSymbolicLinks(pStub->GetDeviceObject(), pStub, funcEnum, pContext);
				}
			}
			a++;
		}

		//ExFreePool(pDevRelations);
	}
	return bRet;
}

// 1. Find disk name
// 2. enum value symbol name
// 3. Symbol link of value to device name
// 4. Find all symbol link for device name of value
// 5. Verify symbol link "x:"

NTSTATUS Insulation::Detect_NewStorage(PDEVICE_INTERFACE_CHANGE_NOTIFICATION NotificationStruct, PVOID Context)
{
	CVirtualUsbHubExtension		*pBase = (CVirtualUsbHubExtension*)Context;
	bool						bFindFlashDevice = false;

	pBase->DebugPrint(CBaseClass::DebugError, "Detect_NewStorage %d", pBase->m_arrPdo.GetCount());

	pBase->m_timerSymbolLink.CancelTimer();
	for (int a = 0; a < pBase->m_arrPdo.GetCount(); a++)
	{
		TPtrPdoExtension pPdo = pBase->m_arrPdo[a];
		if (pPdo.get() && pPdo->m_pTcp && wcslen(pPdo->m_pTcp->m_szDosDeviceUser))
		{
			bFindFlashDevice = FindAllChildGlobaleSymbolicLinks(pPdo->GetDeviceObject (), pPdo.get(), EnunSymbolicLink, pPdo.get());
		}
	}
	pBase->DebugPrint(CBaseClass::DebugError, "exit Detect_NewStorage");

	if (bFindFlashDevice)
	{
		pBase->m_timerSymbolLink.SetTimer(1000);
	}

	return STATUS_SUCCESS;
}

VOID Insulation::ItemSymbolLynk(Thread::CThreadPoolTask *pTask, PVOID Context)
{
	while (pTask->WaitTask())
	{
		Detect_NewStorage(NULL, Context);
	}
}

VOID Insulation::FindSymbolLynk(struct _KDPC *Dpc, PVOID  DeferredContext, PVOID  SystemArgument1, PVOID  SystemArgument2)
{
	CVirtualUsbHubExtension *pBase = (CVirtualUsbHubExtension*)DeferredContext;

	pBase->m_poolInsulation.PushTask((PVOID)1);
}

bool Insulation::EnunSymbolicLink(PVOID pContext, PDEVICE_OBJECT pDev, PUNICODE_STRING strDev, PUNICODE_STRING strSymboleLink)
{
	CPdoExtension				*pBase = (CPdoExtension*)pContext;
	evuh::CUsbDevToTcp				*pTcp = pBase->m_pTcp;
	UNICODE_STRING				newLink;
	CBuffer						pBuf;
	WCHAR						*szName;

	pBase->DebugPrint(CBaseClass::DebugError, "EnunSymboleLink %S %S", strDev->Buffer, strSymboleLink->Buffer);

	if (strSymboleLink->Buffer[wcslen(strSymboleLink->Buffer) - 1] == ':' && pTcp)
	{
		++pTcp->m_WaitUse;
		pBuf.SetSize(NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR));
		szName = wcsrchr(strSymboleLink->Buffer, '\\');
		RtlStringCchCopyW((PWCHAR)pBuf.GetData(), NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), pTcp->m_szDosDeviceUser);
		RtlStringCchCatW((PWCHAR)pBuf.GetData(), NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), szName);
		RtlInitUnicodeString(&newLink, (PWCHAR)pBuf.GetData());

		IoDeleteSymbolicLink(strSymboleLink);
		IoCreateSymbolicLink(&newLink, strDev);

		pBase->AddLink(strDev, &newLink);

		pBase->DebugPrint(CBaseClass::DebugError, "EnunSymboleLink change %S %S", strDev->Buffer, strSymboleLink->Buffer);
		--pTcp->m_WaitUse;
	}
	return false;
}

