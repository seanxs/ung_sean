/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    StringUtils.h

Abstract:

	Helper methods to work with [unicode] strings.

Environment:

   user mode

--*/

#pragma once

// convert TCHAR buffer to std::string
// (using utf-8 encoding (if UNICODE) or as-is simple bytestring).
std::string StringFromTchar(const TCHAR *pbuf);

// convert std::string to TCHAR
// (using utf-8 encoding (if UNICODE) or as-is simple bytestring).
// Parameter pBufSize - number of characters (not bytes).
bool StringToTchar(const std::string &str, TCHAR *pBuf, size_t *pBufSize /*in/out*/);
