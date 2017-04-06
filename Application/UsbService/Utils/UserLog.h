/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

	UserLog.h

Abstract:

	Helper functions to log important events about sharing and connecting devices,
	this log messages then visible in GUI log window.

--*/

#pragma once

#include "Consts\consts.h"
#include "Manager.h"
#include "Transport\ITransport.h"
#include "Usb\UsbClient.h"
#include "Usb\UsbServer.h"


namespace eltima { namespace ung {
namespace UserLog {

//---------------------------------------------------------------------------------
//                            S E R V E R

class Server
{
public:
	static void Add (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_ADD_SHARE],
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (), 
			pServer->GetNetSettings ()
			);
		CManager::AddLogFile (str);
	}

	static void NotShared (const LPCTSTR usbHub, const int usbPort, const LPCTSTR netSettings)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_NOT_SHARE_DEVICE], 
			CManager::GetHubName (usbHub), 
			usbPort, 
			netSettings);
		CManager::AddLogFile (str);
	}

	static void NotStarted (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_NOT_SHARE_DEVICE], 
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (), 
			pServer->GetNetSettings ());
		CManager::AddLogFile (str);
	}

	static void Delete (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_DEL_SHARE],
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (), 
			pServer->GetTcpPort ()
			);
		CManager::AddLogFile (str);	
	}

	static void Waiting (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_SHARED_DEVICE_WAITING],
			pServer->GetDeviceName (), 
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (),
			pServer->GetNetSettings ()
			);
		CManager::AddLogFile (str);	
	}

	static void DeviceMissing (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_SHARED_DEVICE_MISSING],
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort ()
			);
		CManager::AddLogFile (str);	
	}

	static void Connected (TPtrUsbServer pServer)
	{
		CString strRemoteHost = _T("???");
		Transport::TPtrTransport pTr = pServer->GetTrasport();
		if (pTr)
		{
			strRemoteHost = pTr->GetRemoteHost ();
			if (pTr->IsRdpConnected ())
				strRemoteHost = strRemoteHost + _T(" [RDP]");
		}
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_SHARE_CONNECTED],
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (),
			pServer->GetDeviceName (),
			strRemoteHost
			);
		CManager::AddLogFile (str);	
	}

	static void Disconnected (TPtrUsbServer pServer)
	{
		CString strRemoteHost = _T("???");
		Transport::TPtrTransport pTr = pServer->GetTrasport();
		if (pTr)
		{
			strRemoteHost = pTr->GetRemoteHost ();
			if (pTr->IsRdpConnected ())
				strRemoteHost = strRemoteHost + _T(" [RDP]");
		}
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_SHARE_DISCONNECTED],
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (),
			pServer->GetDeviceName (),
			strRemoteHost
			);
		CManager::AddLogFile (str);	
	}

	static void ConnectAuthError (TPtrUsbServer pServer, Transport::TPtrTransport pTr)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_SHARE_NO_AUTH], 
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (), 
			pTr->GetRemoteHost ()
			);
		CManager::AddLogFile (str);
	}

	static void ConnectCryptError (TPtrUsbServer pServer, Transport::TPtrTransport pTr)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_SHARE_NO_CRYPT], 
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort (), 
			pTr->GetRemoteHost ()
			);
		CManager::AddLogFile (str);
	}

	static void CompressionError (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_ZIP_SERVER_ERROR], 
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort ()
			);
		CManager::AddLogFile (str);
	}
 
	static void AuthInitError (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_ERR_AUTH], 
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort ()
			);
		CManager::AddLogFile (str);
	}

	// it's not obvious why we may need this, maybe it should be deleted?
	static void AvailableForSharing (TPtrUsbServer pServer)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_DEVICE_PRESENT], 
			pServer->GetDeviceName (), 
			CManager::GetHubName (pServer->GetHubName ()), 
			pServer->GetUsbPort ()
			);
		CManager::AddLogFile (str);
	}

};

//---------------------------------------------------------------------------------
//                             C L I E N T

class Client
{
public:
	static void Add (TPtrUsbClient pClient)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_ADD_CLIENT], pClient->GetVaribleName ());
		CManager::AddLogFile (str);
	}

	static void AddRdp (const CUngNetSettings &settings)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_ADD_CLIENT_RDP], CSystem::BuildClientName (settings));
		CManager::AddLogFile (str);
	}

	static void NotAdded (const LPCTSTR strName, const int tcpPort)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_NOT_CONN_DEVICE], strName, tcpPort);
		CManager::AddLogFile (str);
	}

	static void Start (Transport::TPtrTransport pTransport)
	{
		if (!pTransport) return;
		CString str;
		if (pTransport->IsRdpConnected ())
			str.Format (Consts::szpLogMes [Consts::LM_START_CLIENT_RDP], pTransport->GetRemoteHost ());
		else
			str.Format (Consts::szpLogMes [Consts::LM_START_CLIENT], pTransport->GetHostName (), pTransport->GetTcpPort ());
		CManager::AddLogFile (str);
	}

	static void Connected (Transport::TPtrTransport pTransport)
	{
		if (!pTransport) return;
		CString str;
		if (pTransport->IsRdpConnected ())
			str.Format (Consts::szpLogMes [Consts::LM_CLIENT_CONN_RDP], pTransport->GetRemoteHost ());
		else
			str.Format (Consts::szpLogMes [Consts::LM_CLIENT_CONN], pTransport->GetHostName (), pTransport->GetTcpPort ());
		CManager::AddLogFile (str);
	}

	static void ConnectionBroken (Transport::TPtrTransport pTransport)
	{
		if (!pTransport) return;
		CString str;
		if (pTransport->IsRdpConnected ())
			str.Format (Consts::szpLogMes [Consts::LM_CLIENT_STOP_RDP], pTransport->GetRemoteHost ());
		else
			str.Format (Consts::szpLogMes [Consts::LM_CLIENT_STOP], pTransport->GetHostName (), pTransport->GetTcpPort ());
		CManager::AddLogFile (str);
	}

	static void Stop (Transport::TPtrTransport pTransport)
	{
		if (!pTransport) return;
		CString str;
		if (pTransport->IsRdpConnected ())
			str.Format (Consts::szpLogMes [Consts::LM_STOP_CLIENT_RDP], pTransport->GetRemoteHost ());
		else
			str.Format (Consts::szpLogMes [Consts::LM_STOP_CLIENT], pTransport->GetHostName (), pTransport->GetTcpPort ());
		CManager::AddLogFile (str);
	}

	static void Delete (TPtrUsbClient pClient)
	{
		if (!pClient) return;
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_DEL_CLIENT], pClient->GetHostName (), pClient->GetTcpPort ());
		CManager::AddLogFile (str);	
	}

	static void DeleteRdp (TPtrUsbClient pClient)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_DEL_CLIENT_RDP], pClient->GetHostName ());
		CManager::AddLogFile (str);
	}

	static void CompressionError (Transport::TPtrTransport pTransport)
	{
		if (!pTransport) return;
		CString str;
		if (pTransport->IsRdpConnected ())
			str.Format (Consts::szpLogMes [Consts::LM_ZIP_CLIENT_RDP_ERROR], pTransport->GetRemoteHost ());
		else
			str.Format (Consts::szpLogMes [Consts::LM_ZIP_CLIENT_ERROR], pTransport->GetHostName (), pTransport->GetTcpPort ());
		CManager::AddLogFile (str);
	}

	static void VuhubAddError (const int error)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_ERROR_VUHUB_ADD_DEV], error);
		CManager::AddLogFile (str);
	}

	static void VuhubCtrlError (const int error)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_ERROR_VUHUB_CTRL], error);
		CManager::AddLogFile (str);
	}

	static void HandshakeBitsError (TPtrUsbClient pClient)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_HANDSHAKE_ERROR], pClient->GetHostName (), pClient->GetUsbPort ());
		CManager::AddLogFile (str);
	}

	static void HandshakeCompressionError (TPtrUsbClient pClient)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_COMPR_TYPE_ERROR], pClient->GetHostName (), pClient->GetUsbPort ());
		CManager::AddLogFile (str);
	}

	static void HandshakeAuthError (TPtrUsbClient pClient)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_CLIENT_NO_AUTH], pClient->GetHostName (), pClient->GetUsbPort ());
		CManager::AddLogFile (str);
	}

	static void HandshakeCryptError (TPtrUsbClient pClient)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_CLIENT_NO_CRYPT], pClient->GetHostName (), pClient->GetUsbPort ());
		CManager::AddLogFile (str);
	}

	static void CantInitCrypt (TPtrUsbClient pClient)
	{
		CString str;
		str.Format (Consts::szpLogMes [Consts::LM_CLIENT_CANT_CRYPT], pClient->GetHostName (), pClient->GetUsbPort ());
		CManager::AddLogFile (str);
	}

};

//---------------------------------------------------------------------------------

} // namespace UserLog
}} // namespace eltima { namespace ung {
