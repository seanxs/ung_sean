/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbService.cpp

Abstract:

	Main module. Processing of service's console commands

Environment:

    User mode

--*/

#include "stdafx.h"
#include <atlstr.h>
#include <WtsApi32.h>
#include "..\Consts\log.h"
#include "..\Consts\VersionInfo.hpp"
#include "..\Consts\consts.h"
#include "rdp_files\common\filetransport.h"
#include "rdp_files\RemoteSession.h"
#include "UsbService.h"
#include "service.h"
#include "consts.h"
#include "CommandService.h"
#include "Manager.h"

BOOL CUsbServiceApp::PreMain( int argc, LPTSTR* argv ) 
{
    bool	bRet = true;
    int		iLastError = 0;

    if (argc < 2)
    {  
        return bRet;
    }

    //return false;
    bRet = false;
    // install
    if (_tcsnicmp (Consts::szpCommand [Consts::SC_INSTALL], argv [1], _tcslen (Consts::szpCommand [Consts::SC_INSTALL])) == 0)
    {
        LPTSTR pszExtra = (argc > 2) ? argv [2] : NULL;
        if ((iLastError = CCommandService::InstallServiceEx (pszExtra)) != 0)
        {
            PrintErrorUtf8 (Consts::szpMessage [Consts::MES_BAD_INSTALL], iLastError);
        }
        else
        {
			_tprintf (Consts::szpMessage [Consts::MES_INSTALL]);
        }
    }
    // uninstall
    else if (_tcsnicmp (Consts::szpCommand [Consts::SC_UNINSTALL], argv [1], _tcslen (Consts::szpCommand [Consts::SC_UNINSTALL])) == 0)
    {
        if ((iLastError = CCommandService::UninstallService ()) != 0)
        {
			PrintErrorUtf8 (Consts::szpMessage [Consts::MES_BAD_UNINSTALL], iLastError);
        }
        else
        {
            _tprintf (Consts::szpMessage [Consts::MES_UNINSTALL]);
        }
    }
    // enable
    else if (_tcsnicmp (Consts::szpCommand [Consts::SC_ENABLE], argv [1], _tcslen (Consts::szpCommand [Consts::SC_ENABLE])) == 0)
    {
        if ((iLastError = CCommandService::EnableService  ()) != 0)
        {
            PrintErrorUtf8 (Consts::szpMessage [Consts::MES_BAD_ENABLE], iLastError);
        }
        else
        {
            _tprintf (Consts::szpMessage [Consts::MES_ENABLE]);
        }
    }
    // disable
    else if (_tcsnicmp (Consts::szpCommand [Consts::SC_DISABLE], argv [1], _tcslen (Consts::szpCommand [Consts::SC_DISABLE])) == 0)
    {
        if ((iLastError = CCommandService::DisableService ()) != 0)
        {
            PrintErrorUtf8 (Consts::szpMessage [Consts::MES_BAD_DISABLE], iLastError);
        }
        else
        {
            _tprintf (Consts::szpMessage [Consts::MES_DISABLE]);
        }
    }
	else
	{
		CCommandService::ParseAdditionCommand (argc, argv);
	}
    return false;
}

void CUsbServiceApp::PrintErrorUtf8 (LPCTSTR formatStr, int iError)
{
	CString str;
	str.Format (formatStr, ErrorToString(iError));
	CStringA utf8 = CT2CA (str, CP_UTF8);
	printf (utf8);
}

CString CUsbServiceApp::ErrorToString (int iError)
{
    CString str;
    LPVOID lpMessageBuffer = NULL;

    if (FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, 
        iError,  
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMessageBuffer,  
        0,  
        NULL )) 
    {
        str.Format(_T("(error %d) %s"), iError, (TCHAR*)lpMessageBuffer);
    } 
    else
    {
        str.Format (_T("%d"), iError);
    }

    if (lpMessageBuffer)
        LocalFree(lpMessageBuffer);

    str.Replace (_T("\r\n"), _T(""));

    return str;
}

SFL_BEGIN_SERVICE_MAP(CUsbServiceApp)
    SFL_SERVICE_ENTRY (CService, Consts::szServiceName)
SFL_END_SERVICE_MAP()

