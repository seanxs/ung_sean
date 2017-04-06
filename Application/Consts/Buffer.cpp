/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Buffer.cpp

Environment:

    User mode

--*/
#include "StdAfx.h"
#include "Buffer.h"

CBuffer::CBuffer(void)
	: m_pBuf (NULL)
	, m_iSize (0)
	, m_iActualSize (0)
	, m_iResrv (0)
{
    InitializeCriticalSection (&m_cs);
}

CBuffer::CBuffer(int iSize)
	: m_pBuf (NULL)
	, m_iSize (0)
	, m_iActualSize (0)
	, m_iResrv (0)
{
    InitializeCriticalSection (&m_cs);

	SetSize (iSize);
}

CBuffer::CBuffer(const CBuffer &other)
	: m_pBuf (NULL)
	, m_iSize (0)
	, m_iActualSize (0)
	, m_iResrv (0)
{
	InitializeCriticalSection (&m_cs);

	AddArray ((BYTE*)other.GetData (), other.GetCount ());
}

CBuffer::~CBuffer(void)
{
	if (m_pBuf)
	{
		delete [] m_pBuf;
		m_pBuf = NULL;
	}

    DeleteCriticalSection (&m_cs);
}

CBuffer& CBuffer::operator=(const CBuffer &other)
{
	if (this != &other)
	{
		RemoveAll ();
		AddArray ((BYTE*)other.GetData (), other.GetCount ());
	}
	return *this;
}

int CBuffer::GetCount () const
{
	return m_iSize;
}

int CBuffer::GetBufferSize () const
{
	return m_iActualSize;
}

int CBuffer::SetSize (int iNewSize)
{
	PVOID pTemp;

    EnterCriticalSection (&m_cs);

	if (iNewSize > m_iActualSize)
	{
		m_iActualSize = (int)(iNewSize * 1.2);
		pTemp = m_pBuf;
		m_pBuf = new BYTE [m_iActualSize + m_iResrv];
        if (m_pBuf)
        {
		    if (m_iSize && m_pBuf)
		    {
			    CopyMemory (m_pBuf, pTemp, m_iSize + m_iResrv);
		    }

		    if (pTemp)
		    {
			    delete [] pTemp;
		    }
        }
        else
        {
            iNewSize = m_iSize;
        }
	}
	m_iSize = iNewSize;

    LeaveCriticalSection (&m_cs);
    return m_iSize;
}

PVOID CBuffer::GetData () const
{
	return (m_pBuf + m_iResrv);
}

void CBuffer::RemoveAll ()
{
    EnterCriticalSection (&m_cs);
	m_iSize = 0;
    LeaveCriticalSection (&m_cs);
}

int CBuffer::Add (BYTE bt)
{
    int iPos = m_iSize;

    EnterCriticalSection (&m_cs);
    if (SetSize (m_iSize + 1) != iPos)
    {
        m_pBuf [iPos + m_iResrv] = bt;
    }
    else
    {
        iPos = 0;
    }
    LeaveCriticalSection (&m_cs);

    return iPos;
}

int CBuffer::AddArray (BYTE *bt, int iSize)
{
    int iPos = m_iSize;

    EnterCriticalSection (&m_cs);
    if (SetSize (m_iSize + iSize) != iPos)
    {
		CopyMemory (m_pBuf + iPos + m_iResrv, bt, iSize);
		iPos += iSize;
    }
    else
    {
        iPos = 0;
    }
    LeaveCriticalSection (&m_cs);

    return iPos;
}

void CBuffer::SetReserv (int iSize)
{
	m_iResrv = iSize;
	SetSize(GetBufferSize());

}

PVOID CBuffer::GetFullData () const
{
	return m_pBuf;
}

int CBuffer::GetReservSize () const
{
	return m_iResrv;
}

int CBuffer::AddString (CStringA str)
{
	return AddArray (LPBYTE (LPCSTR (str)), str.GetLength ());
}