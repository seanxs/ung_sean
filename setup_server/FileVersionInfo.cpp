/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    FileVersionInfo.h

Abstract:

	Helper methods to work with version info of files.

Environment:

   user mode

--*/

#include "stdafx.h"
#include <vector>
#include "FileVersionInfo.h"
#include "StringUtils.h"

#pragma comment(lib, "version.lib")

bool GetVersionInfoFromFile(LPCTSTR lpFilename, std::string &strVersion)
{
    const DWORD need_size = ::GetFileVersionInfoSize(lpFilename, NULL);
    if (!need_size) {
        return false;
    }

    std::vector<BYTE> data(need_size, 0);
    LPVOID pBlock = &data[0];

    if (::GetFileVersionInfo(lpFilename, 0, need_size, pBlock) == FALSE) {
        return false;
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;
    UINT cbTranslate = 0;

    // Read the list of languages and code pages.
    ::VerQueryValue(pBlock,
                    TEXT("\\VarFileInfo\\Translation"),
                    (LPVOID*)&lpTranslate,
                    &cbTranslate);

    size_t n_lang = cbTranslate/sizeof(struct LANGANDCODEPAGE);
    (void)n_lang;

    // get info from the very first language block
    TCHAR langBlockName[256];
    int res = _sntprintf_s(langBlockName,
                           _TRUNCATE,
                           _T("\\StringFileInfo\\%04x%04x\\FileVersion"),
                           lpTranslate[0].wLanguage,
                           lpTranslate[0].wCodePage);
    if (res < 0) {
        return false;
    }

    UINT cbVersion;
    TCHAR *lpVerInfo;
    BOOL bOk = ::VerQueryValue(pBlock,
                               langBlockName,
                               (LPVOID*)&lpVerInfo,
                               &cbVersion);

    if (!bOk) {
        return false;
    }

    strVersion = StringFromTchar(lpVerInfo);
    return true;
}

typedef union VersionParts {
    struct {
        unsigned int major;
        unsigned int minor;
        unsigned int build;
        unsigned int extra;
    } m;
    unsigned int part[4];
} VersionParts;

bool SplitVersionIntoParts(const std::string &version, VersionParts *pParts)
{
    std::vector<std::string> ta;
    size_t old_pos = 0;
    for (;;) {
        const size_t pos = version.find('.', old_pos);
        if (pos == std::string::npos) {
            // tail of the string is the last token
            std::string token = version.substr(old_pos, std::string::npos);
            if (token.empty())
                return false;
            ta.push_back(token);
            break;
        }
        if (pos > old_pos) {
            std::string token = version.substr(old_pos, pos-old_pos);
            if (token.empty())
                return false;
            ta.push_back(token);
        }
        old_pos = pos + 1;
    }

    const size_t n = ta.size();
    if (n > 4)
        return false;

    for (size_t i=0; i<4; ++i) {
        if (i < n) {
            int x = ::atoi(ta[i].c_str());
            if (x < 0)
                return false;
            pParts->part[i] = (unsigned)x;
        }
        else
            pParts->part[i] = 0;
    }

    return true;
}

int CompareVersions(const std::string &versionA, const std::string &versionB)
{
    if (versionA == versionB)
        return 0;

    VersionParts vpa = {0,0,0,0};
    VersionParts vpb = {0,0,0,0};

    SplitVersionIntoParts(versionA, &vpa);
    SplitVersionIntoParts(versionB, &vpb);

    for (int i=0; i<4; ++i) {
        if (vpa.part[i] == vpb.part[i])
            continue;
        if (vpa.part[i] < vpb.part[i])
            return -1;
        else
            return +1;
    }

    return 0;
}
