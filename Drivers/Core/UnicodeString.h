/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UnicodeString.h

Environment:

    Kernel mode

--*/

#pragma once

#ifndef UNICODE_STRING_H_49DA26FBF29848FBBE45E4B654BBE07B
#define UNICODE_STRING_H_49DA26FBF29848FBBE45E4B654BBE07B

#include "BaseClass.h"

class CUnicodeString : public CBaseClass
{
public:
	CUnicodeString(POOL_TYPE PoolType = PagedPool);
	CUnicodeString(const CUnicodeString& Source);
	CUnicodeString(PCWSTR Source, POOL_TYPE PoolType = PagedPool);
	CUnicodeString(const UNICODE_STRING& Source, POOL_TYPE PoolType = PagedPool);

	~CUnicodeString(void);

	CUnicodeString& operator+= (const CUnicodeString& String);
	CUnicodeString& operator+= (const UNICODE_STRING& String);
	CUnicodeString& operator+= (PCWSTR String);

	CUnicodeString& operator+ (const CUnicodeString& String);
	CUnicodeString& operator+ (const UNICODE_STRING& String);
	CUnicodeString& operator+ (PCWSTR Source);

	CUnicodeString& operator= (const CUnicodeString& String);
	CUnicodeString& operator= (const UNICODE_STRING& String);
	CUnicodeString& operator= (PCWSTR String);


	bool operator== (const CUnicodeString& string);
	bool CompareNoCase (const CUnicodeString& string);

	bool operator== (const UNICODE_STRING& string);
	bool CompareNoCase (const UNICODE_STRING& string);


	PUNICODE_STRING GetString() 
	{
		return &m_string;
	}

	USHORT	GetSize() { return m_string.Length; }
	USHORT	GetStringSize() { return m_string.Length / sizeof (WCHAR); }
	PWSTR	GetBuffer() { return m_string.Buffer; }

	UNICODE_STRING Detach ();

protected:
	UNICODE_STRING AllocString (int iSize);
	void FreeString (UNICODE_STRING &string);

protected:
	UNICODE_STRING m_string;
	POOL_TYPE	m_PoolType;
};

typedef boost::intrusive_ptr <CUnicodeString> TPtrUnicodeString;

#endif