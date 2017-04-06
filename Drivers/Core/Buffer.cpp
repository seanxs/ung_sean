/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   u2ec.h

Environment:

    Kernel mode

--*/
#include "Buffer.h"

CBuffer::CBuffer(POOL_TYPE PoolType, int iSize)
	: m_pBuf (NULL)
	, m_iSize (0)
	, m_iActualSize (0)
	, m_PoolType (PoolType)
{
	SetSize (iSize);
}

CBuffer::CBuffer(const CBuffer &other)
	: m_pBuf (NULL)
	, m_iSize (0)
	, m_iActualSize (0)
	, m_PoolType (other.m_PoolType)
{
	AddArray ((BYTE*)other.GetData (), other.GetCount ());
}

CBuffer::~CBuffer(void)
{
	if (m_pBuf)
	{
		ExFreePool (m_pBuf);
		m_pBuf = NULL;
	}
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

	if (iNewSize > m_iActualSize)
	{
		m_iActualSize = iNewSize * 6 / 5;
		pTemp = m_pBuf;
		m_pBuf = (BYTE*)ExAllocatePoolWithTag (m_PoolType, m_iActualSize, 'MEME');
        if (m_pBuf)
        {
		    if (m_iSize && m_pBuf)
		    {
			    RtlCopyMemory (m_pBuf, pTemp, m_iSize);
		    }

		    if (pTemp)
		    {
			    ExFreePool (pTemp);
		    }
        }
        else
        {
            iNewSize = m_iSize;
        }
	}
	m_iSize = iNewSize;

    return m_iSize;
}

PVOID CBuffer::GetData () const
{
	return m_pBuf;
}

void CBuffer::RemoveAll ()
{
	m_iSize = 0;
}

int CBuffer::Add (BYTE bt)
{
    int iPos = m_iSize;

    if (SetSize (m_iSize + 1) != iPos)
    {
        m_pBuf [iPos] = bt;
    }
    else
    {
        iPos = 0;
    }

    return iPos;
}

int CBuffer::AddArray (BYTE *bt, int iSize)
{
    int iPos = m_iSize;

    if (SetSize (m_iSize + iSize) != iPos)
    {
		RtlCopyMemory (m_pBuf + iPos, bt, iSize);
		iPos += iSize;
    }
    else
    {
        iPos = 0;
    }

    return iPos;
}
