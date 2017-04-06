/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   trace.h

Environment:

    User mode

--*/

#pragma once

static inline void TraceSendCommand(LPCTSTR szServer, USHORT uPort, LPCSTR szCommand, const char *caller)
{
	CString str;

	SYSTEMTIME st = {0};
	::GetLocalTime(&st);

	str.Format(_T("%04d/%02d/%02d %02d:%02d:%02d: %s: SendCommand(%s:%d)\n"), 
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
		CString(caller), CString(szServer), uPort);
	AtlTrace(str);
}

static inline void TraceOpenConnection(const char *caller, const CString &strServer)
{
	CString str;

	SYSTEMTIME st = {0};
	::GetLocalTime(&st);

	str.Format(_T("%04d/%02d/%02d %02d:%02d:%02d: %s: OpenConnection(%s)\n"), 
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
		CString(caller), strServer);
	AtlTrace(str);
}

static inline void TraceSocketError()
{
	CString str;

	SYSTEMTIME st = {0};
	::GetLocalTime(&st);

	str.Format(_T("*************** %04d/%02d/%02d %02d:%02d:%02d: FAILED to open socket\n"), 
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	AtlTrace(str);
}

static inline void TraceSocketCreate()
{
	CString str;

	SYSTEMTIME st = {0};
	::GetLocalTime(&st);

	str.Format(_T("%04d/%02d/%02d %02d:%02d:%02d: open socket\n"), 
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	AtlTrace(str);
}
