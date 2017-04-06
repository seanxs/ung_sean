/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   u2ec.cpp

Abstract:

	Additional functions for working with server

Environment:

    Kernel mode

--*/

#include "stdafx.h"
#include <Winsock2.h>
#include "dllmain.h"
#include "u2ec.h"
#include "..\Consts\consts.h"
#include "..\Consts\system.h"


TMAPUSBDEV g_mapEnum;
TVECCLIENT g_Clinet;
CString g_strServer = _T("localhost");
CString g_strRemoteHost;
CEventWindow window;
CCriticalSectionDll	g_csClient;
CCriticalSectionDll	g_csServer;
TMapContextError g_mapServerLastError;

// persistent connection to usb service (localhost:5475) per process
static SOCKET g_SockService = INVALID_SOCKET; // one socket per process to send commands to usb service
static CCriticalSectionDll g_csSock;


TPtrVECCLEINTS FindClient (PVOID pContext)
{
	TPtrVECCLEINTS pRet;
	TVECCLIENT::iterator item;
	g_csClient.Lock (0);

	for (item = g_Clinet.begin (); item != g_Clinet.end (); item++)
	{
		if (item->get () == pContext)
		{
			pRet = *item;
			break;
		}
	}
	g_csClient.UnLock (0);

	return pRet;
}


// NB: don't use this function, it may block on indefinite amout of time.
// Please, use OpenConnectionWithTimeout instead.
SOCKET OpenConnection (LPCTSTR szHost, USHORT uPort)
{
	SOCKET sock;
	LPHOSTENT hostEntry;
	int rVal;
	SOCKADDR_IN serverInfo;

	hostEntry = gethostbyname(CStringA(szHost));
	if (!hostEntry)
	{
		return INVALID_SOCKET;
	}

	serverInfo.sin_family = PF_INET;
	serverInfo.sin_addr = *((LPIN_ADDR)*hostEntry->h_addr_list);
	serverInfo.sin_port = htons(uPort);

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		return INVALID_SOCKET;
	}

	rVal = connect(sock, (LPSOCKADDR)&serverInfo, sizeof(serverInfo));
	if (rVal != SOCKET_ERROR)
	{
		return sock;
	}
	closesocket (sock);

	return INVALID_SOCKET;
}

// connect to remote server with specified timeout.
SOCKET OpenConnectionWithTimeout (LPCTSTR szHost, USHORT uPort, DWORD dwTimeoutMs)
{
	SOCKET sock;
	LPHOSTENT hostEntry;
	int rVal;
	SOCKADDR_IN serverInfo;

	hostEntry = gethostbyname(CStringA(szHost));
	if (!hostEntry)
	{
		return INVALID_SOCKET;
	}

	serverInfo.sin_family = PF_INET;
	serverInfo.sin_addr = *((LPIN_ADDR)*hostEntry->h_addr_list);
	serverInfo.sin_port = htons(uPort);

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		return INVALID_SOCKET;
	}
	
	// set non-blocking mode
	u_long uIoctlParam = 1; // non-blocking mode
	if (::ioctlsocket (sock, FIONBIO, &uIoctlParam) == SOCKET_ERROR)
	{
		closesocket (sock);
		return INVALID_SOCKET;
	}

	rVal = connect(sock, (LPSOCKADDR)&serverInfo, sizeof(serverInfo));
	if (rVal == SOCKET_ERROR)
	{
		// that's ok for non-blocking socket
		if (::WSAGetLastError () != WSAEWOULDBLOCK)
		{
			// that's not ok
			closesocket (sock);
			return INVALID_SOCKET;
		}

		// using select to check socket for writability to determine when connect finished
		fd_set fdsConnect = {1, {sock}}; 
		struct timeval timeout;
		timeout.tv_sec  = dwTimeoutMs / 1000; // seconds
		timeout.tv_usec = (dwTimeoutMs % 1000) * 1000; // microseconds 
		int result = ::select (	0/*ignored param*/, 
								NULL /*fd_read*/, 
								&fdsConnect /*fd_write*/, 
								NULL /*fd_error*/,
								&timeout);
		if (result == 0 || result == SOCKET_ERROR)
		{
			// timeout expired or some error
			closesocket (sock);
			return INVALID_SOCKET;
		}
	}

	// set sock back to blocking mode
	uIoctlParam = 0; // blocking mode
	if (::ioctlsocket (sock, FIONBIO, &uIoctlParam) == SOCKET_ERROR)
	{
		closesocket (sock);
		return INVALID_SOCKET;
	}

	return sock;
}

bool SendCommand (LPCTSTR szServer, USHORT uPort, LPCSTR szCommand, CBuffer &answer)
{
	bool	bRet = false;
	SOCKET  Sock;
	const int iSize = (int)strlen (szCommand);

	Sock = OpenConnectionWithTimeout (szServer, uPort, 1*1000/*ms*/);
	if (Sock == INVALID_SOCKET)
	{
		return false;
	}

	if (send (Sock, szCommand, iSize, 0) == iSize)
	{
		while (true)
		{
			char ch;
			const int iCount = recv (Sock, &ch, 1, 0);
			if (iCount == 0 || ch == CUngNetSettings::GetEnd () || iCount == SOCKET_ERROR)
			{
				break;
			}
			answer.Add (ch);
		}
		answer.Add (0);
		bRet = ParseAnsver (answer);
	}

	closesocket (Sock);

	return bRet;
}

// Send command to localhost:5475, re-use the same connection if possible
bool SendCommandToUsbService (LPCSTR szCommand, CBuffer &answer, bool bCheckAnswer/*=true*/)
{
	g_csSock.Lock (-1);	
	SOCKET Sock = g_SockService;
	g_csSock.UnLock (-1);

	if (Sock == INVALID_SOCKET)
	{
		Sock = OpenConnectionWithTimeout (g_strServer, Consts::uTcpControl, 50/*ms*/);
		if (Sock == INVALID_SOCKET)
		{
			return false;
		}
	}
		
	g_csSock.Lock (-1);	

	const int iSize = (int)strlen (szCommand);
	if (send (Sock, szCommand, iSize, 0) != iSize)
	{
		closesocket (Sock);
		g_SockService = INVALID_SOCKET;
		g_csSock.UnLock (-1);
		return false;
	}

	while (true)
	{
		char ch;
		const int iCount = recv (Sock, &ch, 1, 0);
		if (iCount == SOCKET_ERROR)
		{
			closesocket (Sock);
			g_SockService = INVALID_SOCKET;
			g_csSock.UnLock (-1);
			return false;
		}
		if (iCount == 0 || ch == CUngNetSettings::GetEnd ())
		{
			break;
		}
		answer.Add (ch);
	}
	answer.Add (0);

	bool bRet = false;

	if (bCheckAnswer)
		bRet = ParseAnsver (answer);
	else
		bRet = true;

	g_SockService = Sock;
	g_csSock.UnLock (-1);

	return bRet;
}

bool ParseAnsver (const CBuffer &answer)
{
	bool bRet = false;
	CUngNetSettings settings;
	CStringA strErrorCode;

	if (settings.ParsingSettings (CStringA ((PCHAR)answer.GetData ())))
	{
		strErrorCode = settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_ERROR]);
		if (strErrorCode.GetLength ())
		{
			if (_stricmp (strErrorCode, Consts::szUngErrorCodes [Consts::UEC_OK]) == 0)
			{
				bRet = true;
			}
		}
		else
		{
			bRet = true;
		}
	}

	return bRet;
}

CStringA GetErrorCodeFromAnswer (const CBuffer &answer)
{
	CUngNetSettings settings;
	CStringA strErrorCode;

	if (settings.ParsingSettings (CStringA ((PCHAR)answer.GetData ())))
	{
		strErrorCode = settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_ERROR]);
		if (strErrorCode.GetLength ())
		{
			if (_stricmp (strErrorCode, Consts::szUngErrorCodes [Consts::UEC_OK]) != 0)
			{
				// detailed error parameter in UNS_PARAM if any
				CStringA strDetailedError = settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_PARAM]);
				if (!strDetailedError.IsEmpty ())
				{
					return strDetailedError;
				}
			}
			return strErrorCode;
		}
		else
		{
			return Consts::szUngErrorCodes [Consts::UEC_OK];
		}
	}

	return Consts::szUngErrorCodes [Consts::UEC_ERROR];
}

// Simple hash function for strings
// http://stackoverflow.com/a/107657/65736
// We need to hash strings like this: {36fc9e60-c465-11cf-8056-444553540000}\xxxx
// where xxxx is number from 0000 to 9999
// I've tested this hash on any possible string in the form above
// and found no collisions
static USHORT HashStr101 (LPCTSTR str, USHORT seed=0)
{
	USHORT result = seed;
	TCHAR x;

	for (size_t i=0; x=str[i]; ++i) 
	{
		result = result * 101 + x;
	}
	
	return result;
}

USHORT CalcTcpPort ( LPCTSTR HubName, ULONG lUsbPort)
{
    USHORT uPort = 0x800;
    USHORT uTemp = HashStr101 (HubName);
    uTemp <<= 4;
    uPort += uTemp + (USHORT)lUsbPort;
    return uPort;
}

// returns net settings of the device if they're present, or computes default tcp port for sharing
CString GetNetSettingsForServerDevice (const CUsbDevInfo *pDev)
{
	CString strNetSettings = pDev->GetNetSettings ();
	if (strNetSettings.IsEmpty ())
	{
		strNetSettings.Format (_T("%d"), CalcTcpPort (pDev->m_strHubDev, pDev->m_iPort));
	}

	return strNetSettings;
}

bool GetNetSettingsFromVariant (const VARIANT &varNetSettings, CString &strServer, int &iTcpPort)
{
	bool bRet = false;

	switch (varNetSettings.vt)
	{
	case VT_BSTR:
		if (varNetSettings.bstrVal && (*varNetSettings.bstrVal))
		{
			const CString strNetSettings = CString (varNetSettings);
			if (!strNetSettings.IsEmpty ())
			{
				CSystem::ParseNetSettings (strNetSettings, strServer, iTcpPort);
				bRet = true;
			}
		}
		break;

	case VT_INT:
	case VT_UINT:
	case VT_I4:
	case VT_UI4:
		if (varNetSettings.intVal != 0)
		{
			iTcpPort = varNetSettings.intVal;
			bRet = true;
		}
		break;

	case VT_I2:
	case VT_UI2:
		if (varNetSettings.cVal != 0)
		{
			iTcpPort = varNetSettings.uiVal;
			bRet = true;
		}
		break;

	default:
		break;
	}

	return bRet;
}

bool GetPswdEncyptedFromVariant (const VARIANT &varValue, CStringA &strPswdEncrypted)
{
	bool bRet = false;

	switch (varValue.vt)
	{
	case VT_BSTR:
		if (varValue.bstrVal)
		{
			strPswdEncrypted = CSystem::Encrypt (varValue);
			bRet = true;
		}
		break;

	case VT_BOOL:
		if (!varValue.boolVal)
		{
			strPswdEncrypted = "";
			bRet = true;
		}
		break;

	default:
		break;
	}

	return bRet;
}

bool GetBoolFromVariant (const VARIANT &varValue, bool &bValue)
{
	bool bRet = false;

	switch (varValue.vt)
	{
	case VT_INT:
	case VT_UINT:
	case VT_I4:
	case VT_UI4:
		bValue = (varValue.intVal != 0);
		bRet = true;
		break;

	case VT_I2:
	case VT_UI2:
		bValue = (varValue.cVal != 0);
		bRet = true;
		break;

	case VT_BOOL:
		bValue = (varValue.boolVal != VARIANT_FALSE);
		bRet = true;
		break;

	default:
		break;
	}

	return bRet;
}

bool GetCompressFromVariant (const VARIANT &varValue, bool &bCompress, int &iCompressType)
{
	bool bRet = false;

	switch (varValue.vt)
	{
	case VT_INT:
	case VT_UINT:
	case VT_I4:
	case VT_UI4:
		iCompressType = varValue.intVal;
		bCompress = iCompressType > 0;
		bRet = true;
		break;

	case VT_I2:
	case VT_UI2:
		iCompressType = varValue.cVal;
		bCompress = iCompressType > 0;
		bRet = true;
		break;

	case VT_BOOL:
		bCompress = (varValue.boolVal != VARIANT_FALSE);
		if (bCompress)
		{
			iCompressType = Consts::UCT_COMPRESSION_DEFAULT;
		}
		bRet = true;
		break;

	default:
		break;
	}

	return bRet;
}

bool GetIntFromVariant (const VARIANT &varValue, int &iValue)
{
	bool bRet = false;

	switch (varValue.vt)
	{
	case VT_INT:
	case VT_UINT:
	case VT_I4:
	case VT_UI4:
		iValue = varValue.intVal;
		bRet = true;
		break;

	case VT_I2:
	case VT_UI2:
		iValue = varValue.cVal;
		bRet = true;
		break;

	default:
		break;
	}

	return bRet;
}

bool RecvData (SOCKET socket, CBuffer &buffer)
{
    fd_set			fdRead;
    timeval         wait = { 10, 0 };
	int				rVal;
	char			pChar;

    while (true)
    {
        // Getting devices list
        FD_ZERO (&fdRead);
        FD_SET (socket, &fdRead);
        if (select (0, &fdRead, NULL, NULL, &wait))
        {
            rVal = recv (socket, &pChar, 1, 0);
            if (rVal == INVALID_SOCKET || rVal == 0)
            {
                
                return false;
            }
            if (pChar == CUngNetSettings::GetEnd ())
            {
                break; 
            }
            buffer.Add (pChar);
        }
        else
        {
            break;
        }
    }

	return true;
}

PVOID GetCleintContext (SOCKET sock, LPCTSTR szServer)
{
	CStringA		str;
	CStringA		strRes;
	CBuffer			buffer;
	int				rVal;

	str.Format ("%s%c", Consts::szpPipeCommand [Consts::CP_GET_CONFIG], CUngNetSettings::GetEnd ());

    strRes.Empty ();

	// sending enquiry to get devices list
    rVal = send (sock, (LPCSTR)str, str.GetLength (), 0);
	while (true)
	{
		if (rVal == SOCKET_ERROR)
		{
			break;
		}
		if (!RecvData (sock, buffer))
		{
			rVal = SOCKET_ERROR;
			break;
		}
		if (buffer.GetCount () == 0)
		{
			rVal = SOCKET_ERROR;
			break;
		}
		break;
	}

	if (rVal == SOCKET_ERROR)
	{
		boost::shared_ptr <TVECCLEINTS> newClient (new TVECCLEINTS);
		g_csClient.Lock (1);
		g_Clinet.push_back (newClient);
		g_csClient.UnLock (1);
		return newClient.get ();
	}


	// parse list
	boost::shared_ptr <TVECCLEINTS> newClient (new TVECCLEINTS);
	newClient->strServer = szServer;

	ParseClientConfiguration (newClient, buffer);

	g_csClient.Lock (2);
	g_Clinet.push_back (newClient);
	g_csClient.UnLock (2);

	return newClient.get ();
}

bool ParseUsbHub (CUsbDevInfo *pDev, CBuffer &answer, int &iPos, int iDev)
{
	int iCount = 0;
	while ((iPos < answer.GetCount ()) && (iCount < iDev))
	{
		
		CUngNetSettings settings;

		if (!settings.ParsingSettings (CStringA (PCHAR(answer.GetData ()) + iPos)))
		{
			break;
		}

		pDev->m_arrDevs.push_back (CUsbDevInfo ());

		pDev->m_arrDevs.back ().m_strGuid =			settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME]);
		pDev->m_arrDevs.back ().m_strHubDev =		settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_PARAM]);
		pDev->m_arrDevs.back ().m_strName =			settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_NAME]);
		pDev->m_arrDevs.back ().m_iPort =			settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT]);
		pDev->m_arrDevs.back ().m_strUsbLoc =		settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_USB_LOC]);
		pDev->m_arrDevs.back ().m_bConnected =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_CONN])?true:false;
		pDev->m_arrDevs.back ().m_bHub =			settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_HUB])?true:false;

		pDev->m_arrDevs.back ().m_bShare =			settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_SHARED])?true:false;
		pDev->m_arrDevs.back ().m_bCompress =		settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_COMPRESS])?true:false;
		pDev->m_arrDevs.back ().m_iCompressType =	settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE], 0); // see CUsbDevInfo::GetCompressType()
		pDev->m_arrDevs.back ().m_bAuth =			settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_AUTH])?true:false;
		pDev->m_arrDevs.back ().m_bCrypt =			settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_CRYPT])?true:false;
		pDev->m_arrDevs.back ().m_strDescription =	settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_DESK]);
		pDev->m_arrDevs.back ().m_bAllowRDisconn =	settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON])?true:false;

		pDev->m_arrDevs.back ().m_iTcpPort =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);
		pDev->m_arrDevs.back ().m_strServer =		settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);

		// usb device descriptor data
		pDev->m_arrDevs.back ().m_bBaseClass =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_CLASS]);
		pDev->m_arrDevs.back ().m_bSubClass =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_SUBCLASS]);
		pDev->m_arrDevs.back ().m_bProtocol =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_PROTOCOL]);
		pDev->m_arrDevs.back ().m_bcdUSB =			settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_BCDUSB]);
		pDev->m_arrDevs.back ().m_idVendor =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_VID]);
		pDev->m_arrDevs.back ().m_idProduct =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_PID]);
		pDev->m_arrDevs.back ().m_bcdDevice =		settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_REV]);
		pDev->m_arrDevs.back ().m_strSerialNumber =	settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_DEV_SN]);
		pDev->m_arrDevs.back ().m_strDevUnicId =	settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_DEV_UNIC_ID]);

		pDev->m_arrDevs.back ().m_iStatus =			settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS]);
		pDev->m_arrDevs.back ().m_bRdp =			settings.GetParamFlag (Consts::szUngNetSettings [Consts::UNS_RDP]);
		pDev->m_arrDevs.back ().m_strSharedWith =	settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH]);
		

		iPos += int (strlen (PCHAR(answer.GetData ()) + iPos)) + 1;

		if (pDev->m_arrDevs.back ().m_bHub)
		{
			ParseUsbHub (&pDev->m_arrDevs.back (), answer, iPos, settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_COUNT]));
		}

		iCount++;

	}
	return true;
}

bool ParseClientConfiguration (boost::shared_ptr <TVECCLEINTS> &Clients, CBuffer &answer)
{
	int iPos = 0;

	answer.Add (CUngNetSettings::GetSeparatorConfig ());
	while (iPos < answer.GetCount ())
	{
		CUngNetSettings settings;
		if (settings.ParsingSettings (CStringA (PCHAR(answer.GetData ()) + iPos)))
		{
			Clients->Clients.push_back (CLIENT_ELMENT ());
			// name, traffic options
			FillOutClientStructFromParsedAnswer (&(Clients->Clients.back ()), settings);
			// net settings
			Clients->Clients.back ().strNetSettings  = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_REMOTE_HOST]);
			if (Clients->Clients.back ().strNetSettings.GetLength ())
			{
				Clients->Clients.back ().strNetSettings += ":";	
			}
			Clients->Clients.back ().strNetSettings +=	settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_TCP_PORT]);
			Clients->Clients.back ().strRemoteServer =	settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_SHARED_WITH]);
		}

		iPos += int (strlen (PCHAR(answer.GetData ()) + iPos)) + 1;
	}

	return true;
}

CString ConvertNetSettingsClientToServer (const CString &strServer, const CString &strNetSettings, const CString &strRemoteServer)
{
	CString strRet;
	if (strServer.GetLength ())
	{
		return strNetSettings;
	}

	int iPos = 0;
	iPos = strNetSettings.Find (_T(":"));
	if (iPos == -1)
	{
		strRet.Format (_T("%s:%s"), strRemoteServer, strNetSettings);
	}
	else
	{
		strRet = strNetSettings.Mid (iPos + 1);
	}

	return strRet;
}

CString ConvertNetSettingsServerToClient (const CString &strServer, const CString &strNetSettings)
{
	CString strRet;

	if (strServer.IsEmpty ())
	{
		return strNetSettings;
	}

	int iPos = 0;
	iPos = strNetSettings.Find (_T(":"));
	if (iPos == -1)
	{
		strRet.Format (_T("%s:%s"), strServer, strNetSettings);
	}
	else
	{
		strRet = strNetSettings.Mid (iPos + 1);
	}

	return strRet;
}

bool SendRemoteDisconnect (	const CString &strServer, 
	const CString &strNetSettings,
	const CLIENT_ELMENT *pElement)
{
	CUngNetSettings settings;
	CBuffer			answer;
	CString			strSer;
	CStringA		str;
	int				iTcpPort;

	CSystem::ParseNetSettings (strNetSettings, strSer, iTcpPort);

	settings.AddParamStrA (Consts::szpPipeCommand [Consts::CP_RDISCONNECT], "");
	if (strSer.GetLength ())
	{
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strSer);
	}
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iTcpPort);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

	SendCommand (strServer, Consts::uTcpPortConfig, str, answer);
	{
		settings.Clear ();
		if (settings.ParsingSettings (CStringA ((PCHAR)answer.GetData ())))
		{
			return (settings.GetParamStrA(Consts::szUngNetSettings [Consts::UNS_ERROR]) == Consts::szUngErrorCodes [Consts::UEC_OK]);
		}
	}

	return false;
}


bool GetShareDevInfoEx (const CString &strServer, 
                        const CString &strNetSettings,
                        CLIENT_ELMENT *pElement,
                        CString *strShared,
                        LONG *pState)
{
	CUngNetSettings settings;
	CBuffer			answer;
	CString			strSer;
	CStringA		str;
	int				iTcpPort;

	CSystem::ParseNetSettings (strNetSettings, strSer, iTcpPort);

	settings.AddParamStrA (Consts::szpPipeCommand [Consts::CP_GET_NAME_DEVICE], "");
	if (strSer.GetLength ())
	{
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strSer);
	}
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iTcpPort);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

	SendCommand (strServer, Consts::uTcpPortConfig, str, answer);
	{
		settings.Clear ();
		if (settings.ParsingSettings (CStringA ((PCHAR)answer.GetData ())))
		{
			FillOutClientStructFromParsedAnswer (pElement, settings);

			if (strShared)
			{
				*strShared = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH]);
			}
			if (pState)
			{
				*pState = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS]);
			}
			return true;
		}
	}

	return false;
}

void FillOutClientStructFromParsedAnswer (CLIENT_ELMENT *pElement, 
                                          const CUngNetSettings &settings)
{
	if (pElement)
	{
		pElement->strName = CSystem::BuildClientName (settings);
		pElement->bAuth = settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_AUTH]);
		pElement->bCrypt = settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_CRYPT]);
		pElement->bCompress = settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_COMPRESS]);
		pElement->bRDisconn = settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON]);
		
		pElement->strRealName = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_NAME]);
		pElement->strNick = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_DESK]);
		pElement->strUsbHub = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_HUB_NAME]);
		pElement->strUsbPort = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_USB_PORT]);
		pElement->strUsbLoc = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_USB_LOC]);

		pElement->iStatus = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS]);
		pElement->strSharedWith = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH]);

		pElement->bAutoAdded = settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_AUTO_ADDED]);

		if (settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_DEV_USBCLASS]))
		{
			const CStringA strHex = settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_DEV_USBCLASS]);
			CUsbDevInfo::UsbClassFromHexStrA (strHex, pElement->nUsbBaseClass, pElement->nUsbSubClass, pElement->nUsbProtocol);
		}

		pElement->bcdUSB	= settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_BCDUSB]);
		pElement->idVendor	= settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_VID]);
		pElement->idProduct	= settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_PID]);
		pElement->bcdDevice	= settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_REV]);
		pElement->strSerialNumber = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_DEV_SN]);
	}
}

bool GetShareDevClientDesk (const CString &strServer, 
                            const CString &strNetSettings,
                            CString *strDesk)
{
	if (strDesk)
	{
		CLIENT_ELMENT element;
		if (GetShareDevInfoEx (strServer, strNetSettings, &element, NULL, NULL))
		{
			*strDesk = element.strName;
			return true;
		}
	}

	return false;
}

CString ServerDevDesc (LPCTSTR HubName, ULONG lUsbPort)
{
    CStringA	str;

	CUngNetSettings settings;
	CBuffer	answer;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_GETDEVID]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], HubName);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], lUsbPort);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

    if (SendCommandToUsbService (str, answer))
	{
		// aswer
		settings.Clear ();
		if (settings.ParsingSettings (CStringA (PCHAR (answer.GetData ()))))
		{
			return settings.GetParamStr(Consts::szUngNetSettings [Consts::UNS_NAME]);
		}
	}
	return _T("");

}

bool AddHubName (LPCTSTR szHubId, LPCTSTR szHubName)
{
    CStringA str;
	CUngNetSettings settings;
	CBuffer	answer;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_HUBNAME]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], szHubId);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_PARAM], szHubName);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

    return SendCommandToUsbService (str, answer);
}

bool SetUserDescriptionForSharedDevice (const CUsbDevInfo *pDev, const CString &strUserDescription)
{
	ATLASSERT (pDev);

	CUngNetSettings	settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], 
		Consts::szpPipeCommand [Consts::CP_CHANGEDESK]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], 
		Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], strUserDescription);

	const CStringA strCmd = settings.ComposeCommand ();

	CBuffer answer;
	if (SendCommandToUsbService (strCmd, answer))
	{
		window.PostMessage (WM_DEVICECHANGE); // force trigger user callback on changes (set up with SetCallBackOnChangeDevList)
		return true;
	}
	
	return false;
}

bool SetAuthForSharedDevice (const CUsbDevInfo *pDev, const CStringA &strPswdEncrypted)
{
	ATLASSERT (pDev);

	CUngNetSettings	settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], 
		Consts::szpPipeCommand [Consts::CP_SET_AUTH]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], 
		Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);
	// pass empty string if no auth required
	if (strPswdEncrypted.GetLength())
	{
		settings.AddParamStrA(Consts::szUngNetSettings[Consts::UNS_AUTH], strPswdEncrypted);
	}

	const CStringA strCmd = settings.ComposeCommand ();

	CBuffer answer;
	if (SendCommandToUsbService (strCmd, answer))
	{
		if (window.m_hWnd)
		{
			window.PostMessage (WM_DEVICECHANGE); // force trigger user callback on changes (set up with SetCallBackOnChangeDevList)
		}
		return true;
	}
	
	return false;
}

bool SetCryptForSharedDevice (const CUsbDevInfo *pDev, const bool bCrypt)
{
	ATLASSERT (pDev);

	CUngNetSettings	settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], 
		Consts::szpPipeCommand [Consts::CP_SET_CRYPT]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], 
		Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);
	if (bCrypt)
	{
		settings.AddParamBoolean(Consts::szUngNetSettings[Consts::UNS_CRYPT], bCrypt);
	}

	const CStringA strCmd = settings.ComposeCommand ();

	CBuffer answer;
	if (SendCommandToUsbService (strCmd, answer))
	{
		if (window.m_hWnd)
		{
			window.PostMessage (WM_DEVICECHANGE); // force trigger user callback on changes (set up with SetCallBackOnChangeDevList)
		}
		return true;
	}
	
	return false;
}

// iCompressType - optional parameter, if < 0 then default compression type should be used.
bool SetCompressForSharedDevice (const CUsbDevInfo *pDev, const bool bCompress, int iCompressType)
{
	ATLASSERT (pDev);

	if (bCompress && iCompressType == Consts::UCT_COMPRESSION_NONE)
	{
		return false;
	}

	if (iCompressType < 0)
	{
		iCompressType = Consts::UCT_COMPRESSION_DEFAULT;
	}

	CUngNetSettings	settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], 
		Consts::szpPipeCommand [Consts::CP_SET_COMPRESS]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], 
		Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);
	if (bCompress)
	{
		settings.AddParamBoolean(Consts::szUngNetSettings[Consts::UNS_COMPRESS], bCompress);
		settings.AddParamInt(Consts::szUngNetSettings[Consts::UNS_COMPRESS_TYPE], iCompressType);
	}

	const CStringA strCmd = settings.ComposeCommand ();

	CBuffer answer;
	if (SendCommandToUsbService (strCmd, answer))
	{
		if (window.m_hWnd)
		{
			window.PostMessage (WM_DEVICECHANGE); // force trigger user callback on changes (set up with SetCallBackOnChangeDevList)
		}
		return true;
	}
	
	return false;	
}

CString GetRegistry (CString strFullPath)
{
	int iPos;
	CString strRet;

	iPos = strFullPath.Find (_T("\\"));
	if (iPos != -1)
	{
		strRet = strFullPath.Right (strFullPath.GetLength () - iPos - 1);
	}

	return strRet;
}

void SetServerLastError (PVOID pContext, const CStringA &strError)
{
	if (!strError.IsEmpty ())
	{
		// has data
		g_csServer.Lock (-1);
		g_mapServerLastError [pContext] = strError;
		g_csServer.UnLock (-1);
	}
	else
	{
		// remove entry from map
		g_csServer.Lock (-1);
		TMapContextError::const_iterator it = g_mapServerLastError.find (pContext);
		if (it != g_mapServerLastError.end ())
		{
			g_mapServerLastError.erase (it);
		}
		g_csServer.UnLock (-1);
	}
}

CStringA GetServerLastError (PVOID pContext)
{
	CStringA strError;
	g_csServer.Lock (-1);
	TMapContextError::const_iterator it = g_mapServerLastError.find (pContext);
	if (it != g_mapServerLastError.end ())
	{
		strError = it->second;
	}
	g_csServer.UnLock (-1);
	return strError;
}

bool CheckSocketToUsbService ()
{
	g_csSock.Lock (-1);
	if (g_SockService != INVALID_SOCKET)
	{
		g_csSock.UnLock (-1);
		return true;
	}
	g_csSock.UnLock (-1);

	SOCKET Sock = OpenConnectionWithTimeout (g_strServer, Consts::uTcpControl, 50/*ms*/);

	if (Sock == INVALID_SOCKET)
		return false;

	g_csSock.Lock (-1);
	g_SockService = Sock;
	g_csSock.UnLock (-1);
	return true;
}

void CloseSocketToUsbService ()
{
	g_csSock.Lock (-1);
	if (g_SockService != INVALID_SOCKET)
	{
		closesocket (g_SockService);
		g_SockService = INVALID_SOCKET;
	}
	g_csSock.UnLock (-1);
}
