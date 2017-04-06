/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Buffer.h

Environment:

    Kernel mode

--*/
#include "baseclass.h"

#pragma once

class CBuffer : public CBaseClass
{

public:

	NEW_ALLOC_DEFINITION ('CBC')
	CBuffer(POOL_TYPE PoolType = PagedPool, int iSize = 0);
	CBuffer(const CBuffer &other);
	~CBuffer(void);

	CBuffer& operator=(const CBuffer &other);

protected:
	POOL_TYPE	m_PoolType;
	BYTE		*m_pBuf;
	int			m_iSize;
	int			m_iActualSize;

public:
	int GetCount () const;
	int GetBufferSize () const;
	int SetSize (int iNewSize);
	PVOID GetData () const;
	void RemoveAll ();
    int Add (BYTE bt);
	int AddArray (BYTE *bt, int iSize);
 
};
