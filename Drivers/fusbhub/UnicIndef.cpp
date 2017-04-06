/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UnicIndef.cpp

Environment:

    Kernel mode

--*/

#include "UnicIndef.h"

CUnicId *CUnicId::m_base = NULL;

CUnicId::CUnicId(void)
{
}

CUnicId::~CUnicId(void)
{
}

void CUnicId::Init ()
{
	m_base = new CUnicId ();
}

void CUnicId::Delete ()
{
	if (m_base)
	{
		delete m_base;
		m_base = NULL;
	}
}


bool CUnicId::AddUnicId (ULONG uId)
{
	bool bRet = true;
	m_lock.Lock ();
	for (int a = 0; a < m_listId.GetCount (); a++)
	{
		if (ULONG (ULONG_PTR (m_listId.GetItem (a))) == uId)
		{
			bRet = false;
			break;
		}
	}
	if (bRet)
	{
		m_listId.Add ((void*)((ULONG_PTR)uId));
	}
	m_lock.UnLock ();
	return bRet;
}

bool CUnicId::DelUnicId (ULONG uId)
{
	bool bRet = false;
	m_lock.Lock ();
	for (int a = 0; a < m_listId.GetCount (); a++)
	{
		if (ULONG (ULONG_PTR (m_listId.GetItem (a))) == uId)
		{
			m_listId.Remove (a);
			bRet = true;
			break;
		}
	}
	m_lock.UnLock ();
	return bRet;
}

ULONG CUnicId::BuildUnicId ()
{
	ULONG	uStart = 100;
	bool bFind = false;

	m_lock.Lock ();

	while (!bFind)
	{
		for (int a = 0; a < m_listId.GetCount (); a++)
		{
			if (ULONG (ULONG_PTR (m_listId.GetItem (a))) == uStart)
			{
				bFind = true;
				break;
			}
		}

		if (bFind)
		{
			uStart++;
			bFind = false;
		}
		else
		{
			m_listId.Add ((void*)((ULONG_PTR)uStart));
			break;
		}
	}
	m_lock.UnLock ();
	return uStart;
}

CUnicId* CUnicId::Instance ()
{
	return m_base;
}
