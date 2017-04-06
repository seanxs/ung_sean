/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    FileVersionInfo.h

Abstract:

	Helper methods to work with version info of files.

Environment:

   user mode

--*/

#pragma once

// Get FileVersion info from filename version info (if any).
bool GetVersionInfoFromFile(LPCTSTR lpFilename, std::string &strVersion);

// Compare 2 versions strings in the form X.Y.Z.N
// Return:
//  0 - versions are equal;
// -1 - versionA less than versionB
// +1 - versionA greater than versionB
int CompareVersions(const std::string &versionA, const std::string &versionB);
