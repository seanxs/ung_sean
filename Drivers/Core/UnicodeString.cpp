/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UnicodeString.cpp

Environment:

    Kernel mode

--*/

#include "UnicodeString.h"
#include "Ntstrsafe.h"

CUnicodeString::CUnicodeString(POOL_TYPE PoolType)
	: m_PoolType (PoolType)
{
	RtlZeroMemory (&m_string, sizeof (UNICODE_STRING));
}

CUnicodeString::CUnicodeString(const CUnicodeString& Source)
	: m_PoolType (Source.m_PoolType)
{
	RtlZeroMemory (&m_string, sizeof (UNICODE_STRING));
	if (Source.m_string.MaximumLength)
	{
		m_string = AllocString(Source.m_string.MaximumLength);
		RtlUnicodeStringCopy (&m_string, &Source.m_string);
	}
}

CUnicodeString::CUnicodeString(const UNICODE_STRING& Source, POOL_TYPE PoolType)
	: m_PoolType (PoolType)
{
	RtlZeroMemory (&m_string, sizeof (UNICODE_STRING));
	if (Source.MaximumLength)
	{
		m_string = AllocString(Source.MaximumLength);
		RtlUnicodeStringCopy (&m_string, &Source);
	}
}

CUnicodeString::CUnicodeString(PCWSTR Source, POOL_TYPE PoolType)
	: m_PoolType (PoolType)
{
	RtlZeroMemory (&m_string, sizeof (UNICODE_STRING));
	if (Source)
	{
		int iSize = wcslen (Source);
		m_string = AllocString ((iSize + 1) * sizeof (WCHAR));
		if (m_string.Buffer)
		{
			m_string.Length = iSize * sizeof (WCHAR);
			RtlStringCbCopyW (m_string.Buffer, m_string.MaximumLength, Source);
		}
	}
}

CUnicodeString::~CUnicodeString(void)
{
	FreeString(m_string);
}

CUnicodeString& CUnicodeString::operator+= (const CUnicodeString& String)
{
	*this += String.m_string;
	return *this;
}

CUnicodeString& CUnicodeString::operator+= (const UNICODE_STRING& String)
{
	UNICODE_STRING	Temp;

	Temp = AllocString(String.Length + m_string.Length + sizeof (WCHAR));

	RtlUnicodeStringCopy (&Temp, &m_string);
	RtlUnicodeStringCat (&Temp, &String);

	FreeString(m_string);
	m_string = Temp;

	return *this;
}

CUnicodeString& CUnicodeString::operator+= (PCWSTR String)
{
	UNICODE_STRING Temp;// (Source);

	RtlInitUnicodeString(&Temp, String);

	*this += Temp;
	return *this;
}

CUnicodeString& CUnicodeString::operator+ (const CUnicodeString& String)
{
	*this += String;
	return *this;
}

CUnicodeString& CUnicodeString::operator+ (PCWSTR Source)
{
	*this += Source;
	return *this;
}

CUnicodeString& CUnicodeString::operator+ (const UNICODE_STRING& String)
{
	*this += String;
	return *this;
}

bool CUnicodeString::operator== (const CUnicodeString& String)
{
	return (wcscmp (String.m_string.Buffer, m_string.Buffer) == 0);
}

bool CUnicodeString::CompareNoCase (const CUnicodeString& String)
{
	return (_wcsicmp (String.m_string.Buffer, m_string.Buffer) == 0);
}

bool CUnicodeString::operator== (const UNICODE_STRING& String)
{
	return (wcscmp (String.Buffer, m_string.Buffer) == 0);
}

bool CUnicodeString::CompareNoCase (const UNICODE_STRING& String)
{
	return (_wcsicmp (String.Buffer, m_string.Buffer) == 0);
}

UNICODE_STRING CUnicodeString::Detach ()
{
	UNICODE_STRING temp = m_string;
	RtlZeroMemory (&m_string, sizeof (UNICODE_STRING));
	return temp;
}

UNICODE_STRING CUnicodeString::AllocString (int iSize)
{
	UNICODE_STRING strTemp;

	RtlZeroMemory (&strTemp, sizeof (UNICODE_STRING));

	strTemp.Buffer = (PWCH)ExAllocatePoolWithTag (m_PoolType, iSize, 'MEME');

	if (strTemp.Buffer)
	{
		RtlZeroMemory (strTemp.Buffer, iSize);
		strTemp.MaximumLength = (USHORT)iSize;
	}

	return strTemp;
}

void CUnicodeString::FreeString (UNICODE_STRING &String)
{
	if (String.Buffer)
	{
		ExFreePool (String.Buffer);
		RtlZeroMemory (&String, sizeof (UNICODE_STRING));
	}
}

CUnicodeString& CUnicodeString::operator= (const CUnicodeString& String)
{
	*this = String.m_string;
	return *this;
}

CUnicodeString& CUnicodeString::operator= (const UNICODE_STRING& String)
{
	FreeString (m_string);

	if (String.Length)
	{
		m_string = AllocString(String.Length + sizeof (WCHAR));
		RtlUnicodeStringCopy (&m_string, &String);
	}

	return *this;
}

CUnicodeString& CUnicodeString::operator= (PCWSTR String)
{
	UNICODE_STRING Temp;// (Source);

	RtlInitUnicodeString(&Temp, String);
	*this = Temp;

	return *this;
}
