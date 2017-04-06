/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    CommandService.h

Abstract:

	Working with service class. Allows installing, uninstalling, enabling and disabling service.

Environment:

    User mode

--*/

#pragma once

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <atlstr.h>

using namespace boost;
using namespace boost::program_options;

typedef void (* FCOMMAND)(variables_map &);
typedef void (* FHELP)();

class CCommandService
{
public:

	static int InstallService ();
	static int InstallServiceEx (LPTSTR pszExtra);
	static int UninstallService ();
	static int EnableService ();
	static int DisableService ();
	static bool ParseAdditionCommand (int argc, LPTSTR* argv);
	static void Help (variables_map &vm);

private:
	static PVOID FindContext (PVOID pServer, LPCTSTR szPath, CString &strShowPath);
	static PVOID InitServer ();
	static void SharedUsbDevice (variables_map &vm);
	static void UnsharedUsbDevice (variables_map &vm);
	static void ShowTree (variables_map &vm);
	static void ShowSharedTree (variables_map &vm);
	static void ShowUsbDev (variables_map &vm);
	static void AddRemoteDevice (variables_map &vm);
	static void RemoveRemoteDevice (variables_map &vm);
	static void ConnectRemoteDevice (variables_map &vm);
	static void DisconnectRemoteDevice (variables_map &vm);
	static void ShowRemoteDevice (LPVOID lpClient, bool Rdp, bool bVerbose=false);
	static void ShowAddedRemoteDevice (variables_map &vm);
	static void ShowAddedRemoteRdpDevice (variables_map &vm);
	static void ShowRemoteDeviceOnRemoteServer (variables_map &vm);
	static void SharedManualDisconnect (variables_map &vm);
	static void UnsharedAllUsbDevice (variables_map &vm);

	static void UnsharedUsbDeviceName (LPCTSTR szDev);
	static bool RemoteDeviceExist (LPCTSTR szNetSettings);

	// help
	static void HelpSharedUsbDevice ();
	static void HelpUnsharedUsbDevice ();
	static void HelpShowTree ();
	static void HelpShowSharedTree ();
	static void HelpShowUsbDev ();
	static void HelpAddRemoteDevice ();
	static void HelpRemoveRemoteDevice ();
	static void HelpConnectRemoteDevice ();
	static void HelpDisconnectRemoteDevice ();
	static void HelpShowAddedRemoteDevice ();
	static void HelpShowAddedRemoteRdpDevice ();
	static void HelpShowRemoteDeviceOnRemoteServer ();
	static void HelpInstallService ();
	static void HelpUninstallService ();
	static void HelpEnableService ();
	static void HelpDisableService ();
	static void HelpHelp ();
	static void HelpSharedManualDisconnect ();
	static void HelpUnsharedAllUsbDevice ();
};
