/*++

Copyright (c) 2000-2013 ELTIMA Software All Rights Reserved

Module Name:

    setup_server.h

Abstract:

	The main module of installation and deinstallation of U2EC

Environment:

   user mode

--*/

	
#ifndef SETUP_INSTALL
#define SETUP_INSTALL

BOOL IsWow64();

int MainInstallDrivers(const std::string &token, const bool force);
int MainUninstallDrivers(const std::string &token, const bool force);

bool InstallUsbHubFilter (bool &bRestart, const bool force);
bool InstallUsbHubFilter (bool &bRestart, const bool force);

bool UnInstallUsbHubFilter (bool &bRestart);
bool CheckUsbHubFilterNeedUpdate ();

bool InstallUsbStub (bool &bRestart);
bool UnInstallUsbStub (bool &bRestart);

bool InstallVirtualUsbHub (bool &bRestart);
bool UnInstallVirtualUsbHub (bool &bRestart);
bool FindVirtualUsbHub (LPCTSTR HardwareId);

bool RestartRootHubs ();
bool AttachToUsbHubs ();
bool AttachToUsbHub (LPCTSTR HardwareId);

#endif
