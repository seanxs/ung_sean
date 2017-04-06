/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   PullBuffs.cpp

Environment:

    Kernel mode

--*/

#include "PullBuffs.h"

CPullBuffer::CPullBuffer(void)
	: m_AllocInfo (NonPagedPool, sizeof (SAVE_BUFFER), 'FUB')
	, m_lCountNP (0)
	, m_lSizeNP (0)
	, m_lCountP (0)
	, m_lSizeP (0)
{
	SetDebugLevel( DebugError );
}

CPullBuffer::~CPullBuffer(void)
{
	--m_CountUsing;
	ClearBuffer ();
}

// ***************************************************
//						add buffer
bool CPullBuffer::FreeBuffer (PVOID Buffer)
{
	PSAVE_BUFFER	pSaveBuffer;
	PSAVE_BUFFER	pItem;
	bool			bRet = false;
	CList			*pList;

	DebugPrint (DebugTrace, "AddBuffer");

	if (!Buffer)
	{
		return false;
	}

	pSaveBuffer = *((PSAVE_BUFFER*)(((PUCHAR)Buffer) - sizeof (PVOID)));

	// get list
	if (pSaveBuffer->PoolType == NonPagedPool)
	{
		pList = &m_listBufferNonPagedPool;
	}
	else
	{
		pList = &m_listBufferPagedPool;
	}

	m_lock.Lock ();

	if (pSaveBuffer)
	{
		DebugPrint (DebugInfo, "Add to list 0x%x", pSaveBuffer);
		pList->Add (pSaveBuffer);
		bRet = true;
		--m_CountUsing;
	}

	m_lock.UnLock ();

	return bRet;
}

PVOID CPullBuffer::AllocBuffer (LONG lSize, POOL_TYPE	PoolType)
{
	PSAVE_BUFFER	pSaveBuffer;
	PVOID			pRet = NULL;
	CList			*pList;

	DebugPrint (DebugTrace, "FindBuffer");

	// get list
	if (PoolType ==  NonPagedPool)
	{
		pList = &m_listBufferNonPagedPool;
	}
	else
	{
		pList = &m_listBufferPagedPool;
	}

	m_lock.Lock ();
	pSaveBuffer = (PSAVE_BUFFER)pList->PopFirst ();
	m_lock.UnLock ();

	if (pSaveBuffer)
	{
		if (pSaveBuffer->lSize >= lSize)
		{
			DebugPrint (DebugInfo, "Senond use 0x%x %d=<%d", pSaveBuffer, lSize, pSaveBuffer->lSize);
			pRet = PUCHAR (pSaveBuffer->pBuffer) + sizeof (PVOID);
		}
		else
		{
			m_lock.Lock ();
			if (pSaveBuffer->PoolType == NonPagedPool)
			{
				--m_lCountNP;
				m_lSizeNP -= pSaveBuffer->lSize;
			}
			else
			{
				--m_lCountP;
				m_lSizeP -= pSaveBuffer->lSize;
			}
			m_lock.UnLock ();

			DebugPrint (DebugInfo, "Delete2 buffer 0x%x", pSaveBuffer);
			DebugPrint (DebugInfo, "Delete2 data 0x%x", pSaveBuffer->pBuffer);

			ExFreePool (pSaveBuffer->pBuffer);
			m_AllocInfo.Free (pSaveBuffer);
		}
	}

	if (!pRet)
	{
		pSaveBuffer = (PSAVE_BUFFER)m_AllocInfo.Alloc ();
		if (pSaveBuffer)
		{
			DebugPrint (DebugInfo, "New buffer 0x%x", pSaveBuffer);
			pSaveBuffer->pBuffer = ExAllocatePoolWithTag (PoolType, (lSize * 2) + sizeof (PVOID), 'MEM');
			if (pSaveBuffer->pBuffer)
			{
				DebugPrint (DebugInfo, "Data 0x%x", pSaveBuffer->pBuffer);

				pSaveBuffer->PoolType = PoolType;
				pSaveBuffer->lSize = lSize * 2;
				*((PVOID*)pSaveBuffer->pBuffer) = pSaveBuffer;
				pRet = PUCHAR (pSaveBuffer->pBuffer) + sizeof (PVOID);

				m_lock.Lock ();
				if (pSaveBuffer->PoolType == NonPagedPool)
				{
					++m_lCountNP;
					m_lSizeNP += pSaveBuffer->lSize;
				}
				else
				{
					++m_lCountP;
					m_lSizeP += pSaveBuffer->lSize;
				}
				m_lock.UnLock ();
			}
			else
			{
				DebugPrint (DebugError, "FindBuffer: not memory");
				m_AllocInfo.Free (pSaveBuffer);
			}
		}
		else
		{
			DebugPrint (DebugError, "FindBuffer2: not memory");
		}
	}
	if (pRet)
	{
		++m_CountUsing;
	}
	return pRet;
}

void CPullBuffer::ClearBuffer ()
{
	PSAVE_BUFFER	pSaveBuffer;
	KIRQL			OldIrql;
	PVOID			pRet = NULL;

	DebugPrint (DebugTrace, "ClearBuffer");

	// NonPagedPool
	do
	{
		m_lock.Lock ();
		pSaveBuffer = (PSAVE_BUFFER)m_listBufferNonPagedPool.PopFirst ();
		m_lock.UnLock ();

		if (pSaveBuffer)
		{
			ExFreePool (pSaveBuffer->pBuffer);
			m_AllocInfo.Free (pSaveBuffer);
			continue;
		}
		break;
	}while (true);

	// PagedPool
	do
	{
		m_lock.Lock ();
		pSaveBuffer = (PSAVE_BUFFER)m_listBufferPagedPool.PopFirst();
		m_lock.UnLock ();

		if (pSaveBuffer)
		{
			ExFreePool (pSaveBuffer->pBuffer);
			m_AllocInfo.Free (pSaveBuffer);
			continue;
		}
		break;
	}while (true);
}

LONG CPullBuffer::GetUsedSize (PVOID pBuff)
{
	PSAVE_BUFFER	pSaveBuffer;

	pSaveBuffer = *((PSAVE_BUFFER*)(((PUCHAR)pBuff) - sizeof (PVOID)));

	return pSaveBuffer->lUsed;
}

void CPullBuffer::SetUsedSize (PVOID pBuff, LONG lSize)
{
	PSAVE_BUFFER	pSaveBuffer;

	pSaveBuffer = *((PSAVE_BUFFER*)(((PUCHAR)pBuff) - sizeof (PVOID)));

	pSaveBuffer->lUsed = lSize;
}

