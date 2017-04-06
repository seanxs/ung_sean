/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Registry.h

Environment:

    Kernel mode

--*/
#pragma once
#include "BaseClass.h"
#include "UnicodeString.h"
#include "Buffer.h"

class CRegistry : public CBaseClass
{
public:
	CRegistry(void);
	~CRegistry(void);

	NTSTATUS Open (CUnicodeString &Path, ACCESS_MASK Access = GENERIC_READ);
	NTSTATUS Close ();


	NTSTATUS GetNumber (CUnicodeString &Path, LONG &Value);
	NTSTATUS GetString (CUnicodeString &Path, CUnicodeString &Value);
	NTSTATUS GetQuery (CUnicodeString &Path, CBuffer &buffer);

	bool IsOpen () {return (m_hKey != NULL);}

private:
	OBJECT_ATTRIBUTES	m_obAttrs;
	HANDLE				m_hKey;
};

