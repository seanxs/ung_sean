/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    StringUtils.cpp

Abstract:

	Helper methods to work with [unicode] strings.

Environment:

   user mode

--*/

#include "stdafx.h"
#include "StringUtils.h"

std::string StringFromTchar(const TCHAR *pBuf)
{
#ifdef UNICODE

    std::string result;
    const int need_size = ::WideCharToMultiByte(CP_UTF8, 0, pBuf, -1, NULL, 0, NULL, NULL);
    if (need_size) {
        char *pUtf8 = new char[(size_t)need_size];
        const int real_size = ::WideCharToMultiByte(CP_UTF8, 0, pBuf, -1, pUtf8, need_size, NULL, NULL);
        if (real_size)
            result = std::string(pUtf8);
        delete[] pUtf8;
    }

    return result;

#else

    return std::string(pBuf);

#endif
}

// pBufSize - buffer size in characters.
bool StringToTchar(const std::string &str, TCHAR *pBuf, size_t *pBufSize /*in/out*/)
{
    if (!pBuf || !pBufSize)
        return false;

    bool bRet = true;

    if (str.empty()) {
        if (*pBufSize != 0)
            pBuf[0] = 0;
        else
            bRet = false;
        *pBufSize = (size_t)1;
        return true;
    }

#ifdef UNICODE

    // convert bytes to unicode
    const char *pUtf8 = str.c_str();
    const int need_size = ::MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, NULL, 0);
    if (!need_size)
        return false;
    if (*pBufSize >= (size_t)need_size) {
        const int real_size = ::MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, pBuf, (int)*pBufSize);
        if (!real_size)
            return false;
    }
    else
        bRet = false;

    *pBufSize = (size_t)need_size;
    return bRet;

#else

    // copy bytes
    size_t need_size = str.size();
    if (str[need_size-1] != 0)
        need_size += 1;

    if (*pBufSize >= need_size) {
        if (_tcscpy_s(pBuf, *pBufSize, str.c_str()) != 0) {
            // copy error
            return false;
        }
    }
    else
        bRet = false;

    *pBufSize = need_size;
    return bRet;

#endif
}
