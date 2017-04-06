/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BaseClass.cpp

Environment:

    Kernel mode

--*/

#pragma once
#include "BaseClass.h"
 
template <class ITEM >
class CArrayT : public CBaseClass
{
public:
	CArrayT(POOL_TYPE PoolType = PagedPool, int iSize = 10)
		: m_iSize (iSize)
		, m_PoolType (PoolType)
		, m_iCount (0)
		, m_Buffer (NULL)
	{
		if (m_iSize)
		{
			m_Buffer = new (m_PoolType) ITEM  [m_iSize];
		}
	}

	virtual ~CArrayT(void)
	{
		if (m_Buffer)
		{
			delete [] m_Buffer;
		}
	}
	NEW_ALLOC_DEFINITION ('TACE')

public:
	void Add (const ITEM& Element)
	{
		if (m_iSize <= m_iCount)
		{
			ITEM *pTemp;
			m_iSize = m_iSize * 14 / 10;
			pTemp = new ITEM [m_iSize];

			for (int a = 0; a < m_iCount; a++)
			{
				pTemp [a] = m_Buffer [a];
			}
			delete [] m_Buffer;
			m_Buffer = NULL;
			m_Buffer = pTemp;
		}
		m_Buffer [m_iCount] = Element;
		++m_iCount;
	}

	bool Remove(int iNumber)
	{
		if (iNumber < m_iCount)
		{
			m_Buffer [iNumber] = NULL;

			for (int a = iNumber; a < m_iCount - 1; a++)
			{
				m_Buffer [a] = m_Buffer [a + 1];
			}
			--m_iCount;
			m_Buffer [m_iCount] = NULL;
			return true;
		}
		return false;
	}

	ITEM& GetItem (int iNumber)
	{
		if (iNumber < m_iCount)
		{
			return m_Buffer [iNumber];
		}
		return m_ret;
	}

	ITEM& operator[](int iNumber)
	{
		return GetItem (iNumber);
	}

	int GetCount () const
	{
		return m_iCount;
	}

	ITEM& PopFirst()
	{
		ITEM pTemp;
		if (GetCount())
		{
			pTemp = GetItem (0);
			Remove(0);
		}
		return pTemp;
	}

private:
	int m_iSize;
	int m_iCount;
	ITEM *m_Buffer;
	POOL_TYPE	m_PoolType;
	ITEM m_ret;
};

