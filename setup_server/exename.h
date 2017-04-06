/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    exename.h

Abstract:

	Get name of executable file and extract token from the name.
	Token here is the last part of name after last underscore.
	(E.g. for name "setup_server_foobar.exe" token is "foobar").

Environment:

   user mode

--*/

#pragma once

bool GetExeName(LPTSTR buf, const DWORD buf_size);

bool GetTokenFromExeName(LPTSTR buf, const DWORD buf_size);
bool GetTokenFromExeNameAsStr(std::string &result);

bool GetExeDir(CString *pStr);
bool GetSystem32Dir(CString *pStr);

CString JoinPath(const CString &dirname, const CString &relpath);
