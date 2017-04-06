/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   dllmain.cpp

Abstract:

    Main module. Working with U2EC service.

Environment:

    Kernel mode

--*/

#include "stdafx.h"
#include "dllmain.h"
#include "u2ec.h"
#include "resource.h"
#include "..\Consts\consts_helper.h"
#include "..\Consts\Registry.h"
#include "..\Consts\System.h"

#include "LicenseDialog.h"
#include <WtsApi32.h>

#pragma comment( lib, "WtsApi32.lib" )

FNOnChangeDevList pOnChangeDevList = NULL;

static HINSTANCE dllHinstance = NULL;


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		WORD sockVersion;
		WSADATA wsaData;

		dllHinstance = hModule; // store hinstance of dll to be able load license dialog from resources

		sockVersion = MAKEWORD(1,1);
		WSAStartup(sockVersion, &wsaData);

		{
			RECT rect = {0,0,0,1};
			window.Create (NULL, rect);
		}

		if (CSystem::IsGuiExe())
		{
			// skip the following armadillo+license check if main process running as UsbConfig.exe
		}

#ifdef ARM
		// [2015/06/18] there was code to support Armadillo protection directly in dll.
		// as of UNG 6.2 we don't use this code, symbol ARM is not defined in any place.
		// Therefore that code was deleted to avoid unneeded mess with activator.
		// If you need this code, look at svn rev.555 or just before this text was added.
		// see also LicenseDialog.h/cpp and WaitDlg.h/cpp for changes.
#endif

		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		if (window.m_hWnd)
		{
			window.DestroyWindow ();
		}
		dllHinstance = NULL;
		CloseSocketToUsbService ();
		break;
	}
	return TRUE;
}

PCHAR GetVendorString (USHORT     idVendor)
{
    Consts::PUSBVENDORID vendorID;

    if (idVendor != 0x0000)
    {
        vendorID = Consts::USBVendorIDs;

        while (vendorID->usVendorID != 0x0000)
        {
            if (vendorID->usVendorID == idVendor)
            {
                return (vendorID->szVendor);
            }
            vendorID++;
        }
    }

    return NULL;
}

// local helpers
namespace {
class CPipeCommandBuilder: public CUngNetSettings
{
public:
	CPipeCommandBuilder() {}
	inline void AddCommand (enum Consts::CommandPipe ixCmd)
	{
		AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [ixCmd]);
	}
	inline void AddCmdParamA (enum Consts::UNGNetSettings ixParam, const CStringA &strParam)
	{
		AddParamStrA (Consts::szUngNetSettings [ixParam], strParam);
	};
	inline void AddCmdFlag (enum Consts::UNGNetSettings ixParam)
	{
		AddParamFlag (Consts::szUngNetSettings [ixParam]);
	}
	inline CStringA GetAnswerErrorCode ()
	{
		return GetParamStrA (Consts::szUngNetSettings [Consts::UNS_ERROR]);
	}
};
}


// ********************************************************
//				enum hub
FUNC_API	BOOL WINAPI ServerCreateEnumUsbDev (PVOID *ppContext)
{
	CStringA str;
	CUngNetSettings settings;
	int		iPos = 0;

	g_csServer.Lock (-1);
	TPtrUsbDevInfo pEnum (new CUsbDevInfo);
	g_mapEnum [pEnum.get ()] = pEnum;
	*ppContext = pEnum.get ();

	g_csServer.UnLock (-1);

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_GETUSBTREE]);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

	CBuffer answer;				
	if (SendCommandToUsbService (str, answer))
	{
		answer.Add (CUngNetSettings::GetSeparatorConfig ());
		
		return ParseUsbHub (pEnum.get (), answer, iPos) ? VARIANT_TRUE : VARIANT_FALSE;
	}

	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerRemoveEnumUsbDev (PVOID pContext)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		g_mapEnum.erase (item);
		SetServerLastError (pContext, ""); // erase last error if any
		g_csServer.UnLock (-1);
		return VARIANT_TRUE;
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerGetUsbDevFromHub (PVOID pContext, PVOID pHubContext, int iIndex, PVOID *ppDevContext)
{
	TMAPUSBDEV::iterator itemFind;

	g_csServer.Lock (-1);
	itemFind = g_mapEnum.find (pContext);
	if (itemFind != g_mapEnum.end ())
	{
		TPtrUsbDevInfo	pEnum = itemFind->second;
		CUsbDevInfo *pDev = pEnum->m_mapDevs [pHubContext];
		if (!pDev)
		{
			pDev = pEnum.get ();
		}
		if ((iIndex >= 0) && int (pDev->m_arrDevs.size ()) > iIndex)
		{
			g_csServer.UnLock (-1);
			pEnum->m_mapDevs [&pDev->m_arrDevs [iIndex]] = &pDev->m_arrDevs [iIndex];
			*ppDevContext = &pDev->m_arrDevs [iIndex];
			return VARIANT_TRUE;
		}
	}
	g_csServer.UnLock (-1);
	*ppDevContext = NULL;
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerUsbDevIsHub (PVOID pContext, PVOID pDevContext)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		if (pDevContext == item->first)
		{
			g_csServer.UnLock (-1);
			return VARIANT_TRUE;
		}
		else
		{
			CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
			if (pDev)
			{
				if (pDev->m_bHub)
				{
					g_csServer.UnLock (-1);
					return VARIANT_TRUE;
				}
			}
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerUsbDevIsShared (PVOID pContext, PVOID pDevContext)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			g_csServer.UnLock (-1);
			return pDev->m_bShare?VARIANT_TRUE:VARIANT_FALSE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerUsbDevIsConnected (PVOID pContext, PVOID pDevContext)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			g_csServer.UnLock (-1);
			return pDev->m_bConnected?VARIANT_TRUE:VARIANT_FALSE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

static bool _ServerGetUsbDevNameAndNick (PVOID pContext, PVOID pDevContext, 
                                         CString *strName, CString *strNick)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			CString str = pDev->m_strName;
			if (str.IsEmpty ())
			{
				str = ServerDevDesc (pDev->m_strHubDev, pDev->m_iPort);
			}
			if (str.IsEmpty ())
			{
				str = _T("Unknown device");
			}

			*strName = str;
			*strNick = pDev->m_strDescription;

			g_csServer.UnLock (-1);
			return true;
		}
	}
	g_csServer.UnLock (-1);
	return false;
}

FUNC_API	BOOL WINAPI ServerGetUsbDevName (PVOID pContext, PVOID pDevContext, VARIANT *strName)
{
	CString str;
	CString strDesc;

	bool ok = _ServerGetUsbDevNameAndNick(pContext, pDevContext, &str, &strDesc);

	if (!ok)
		return VARIANT_FALSE;

	// join name and nick together
	if (strDesc.GetLength ())
	{
		str.Append (_T(" / ") + strDesc);
	}

	// convert result to VARIANT
	CComVariant var = str;
	var.Detach (strName);

	return VARIANT_TRUE;
}

FUNC_API    BOOL WINAPI ServerGetUsbDevNameEx (PVOID pContext, PVOID pDevContext, VARIANT *strName, VARIANT *strUserDescription)
{
	CString str;
	CString strDesc;

	bool ok = _ServerGetUsbDevNameAndNick(pContext, pDevContext, &str, &strDesc);

	if (!ok)
		return VARIANT_FALSE;

	// convert strings to VARIANTs
	CComVariant varName = str;
	varName.Detach (strName);

	CComVariant varNick = strDesc;
	varNick.Detach (strUserDescription);

	return VARIANT_TRUE;
}

FUNC_API    BOOL WINAPI ServerGetUsbDevClassCode (PVOID pContext, PVOID pDevContext, 
	BYTE *bBaseClass, BYTE *bSubClass, BYTE *bProtocol)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			g_csServer.UnLock (-1);

			*bBaseClass = pDev->m_bBaseClass;
			*bSubClass = pDev->m_bSubClass;
			*bProtocol = pDev->m_bProtocol;

			return VARIANT_TRUE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}



FUNC_API	void WINAPI SetCallBackOnChangeDevList (FNOnChangeDevList pFunc)
{
	pOnChangeDevList = pFunc;
	return;
}


// server
FUNC_API	BOOL WINAPI ServerShareUsbDev (PVOID pContext, PVOID pDevContext, VARIANT varNetSettings, VARIANT strDescription, BOOL bAuth, VARIANT strPassord, BOOL bCrypt, BOOL bCompress)
{
	//return ServerShareUsbDev2 (pContext, pDevContext, varNetSettings, strDescription, bAuth, strPassord, bCrypt, bCompress);

	CStringA strPassw = CSystem::Encrypt (strPassord);

	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	CUngNetSettings settings;

	if (item != g_mapEnum.end ())
	{
		CString strServer;
		int		iTcpPort;
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			// calc connection params
			iTcpPort = CalcTcpPort (pDev->m_strHubDev, pDev->m_iPort);
			GetNetSettingsFromVariant (varNetSettings, strServer, iTcpPort);

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_ADD]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iTcpPort);

			if (strDescription.bstrVal && wcslen (strDescription.bstrVal))
			{
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], strDescription);
			}

			if (bAuth && strPassw.GetLength ())
			{
				settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH], strPassw);
			}

			if (bCrypt)
			{
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_CRYPT], NULL);
			}
			if (bCompress)
			{
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_COMPRESS], NULL);
			}

			strPassw = settings.BuildSetttings ();
			strPassw += CUngNetSettings::GetEnd ();

			g_csServer.UnLock (-1);

			CBuffer answer;

			if (SendCommandToUsbService (strPassw, answer))
			{
				pDev->m_bShare = true;
				return VARIANT_TRUE;
			}
			else
			{
				CStringA strErrorCode = GetErrorCodeFromAnswer (answer);
				SetServerLastError (pContext, strErrorCode);
			}
			return VARIANT_FALSE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerShareUsbDev2 (PVOID pContext, PVOID pDevContext, 
	VARIANT varNetSettings, VARIANT strDescription, BOOL bAuth, VARIANT strPassword, 
	BOOL bCrypt, BOOL bCompress)
{
	CStringA strPassw = CSystem::Encrypt (strPassword);

	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	CUngNetSettings settings;

	if (item != g_mapEnum.end ())
	{
		CString strServer;
		int		iTcpPort;
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			// calc connection params
			iTcpPort = CalcTcpPort (pDev->m_strHubDev, pDev->m_iPort);
			GetNetSettingsFromVariant (varNetSettings, strServer, iTcpPort);

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_ADD]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strDevUnicId);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], 0);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iTcpPort);

			if (strDescription.bstrVal && wcslen (strDescription.bstrVal))
			{
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], strDescription);
			}

			if (bAuth && strPassw.GetLength ())
			{
				settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH], strPassw);
			}

			if (bCrypt)
			{
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_CRYPT], NULL);
			}
			if (bCompress)
			{
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_COMPRESS], NULL);
			}

			strPassw = settings.BuildSetttings ();
			strPassw += CUngNetSettings::GetEnd ();

			g_csServer.UnLock (-1);

			CBuffer answer;

			if (SendCommandToUsbService (strPassw, answer))
			{
				pDev->m_bShare = true;
				return VARIANT_TRUE;
			}
			else
			{
				CStringA strErrorCode = GetErrorCodeFromAnswer (answer);
				SetServerLastError (pContext, strErrorCode);
			}
			return VARIANT_FALSE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerShareUsbWithPredefinedValues (PVOID pContext, PVOID pDevContext)
{
	TMAPUSBDEV::iterator item;
	CUngNetSettings settings;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item == g_mapEnum.end ())
	{
		SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_NOT_FOUND]);
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
	if (!pDev)
	{
		SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_NOT_FOUND]);
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_ADD]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);

	const CString strNetSettings = GetNetSettingsForServerDevice (pDev);
	CString strServer;
	int iTcpPort;
	CSystem::ParseNetSettings (strNetSettings, strServer, iTcpPort);

	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iTcpPort);

	if (!pDev->m_strDescription.IsEmpty ())
	{
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], pDev->m_strDescription);
	}

	if (pDev->m_bAuth && !pDev->m_strPswdEncrypted.IsEmpty ())
	{
		settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH], pDev->m_strPswdEncrypted);
	}

	if (pDev->m_bCrypt)
	{
		settings.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_CRYPT]);
	}

	if (pDev->m_bCompress && pDev->m_iCompressType)
	{
		settings.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_COMPRESS]);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE], pDev->m_iCompressType);
	}

	if (pDev->m_bAllowRDisconn)
	{
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON], pDev->m_bAllowRDisconn);
	}

	const CStringA strCmd = settings.ComposeCommand ();

	g_csServer.UnLock (-1);

	CBuffer answer;
	if (SendCommandToUsbService (strCmd, answer))
	{
		pDev->m_bShare = true;
		SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_OK]);
		return VARIANT_TRUE;
	}
	else
	{
		CStringA strErrorCode = GetErrorCodeFromAnswer (answer);
		SetServerLastError (pContext, strErrorCode);
	}
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerUnshareAllUsbDev (void)
{
	TMAPUSBDEV::iterator item;

	CStringA str;
	CUngNetSettings settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_DEL_ALL]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

	CBuffer answer;
	return SendCommandToUsbService (str, answer) ? VARIANT_TRUE : VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerUnshareUsbDev (PVOID pContext, PVOID pDevContext)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			CStringA str;
			CUngNetSettings settings;

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_DEL]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			g_csServer.UnLock (-1);

			CBuffer answer;
			return SendCommandToUsbService (str, answer) ? VARIANT_TRUE : VARIANT_FALSE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerGetSharedUsbDevIsCrypt (PVOID pContext, PVOID pDevContext, BOOL *bCrypt)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			g_csServer.UnLock (-1);
			*bCrypt = pDev->m_bCrypt;
			return VARIANT_TRUE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerGetSharedUsbDevRequiresAuth (PVOID pContext, PVOID pDevContext, BOOL *bAuth)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			g_csServer.UnLock (-1);
			*bAuth = pDev->m_bAuth;
			return VARIANT_TRUE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerGetSharedUsbDevIsCompressed (PVOID pContext, PVOID pDevContext, BOOL *bCompress)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			g_csServer.UnLock (-1);
			*bCompress = pDev->m_bCompress;
			return VARIANT_TRUE;
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}


FUNC_API	BOOL WINAPI ServerGetUsbDevStatus (PVOID pContext, PVOID pDevContext, LONG *piState, VARIANT *strHostConnected)
{
	CStringA strAnswer;
	TMAPUSBDEV::iterator item;
	CComVariant var;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			CStringA str;
			CUngNetSettings	settings;

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_INFO]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			g_csServer.UnLock (-1);

			CBuffer answer;
			if (SendCommandToUsbService (str, answer))
			{
				settings.Clear ();
				if (settings.ParsingSettings (CStringA ((PCHAR)answer.GetData ())))
				{
					*piState = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS]);
					var = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH]);
					var.Detach (strHostConnected);
					return VARIANT_TRUE;
				}
			}
			g_csServer.Lock (-1);
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerGetUsbDevValueByName (PVOID pContext, PVOID pDevContext, 
													VARIANT strValueName, VARIANT *pValue)
{
	if (!pValue)
	{
		return VARIANT_FALSE;
	}

	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		g_csServer.UnLock (-1);
		if (!pDev)
		{
			return VARIANT_FALSE;
		}

		// return data
		CComVariant varResult;
		const int ix = Consts::SettingsCodeFromStr (CStringA (strValueName));
		switch (ix)
		{
		case Consts::UNS_NAME:				varResult = pDev->m_strName;						break;
		case Consts::UNS_DESK:				varResult = pDev->m_strDescription;					break;
		case Consts::UNS_AUTH:				varResult = pDev->m_bAuth;							break;
		case Consts::UNS_CRYPT:				varResult = pDev->m_bCrypt;							break;
		case Consts::UNS_COMPRESS:			varResult = pDev->m_bCompress;						break;
		case Consts::UNS_COMPRESS_TYPE:		varResult = pDev->GetCompressType ();				break;
		case Consts::UNS_STATUS:			varResult = pDev->m_iStatus;						break;
		case Consts::UNS_SHARED_WITH:		varResult = pDev->m_strSharedWith;					break;
		case Consts::UNS_DEV_NET_SETTINGS:	varResult = GetNetSettingsForServerDevice (pDev);	break;
		case Consts::UNS_DEV_ALLOW_RDISCON:	varResult = pDev->m_bAllowRDisconn;					break;
		case Consts::UNS_USB_LOC:			varResult = pDev->m_strUsbLoc;						break;
		case Consts::UNS_DEV_USBCLASS:		varResult = pDev->UsbClassAsHexStrA ();				break;	// USBCLASS - USB device class triple as single hex-ascii string xxyyzz (where xx=class, yy=subclass, zz=protocol)
		case Consts::UNS_DEV_CLASS:			varResult = pDev->m_bBaseClass;						break;
		case Consts::UNS_DEV_SUBCLASS:		varResult = pDev->m_bSubClass;						break;
		case Consts::UNS_DEV_PROTOCOL:		varResult = pDev->m_bProtocol;						break;
		case Consts::UNS_DEV_BCDUSB:		varResult = pDev->m_bcdUSB;							break;	// BCDUSB - version of the USB specification
		case Consts::UNS_DEV_VID:			varResult = pDev->m_idVendor;						break;	// VID - vendor identifier for the device
		case Consts::UNS_DEV_PID:			varResult = pDev->m_idProduct;						break;	// PID - product identifier
		case Consts::UNS_DEV_REV:			varResult = pDev->m_bcdDevice;						break;	// REV - version of the device
		case Consts::UNS_DEV_SN:			varResult = pDev->m_strSerialNumber;				break;	// SERIAL - serial number of device

		default:
			return VARIANT_FALSE;
		}

		varResult.Detach (pValue);
		return VARIANT_TRUE;
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerSetUsbDevValueByName (PVOID pContext, PVOID pDevContext, 
													VARIANT strValueName, VARIANT varValue)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item == g_mapEnum.end ())
	{
		g_csServer.UnLock (-1);
		SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_NOT_FOUND]);
		return VARIANT_FALSE;
	}

	CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
	if (!pDev)
	{
		g_csServer.UnLock (-1);
		SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_NOT_FOUND]);
		return VARIANT_FALSE;
	}

	BOOL bRet = VARIANT_TRUE;

	const int ixValue = Consts::SettingsCodeFromStr (CStringA (strValueName));
	switch (ixValue)
	{
	case Consts::UNS_DEV_NET_SETTINGS:
		{
			if (pDev->m_bShare)
			{
				SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_CANT_SET]);
				bRet = VARIANT_FALSE;
				break;
			}
			CString strServer;
			int iTcpPort = -1;
			GetNetSettingsFromVariant (varValue, strServer, iTcpPort);
			pDev->SetNetSettings (strServer, iTcpPort);
		}
		break;

	case Consts::UNS_DESK: // nick, user description
		{
			CString strNick;
			if (varValue.bstrVal)
			{
				strNick = CString (varValue);
			}
			if (!pDev->m_bShare)
			{
				pDev->m_strDescription = strNick;
			}
			else
			{
				// try to update value in service for shared device
				g_csServer.UnLock (-1);
				const bool bOk = SetUserDescriptionForSharedDevice (pDev, strNick);
				g_csServer.Lock (-1);
				if (bOk)
				{
					pDev->m_strDescription = strNick;
				}
				else
				{
					SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_ERROR_UPDATE]);
					bRet = VARIANT_FALSE;
					break;
				}
			}
		}
		break;

	case Consts::UNS_AUTH:
		{
			CStringA strPswdEncrypted;
			if (!GetPswdEncyptedFromVariant (varValue, strPswdEncrypted))
			{
				SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_BAD_VALUE]);
				bRet = VARIANT_FALSE;
				break;
			}

			if (!pDev->m_bShare)
			{
				pDev->m_bAuth = !strPswdEncrypted.IsEmpty ();
				pDev->m_strPswdEncrypted = strPswdEncrypted;
			}
			else
			{
				// try to update value in service for shared device
				g_csServer.UnLock (-1);
				const bool bOk = SetAuthForSharedDevice (pDev, strPswdEncrypted);
				g_csServer.Lock (-1);
				if (bOk)
				{
					pDev->m_bAuth = !strPswdEncrypted.IsEmpty ();
					pDev->m_strPswdEncrypted = strPswdEncrypted;
				}
				else
				{
					SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_ERROR_UPDATE]);
					bRet = VARIANT_FALSE;
					break;
				}
			}
		}
		break;

	case Consts::UNS_CRYPT:
		{
			bool bCrypt;
			if (!GetBoolFromVariant (varValue, bCrypt))
			{
				SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_BAD_VALUE]);
				bRet = VARIANT_FALSE;
				break;
			}

			if (!pDev->m_bShare)
			{
				pDev->m_bCrypt = bCrypt;
			}
			else
			{
				// try to update value in service for shared device
				g_csServer.UnLock (-1);
				const bool bOk = SetCryptForSharedDevice (pDev, bCrypt);
				g_csServer.Lock (-1);
				if (bOk)
				{
					pDev->m_bCrypt = bCrypt;
				}
				else
				{
					SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_ERROR_UPDATE]);
					bRet = VARIANT_FALSE;
					break;
				}
			}
		}
		break;

	case Consts::UNS_COMPRESS:
		{
			bool bCompress = false;
			int  iCompressType = Consts::UCT_COMPRESSION_NONE;
			if (!GetCompressFromVariant (varValue, bCompress, iCompressType))
			{
				SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_BAD_VALUE]);
				bRet = VARIANT_FALSE;
				break;
			}

			if (!pDev->m_bShare)
			{
				pDev->m_bCompress = bCompress;
				pDev->m_iCompressType = iCompressType;
			}
			else
			{
				// try to update value in service for shared device
				g_csServer.UnLock (-1);
				const bool bOk = SetCompressForSharedDevice (pDev, bCompress, iCompressType);
				g_csServer.Lock (-1);
				if (bOk)
				{
					pDev->m_bCompress = bCompress;
					pDev->m_iCompressType = iCompressType;
				}
				else
				{
					SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_ERROR_UPDATE]);
					bRet = VARIANT_FALSE;
					break;
				}
			}
		}
		break;

	case Consts::UNS_COMPRESS_TYPE:
		{
			int iCompressType = Consts::UCT_COMPRESSION_NONE;
			if (!GetIntFromVariant (varValue, iCompressType))
			{
				SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_BAD_VALUE]);
				bRet = VARIANT_FALSE;
				break;
			}
			const bool bCompress = (iCompressType != Consts::UCT_COMPRESSION_NONE);

			if (!pDev->m_bShare)
			{
				pDev->m_bCompress = bCompress;
				pDev->m_iCompressType = iCompressType;
			}
			else
			{
				// try to update value in service for shared device
				g_csServer.UnLock (-1);
				const bool bOk = SetCompressForSharedDevice (pDev, bCompress, iCompressType);
				g_csServer.Lock (-1);
				if (bOk)
				{
					pDev->m_bCompress = bCompress;
					pDev->m_iCompressType = iCompressType;
				}
				else
				{
					SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_ERROR_UPDATE]);
					bRet = VARIANT_FALSE;
					break;
				}
			}
		}
		break;


	case Consts::UNS_DEV_ALLOW_RDISCON:
		{
			bool bEnableRd;
			if (!GetBoolFromVariant (varValue, bEnableRd))
			{
				SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_BAD_VALUE]);
				bRet = VARIANT_FALSE;
				break;
			}

			if (!pDev->m_bShare)
			{
				pDev->m_bAllowRDisconn = bEnableRd;
			}
			else
			{
				// try to update value in service for shared device
				g_csServer.UnLock (-1);
				const bool bOk = ServerAllowDevRemoteDisconnect (pContext, pDevContext, bEnableRd)?true:false;
				g_csServer.Lock (-1);
				if (bOk)
				{
					pDev->m_bAllowRDisconn = bEnableRd;
				}
				else
				{
					SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_ERROR_UPDATE]);
					bRet = VARIANT_FALSE;
					break;
				}
			}
		}
		break;

	default:
		SetServerLastError (pContext, Consts::szUngErrorCodes [Consts::UEC_BAD_NAME]);
		bRet = VARIANT_FALSE;
		break;
	}

	g_csServer.UnLock (-1);
	return bRet;
}

FUNC_API	BOOL WINAPI ServerGetSharedUsbDevNetSettings (PVOID pContext, PVOID pDevContext, VARIANT *strParam)
{
	TMAPUSBDEV::iterator item;
	CComVariant var;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			g_csServer.UnLock (-1);
			if (pDev->m_bShare)
			{
				var = pDev->GetNetSettings ();
				var.Detach (strParam);

				return VARIANT_TRUE;
			}
			else
			{
				CString str;
				str.AppendFormat (_T("%d"), CalcTcpPort (pDev->m_strHubDev, pDev->m_iPort));
				var = str;
				var.Detach (strParam);

				return VARIANT_TRUE;
			}
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API    BOOL WINAPI ServerSetSharedUsbDevCrypt (PVOID pContext, PVOID pDevContext, 
                                                    BOOL bCrypt)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item == g_mapEnum.end ())
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
	if (!pDev)
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	g_csServer.UnLock (-1);

	const bool boolCrypt = bCrypt ? true : false;
	if (SetCryptForSharedDevice (pDev, boolCrypt))
	{
		g_csServer.Lock (-1);
		pDev->m_bCrypt = boolCrypt;
		g_csServer.UnLock (-1);
		return VARIANT_TRUE;
	}
	return VARIANT_FALSE;
}

FUNC_API    BOOL WINAPI ServerSetSharedUsbDevCompress (PVOID pContext, PVOID pDevContext, 
													   BOOL bCompress)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item == g_mapEnum.end ())
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
	if (!pDev)
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	g_csServer.UnLock (-1);

	const bool boolCompress = bCompress ? true : false;
	if (SetCompressForSharedDevice (pDev, boolCompress))
	{
		g_csServer.Lock (-1);
		pDev->m_bCompress = boolCompress;
		pDev->m_iCompressType = bCompress ? Consts::UCT_COMPRESSION_DEFAULT : Consts::UCT_COMPRESSION_NONE;
		g_csServer.UnLock (-1);
		return VARIANT_TRUE;
	}

	return VARIANT_FALSE;
}

FUNC_API    BOOL WINAPI ServerAllowDevRemoteDisconnect (PVOID pContext, PVOID pDevContext, BOOL bEnable)
{
	CStringA strAnswer;
	TMAPUSBDEV::iterator item;
	CComVariant var;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			CStringA str;
			CUngNetSettings	settings;

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_SET_RDISCONNECT]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_SERVER]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON], bEnable);

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			g_csServer.UnLock (-1);

			CBuffer answer;
			if (SendCommandToUsbService (str, answer))
			{
				settings.Clear ();
				if (settings.ParsingSettings (CStringA ((PCHAR)answer.GetData ())))
				{
					return VARIANT_TRUE;
				}
			}
			g_csServer.Lock (-1);
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API    BOOL WINAPI ServerSetSharedUsbDevAuth (PVOID pContext, PVOID pDevContext, 
                                                   BOOL bAuth, VARIANT strPassword)
{
	CStringA strPassw = CSystem::Encrypt (strPassword);
	if (bAuth && strPassw.IsEmpty ())
	{
		// password cannot be empty if auth is enabled!
		return VARIANT_FALSE;
	}
	if (!bAuth)
	{
		// empty string if no auth required
		strPassw = "";
	}

	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item == g_mapEnum.end ())
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
	if (!pDev)
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}

	g_csServer.UnLock (-1);

	if (SetAuthForSharedDevice (pDev, strPassw))
	{
		g_csServer.Lock (-1);
		pDev->m_bAuth = (bAuth != FALSE);
		g_csServer.UnLock (-1);
		return VARIANT_TRUE;
	}
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ServerSetDevUserDescription (PVOID pContext, PVOID pDevContext,
													VARIANT strUserDescription)
{
	TMAPUSBDEV::iterator item;

	g_csServer.Lock (-1);
	item = g_mapEnum.find (pContext);
	if (item == g_mapEnum.end ())
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}		

	CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
	if (!pDev)
	{
		g_csServer.UnLock (-1);
		return VARIANT_FALSE;
	}		

	g_csServer.UnLock (-1);

	CString cstrNick;
	if (strUserDescription.bstrVal)
	{
		cstrNick = CString(strUserDescription);
	}

	if (SetUserDescriptionForSharedDevice (pDev, cstrNick))
	{
		g_csServer.Lock (-1);
		pDev->m_strDescription = cstrNick;
		g_csServer.UnLock (-1);
		return VARIANT_TRUE;
	}
	return VARIANT_FALSE;
}

FUNC_API    BOOL WINAPI ServerGetLastError (PVOID pContext, VARIANT *strErrorCode)
{
	if (!strErrorCode)
	{
		return VARIANT_FALSE;
	}

	// convert result to VARIANT
	CComVariant var = GetServerLastError (pContext);
	var.Detach (strErrorCode);
	
	return VARIANT_TRUE;
}


// client

FUNC_API	BOOL WINAPI ClientAddRemoteDevManually (VARIANT szNetSettings)
{
	return ClientAddRemoteDevManuallyEx (szNetSettings, TRUE, FALSE);
}

FUNC_API	BOOL WINAPI ClientAddRemoteDevManuallyEx (VARIANT szNetSettings, BOOL bRemember, BOOL bAutoAdded)
{
	CString strNetSettings = szNetSettings;

	CUngNetSettings settings;
	if (strNetSettings.GetLength ())
	{
		CStringA str;
		CString strServer;
		int iPort;

		CSystem::ParseNetSettings (strNetSettings, strServer, iPort);

		settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_ADD]);
		settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_CLIENT]);
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);
		if (!bRemember)
			settings.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_DONT_SAVE_REG]);
		if (bAutoAdded)
			settings.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_AUTO_ADDED]);

		str  = settings.BuildSetttings ();
		str += CUngNetSettings::GetEnd ();
		
		CBuffer answer;
		return SendCommandToUsbService (str, answer) ? VARIANT_TRUE : VARIANT_FALSE;
	}
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientAddRemoteDev (PVOID pClientContext, long iIndex)
{
	return ClientAddRemoteDevEx (pClientContext, iIndex, TRUE, FALSE);
}

FUNC_API	BOOL WINAPI ClientAddRemoteDevEx (PVOID pClientContext, long iIndex, BOOL bRemember, BOOL bAutoAdded)
{
	TPtrVECCLEINTS pClint;
	CComVariant var;
	CUngNetSettings settings;

	g_csClient.Lock (4);
	pClint = FindClient (pClientContext);
	if (pClint)
	{
		if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			CString strServer;
			int iPort;

			CStringA str;

			// only remote server
			if (pClint->strServer.GetLength ())
			{
				CSystem::ParseNetSettings (ConvertNetSettingsServerToClient (pClint->strServer, pClint->Clients.at (iIndex).strNetSettings), strServer, iPort);

				settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_ADD]);
				settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_CLIENT]);
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
				settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);
				if (!bRemember)
					settings.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_DONT_SAVE_REG]);
				if (bAutoAdded)
					settings.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_AUTO_ADDED]);

				str = settings.BuildSetttings ();
				str += CUngNetSettings::GetEnd ();

				g_csClient.UnLock (4);

				CBuffer answer;
				return SendCommandToUsbService (str, answer) ? VARIANT_TRUE : VARIANT_FALSE;
			}
		}
	}
	g_csClient.UnLock (4);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientStartRemoteDev (PVOID pClientContext, long iIndex, BOOL bAutoReconnect, VARIANT varPassword)
{
	CStringA strAnswer;
	TPtrVECCLEINTS pClint;
	CComVariant var;
	CStringA	strPassword = CSystem::Encrypt (varPassword);
    CStringA str;


	g_csClient.Lock (5);
	pClint = FindClient (pClientContext);
	if (pClint)
	{
		if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			CString strServer;
			int iPort;
			CUngNetSettings settings;

			g_csClient.UnLock (5);

			CSystem::ParseNetSettings (ConvertNetSettingsServerToClient (pClint->strServer, pClint->Clients.at (iIndex).strNetSettings), strServer, iPort);

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_START_CLIENT]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_CLIENT]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM], bAutoReconnect);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_RDP], pClint->Clients.at (iIndex).bRdp);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_SESSION_ID], pClint->Clients.at (iIndex).uSession);
			if (strPassword.GetLength ())
			{
				settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH], strPassword);
			}

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			CBuffer answer;
		    return SendCommandToUsbService (str, answer) ? VARIANT_TRUE : VARIANT_FALSE;
		}
	}
	g_csClient.UnLock (5);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientStopRemoteDev (PVOID pClientContext, long iIndex)
{
    CStringA str;
	TPtrVECCLEINTS pClint;;

	g_csClient.Lock (6);
	pClint = FindClient (pClientContext);
	if (pClint)
	{
		if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			CString strServer;
			int iPort;
			CUngNetSettings settings;

			CSystem::ParseNetSettings (ConvertNetSettingsServerToClient (pClint->strServer, pClint->Clients.at (iIndex).strNetSettings), strServer, iPort);

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_STOP_CLIENT]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_CLIENT]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_RDP], pClint->Clients.at (iIndex).bRdp);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_SESSION_ID], pClint->Clients.at (iIndex).uSession);

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			g_csClient.UnLock (6);
			CBuffer answer;
		    return SendCommandToUsbService (str, answer) ? VARIANT_TRUE : VARIANT_FALSE;
		}
	}
	g_csClient.UnLock (6);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientRemoveRemoteDev (PVOID pClientContext, long iIndex)
{
    CStringA str;
	TPtrVECCLEINTS pClint;

	g_csClient.Lock (7);
	pClint = FindClient (pClientContext);
	if (pClint)
	{
		if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			CString strServer;
			int iPort;
			CUngNetSettings settings;


			CSystem::ParseNetSettings (ConvertNetSettingsServerToClient (pClint->strServer, pClint->Clients.at (iIndex).strNetSettings), strServer, iPort);

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_DEL]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_CLIENT]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			g_csClient.UnLock (7);
			CBuffer answer;
		    return SendCommandToUsbService (str, answer) ? VARIANT_TRUE : VARIANT_FALSE;
		}
	}
	g_csClient.UnLock (7);
	return VARIANT_FALSE;
}

// Helper class to keep common code in one place
// It finds, and possible updates, client element struct of [remote] shared device.
// Calling code can get element struct later with getter call.
class CClientElementGetter
{
	TPtrVECCLEINTS pClint;
	const CLIENT_ELMENT* pElement;

public:
	CClientElementGetter (PVOID pClientContext, long iIndex)
	{
		pElement = NULL;
		
		g_csClient.Lock (-1);
		pClint = FindClient (pClientContext);
		if (pClint)
		{
			if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
			{
				g_csClient.UnLock (-1);
				if (pClint->Clients [iIndex].strName.IsEmpty ())
				{
					ClientGetStateSharedDevice (pClientContext, iIndex, NULL, NULL);
				}

				pElement = &(pClint->Clients [iIndex]);
				return;
			}
		}
		g_csClient.UnLock (-1);
	}

	const CLIENT_ELMENT* GetClientStruct () {return pElement;}
};

FUNC_API BOOL WINAPI ClientTrafficRemoteDevIsCompressed (PVOID pClientContext, long iIndex, BOOL *bCompress)
{
	if (bCompress)
	{
		*bCompress = VARIANT_FALSE;

		CClientElementGetter helper (pClientContext, iIndex);
		const CLIENT_ELMENT *pElement = helper.GetClientStruct ();
		if (pElement)
		{
			*bCompress = pElement->bCompress?VARIANT_TRUE:VARIANT_FALSE;
			return VARIANT_TRUE;
		}
	}
	return VARIANT_FALSE;
}

FUNC_API BOOL WINAPI ClientTrafficRemoteDevIsEncrypted (PVOID pClientContext, long iIndex, BOOL *bCrypt)
{
	if (bCrypt)
	{
		*bCrypt = VARIANT_FALSE;

		CClientElementGetter helper (pClientContext, iIndex);
		const CLIENT_ELMENT *pElement = helper.GetClientStruct ();
		if (pElement)
		{
			*bCrypt = pElement->bCrypt?VARIANT_TRUE:VARIANT_FALSE;
			return VARIANT_TRUE;
		}
	}
	return VARIANT_FALSE;
}

FUNC_API BOOL WINAPI ClientRemoteDevRequiresAuth (PVOID pClientContext, long iIndex, BOOL *bAuth)
{
	if (bAuth)
	{
		*bAuth = VARIANT_FALSE;

		CClientElementGetter helper (pClientContext, iIndex);
		const CLIENT_ELMENT *pElement = helper.GetClientStruct ();
		if (pElement)
		{
			*bAuth = pElement->bAuth?VARIANT_TRUE:VARIANT_FALSE;
			return VARIANT_TRUE;
		}
	}
	return VARIANT_FALSE;
}

FUNC_API BOOL WINAPI ClientRemoteDisconnectIsEnabled (PVOID pClientContext, long iIndex, BOOL *bEnable)
{
	if (bEnable)
	{
		*bEnable = VARIANT_FALSE;

		CClientElementGetter helper (pClientContext, iIndex);
		const CLIENT_ELMENT *pElement = helper.GetClientStruct ();
		if (pElement)
		{
			*bEnable = pElement->bRDisconn?VARIANT_TRUE:VARIANT_FALSE;
			return VARIANT_TRUE;
		}
	}
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientGetStateRemoteDev (PVOID pClientContext, long iIndex, LONG *piState, VARIANT *RemoteHost)
{
	TPtrVECCLEINTS pClint;
	CComVariant var;

	g_csClient.Lock (11);
	pClint = FindClient (pClientContext);
	if (pClint)
	{
		if ((iIndex >= 0) && (iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			CStringA str;
			CString strServer;
			int iPort;
			CUngNetSettings settings;

			CSystem::ParseNetSettings (ConvertNetSettingsServerToClient (pClint->strServer, pClint->Clients.at (iIndex).strNetSettings), strServer, iPort);

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_INFO]);
			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_CLIENT]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_RDP], pClint->Clients.at (iIndex).bRdp);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_SESSION_ID], pClint->Clients.at (iIndex).uSession);

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			g_csClient.UnLock (11);

			CBuffer answer;
			if (SendCommandToUsbService (str, answer))
			{
				settings.Clear ();

				if (settings.ParsingSettings (CStringA (PCHAR (answer.GetData ()))))
				{
					*piState = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS]);
					var = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH]);
					return VARIANT_TRUE;
				}
			}
			return VARIANT_FALSE;
		}
	}
	g_csClient.UnLock (11);
	return VARIANT_FALSE;
}


// enum client devices
FUNC_API	BOOL WINAPI ClientEnumAvailRemoteDevOnServer (VARIANT szServer, PVOID *ppFindContext)
{
	CString strServer = szServer;
	SOCKET  Sock;

	if (strServer.IsEmpty ())
	{
		return VARIANT_FALSE;
	}

	Sock = OpenConnectionWithTimeout (strServer, Consts::uTcpPortConfig, 15*1000/*ms*/);
	if (Sock != INVALID_SOCKET)
	{
		*ppFindContext = GetCleintContext (Sock, strServer);
		closesocket (Sock);
		return VARIANT_TRUE;
	}
	
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientEnumAvailRemoteDevOnServerTimeout (VARIANT szServer, PVOID *ppFindContext, DWORD dwTimeoutMs)
{
	CString strServer = szServer;
	SOCKET  Sock;

	if (strServer.IsEmpty ())
	{
		return VARIANT_FALSE;
	}

	Sock = OpenConnectionWithTimeout (strServer, Consts::uTcpPortConfig, dwTimeoutMs);
	if (Sock != INVALID_SOCKET)
	{
		*ppFindContext = GetCleintContext (Sock, strServer);
		closesocket (Sock);
		return VARIANT_TRUE;
	}
	
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientEnumAvailRemoteDev (PVOID *ppFindContext)
{
	CStringA str;
	TVECCLEINT::iterator item;
	boost::shared_ptr <TVECCLEINTS> newClient (new TVECCLEINTS);
	CUngNetSettings settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_GETLISTOFCLIENTS]);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

	CBuffer answer;				
	if (SendCommandToUsbService (str, answer))
	{
		ParseClientConfiguration (newClient, answer);
	}

	g_csClient.Lock (13);
	g_Clinet.push_back (newClient);
	*ppFindContext = newClient.get ();
	g_csClient.UnLock (13);

	return VARIANT_TRUE;
}

FUNC_API	BOOL WINAPI ClientRemoveEnumOfRemoteDev (PVOID pFindContext)
{
	TVECCLIENT::iterator item;

	g_csClient.Lock (14);
	for (item = g_Clinet.begin (); item != g_Clinet.end (); item++)
	{
		if (item->get () == pFindContext)
		{
			g_Clinet.erase (item);
			g_csClient.UnLock (14);
			return VARIANT_TRUE;
		}
	}
	g_csClient.UnLock (14);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientGetRemoteDevNetSettings (PVOID pFindContext, long iIndex, VARIANT *NetSettings)
{
	TPtrVECCLEINTS pClint;

	g_csClient.Lock (15);
	pClint = FindClient (pFindContext);
	if (pClint)
	{
		if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			CComVariant var;

			g_csClient.UnLock (15);
			if (pClint->Clients [iIndex].bRdp)
			{
				CString strRet;
				LONG uPort = _wtoi (pClint->Clients [iIndex].strNetSettings);
				uPort = uPort & 0x7FFFFFFF;
				strRet.Format (_T("RDP%d"), uPort);		
				var = strRet;
			}
			else
			{
				var = ConvertNetSettingsServerToClient (pClint->strServer, pClint->Clients [iIndex].strNetSettings);
			}
			var.Detach (NetSettings);

			return VARIANT_TRUE;
		}
	}
	g_csClient.UnLock (15);
	return VARIANT_FALSE;
}

FUNC_API BOOL WINAPI ClientGetRemoteDevName (PVOID pFindContext, long iIndex, VARIANT *strName)
{
	TPtrVECCLEINTS pClint;

	g_csClient.Lock (16);
	pClint = FindClient (pFindContext);
	if (pClint)
	{
		if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			LONG State = 0;
			CComVariant var;

			g_csClient.UnLock (16);

			if (pClint->strServer.GetLength ())
			{
				CComVariant var;
				var = pClint->Clients [iIndex].strName;
				var.Detach (strName);

				return VARIANT_TRUE;
			}

			if (pClint->Clients [iIndex].strName.GetLength ())
			{
				CComVariant var;
				var = pClint->Clients [iIndex].strName;
				var.Detach (strName);

				return VARIANT_TRUE;
			}

			if (ClientGetStateRemoteDev (pFindContext, iIndex, &State, &var) && State == 2)
			{
				CString strServer;
				CStringA str;
				int iPort;
				CUngNetSettings settings;

				CSystem::ParseNetSettings (ConvertNetSettingsServerToClient (pClint->strServer, pClint->Clients.at (iIndex).strNetSettings), strServer, iPort);

				settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_GETDEVID]);
				settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], Consts::szpTypeItem [Consts::TI_CLIENT]);
				settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
				settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);

				str = settings.BuildSetttings ();
				str += CUngNetSettings::GetEnd ();

				CBuffer answer;				
				if (SendCommandToUsbService (str, answer))
				{
					settings.Clear ();
					if (settings.ParsingSettings (CStringA (PCHAR (answer.GetData ()))))
					{
						CComVariant var;
						var = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_NAME]);
						var.Detach (strName);
						return VARIANT_TRUE;
					}
				}
			}
			if (pClint->Clients [iIndex].strName.GetLength ())
			{
				CComVariant var;
				var = pClint->Clients [iIndex].strName;
				var.Detach (strName);
				return VARIANT_TRUE;
			}
			else
			{
				int iPos;
				CString strServer;

				iPos = pClint->Clients [iIndex].strNetSettings.Find (_T(":"));
				if (iPos == -1)
				{
					strServer = pClint->Clients [iIndex].strRemoteServer;
				}
				else
				{
					strServer = pClint->Clients [iIndex].strNetSettings.Left (iPos);
				}

				if (strServer.GetLength ())
				{
					CString str;
					CString strNetSettings = ConvertNetSettingsClientToServer (
						pClint->strServer, 
						pClint->Clients.at (iIndex).strNetSettings, 
						pClint->Clients.at (iIndex).strRemoteServer);

					if (GetShareDevClientDesk (strServer, strNetSettings, &str))
					{
						CComVariant var;
						var = str;
						var.Detach (strName);
						return VARIANT_TRUE;

					}
				}
				CComVariant var;
				var = L"";
				var.Detach (strName);
				return VARIANT_TRUE;
			}
		}
	}
	g_csClient.UnLock (16);
	return VARIANT_FALSE;
}

BOOL WINAPI ClientGetStateSharedDevice (PVOID pClientContext, long iIndex, LONG *piState, VARIANT *RemoteHost)
{
	CStringA str;
	TPtrVECCLEINTS pClint;

	g_csClient.Lock (17);
	pClint = FindClient (pClientContext);
	if (pClint)
	{
		if ((iIndex >= 0) && (iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
		{
			int iPos;
			CString strServer;

			g_csClient.UnLock (17);

			if (pClint->strServer.GetLength ())
			{
				CComVariant var;
				var = pClint->Clients [iIndex].strSharedWith;
				var.Detach (RemoteHost);
				*piState = pClint->Clients [iIndex].iStatus;
				return VARIANT_TRUE;
			}
			else
			{
				iPos = pClint->Clients [iIndex].strNetSettings.Find (_T(":"));
				if (iPos == -1)
				{
					strServer = pClint->Clients [iIndex].strRemoteServer;
				}
				else
				{
					strServer = pClint->Clients [iIndex].strNetSettings.Left (iPos);
				}

				CString strShared;
				CString strNetSettings = ConvertNetSettingsClientToServer (
					pClint->strServer, 
					pClint->Clients.at (iIndex).strNetSettings, 
					pClint->Clients.at (iIndex).strRemoteServer);

				if (GetShareDevInfoEx (strServer, strNetSettings, &(pClint->Clients.at (iIndex)), &strShared, piState)) 
				{
						if (RemoteHost)
						{
							CComVariant var;
							var = strShared;
							var.Detach (RemoteHost);
						}
						return VARIANT_TRUE;
				}
				return VARIANT_FALSE;
			}
		}
	}
	g_csClient.UnLock (17);
	return VARIANT_FALSE;
}

BOOL WINAPI ClientEnumRemoteDevOverRdp (PVOID *ppFindContext)
{
	CStringA str;
	TVECCLEINT::iterator item;
	boost::shared_ptr <TVECCLEINTS> newClient (new TVECCLEINTS);
	// Get session id
	ULONG	uSessionId = -1;
	LPTSTR szName = NULL;
	DWORD	dwSize = 0;
	WTSQuerySessionInformation ( WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId, 
					&szName, &dwSize );

	if (szName)
	{
		uSessionId = *((ULONG*)szName);
		WTSFreeMemory (szName);
	}

	CUngNetSettings settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_GETRDPSHARES]);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM], uSessionId);

	str = settings.BuildSetttings ();
	str += CUngNetSettings::GetEnd ();

	CBuffer answer;				
	if (SendCommandToUsbService (str, answer))
	{
		if (ParseClientConfiguration (newClient, answer))
		{
			for (item = newClient->Clients.begin (); item != newClient->Clients.end (); item++)
			{
				item->uSession = uSessionId;
				item->bRdp = true;
			}
		}
	}

	g_csClient.Lock (18);
	g_Clinet.push_back (newClient);

	*ppFindContext = newClient.get ();
	g_csClient.UnLock (18);

	return VARIANT_TRUE;
}

FUNC_API BOOL WINAPI ServerDisconnectRemoteDev (PVOID pContext, PVOID pDevContext)
{
	CStringA strConn;
	TMAPUSBDEV::iterator item;
	item = g_mapEnum.find (pContext);

	g_csServer.Lock (-1);
	if (item != g_mapEnum.end ())
	{
		CUsbDevInfo *pDev = item->second->m_mapDevs [pDevContext];
		if (pDev)
		{
			CStringA str;
			CUngNetSettings settings;

			g_csServer.UnLock (-1);

			settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_SRVDISCONN]);
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], pDev->m_strHubDev);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], pDev->m_iPort);

			str = settings.BuildSetttings ();
			str += CUngNetSettings::GetEnd ();

			CBuffer answer;				
			if (SendCommandToUsbService (str, answer))
			{
				return VARIANT_TRUE;
			}
			g_csServer.Lock (-1);
		}
	}
	g_csServer.UnLock (-1);
	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientGetRdpAutoconnect (void)
{
	BOOL bAuto = FALSE;

	CRegistry reg (Consts::szRegistry_usb4rdp, 0, KEY_READ);

	if (reg.ExistsValue(Consts::szAutoConnectParam)) 
	{
		bAuto = reg.GetBool (Consts::szAutoConnectParam);
	}
	else
	{
		ClientSetRdpAutoconnect (bAuto);
	}

	return bAuto ? VARIANT_TRUE : VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientGetRdpIsolation (void)
{
	BOOL bAuto = FALSE;

	CRegistry reg (Consts::szRegistry_usb4rdp, 0, KEY_READ);

	if (reg.ExistsValue(Consts::szIsolation)) 
	{
		bAuto = reg.GetBool (Consts::szIsolation);
	}
	else
	{
		ClientSetRdpIsolation(bAuto);
	}

	return bAuto ? VARIANT_TRUE : VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientSetRdpAutoconnect (BOOL bEnableAutoconnect)
{
	CStringA strCmd;
	CUngNetSettings	settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], 
		Consts::szpPipeCommand [Consts::CP_SET_AUTOCONNECT]);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM], bEnableAutoconnect);

	strCmd  = settings.BuildSetttings ();
	strCmd += CUngNetSettings::GetEnd ();

	CBuffer answer;				
	if (SendCommandToUsbService (strCmd, answer))
	{
		return VARIANT_TRUE;
	}

	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientSetRdpIsolation (BOOL bEnable)
{
	CStringA strCmd;
	CUngNetSettings	settings;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], 
		Consts::szpPipeCommand [Consts::CP_SET_RDP_ISOLATION]);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM], bEnable);

	strCmd  = settings.BuildSetttings ();
	strCmd += CUngNetSettings::GetEnd ();

	CBuffer answer;				
	if (SendCommandToUsbService (strCmd, answer))
	{
		return VARIANT_TRUE;
	}

	return VARIANT_FALSE;
}

FUNC_API	BOOL WINAPI ClientGetRemoteDevValueByName (PVOID pClientContext, long iIndex, 
                                                       VARIANT strValueName, VARIANT *pValue)
{
	TPtrVECCLEINTS pClint;
	const CLIENT_ELMENT* pElement = NULL;

	if (!pValue)
		return VARIANT_FALSE;

	do {
		g_csClient.Lock (-1);
		pClint = FindClient (pClientContext);
		if (pClint)
		{
			if ((iIndex >= 0) && int (pClint->Clients.size ()) > iIndex)
			{
				g_csClient.UnLock (-1);
				pElement = &(pClint->Clients [iIndex]);
				break;
			}
		}
		g_csClient.UnLock (-1);
		return VARIANT_FALSE;
	} while (0);

	CComVariant varResult;
	const int ix = Consts::SettingsCodeFromStr (CStringA (strValueName));
	switch (ix)
	{
	case Consts::UNS_NAME:				varResult = pElement->strRealName;					break;
	case Consts::UNS_DESK:				varResult = pElement->strNick;						break;
	case Consts::UNS_AUTH:				varResult = pElement->bAuth;						break;
	case Consts::UNS_CRYPT:				varResult = pElement->bCrypt;						break;
	case Consts::UNS_COMPRESS:			varResult = pElement->bCompress;					break;
	case Consts::UNS_STATUS:			varResult = pElement->iStatus;						break;
	case Consts::UNS_SHARED_WITH:		varResult = pElement->strSharedWith;				break;
	case Consts::UNS_DEV_ALLOW_RDISCON:	varResult = pElement->bRDisconn;					break;
	case Consts::UNS_HUB_NAME:			varResult = pElement->strUsbHub;					break;
	case Consts::UNS_USB_PORT:			varResult = pElement->strUsbPort;					break;
	case Consts::UNS_USB_LOC:			varResult = pElement->strUsbLoc;					break;
	case Consts::UNS_AUTO_ADDED:		varResult = pElement->bAutoAdded;					break;
	case Consts::UNS_DEV_USBCLASS:		varResult = pElement->UsbClassAsHexStrA ();			break;	// USBCLASS - USB device class triple as single hex-ascii string xxyyzz (where xx=class, yy=subclass, zz=protocol)
	case Consts::UNS_DEV_CLASS:			varResult = pElement->nUsbBaseClass;				break;
	case Consts::UNS_DEV_SUBCLASS:		varResult = pElement->nUsbSubClass;					break;
	case Consts::UNS_DEV_PROTOCOL:		varResult = pElement->nUsbProtocol;					break;
	case Consts::UNS_DEV_BCDUSB:		varResult = pElement->bcdUSB;						break;	// BCDUSB - version of the USB specification
	case Consts::UNS_DEV_VID:			varResult = pElement->idVendor;						break;	// VID - vendor identifier for the device
	case Consts::UNS_DEV_PID:			varResult = pElement->idProduct;					break;	// PID - product identifier
	case Consts::UNS_DEV_REV:			varResult = pElement->bcdDevice;					break;	// REV - version of the device
	case Consts::UNS_DEV_SN:			varResult = pElement->strSerialNumber;				break;	// SERIAL - serial number of device

	default:
		return VARIANT_FALSE;
	}

	varResult.Detach (pValue);
	return VARIANT_TRUE;
}	

FUNC_API	BOOL WINAPI ClientGetConnectedDevValueByName (PVOID pClientContext, long iIndex,
														  VARIANT strValueName, VARIANT *pValue)
{
	if (!pValue)
		return VARIANT_FALSE;

	TPtrVECCLEINTS pClient;
	const CLIENT_ELMENT* pElement = NULL;

	do {
		g_csClient.Lock (-1);
		pClient = FindClient (pClientContext);
		if (pClient)
		{
			if ((iIndex >= 0) && iIndex < int (pClient->Clients.size ()))
			{
				g_csClient.UnLock (-1);
				pElement = &(pClient->Clients [iIndex]);
				break;
			}
		}
		g_csClient.UnLock (-1);
		return VARIANT_FALSE;
	} while (0);
	
	// client found, ask usb service about its data
	// net settings of client
	CString strServer;
	int iPort;
	CSystem::ParseNetSettings (ConvertNetSettingsServerToClient (pClient->strServer, pClient->Clients.at (iIndex).strNetSettings), strServer, iPort);

	// build up command to usb service
	CUngNetSettings	settings;
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], 
		Consts::szpPipeCommand [Consts::CP_GET_CONN_CLIENT_INFO]);
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE], 
		Consts::szpTypeItem [Consts::TI_CLIENT]);
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], strServer);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], iPort);

	CStringA strCmd = settings.ComposeCommand ();

	CBuffer answer;
	if (!SendCommandToUsbService (strCmd, answer))
	{
		return VARIANT_FALSE;
	}

	settings.Clear ();
	if (!settings.ParsingSettings (CStringA ((PCHAR)answer.GetData ())))
	{
		return VARIANT_FALSE;
	}

	CStringA straName = CStringA (strValueName);
	int ixData = Consts::SettingsCodeFromStr (straName);
	switch (ixData)
	{
	case Consts::UNS_AUTH:
	case Consts::UNS_COMPRESS:
	case Consts::UNS_CRYPT:
		{
			// bools
			const bool bResult = settings.GetParamBoolean (straName);
			CComVariant var = bResult;
			var.Detach (pValue);
			return VARIANT_TRUE;
		}
		break;

	case Consts::UNS_STATUS:
		{
			// int
			if (!settings.ExistParam (straName))
				return VARIANT_FALSE;
			int iResult = settings.GetParamInt (straName);
			CComVariant varResult = iResult;
			varResult.Detach (pValue);
			return VARIANT_TRUE;
		}
		break;

	case Consts::UNS_DEV_USBCLASS:
		{
			// ascii string
			if (!settings.ExistParam (straName))
				return VARIANT_FALSE;
			const CStringA straResult = settings.GetParamStrA (straName);
			CComVariant varResult = straResult;
			varResult.Detach (pValue);
			return VARIANT_TRUE;
		}
		break;

	case Consts::UNS_NAME:
	case Consts::UNS_DESK:
	case Consts::UNS_HUB_NAME:
	case Consts::UNS_USB_PORT:
	case Consts::UNS_USB_LOC:
	case Consts::UNS_SHARED_WITH:
	default:
		{
			// strings
			const CString strResult = settings.GetParamStr (straName);
			CComVariant varResult = strResult;
			varResult.Detach (pValue);
			return VARIANT_TRUE;
		}
		break;
	}

	return VARIANT_FALSE;
}


FUNC_API	BOOL WINAPI CheckUsbService (void)
{
	return CheckSocketToUsbService () ? VARIANT_TRUE : VARIANT_FALSE;
}


FUNC_API void WINAPI UpdateUsbTree (void)
{
	CUngNetSettings	settings;
	CStringA str;

	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand [Consts::CP_UPDATEUSBTREE]);

	str  = settings.BuildSetttings ();
	str += settings.GetEnd ();

	CBuffer answer;				
	SendCommandToUsbService (str, answer);
}


BOOL WINAPI ClientRemoteDevDisconnect (PVOID pClientContext, long iIndex)
{
	TPtrVECCLEINTS pClient;
	const CLIENT_ELMENT* pElement = NULL;

	do {
		g_csClient.Lock (-1);
		pClient = FindClient (pClientContext);
		if (pClient)
		{
			if ((iIndex >= 0) && iIndex < int (pClient->Clients.size ()))
			{
				g_csClient.UnLock (-1);
				pElement = &(pClient->Clients [iIndex]);
				break;
			}
		}
		g_csClient.UnLock (-1);
		return VARIANT_FALSE;
	} while (0);

	// get address server and setting dev
	int iPos;
	CString strServer;

	iPos = pElement->strNetSettings.Find (_T(":"));
	if (iPos == -1)
	{
		strServer = pClient->strServer;
	}
	else
	{
		strServer = pElement->strNetSettings.Left (iPos);
	}

	if (strServer.GetLength ())
	{
		CString str;
		CString strNetSettings = ConvertNetSettingsClientToServer (
			pClient->strServer, 
			pElement->strNetSettings, 
			pElement->strRemoteServer);

		if (SendRemoteDisconnect (strServer, strNetSettings, pElement))
		{
			return VARIANT_TRUE;

		}
	}

	return VARIANT_FALSE;
}
