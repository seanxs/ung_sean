/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    VersionInfo.hpp

Abstract:

	Ad-hoc reading of version info.

Environment:

    User mode

--*/

#pragma once

#ifndef VERSION__INFO__HPP
#define VERSION__INFO__HPP

#include <vector>

#pragma comment(lib, "version.lib")


class VersionInfo
{
public:
  static CString Get2() 
  {
    return Get(_T("\\StringFileInfo\\000904b0\\ProductVersion"));
  }

  static CString Get4()
  {
    return Get(_T("\\StringFileInfo\\000904b0\\FileVersion"));
  }

  static CString GetProductVersion()
  {
    return VersionInfo::Get2();
  }

  static CString GetFileVersion()
  {
    return VersionInfo::Get4();
  }

private:
  static CString Get(LPCTSTR id)
  {
    std::vector<TCHAR> path(_MAX_PATH);

    for (;;)
    {
      DWORD dwRes = ::GetModuleFileName(NULL, &path[0], (DWORD)path.size());

      // if 0K and result fits into buffer, breaks
      if (dwRes && dwRes < path.size())
        break;
      
      // if filename was trunkated, reallocate buffer, else fail.
      if (dwRes)
        std::vector<TCHAR>(path.size() * 2).swap(path);
      else
        return CString();
    }

    DWORD dwHandle = NULL;
    DWORD dwInfoSize = ::GetFileVersionInfoSize(&path[0], &dwHandle);

    if (0 == dwInfoSize)
      return CString();

    std::vector<BYTE> data(dwInfoSize, 0);

    if (FALSE == ::GetFileVersionInfo(&path[0], 0, dwInfoSize, &data[0]))
      return CString();

    LPVOID result = NULL;
    UINT len = 0;
    CString strID = id;
    if (FALSE == VerQueryValue(&data[0], strID.GetBuffer(), &result, &len))
      return CString();

    return CString((LPCTSTR)result);
  }
};

#endif //VERSION__INFO__HPP
