/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UnicIndef.h

Environment:

    Kernel mode

--*/

#pragma once

#include "..\core\BaseClass.h"
#include "..\core\list.h"
#include "..\core\lock.h"

class CUnicId;

class CUnicId : public CBaseClass
{
private:
	CUnicId(void);

public:
	~CUnicId(void);

public:
	static CUnicId* Instance ();
	static void Init ();
	static void Delete ();

	bool AddUnicId (ULONG uId);
	bool DelUnicId (ULONG uId);
	ULONG BuildUnicId ();

private:
	CList		m_listId;
	CSpinLock	m_lock;

	static CUnicId* m_base;
};

