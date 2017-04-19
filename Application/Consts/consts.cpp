/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    consts.cpp

Environment:

    User mode

--*/
#include "stdafx.h"
#include "consts.h"

wchar_t * g_wsPipeName = L"\\\\.\\pipe\\usb4rdppipe";

namespace Consts
{

	LPCWSTR wsRemoteControlMutexName = _T("Local\\EltimaUsbNetworkGate_MutexForRemoteControlConnection");

// message to  log
#define X(e,s) _T(s),
LPCTSTR szpLogMes [] =
{
	LOG_MESSAGES_LIST
};
#undef X

// info device
LPCTSTR szpInfoName [] = 
{
    _T("No device connected"),
};

LPCTSTR szNotDetials = _T("no details");

// remote communication
const USHORT uTcpPortConfig = 4567;
const USHORT uTcpUdp = 4568;
const USHORT uTcpControl = 4569;
const char chHub = '*';
// tree
LPCTSTR szRootHub = _T("RootHub");
LPCTSTR szEltimaHub = _T("Eltima USB Hub");
LPCTSTR szRdpHub = _T("RDP USB Hub");
LPCTSTR szFreePort = _T("Port%d: Free");
LPCTSTR szUnknowDevice = _T("Unknown device");
LPCTSTR szNotDevice = _T("Device on %s is not shared or computer is off");
LPCTSTR szTextNoConnected = _T("No remote shared USB devices are currently present in your system. \r\nTo search for available ones click \"Add remote device\" button.");
LPCTSTR szTextNoConnected2 = _T("Shared remote USB devices are filtered out. \r\nPlease, uncheck \"Filters\"-> \"Show only connected devices\" menu option to view.");

// driver
LPCTSTR szDeviceBusFilterName = _T("ELTIMA_USB_HUB_FILTER");
LPCTSTR szDeviceBusFilterDisplay = _T("Eltima usb hub filter");
LPCTSTR szDeviceBusFilterFileName = _T("fusbhub.sys");

// Config
#ifdef WIN64
const LPCTSTR szRegistry = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\ELTIMA Software\\UsbToEthernetConnector");
const LPCTSTR szRegistryHost = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\ELTIMA Software\\UsbToEthernetConnector\\Hosts");
const LPCTSTR szRegistryShare = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\ELTIMA Software\\UsbToEthernetConnector\\Server");
const LPCTSTR szRegistryConnect = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\ELTIMA Software\\UsbToEthernetConnector\\Client");
const LPCTSTR szRegistryHubName = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\ELTIMA Software\\UsbToEthernetConnector\\HubName");
#else
const LPCTSTR szRegistry = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\ELTIMA Software\\UsbToEthernetConnector");
const LPCTSTR szRegistryHost = _T("HKEY_CURRENT_USER\\SOFTWARE\\ELTIMA Software\\UsbToEthernetConnector\\Hosts");
const LPCTSTR szRegistryShare = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\ELTIMA Software\\UsbToEthernetConnector\\Server");
const LPCTSTR szRegistryConnect = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\ELTIMA Software\\UsbToEthernetConnector\\Client");
const LPCTSTR szRegistryHubName = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\ELTIMA Software\\UsbToEthernetConnector\\HubName");
#endif
const LPCTSTR szRegLogFile = _T("u2ec_log");

// AppStatico
namespace appstatico
{
int app_id = 31;
const char *app_key = "$2y$28$gUZd7KHuOqXJXt7SR1QDVs";
}

// RDP
LPCTSTR g_wsControlDeviceName = _T("\\\\.\\UHFControl");

#ifdef WIN64
const LPCTSTR szRegistry_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Eltima Software\\Usb4Rdp");
const LPCTSTR szRegistryShare_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Eltima Software\\Usb4Rdp\\Server");
const LPCTSTR szRegistryHubName_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Eltima Software\\Usb4Rdp\\HubName");
const LPCTSTR szRegistryConnect_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Eltima Software\\Usb4Rdp\\Client");
#else
const LPCTSTR szRegistry_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Eltima Software\\Usb4Rdp");
const LPCTSTR szRegistryShare_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Eltima Software\\Usb4Rdp\\Server");
const LPCTSTR szRegistryHubName_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Eltima Software\\Usb4Rdp\\HubName");
const LPCTSTR szRegistryConnect_usb4rdp = _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Eltima Software\\Usb4Rdp\\Client");
#endif
// end RDP

const LPCTSTR szAutoConnectParam = _T("Autoconnect");
const LPCTSTR szIsolation = _T("Isolation");

// Service
const LPCTSTR szServiceName = _T("UsbService");
const LPCTSTR szDisplayName = _T("Eltima USB Network Gate");

// Command service
// Command service
#define COM(e,s)  s,
const LPCTSTR szpCommand [] = 
{ 
	COMMANDS_LIST
};
#undef COM

// message string 
const LPCTSTR szpMessage [] = 
{
	_T("Service installed successfully\n"),
	_T("Error: Cannot install the service: %s\n"),
	_T("Service was uninstalled successfully\n"),
	_T("Error: Cannot uninstall the service: %s\n"),
	_T("Service is enabled\n"),
	_T("Error: Cannot enable the service: %s\n"),
	_T("Service is disabled\n"),
	_T("Error: Service cannot be disabled: %s\n"),
	_T("UsbService.exe [install] [uninstall] [enable] [disable]\n"),
	_T("Warning: USB Network Gate service does not respond to incoming requests or configuration \r\nutility could not contact the service.\r\n\r\n")
		_T("Please make sure that your firewall does not block TCP port #5474 for Usbservice.exe and UsbConfig.exe and \r\nthe registration info was entered correctly. Try to re-register the program and start service manually.\r\n\r\n")
		_T("If the problem still persists, please, contact technical support team with the detailed description of your problem."),
	_T("Cannot share specified USB port"),
	_T("You are already sharing maximum allowed number of USB devices according to your license type. Please, upgrade your license type or obtain another license, or contact us at support@eltima.com"),
	_T("You are using Demo version of USB Network Gate. By using this version you can share 1 USB device only. To extend the number of devices which you can share, please, consider upgrading to Unlimited devices license, or unshare connected devices to be able to connect another one."),
	_T("USB device license limitation"),
	_T("Demo version limitation"),
	_T("Error: Unable to share specified USB port. Unable to restart USB device."),

    _T("Error: port must have value in a range from 1000 to 65535"),
    _T("Error: password you've entered is incorrect, please, enter the password again"),
    _T("Error: the authorization checkbox is ticked, but the password is not entered, please, remove the tick from checkbox or enter the password"),
    _T("Error: this TCP port is already in use, please select another port"),
};

// Pipe
//const TCHAR chEnd = _T('!');
LPCTSTR szPipName = _T("\\\\.\\pipe\\UsbServiceControl");
LPCTSTR szPipeNameLog = _T("\\\\.\\pipe\\UsbServiceControl_log");;

#define X(e,s)  s,
LPCSTR szpPipeCommand [] = 
{
	PIPE_COMMANDS_LIST
};
#undef X

LPCSTR szpTypeItem [] = 
{
	"Server",
	"Client",
};

#define X(e,s)  s,
LPCSTR szUngNetSettings [] =
{
	UNG_NET_SETTINGS_LIST
};
#undef X

#define X(e,s)  s,
LPCSTR szUngErrorCodes [] =
{
	UNG_ERROR_CODES_LIST
};
#undef X

LPCSTR szTokens = ", !";


#define X(e,s) s,
LPCSTR szLicenseDataName [] =
{
	UNG_LICENSE_DATA_LIST
};
#undef X


#define X(e,v,k,d) _T(v),
LPCTSTR szLicenseType [] =
{
	UNG_LICENSE_TYPES_LIST
};
#undef X

#define X(e,v,k,d) _T(d),
LPCTSTR szLicenseDescription [] =
{
	UNG_LICENSE_TYPES_LIST
};
#undef X

#define X(e,n,s) _T(s),
const LPCTSTR szUngCompressionType [] =
{
	UNG_COMPRESSION_TYPES_LIST
};
#undef X

} // namespace Consts
