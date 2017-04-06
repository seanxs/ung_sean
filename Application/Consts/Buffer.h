/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Buffer.h

Environment:

    User mode

--*/
#pragma once

#include <atlstr.h>

class CBuffer
{
public:
	CBuffer(void);
	CBuffer(int iSize);
	CBuffer(const CBuffer &other);
	~CBuffer(void);

	CBuffer& operator=(const CBuffer &other);

protected:
	PBYTE	m_pBuf;
	int		m_iSize;
	int		m_iActualSize;
	int		m_iResrv;
    CRITICAL_SECTION    m_cs;

public:
	int GetCount () const;
	int GetBufferSize () const;
	int SetSize (int iNewSize);
	PVOID GetData () const;
	void RemoveAll ();
    int Add (BYTE bt);
	int AddArray (BYTE *bt, int iSize);

	void SetReserv (int iSize);
	PVOID GetFullData () const;
	int GetReservSize () const;

	int AddString (CStringA str);
};
