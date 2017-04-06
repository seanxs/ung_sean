/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    CommandService.cpp

Abstract:

	Working with service class. Allows installing, uninstalling, enabling and disabling service.

Environment:

    User mode

--*/

#include "stdafx.h"
#include "CommandService.h"
#include "Manager.h"
#include "Consts\consts.h"
#include "Consts\ManagerService.h"

#define U2EC_EXPORTS_STATIC
#include "u2ec\dllmain.h"

using namespace boost;
using namespace boost::program_options;

// from u2ec\u2ec.cpp
void CloseSocketToUsbService ();

namespace 
{
	void PrintAsUtf8 (const CString &str)
	{
		const CStringA utf8 = CT2CA (str, CP_UTF8);
		printf (utf8);
	}

	void PrintAsUtf8 (const char *fmt, const CString &str)
	{
		const CStringA utf8 = CT2CA (str, CP_UTF8);
		printf (fmt, utf8);
	}
}


int CCommandService::InstallService ()
{
	CManagerService	manager;
	if (!manager.CreateService(Consts::szServiceName, Consts::szDisplayName))
	{
		return GetLastError ();
	}

	return 0;
}

int CCommandService::InstallServiceEx (LPTSTR pszExtra)
{
	const int iRet = CCommandService::InstallService ();
	if (iRet != 0)
		return iRet;

#if defined(ACTIVATOR)
	if (pszExtra != NULL) 
	{
		if (_tcscmp (pszExtra, _T(INSTALL_MAGIC_CODE)) == 0) 
		{
			eltima::ung::CProtector::InstallStorage();
		}
	}
#else
	(void)pszExtra;
#endif

	return iRet;
}


int CCommandService::UninstallService ()
{
	CManagerService	manager;
	if (!manager.DeleteService (Consts::szServiceName))
	{
		return GetLastError ();
	}

	return 0;
}

int CCommandService::EnableService ()
{
	CManagerService	manager;
	if (!manager.StartService (Consts::szServiceName))
	{
		return GetLastError ();
	}

	if (!manager.SetServiceStartType (Consts::szServiceName, SERVICE_AUTO_START))
	{
		return GetLastError ();
	}

	return 0;
}

int CCommandService::DisableService ()
{
	CManagerService	manager;
	if (!manager.StopService (Consts::szServiceName))
	{
		return GetLastError ();
	}

	if (!manager.SetServiceStartType (Consts::szServiceName, SERVICE_DEMAND_START))
	{
		return GetLastError ();
	}

	return 0;
}

bool CCommandService::ParseAdditionCommand (int argc, LPTSTR* argv)
{
	bool bRet = true;

	std::map <CString, FCOMMAND > mapCommand;

	// map command
	mapCommand [Consts::szpCommand [Consts::SC_SHARE] ] = SharedUsbDevice;
	mapCommand [Consts::szpCommand [Consts::SC_UNSHARE] ] = UnsharedUsbDevice;
	mapCommand [Consts::szpCommand [Consts::SC_SHOW_TREE] ] = ShowTree;
	mapCommand [Consts::szpCommand [Consts::SC_SHOW_SHARED] ] = ShowSharedTree;
	mapCommand [Consts::szpCommand [Consts::SC_SHOW_USB_DEV] ] = ShowUsbDev;
	mapCommand [Consts::szpCommand [Consts::SC_ADD_CLIENT] ] = AddRemoteDevice;
	mapCommand [Consts::szpCommand [Consts::SC_REMOTE_CLIENT] ] = RemoveRemoteDevice;
	mapCommand [Consts::szpCommand [Consts::SC_CONNECT] ] = ConnectRemoteDevice;
	mapCommand [Consts::szpCommand [Consts::SC_DISCONNECT] ] = DisconnectRemoteDevice;
	mapCommand [Consts::szpCommand [Consts::SC_SHOW_REMOTE] ] = ShowAddedRemoteDevice;
	mapCommand [Consts::szpCommand [Consts::SC_SHOW_RDP_REMOTE] ] = ShowAddedRemoteRdpDevice;
	mapCommand [Consts::szpCommand [Consts::SC_FIND_CLIENT] ] = ShowRemoteDeviceOnRemoteServer;
	mapCommand [Consts::szpCommand [Consts::SC_HELP] ] = Help;
	mapCommand [Consts::szpCommand [Consts::SC_SERV_MANUAL_STOP] ] = SharedManualDisconnect;
	mapCommand [Consts::szpCommand [Consts::SC_UNSHARE_ALL] ] = UnsharedAllUsbDevice;
	
	// parse param
    options_description options("options");
	options.add_options()
		("command", wvalue <std::wstring> ())
		("param", wvalue <std::wstring> ())
		("tcp", wvalue<std::wstring>())
		("crypt", "")
		("auth", wvalue<std::wstring>())
		("desc", wvalue<std::wstring>())
		("auto-reconnect", "")
		("compress", "")
		("compress-type", wvalue<std::wstring>())
		("verbose", "")
		;

    positional_options_description p;
    p.add("command", 1);
	p.add("param", 1);

	try
	{
		variables_map vm;

		_tprintf (_T("USB Network Gate\n\n"));

		wparsed_options parsed = wcommand_line_parser(argc, argv).options(options).positional(p).run();
		store(parsed, vm);

		if (vm.count ("command"))
		{
			CString strCommand (vm ["command"].as <std::wstring> ().c_str ());
			std::map <CString, FCOMMAND >::iterator item;

			strCommand.MakeLower ();

			item = mapCommand.find (strCommand);
			if (item == mapCommand.end ())
			{
				Help (vm);
			}
			else
			{
				item->second (vm);
			}
		}
		else
		{
			bRet = false;
		}

	}
	catch (CString &str)
	{
		printf ("\nError: %s\n", CStringA (str));
		bRet = false;
	}
	catch (...)
	{
		variables_map vm;
		Help (vm);
		return false;
	}

	CloseSocketToUsbService ();

	return bRet;
}

// help
void CCommandService::HelpSharedUsbDevice ()
{
	_tprintf (_T("UsbService.exe share-usb-port usb-port [--tcp=param] [--desc=param] [--auth=pasw] [--crypt] [--compress] [--compress-type=N]\n\n"));
	_tprintf (_T("Share a USB port (USB device)\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("usb-port", "The number of USB port that will be shared. It can be derived with show-usb-list command.")
		("tcp", "The parameters of TCP connection. If only the TCP port number is indicated, then the server "
				"will wait for incoming connection from the client PC. If the full network path is indicated then the server "
				"will initiate the connection with the client PC.; e.g.: 3777 - waiting for connection on port 3777; "
				"localhost:3777 - server will try to connect to a client on localhost:3777." )
		("desc", "The additional description of the device.")
		("auth", "Indicates the password for connection with this device.")
		("crypt", "Indicates that all traffic will be encrypted.")
		("compress", "Indicates that traffic will be compressed.")
		("compress-type", "Selects desired traffic compression method (1=best size, 2=best speed)")
		;

	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --usb-port", "    usb-port");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe shared-usb-port 1:1 --tcp=3777 --desc=\"My usb flash\" --auth=\"123\" --crypt\n\n"));
	_tprintf (_T("I.e. the first USB port will be shared on first USB hub; the connection will require authorization; server will wait for "));
	_tprintf (_T("connection on TCP port 3777; \"My usb flash\" is additional description; all traffic is encrypted.\n"));
}

void CCommandService::HelpUnsharedUsbDevice ()
{
	_tprintf (_T("UsbService.exe unshare-usb-port usb-port\n\n"));
	_tprintf (_T("Make a port available to the local system\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("usb-port", "USB port number that will be unshared. Shared devices list can be derived with show-shared-usb command.");

	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --usb-port", "    usb-port");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe unshare-usb-port 1:1\n\n"));
	_tprintf (_T("USB port 1 on the first hub will become available to the local system.\n"));
}

void CCommandService::HelpShowTree ()
{
	_tprintf (_T("UsbService.exe show-usb-list\n\n"));
	_tprintf (_T("Display the USB ports list\n\n"));
}

void CCommandService::HelpShowSharedTree ()
{
	_tprintf (_T("UsbService.exe show-shared-usb\n\n"));
	_tprintf (_T("Display the list of shared USB ports (devices)\n\n"));
}

void CCommandService::HelpShowUsbDev ()
{
	_tprintf (_T("UsbService.exe show-usb-dev usb-port\n\n"));
	_tprintf (_T("Display detailed info about USB device on the specified USB port.\n\n"));
}

void CCommandService::HelpUnsharedAllUsbDevice ()
{
	_tprintf (_T("UsbService.exe unshare-all-usb-ports\n\n"));
	_tprintf (_T("Unshare all ports (devices) on server and make them available on this computer again\n\n"));
}

void CCommandService::HelpAddRemoteDevice ()
{
	_tprintf (_T("UsbService.exe add-remote-device tcp\n\n"));
	_tprintf (_T("Add the remote device\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("tcp", "The parameters of TCP connection. If the full network path is indicated then the client PC "
				"will initiate the connection with the server. If only the port number is indicated, then the "
				"client PC will wait for incoming connection from the server. E.g.: 3777 - waiting for connection "
				"on port 3777; localhost:3777 - client will try to connect to a server on localhost:3777." );

	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --tcp", "    tcp");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe add-remote-device localhost:3777\n\n"));
	_tprintf (_T("Adds the remote shared device on the client via localhost on TCP port 3777.\n"));
}

void CCommandService::HelpRemoveRemoteDevice ()
{
	_tprintf (_T("UsbService.exe remove-remote-device tcp\n\n"));
	_tprintf (_T("Remove remote device on client\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("tcp", "Parameters of the TCP connection that were indicated during creation of connection with remote device." );

	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --tcp", "    tcp");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe remove-remote-device localhost:3777\n"));
	_tprintf (_T("Removes remote device from the client, which connected to the server PC \"localhost\" on TCP port 3777.\n"));
}

void CCommandService::HelpConnectRemoteDevice ()
{
	_tprintf (_T("UsbService.exe connect-remote-device tcp [--auth=passw] [--auto-reconnect]\n\n"));
	_tprintf (_T("Initiate the TCP connection with the remote device. The list of shared remote devices that were added on a client PC and their connection status can be invoked by \"show-remote-devices\" command.\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("tcp", "Parameters of the TCP connection that were indicated during creation of connection with remote device." )
		("auth", "Indicates that the connection will require authentication")
		("auto-reconnect", "Automatically restore the connection after it was lost");


	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --tcp", "    tcp");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe connect-remote-device localhost:3777 --auth=\"123\" --auto-reconnect\n"));
	_tprintf (_T("Initiates the connection with the remote server PC \"localhost\" on TCP port 3777 with authentication. And if the connection was lost tries to reestablish connection with the server.\n"));
}
void CCommandService::HelpDisconnectRemoteDevice ()
{
	_tprintf (_T("UsbService.exe disconnect-remote-device tcp\n\n"));
	_tprintf (_T("Interrupt the connection with the remote device. The list of shared remote devices that were added on a client PC and their connection status can be invoked by \"show-remote-devices\" command.\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("tcp", "Parameters of the TCP connection that were indicated during creation of connection with remote device." );

	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --tcp", "    tcp");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe disconnect-remote-device localhost:3777\n\n"));
	_tprintf (_T("Interrupt the connection with the remote device on server \"localhost\" on TCP port 3777\n"));
}

void CCommandService::HelpShowAddedRemoteDevice ()
{
	_tprintf (_T("UsbService.exe show-remote-devices [--verbose]\n\n"));
	_tprintf (_T("Show all added remote devices and their parameters and connection statuses\n\n"));
}

void CCommandService::HelpShowAddedRemoteRdpDevice ()
{
	_tprintf (_T("UsbService.exe show-remote-rdp-devices\n\n"));
	_tprintf (_T("Show all added remote devices that are available over RDP\n\n"));
}

void CCommandService::HelpShowRemoteDeviceOnRemoteServer ()
{
	_tprintf (_T("UsbService.exe find-remote-devices server [--verbose]\n\n"));
	_tprintf (_T("Shared remote devices lookup on server\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("server", "IP address or DNS name of the server PC to look for shared remote devices at" )
		("verbose", "Show more information about devices")
		;

	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --server", "    server");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe find-remote-devices localhost\n"));
	_tprintf (_T("Look for all shared remote devices on server PC \"localhost\"\n"));
}

void CCommandService::HelpHelp ()
{
#ifndef DEBUG
    TCHAR Buf[255];
    GetEnvironmentVariable(_T("USB_KEY_REG"), Buf, 100);
	//MessageBox (NULL, Buf, _T("1"), MB_OK);
	if (_tcsicmp (Buf, _T("oem")) == 0)
	{
#endif
		_tprintf (_T("Service commands:\n"));
		_tprintf (_T("               install\n"));
		_tprintf (_T("               uninstall\n"));
		_tprintf (_T("               enable\n"));
		_tprintf (_T("               disable\n\n"));
#ifndef DEBUG
	}
#endif
	_tprintf (_T("Server commands:\n"));
	_tprintf (_T("               share-usb-port usb-port [--tcp] [--desc] [--auth] [--crypt] [--compress]\n"));
	_tprintf (_T("               unshare-usb-port usb-port\n"));
	_tprintf (_T("               show-usb-list\n"));
	_tprintf (_T("               show-shared-usb\n"));
	_tprintf (_T("               show-usb-dev usb-port\n"));
	_tprintf (_T("               shared-usb-disconnect usb-port\n"));
	_tprintf (_T("               unshare-all-usb-ports\n\n"));
	_tprintf (_T("Client commands:\n"));
	_tprintf (_T("               add-remote-device tcp\n"));
	_tprintf (_T("               remove-remote-device tcp\n"));
	_tprintf (_T("               connect-remote-device tcp [--auth] [--auto-reconnect]\n"));
	_tprintf (_T("               disconnect-remote-device tcp\n"));
	_tprintf (_T("               show-remote-devices [--verbose]\n"));
	_tprintf (_T("               show-remote-rdp-devices\n"));
	_tprintf (_T("               find-remote-devices server [--verbose]\n\n"));
	_tprintf (_T("help:\n"));
	_tprintf (_T("UsbService.exe help [command]\n"));
}
void CCommandService::HelpInstallService ()
{
	_tprintf (_T("\tUsbService.exe install\n\n"));
	_tprintf (_T("Install USB Network Gate service on your system\n\n"));
}
void CCommandService::HelpUninstallService ()
{
	_tprintf (_T("UsbService.exe uninstall\n\n"));
	_tprintf (_T("Uninstall USB Network Gate service from your system\n\n"));
}

void CCommandService::HelpEnableService ()
{
	_tprintf (_T("UsbService.exe enable\n\n"));
	_tprintf (_T("Enable USB Network Gate service\n\n"));
}
void CCommandService::HelpDisableService ()
{
	_tprintf (_T("UsbService.exe disable\n\n"));
	_tprintf (_T("Disable USB Network Gate service\n\n"));
}

void CCommandService::HelpSharedManualDisconnect ()
{
	_tprintf (_T("UsbService.exe shared-usb-disconnect usb-port\n\n"));
	_tprintf (_T("Interrupt current connection for this device\n\n"));

	options_description Service("Parameters");

	Service.add_options()
		("usb-port", "USB port that needs to be disconnected. Shared devices list and their connection statuses can be derived with show-shared-usb command.");

	// services
	{
		std::stringstream Stream;
		CStringA str;
		Stream << Service;
		str = Stream.str ().c_str ();
		str.Replace ("  --usb-port", "    usb-port");
		printf ("%s\n", str);
	}

	_tprintf (_T("\nExample:\n"));
	_tprintf (_T("UsbService.exe shared-usb-disconnect 1:1\n\n"));
	_tprintf (_T("The connection with USB port 1 on the first hub will be interrupted.\n"));
}

void CCommandService::Help (variables_map &vm)
{
	CString	szParam;
	bool bRet = true;

	std::map <CString, FHELP > mapHelp;
	
	if (vm.count ("param") != 0)
	{
		std::map <CString, FHELP >::iterator item;
		szParam = vm ["param"].as <std::wstring> ().c_str ();

		// map command
		mapHelp [Consts::szpCommand [Consts::SC_SHARE] ] = HelpSharedUsbDevice;
		mapHelp [Consts::szpCommand [Consts::SC_UNSHARE] ] = HelpUnsharedUsbDevice;
		mapHelp [Consts::szpCommand [Consts::SC_SHOW_TREE] ] = HelpShowTree;
		mapHelp [Consts::szpCommand [Consts::SC_SHOW_SHARED] ] = HelpShowSharedTree;
		mapHelp [Consts::szpCommand [Consts::SC_SHOW_USB_DEV] ] = HelpShowUsbDev;
		mapHelp [Consts::szpCommand [Consts::SC_ADD_CLIENT] ] = HelpAddRemoteDevice;
		mapHelp [Consts::szpCommand [Consts::SC_REMOTE_CLIENT] ] = HelpRemoveRemoteDevice;
		mapHelp [Consts::szpCommand [Consts::SC_CONNECT] ] = HelpConnectRemoteDevice;
		mapHelp [Consts::szpCommand [Consts::SC_DISCONNECT] ] = HelpDisconnectRemoteDevice;
		mapHelp [Consts::szpCommand [Consts::SC_SHOW_REMOTE] ] = HelpShowAddedRemoteDevice;
		mapHelp [Consts::szpCommand [Consts::SC_SHOW_RDP_REMOTE] ] = HelpShowAddedRemoteRdpDevice;
		mapHelp [Consts::szpCommand [Consts::SC_FIND_CLIENT] ] = HelpShowRemoteDeviceOnRemoteServer;
		mapHelp [Consts::szpCommand [Consts::SC_SERV_MANUAL_STOP] ] = HelpSharedManualDisconnect;
		mapHelp [Consts::szpCommand [Consts::SC_HELP] ] = HelpHelp;
		mapHelp [Consts::szpCommand [Consts::SC_UNSHARE_ALL] ] = HelpUnsharedAllUsbDevice;

		szParam.MakeLower ();
		item = mapHelp.find (szParam);
		if (item != mapHelp.end ())
		{
			item->second ();
			return;
		}
	}
	
	HelpHelp ();
}


PVOID CCommandService::InitServer ()
{	
	PVOID pServer;
	if (!ServerCreateEnumUsbDev (&pServer))
	{
		throw CString (_T("Could not enumerate USB devices on server"));
	}

	return pServer;
}

PVOID CCommandService::FindContext (PVOID pServer, LPCTSTR szPath, CString &strShowPath)
{
	CString str (szPath);
	CString strTemp;
	int iPos = 0;
	int iNumber;
	LPVOID pHub = pServer;
	LPVOID pDevHandle = NULL;
	CComVariant		Desk;

	if (str.IsEmpty ())
	{
		throw CString (_T("Could not find USB device"));
	}

	while (iPos < str.GetLength ())
	{
		strTemp = str.Tokenize (_T(":"), iPos);
		if (strTemp.IsEmpty ())
		{
			throw CString (_T("Could not find USB device"));
		}
		iNumber = _ttoi (strTemp);
		if (iNumber < 0 || iNumber > 128)
		{
			throw CString (_T("Could not find USB device"));
		}
		if (!ServerUsbDevIsHub(pServer, pHub))
		{
			throw CString (_T("Could not find USB device"));
		}
	
		int index;
		for (index = 0; ServerGetUsbDevFromHub(pServer, pHub, index, &pDevHandle); ++index)
		{
			if (index == (iNumber - 1))		
			{
				break;
			}
		}

		if (pDevHandle)
		{
			ServerGetUsbDevName (pServer, pDevHandle, &Desk);
			strShowPath += strShowPath.GetLength ()?(_T("\\") + CString (Desk)):CString (Desk);
			if (index != (iNumber - 1))
			{
				throw CString (_T("Could not find USB device"));
			}
			pHub = pDevHandle;
		}
	}

	if (pDevHandle && ServerUsbDevIsHub(pServer, pDevHandle))
	{
		throw CString (_T("Could not find USB device"));
	}


	return pDevHandle;
}

void CCommandService::SharedUsbDevice (variables_map &vm)
{
	PVOID pServer;
	PVOID pDevice;
	CString strDesk;
	CString strDev;
	CString strDesc;
	CString srtAuth;
	CString strTcp;
	CString strPath;
	bool	bCrypt;
	bool	bCompress;
	int		iCompressType = Consts::UCT_COMPRESSION_DEFAULT;
	//int		iCompressType = Consts::UCT_COMPRESSION_NONE;

	if (vm.count ("param") == 0)
	{
		throw CString (_T("No device was specified for sharing"));
	}
	strDev = vm ["param"].as <std::wstring> ().c_str ();
	strTcp = vm.count ("tcp")?CString (vm ["tcp"].as <std::wstring> ().c_str ()):_T("");
	strDesc = vm.count ("desc")?CString (vm ["desc"].as <std::wstring> ().c_str ()):_T("");
	srtAuth = vm.count ("auth")?CString (vm ["auth"].as <std::wstring> ().c_str ()):_T("");
	bCrypt = vm.count ("crypt")?true:false;
	bCompress = vm.count ("compress")?true:false;
	if (vm.count ("compress-type"))
	{
		std::wstring strType (vm ["compress-type"].as <std::wstring> ());
		int n = _wtoi (strType.c_str ());
		if (n > 0 && n < Consts::UNG_COMPRESSION_TYPES_MAX)
		{
			iCompressType = n;
			bCompress = true;
		}
		else if (n == 0 && strType.compare (_T("0")) == 0)
		{
			bCompress = false;			
		}
		else
		{
			throw CString (_T("Wrong --compress-type value"));
		}
	}

	pServer = InitServer ();

	try
	{
		pDevice = FindContext (pServer, strDev, strPath);
		if (pDevice)
		{
			if (ServerUsbDevIsShared (pServer, pDevice))
			{
				throw CString (_T("The device is already shared"));
			}
			PrintAsUtf8 ("Sharing device: %s\n", strPath);
			if (ServerShareUsbDev (pServer, pDevice, CComVariant (strTcp), CComVariant (strDesc), 
								   _tcslen (srtAuth)?true:false, CComVariant (srtAuth), bCrypt, bCompress))
			{
				if (bCompress && iCompressType != Consts::UCT_COMPRESSION_DEFAULT)
				{
					CComVariant varName (Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE]);
					CComVariant varValue (iCompressType);
					ServerSetUsbDevValueByName (pServer, pDevice, varName, varValue);
				}

				PrintAsUtf8 ("The device \"%s\" was shared successfully\n", strPath);
			}
			else
			{
				CStringA strErrorCode;
				CComVariant varErrorCode;
				if (/*dllmain*/::ServerGetLastError (pServer, &varErrorCode))
				{
					strErrorCode = CStringA (varErrorCode);
				}

				throw CString ("Could not share specified USB device. Unable to restart USB device.");
			}
		}
		else
		{
			throw CString ("Could not share specified USB device. USB device was not found.");
		}
	}
	catch (...)
	{
		ServerRemoveEnumUsbDev(pServer);
		throw;
	}

	ServerRemoveEnumUsbDev(pServer);
}

void CCommandService::UnsharedAllUsbDevice (variables_map &vm)
{
	PVOID					pServer, pDevHandle;
	PVOID					pCur;
	CString					strTemp;
	std::vector <PVOID>		vecTree;
	std::vector <int>		vecIndex;
	int						iBegin;
	CComVariant				Desk;
	bool					bEmpty = true;

	pServer = InitServer ();

	vecTree.push_back (pServer);
	vecIndex.push_back (0);


	while (vecTree.size ())
	{
		pCur = vecTree.back ();
		iBegin = vecIndex.back ();

		vecTree.pop_back ();
		vecIndex.pop_back ();

		for (int index = iBegin; ServerGetUsbDevFromHub(pServer, pCur, index, &pDevHandle); ++index)
		{
			ServerGetUsbDevName (pServer, pDevHandle, &Desk);

			if (ServerUsbDevIsHub (pServer, pDevHandle))
			{
				vecTree.push_back (pCur);
				vecIndex.push_back (index + 1);

				vecTree.push_back (pDevHandle);
				vecIndex.push_back (0);
				break;
			}
			else
			{
				if (ServerUsbDevIsShared (pServer, pDevHandle))
				{
					CString strName;
					std::vector <int>::iterator item;

					for (item = vecIndex.begin (); item != vecIndex.end (); item++)
					{
						strName.AppendFormat (_T("%s%d"), (item == vecIndex.begin ())?_T(""):_T(":"), *item);
					}
					strName.AppendFormat (_T(":%d"), index + 1);

					try
					{
						UnsharedUsbDeviceName (strName);
					}
					catch (...)
					{
						PrintAsUtf8 ("Could not unshare the device %s \n", strName);
					}
				}
			}
		}
	}

	ServerRemoveEnumUsbDev(pServer);}

void CCommandService::UnsharedUsbDevice (variables_map &vm/*LPCTSTR Dev*/)
{
	if (vm.count ("param") == 0)
	{
		throw CString (_T("No device was specified for unsharing"));
	}

	const CString strDev = vm ["param"].as <std::wstring> ().c_str ();

	UnsharedUsbDeviceName (strDev);
}

void CCommandService::UnsharedUsbDeviceName (LPCTSTR strDev)
{
	PVOID pServer;
	PVOID pDevice;
	CString strPath;

	pServer = InitServer ();

	pDevice = FindContext (pServer, strDev, strPath);
	try
	{
		if (pDevice)
		{
			if (!ServerUsbDevIsShared (pServer, pDevice))
			{
				throw CString (_T("This device is not shared\n"));
			}
			PrintAsUtf8 ("Unsharing device: %s \n", strPath);
			if (ServerUnshareUsbDev (pServer, pDevice))
			{
				PrintAsUtf8 ("The device \"%s\" was unshared successfully \n\n", strPath);
			}
			else
			{
				throw CString ("Please make sure the connection with the server PC is active and GUI is not running.\n\n");
			}
		}
	}
	catch (...)
	{
		ServerRemoveEnumUsbDev(pServer);
		throw;
	}

	ServerRemoveEnumUsbDev(pServer);
}

void CCommandService::ShowTree (variables_map &vm)
{
	PVOID					pServer, pDevHandle;
	PVOID					pCur;
	CString					strTemp;
	std::vector <PVOID>		vecTree;
	std::vector <int>		vecIndex;
	int						iBegin;
	CComVariant				Desk;

	pServer = InitServer ();

	vecTree.push_back (pServer);
	vecIndex.push_back (0);


	while (vecTree.size ())
	{
		pCur = vecTree.back ();
		iBegin = vecIndex.back ();

		vecTree.pop_back ();
		vecIndex.pop_back ();

		for (int index = iBegin; ServerGetUsbDevFromHub(pServer, pCur, index, &pDevHandle); ++index)
		{
			ServerGetUsbDevName (pServer, pDevHandle, &Desk);
			for (size_t a = 0; a < vecTree.size (); a++)
			{
				printf ("    ");
			}
			PrintAsUtf8 (CString (Desk));

			if (ServerUsbDevIsHub (pServer, pDevHandle))
			{
				vecTree.push_back (pCur);
				vecIndex.push_back (index + 1);

				vecTree.push_back (pDevHandle);
				vecIndex.push_back (0);
				_tprintf (_T("\n"));
				break;
			}
			else
			{
				std::vector <int>::iterator item;

				printf ("\t- ");

				for (item = vecIndex.begin (); item != vecIndex.end (); item++)
				{
					_tprintf (_T("%s%d"), (item == vecIndex.begin ())?_T(""):_T(":"), *item);
				}
				_tprintf (_T(":%d"), index + 1);
				_tprintf (_T("\n"));
			}
		}
	}

	ServerRemoveEnumUsbDev(pServer);
}

void CCommandService::ShowSharedTree (variables_map &vm)
{
	PVOID					pServer, pDevHandle;
	PVOID					pCur;
	CString					strTemp;
	std::vector <PVOID>		vecTree;
	std::vector <int>		vecIndex;
	int						iBegin;
	CComVariant				Desk;
	bool					bEmpty = true;

	pServer = InitServer ();

	vecTree.push_back (pServer);
	vecIndex.push_back (0);


	while (vecTree.size ())
	{
		pCur = vecTree.back ();
		iBegin = vecIndex.back ();

		vecTree.pop_back ();
		vecIndex.pop_back ();

		for (int index = iBegin; ServerGetUsbDevFromHub(pServer, pCur, index, &pDevHandle); ++index)
		{
			ServerGetUsbDevName (pServer, pDevHandle, &Desk);

			if (ServerUsbDevIsHub (pServer, pDevHandle))
			{
				vecTree.push_back (pCur);
				vecIndex.push_back (index + 1);

				vecTree.push_back (pDevHandle);
				vecIndex.push_back (0);
				break;
			}
			else
			{
				if (ServerUsbDevIsShared (pServer, pDevHandle))
				{
					LONG state = 0;
					BOOL	bEnable;
					CComVariant Host, NetSettings;

					bEmpty = false;
					PrintAsUtf8 (CString (Desk));
					_tprintf (_T("\t-"));

					std::vector <int>::iterator item;
					for (item = vecIndex.begin (); item != vecIndex.end (); item++)
					{
						_tprintf (_T("%s%d"), (item == vecIndex.begin ())?_T(""):_T(":"), *item);
					}
					_tprintf (_T(":%d"), index + 1);
					_tprintf (_T("\t-"));

					ServerGetSharedUsbDevIsCrypt (pServer, pDevHandle, &bEnable);
					_tprintf (_T("Crypt: %s\t-"), bEnable?_T("enabled"):_T("disabled"));

					ServerGetSharedUsbDevRequiresAuth (pServer, pDevHandle, &bEnable);
					_tprintf (_T("Auth: %s\t-"), bEnable?_T("enabled"):_T("disabled"));

					ServerGetSharedUsbDevIsCompressed (pServer, pDevHandle, &bEnable);
					_tprintf (_T("Compress: %s"), bEnable?_T("enabled"):_T("disabled"));
					if (bEnable)
					{
						CComVariant varName (Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE]);
						CComVariant varValue (0);
						ServerGetUsbDevValueByName (pServer, pDevHandle, varName, &varValue);
						int iComprType = varValue.intVal;
						if (iComprType > 0 && iComprType < Consts::UNG_COMPRESSION_TYPES_MAX)
						{
							_tprintf (_T(" (%s)"), CString (Consts::szUngCompressionType [iComprType]));
						}
					}
					_tprintf (_T("\t-"));

					ServerGetUsbDevStatus(pServer, pDevHandle, &state, &Host);
					ServerGetSharedUsbDevNetSettings(pServer, pDevHandle, &NetSettings);

					CString result;
					if (state == U2EC_STATE_CONNECTED)
						result = CString(_T("connected - ")) + CString(NetSettings) + _T(" - ") + CString(Host);
					else if (state == U2EC_STATE_WAITING)
						result = CString(_T("waiting for connection - ")) + CString(NetSettings);

					_tprintf (_T("%s\n"), result);
				}
			}
		}
	}

	if (bEmpty)
	{
		_tprintf (_T("\nThere are no available shared devices\n"));
	}

	ServerRemoveEnumUsbDev(pServer);
}

void CCommandService::ShowUsbDev (variables_map &vm)
{
	if (vm.count ("param") != 1)
	{
		throw CString (_T("Expected 1 argument: usb-port"));
	}


	const CString strDev = vm ["param"].as <std::wstring> ().c_str ();

	PVOID pServer = InitServer ();
	PVOID pDevice;

	try
	{
		CString strPath;
		pDevice = FindContext (pServer, strDev, strPath);
		if (pDevice)
		{
			PrintAsUtf8 ("USB device: %s\n", strPath);

			CComVariant varKey;
			CComVariant varValue;

			varKey = Consts::szUngNetSettings [Consts::UNS_NAME];	varValue.Clear ();
			if (ServerGetUsbDevValueByName (pServer, pDevice, varKey, &varValue))
			{
				const CString value = CString(varValue);
				if (!value.IsEmpty ())
				{
					printf ("    %s:\t", CStringA(varKey));
					PrintAsUtf8 ("%s\n", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_USB_LOC];	varValue.Clear ();
			if (ServerGetUsbDevValueByName (pServer, pDevice, varKey, &varValue))
			{
				const CString value = CString(varValue);
				if (!value.IsEmpty ())
				{
					printf ("    %s:\t", CStringA(varKey));
					PrintAsUtf8 ("%s\n", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_USBCLASS];	varValue.Clear ();
			if (ServerGetUsbDevValueByName (pServer, pDevice, varKey, &varValue))
			{
				const CStringA value = CStringA(varValue);
				if (!value.IsEmpty ())
				{
					printf ("    %s:\t", CStringA(varKey));
					printf ("%s\n", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_VID];	varValue.Clear ();
			if (ServerGetUsbDevValueByName (pServer, pDevice, varKey, &varValue))
			{
				const int value = varValue.uiVal;
				if (value)
				{
					printf ("    %s:\t", CStringA(varKey));
					printf ("%04x\n", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_PID];	varValue.Clear ();
			if (ServerGetUsbDevValueByName (pServer, pDevice, varKey, &varValue))
			{
				const int value = varValue.uiVal;
				if (value)
				{
					printf ("    %s:\t", CStringA(varKey));
					printf ("%04x\n", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_REV];	varValue.Clear ();
			if (ServerGetUsbDevValueByName (pServer, pDevice, varKey, &varValue))
			{
				const int value = varValue.uiVal;
				if (value)
				{
					printf ("    %s:\t", CStringA(varKey));
					printf ("%04x\n", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_SN];	varValue.Clear ();
			if (ServerGetUsbDevValueByName (pServer, pDevice, varKey, &varValue))
			{
				const CString value = CString(varValue);
				if (!value.IsEmpty ())
				{
					printf ("    %s:\t", CStringA(varKey));
					PrintAsUtf8 ("%s\n", value);
				}
			}
		}
		else
		{
			throw CString ("USB device was not found.");
		}
	}
	catch (...)
	{
		ServerRemoveEnumUsbDev (pServer);
		throw;
	}

	ServerRemoveEnumUsbDev(pServer);
}


void CCommandService::AddRemoteDevice (variables_map &vm)
{
	if (vm.count ("param") == 0)
	{
		throw CString (_T("No device was specified for sharing"));
	}

	CString strNetSettings = vm ["param"].as <std::wstring> ().c_str ();

	if (RemoteDeviceExist (strNetSettings))
	{
		throw CString (_T("Remote device ") + strNetSettings + _T(" is already on the list"));
	}
	
	if (ClientAddRemoteDevManually (CComVariant (strNetSettings)))
	{
		PrintAsUtf8 ("Remote device %s was added successfully\n", strNetSettings);
	}
	else
	{
		throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
	}
}

void CCommandService::RemoveRemoteDevice (variables_map &vm)
{
	PVOID pFindContext;
	CComVariant	varNetSettings;

	if (vm.count ("param") == 0)
	{
		throw CString (_T("No remote device was specified"));
	}

	CString strNetSettings = vm ["param"].as <std::wstring> ().c_str ();

	if (!ClientEnumAvailRemoteDev (&pFindContext))
	{
		throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
	}

	try
	{
		for (int a = 0; ClientGetRemoteDevNetSettings (pFindContext, a, &varNetSettings); a++)
		{
			if (_tcsicmp (strNetSettings, CString (varNetSettings)) == 0)
			{
				if ( ClientRemoveRemoteDev (pFindContext, a) )
				{
					PrintAsUtf8 ("Remote device %s was removed successfully", strNetSettings);
				}
				else
				{
					throw CString (_T("Could not find the remote device"));
				}

				ClientRemoveEnumOfRemoteDev (pFindContext);
				return;
			}
		}
	}
	catch (...)
	{
		ClientRemoveEnumOfRemoteDev (pFindContext);
		throw;
	}

	ClientRemoveEnumOfRemoteDev (pFindContext);
	throw CString (_T("Could not find the remote device"));
}

bool CCommandService::RemoteDeviceExist (LPCTSTR szNetSettings)
{
	PVOID pFindContext;
	CComVariant	varNetSettings;

	if (_tcsnicmp (szNetSettings, _T("RDP"), 3) == 0)
	{
		if (!ClientEnumRemoteDevOverRdp (&pFindContext))
		{
			throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
		}
	}
	else
	{
		if (!ClientEnumAvailRemoteDev (&pFindContext))
		{
			throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
		}
	}

	for (int a = 0; ClientGetRemoteDevNetSettings (pFindContext, a, &varNetSettings); a++)
	{
		if (_tcsicmp (szNetSettings, CString (varNetSettings)) == 0)
		{
			return true;
		}
	}

	ClientRemoveEnumOfRemoteDev (pFindContext);
	return false;
}

void CCommandService::ConnectRemoteDevice (variables_map &vm)
{
	if (vm.count ("param") == 0)
	{
		throw CString (_T("No remote device was specified"));
	}
	
	const CString strNetSettings = vm ["param"].as <std::wstring> ().c_str ();
	const CString srtAuth = vm.count ("auth") ? CString (vm ["auth"].as <std::wstring> ().c_str ()) : _T("");
	const bool bAutoReconnect = vm.count ("auto-reconnect")?true:false;

	PVOID pFindContext;

	if (_tcsnicmp (strNetSettings, _T("RDP"), 3) == 0)
	{
		if (!ClientEnumRemoteDevOverRdp (&pFindContext))
		{
			throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
		}
	}
	else
	{
		if (!ClientEnumAvailRemoteDev (&pFindContext))
		{
			throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
		}
	}

	try
	{
		CComVariant	varNetSettings;
		for (int a = 0; ClientGetRemoteDevNetSettings (pFindContext, a, &varNetSettings); a++)
		{
			if (_tcsicmp (strNetSettings, CString (varNetSettings)) == 0)
			{
				LONG	lState;
				CComVariant	varRemoteHost;
				if (!ClientGetStateRemoteDev (pFindContext, a, &lState, &varRemoteHost))
				{
					throw CString (_T("Could not find the remote device"));
				}

				if (lState != 0)
				{
					throw CString (_T("Remote device is already connected."));
				}

				BOOL bRet = ClientStartRemoteDev (pFindContext, a, bAutoReconnect, CComVariant (srtAuth));

				if (srtAuth.GetLength ())
				{
					Sleep (500);
					if (ClientGetStateRemoteDev (pFindContext, a, &lState, &varRemoteHost))
					{
						if (lState == 0)
						{
							throw CString (_T("Connection failed"));
						}
					}
				}

				if ( bRet )
				{
					PrintAsUtf8 ("Trying to connect to %s.\n", strNetSettings);
					printf ("Check the status with \"show-remote-devices\" to see if a connection was successfully established.\n");
				}
				else
				{
					throw CString (_T("Could not find the remote device"));
				}

				ClientRemoveEnumOfRemoteDev (pFindContext);
				return;
			}
		}
	}
	catch (...)
	{
		ClientRemoveEnumOfRemoteDev (pFindContext);
		throw;
	}

	ClientRemoveEnumOfRemoteDev (pFindContext);
	throw CString (_T("Could not find the remote device"));
}

void CCommandService::DisconnectRemoteDevice (variables_map &vm)
{
	PVOID pFindContext;
	CComVariant	varNetSettings;

	if (vm.count ("param") == 0)
	{
		throw CString (_T("No remote device was specified"));
	}

	CString strNetSettings = vm ["param"].as <std::wstring> ().c_str ();

	if (_tcsnicmp (strNetSettings, _T("RDP"), 3) == 0)
	{
		if (!ClientEnumRemoteDevOverRdp (&pFindContext))
		{
			throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
		}
	}
	else
	{
		if (!ClientEnumAvailRemoteDev (&pFindContext))
		{
			throw CString (_T("Please make sure the connection with the server PC is active and GUI is not running."));
		}
	}

	try
	{
		for (int a = 0; ClientGetRemoteDevNetSettings (pFindContext, a, &varNetSettings); a++)
		{
			if (_tcsicmp (strNetSettings, CString (varNetSettings)) == 0)
			{
				LONG	lState;
				CComVariant	strNetSettings;
				if (!ClientGetStateRemoteDev (pFindContext, a, &lState, &strNetSettings))
				{
					throw CString (_T("Could not find the remote device"));
				}

				if (lState == 0)
				{
					throw CString (_T("Remote device is already disconnected"));
				}

				if ( ClientStopRemoteDev (pFindContext, a) )
				{
					PrintAsUtf8 ("Remote device disconnected successfully\n", strNetSettings);
				}
				else
				{
					throw CString (_T("Could not find the remote device"));
				}

				ClientRemoveEnumOfRemoteDev (pFindContext);
				return;
			}
		}
	}
	catch (...)
	{
		ClientRemoveEnumOfRemoteDev (pFindContext);
		throw;
	}

	ClientRemoveEnumOfRemoteDev (pFindContext);
	throw CString (_T("Could not find the remote device"));
}

void CCommandService::ShowRemoteDevice (LPVOID lpClient, bool bRdp, bool bVerbose/*=false*/)
{
	CComVariant	varSettings;

	long iCount = 0;

	for (iCount = 0; ClientGetRemoteDevName (lpClient, iCount, &varSettings); iCount++)
	{
		CString devName = CString (varSettings);
		
		varSettings.Clear ();
		ClientGetRemoteDevNetSettings (lpClient, iCount, &varSettings);
		CString netSettings = varSettings;
		if (netSettings.Find(_T("RDP")) == -1 && netSettings.Find(_T(':')) == -1)
		{
			netSettings = _T("callback:") + netSettings;
		}

		CString trafficOptions;
		if (!bRdp)
		{
			BOOL bValue;

			bValue = FALSE;
			ClientTrafficRemoteDevIsEncrypted (lpClient, iCount, &bValue);
			trafficOptions.AppendFormat (_T("Crypt: %s\t-"), bValue ? _T("enabled") : _T("disabled"));

			bValue = FALSE;
			ClientRemoteDevRequiresAuth (lpClient, iCount, &bValue);
			trafficOptions.AppendFormat (_T("Auth: %s\t-"), bValue ? _T("enabled") : _T("disabled"));

			bValue = FALSE;
			ClientTrafficRemoteDevIsCompressed (lpClient, iCount, &bValue);
			trafficOptions.AppendFormat (_T("Compressed: %s\t-"), bValue ? _T("enabled") : _T("disabled"));
		}

		LONG lStatus = 0;
		CString status;
		varSettings.Clear ();
		if (!ClientGetStateRemoteDev (lpClient, iCount, &lStatus, &varSettings))
		{
			ClientGetStateSharedDevice (lpClient, iCount, &lStatus, &varSettings);
			lStatus = lStatus?U2EC_STATE_CONNECTED:0;
		}
		if (lStatus == U2EC_STATE_CONNECTED)
		{
			status.Format (_T("connected to %s"), CString(varSettings));
		}
		else
		{
			status = _T("disconnected");
		}

		// get nick
		{
			CString devNick;
			CComVariant varKey = Consts::szUngNetSettings [Consts::UNS_DESK];
			CComVariant varValue;
			if (lStatus == U2EC_STATE_CONNECTED)
			{
				if (ClientGetConnectedDevValueByName (lpClient, iCount, varKey, &varValue))
				{
					devNick = CString (varValue);
				}
			}
			else
			{
				//if (ClientGetRemoteDevValueByName (lpClient, iCount, varKey, &varValue))
				//{
				//	devNick = CString (varValue);
				//}
			}
			if (!devNick.IsEmpty ())
			{
				CString str;
				str.Format (_T("%s / %s"), devName, devNick);
				devName = str;
			}
		}

		PrintAsUtf8 ("%s\t-", devName);
		PrintAsUtf8 ("%s\t-", netSettings);
		_tprintf (trafficOptions);
		PrintAsUtf8 (status);

		// additional info about device
		if (bVerbose)
		{
			CComVariant varKey;
			CComVariant varValue;

			varKey = Consts::szUngNetSettings [Consts::UNS_USB_LOC];	varValue.Clear ();
			if (ClientGetRemoteDevValueByName (lpClient, iCount, varKey, &varValue))
			{
				const CString value = CString(varValue);
				if (!value.IsEmpty ())
				{
					printf ("\t-%s: ", CStringA(varKey));
					PrintAsUtf8 (value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_USBCLASS];	varValue.Clear ();
			if (ClientGetRemoteDevValueByName (lpClient, iCount, varKey, &varValue))
			{
				const CStringA value = CStringA(varValue);
				if (!value.IsEmpty ())
				{
					printf ("\t-%s: ", CStringA(varKey));
					printf ("%s", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_VID];	varValue.Clear ();
			if (ClientGetRemoteDevValueByName (lpClient, iCount, varKey, &varValue))
			{
				const int value = varValue.uiVal;
				if (value)
				{
					printf ("\t-%s: ", CStringA(varKey));
					printf ("%04x", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_PID];	varValue.Clear ();
			if (ClientGetRemoteDevValueByName (lpClient, iCount, varKey, &varValue))
			{
				const int value = varValue.uiVal;
				if (value)
				{
					printf ("\t-%s: ", CStringA(varKey));
					printf ("%04x", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_REV];	varValue.Clear ();
			if (ClientGetRemoteDevValueByName (lpClient, iCount, varKey, &varValue))
			{
				const int value = varValue.uiVal;
				if (value)
				{
					printf ("\t-%s: ", CStringA(varKey));
					printf ("%04x", value);
				}
			}

			varKey = Consts::szUngNetSettings [Consts::UNS_DEV_SN];	varValue.Clear ();
			if (ClientGetRemoteDevValueByName (lpClient, iCount, varKey, &varValue))
			{
				const CString value = CString(varValue);
				if (!value.IsEmpty ())
				{
					printf ("\t-%s: ", CStringA(varKey));
					PrintAsUtf8 (value);
				}
			}
		} // if (bVerbose)

		printf ("\n");
	} // for (iCount = 0; ClientGetRemoteDevName (lpClient, iCount, &varSettings); iCount++)

	if (iCount == 0)
	{
		_tprintf (_T("There are no available remote devices\n"), CString(varSettings));
	}
}

void CCommandService::ShowAddedRemoteDevice (variables_map &vm)
{
	const bool bVerbose = vm.count ("verbose") ? true : false;

	PVOID pFindContext;

	if (!ClientEnumAvailRemoteDev (&pFindContext))
	{
		throw CString (_T("Enumeration error. Please make sure the connection with the server PC is active and GUI is not running."));
	}

	ShowRemoteDevice (pFindContext, false, bVerbose);
	ClientRemoveEnumOfRemoteDev (pFindContext);
}

void CCommandService::ShowAddedRemoteRdpDevice (variables_map &vm)
{
	PVOID pFindContext;

	if (!ClientEnumRemoteDevOverRdp (&pFindContext))
	{
		throw CString (_T("Enumeration error. Please make sure the connection with the server PC is active and GUI is not running."));
	}

	ShowRemoteDevice (pFindContext, true);
	ClientRemoveEnumOfRemoteDev (pFindContext);
}

void CCommandService::ShowRemoteDeviceOnRemoteServer (variables_map &vm)
{
	if (vm.count ("param") == 0)
	{
		throw CString (_T("No remote server was specified"));
	}

	const bool bVerbose = vm.count ("verbose") ? true : false;

	PVOID pFindContext;
	CString strNetSettings = vm ["param"].as <std::wstring> ().c_str ();

	if (!ClientEnumAvailRemoteDevOnServer (CComVariant (strNetSettings), &pFindContext))
	{
		throw CString (_T("Enumeration error. Remote server PC doesn't respond."));
	}

	ShowRemoteDevice (pFindContext, false, bVerbose);
	ClientRemoveEnumOfRemoteDev (pFindContext);
}

void CCommandService::SharedManualDisconnect (variables_map &vm)
{
	if (vm.count ("param") == 0)
	{
		throw CString (_T("No device was specified for sharing"));
	}

	CString strDev = vm ["param"].as <std::wstring> ().c_str ();

	PVOID pServer = InitServer ();
	CString strPath;
	PVOID pDevice = FindContext (pServer, strDev, strPath);

	try
	{
		if (pDevice)
		{
			if (!ServerUsbDevIsShared (pServer, pDevice))
			{
				throw CString (_T("This device was not shared\n"));
			}
			PrintAsUtf8 ("Disconnecting device %s\n", strPath);
			if (ServerDisconnectRemoteDev (pServer, pDevice))
			{
				PrintAsUtf8 ("The device \"%s\" was disconnected successfully \n\n", strPath);
			}
			else
			{
				throw CString ("Please make sure the connection with the server PC is active and GUI is not running.\n\n");
			}
		}
	}
	catch (...)
	{
		ServerRemoveEnumUsbDev(pServer);
		throw;
	}

	ServerRemoveEnumUsbDev(pServer);
}