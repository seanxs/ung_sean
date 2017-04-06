/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    exename.cpp

Abstract:

	Get name of executable file and extract token from the name.
	Token here is the last part of name after last underscore.
	(E.g. for name "setup_server_foobar.exe" token is "foobar").

Environment:

   user mode

--*/

#include "stdafx.h"
#include <Shlwapi.h>
#include "StringUtils.h"


bool GetExeName(LPTSTR buf, const DWORD buf_size)
{
    if (!buf || !buf_size)
        return false;

    // inner buffer to get full name
    TCHAR inner_buf[1024];
    const DWORD inner_size = sizeof(inner_buf) / sizeof(TCHAR);

    const DWORD result = ::GetModuleFileName(NULL, inner_buf, inner_size);
    if (!result) {
        //printf("GetModuleFileName failed with error.\n");
        return false;
    }

    if (result == inner_size) {
        inner_buf[inner_size-1] = 0;
    }

    // remove absolute path
    ::PathStripPath(inner_buf);

    const size_t required_size = _tcsnlen(inner_buf, inner_size);
    if (required_size >= inner_size) {
        // error - no terminated nul
        return false;
    }

    if (buf_size < (required_size+1)) {
        // not enough space in buf to store result
        return false;
    }

    if (_tcscpy_s(buf, buf_size, inner_buf) != 0) {
        // copy error
        return false;
    }

    return true;
}

bool GetTokenFromExeName(LPTSTR buf, const DWORD buf_size)
{
    if (!buf || !buf_size)
        return false;

    // inner buffer to get exe name
    TCHAR inner_buf[1024];
    const DWORD inner_size = sizeof(inner_buf) / sizeof(TCHAR);

    if (!GetExeName(inner_buf, inner_size))
        return false;

    // extract token
    ::PathRemoveExtension(inner_buf);
    PTSTR pLast_ = _tcsrchr(inner_buf, _T('_'));
    if (!pLast_) {
        // underscore not found
        return false;
    }

    ++ pLast_;

    const size_t required_size = _tcsnlen(pLast_, inner_size-1);

    if (buf_size < (required_size+1)) {
        // not enough space in buf to store result
        return false;
    }

    if (_tcscpy_s(buf, buf_size, pLast_) != 0) {
        // copy error
        return false;
    }

    return true;
}

bool GetTokenFromExeNameAsStr(std::string &result)
{
    // inner buffer to get exe name
    TCHAR inner_buf[1024];
    const DWORD inner_size = sizeof(inner_buf) / sizeof(TCHAR);

	if (!GetTokenFromExeName(inner_buf, inner_size))
		return false;

	result = StringFromTchar(inner_buf);
	return true;
}

bool GetExeDir(CString *pStr)
{
	// inner buffer to get full name
	TCHAR inner_buf[1024];
	const DWORD inner_size = sizeof(inner_buf) / sizeof(TCHAR);

	const DWORD result = ::GetModuleFileName(NULL, inner_buf, inner_size);
	if (!result) {
		//printf("GetModuleFileName failed with error.\n");
		return false;
	}

	if (result == inner_size) {
		inner_buf[inner_size-1] = 0;
	}

	// remove filename and only keep absolute path to directory
	if (!::PathRemoveFileSpec(inner_buf)) {
		// something wrong
		return false;
	}

	*pStr = CString(inner_buf);
	return true;
}

bool GetSystem32Dir(CString *pStr)
{
	TCHAR inner_buf[1024];
	const DWORD inner_size = sizeof(inner_buf) / sizeof(TCHAR);

	UINT result = ::GetSystemDirectory(inner_buf, inner_size);
	if (!result)
		return false;

	if (result > inner_size) {
		_tprintf (_T("GetSystem32Dir failed: required buffer of size %d\n"), (int)result);
	}

	*pStr = CString(inner_buf);
	return true;
}

CString JoinPath(const CString &dirname, const CString &relpath)
{
	if (dirname.IsEmpty())
		return relpath;

	if (relpath.IsEmpty())
		return dirname;

	CString result = dirname;

	if (result.Right(1) != _T("\\"))
		result += _T("\\");

	if (relpath.Left(1) == _T("\\"))
		result += relpath.Mid(1);
	else
		result += relpath;

	return result;
}
