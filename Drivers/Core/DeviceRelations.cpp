/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   DeviceRelations.cpp

Abstract:

   Class that monitors new devices apperance and old devices deletion on a bus. 
   Is used to connect filters drivers for new devices on a bus.

Environment:

    Kernel mode

--*/
#include "DeviceRelations.h"
#include "Buffer.h"

CPdoBase::~CPdoBase ()
{
	if (m_pRelation)
	{
		m_pRelation->RemovePdo(GetDevIndef());
	}
}

void CPdoBase::IncMyself ()
{
	if (!m_bMySelf)
	{
		m_bMySelf = true;
		intrusive_ptr_add_ref (this);
	}
}

void CPdoBase::DecMyself ()
{
	if (m_bMySelf)
	{
		m_bMySelf = true;
		intrusive_ptr_release (this);
	}
}

CDeviceRelations::CDeviceRelations(void)
	: CBaseClass ()
    , m_bRelationSelf (false)
	, m_arrPdo (NonPagedPool)
{
	SetDebugName ("CVirtulUsbHubExtension");
}

CDeviceRelations::~CDeviceRelations(void)
{
	// clear all children
	m_spinDr.Lock();
	while (m_arrPdo.GetCount())
	{
		TPtrPdoBase pPdo = m_arrPdo.GetItem(0);
		if (pPdo.get ())
		{
			m_arrPdo.Remove(0);
			m_spinDr.UnLock();
			pPdo = NULL;
			m_spinDr.Lock();
		}
		else
		{
			break;
		}
	}
	m_spinDr.UnLock();
}

void CDeviceRelations::QueryRelations (PDEVICE_RELATIONS RelationsNew)
{
	ULONG iCount = sizeof (DEVICE_RELATIONS) + sizeof (PDEVICE_OBJECT)* (RelationsNew->Count - 1);
	ULONG iItem;
	PDEVICE_RELATIONS Relation;
	CBuffer buf (NonPagedPool);

	buf.SetSize(iCount);
	RtlCopyMemory (buf.GetData(), RelationsNew, iCount);

	Relation = (PDEVICE_RELATIONS)buf.GetData();

    DebugPrint (DebugTrace, "QueryRelations - 0x%x - %d", RelationsNew,RelationsNew?RelationsNew->Count:0);
	// Find new Pdo
    for (int a = 0; a < (int)RelationsNew->Count; a++)
	{
		if (RelationsNew->Objects [a] == NULL)
		{
			continue;
		}
		if (!IsExistPdo (RelationsNew->Objects [a]).get())
		{
			AddPdo (RelationsNew->Objects [a]);
		}
	}

	// remove old pdo
    TPtrPdoBase pPdo;
	m_spinDr.Lock ();
	for (int b = 0; b < m_arrPdo.GetCount (); b++)
	{
		pPdo = m_arrPdo [b];
		ULONG a;
		for (a = 0; a < (ULONG)Relation->Count; a++)
		{
			if (pPdo->GetDevIndef () == Relation->Objects [a])
			{
				break;
			}
		}

		if (a == (int)Relation->Count)
		{
			DebugPrint (DebugInfo, "RemovePdo from listPdo 0x%x", pPdo->GetDevIndef ());
			RemovePdoNum (b, false);
			--b;
		}
	}
    m_spinDr.UnLock ();
}

TPtrPdoBase CDeviceRelations::IsExistPdo (PVOID DeviceIndef)
{
    TPtrPdoBase		pPdo;

	DebugPrint (DebugTrace, "IsExistPdo");

	m_spinDr.Lock ();
	for (int a = 0; a < m_arrPdo.GetCount (); a++)
	{
		pPdo = m_arrPdo [a];
		if (pPdo->GetDevIndef () == DeviceIndef)
		{
			m_spinDr.UnLock ();
			DebugPrint (DebugInfo, "IsExistPdo 0x%x", DeviceIndef);
			return pPdo;
		}

	}
	m_spinDr.UnLock ();
	return NULL;
}

NTSTATUS CDeviceRelations::RemovePdoNum (int uNum, bool bLock)
{
    DebugPrint (DebugTrace, "RemovePdo - %d", uNum);
	if (uNum < m_arrPdo.GetCount ())
	{
		if (bLock) m_spinDr.Lock ();

		m_arrPdo[uNum]->ClearParent();
		if (m_arrPdo.Remove(uNum))
		{
			if (bLock) m_spinDr.UnLock ();
			return STATUS_SUCCESS;
		}
		if (bLock) m_spinDr.UnLock ();
	}
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS CDeviceRelations::RemovePdo (PVOID pIndef)
{
	TPtrPdoBase pPdo;

	DebugPrint (DebugTrace, "RemovePdo - %x", pIndef);

	m_spinDr.Lock ();
	for (int a = 0; a < m_arrPdo.GetCount (); a++)
	{
		pPdo = m_arrPdo [a];
		if (pPdo->GetDevIndef () == pIndef)
		{
			pPdo->ClearParent();
			m_arrPdo.Remove(a);
			m_spinDr.UnLock ();
			return STATUS_SUCCESS;
		}
	}
	m_spinDr.UnLock ();

	return STATUS_UNSUCCESSFUL;
}

TPtrPdoBase CDeviceRelations::GetChild (int iNum)
{
	TPtrPdoBase pPdo;
	if (iNum < m_arrPdo.GetCount())
	{
		pPdo = m_arrPdo [iNum];
	}
	return pPdo;
}