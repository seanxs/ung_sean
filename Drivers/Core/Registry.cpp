/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Registry.cpp

Environment:

    Kernel mode

--*/

#include "Registry.h"

CRegistry::CRegistry(void)
	: m_hKey (NULL)
{
}


CRegistry::~CRegistry(void)
{
	if (IsOpen())
	{
		Close();
	}
}

NTSTATUS CRegistry::Close ()
{
	if (IsOpen())
	{
		ZwClose(m_hKey);
		m_hKey = NULL;
	}
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS CRegistry::Open (CUnicodeString &Path, ACCESS_MASK Access)
{
	if (IsOpen())
	{
		Close();
	}

	InitializeObjectAttributes(&m_obAttrs, Path.GetString(), OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	return ZwOpenKey(&m_hKey, Access, &m_obAttrs);
}

NTSTATUS CRegistry::GetQuery (CUnicodeString &Path, CBuffer &buffer)
{
	NTSTATUS    status = STATUS_SUCCESS;
	ULONG		ResultLength;

	if (!IsOpen())
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (buffer.GetBufferSize() == 0)
	{
		buffer.SetSize(1024);
	}

	while (NT_SUCCESS(status))
	{
		status = ZwQueryValueKey(	m_hKey, 
									Path.GetString(), 
									KeyValueFullInformation,
									buffer.GetData(), 
									buffer.GetBufferSize(), 
									&ResultLength);

		buffer.SetSize(ResultLength);
		if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL) 
		{
			status = STATUS_SUCCESS;

			continue;
		}

		break;
	}

	return status;
}

NTSTATUS CRegistry::GetNumber (CUnicodeString &Path, LONG &Value)
{
	CBuffer							buffer (NonPagedPool);
	PKEY_VALUE_FULL_INFORMATION		pKeyValue = NULL;
	NTSTATUS						status;

	buffer.SetSize(sizeof (KEY_VALUE_FULL_INFORMATION) + sizeof (LONG) + 128*sizeof (WCHAR));

	status = GetQuery (Path, buffer);
	if (!NT_SUCCESS (status))
	{
		return status;
	}

	pKeyValue = (PKEY_VALUE_FULL_INFORMATION)buffer.GetData();

	if (pKeyValue->DataLength == sizeof (LONG) && pKeyValue->Type == REG_DWORD)
	{
		Value = *((ULONG*)(((PCHAR)pKeyValue) + pKeyValue->DataOffset));
	}
	else
	{
		return STATUS_UNSUCCESSFUL;
	}

	return status;
}

NTSTATUS CRegistry::GetString (CUnicodeString &Path, CUnicodeString &Value)
{
	CBuffer							buffer (NonPagedPool);
	PKEY_VALUE_FULL_INFORMATION		pKeyValue = NULL;
	NTSTATUS						status;

	buffer.SetSize(sizeof (KEY_VALUE_FULL_INFORMATION) + sizeof (LONG) + 128*sizeof (WCHAR));

	status = GetQuery (Path, buffer);
	if (!NT_SUCCESS (status))
	{
		return status;
	}

	pKeyValue = (PKEY_VALUE_FULL_INFORMATION)buffer.GetData();

	if (pKeyValue->Type == REG_SZ)
	{
		Value = (PWCHAR)(((PCHAR)pKeyValue) + pKeyValue->DataOffset);
	}
	else
	{
		return STATUS_UNSUCCESSFUL;
	}

	return status;
}

