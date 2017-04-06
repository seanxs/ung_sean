/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    System.cpp

Abstract:
	
	Class with additional utilities functions

Environment:

    User mode

--*/

#include "stdafx.h"
#include "System.h"
#include "Consts.h"

#include <Shellapi.h>  // For CommandLineToArgvW, see IsGuiExe function


typedef BOOL (APIENTRY *LPFN_ISWOW64PROCESS) (HANDLE hProcess,PBOOL Wow64Process);
static LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(_T("kernel32")),"IsWow64Process");
 
BOOL CSystem::IsWow64()
{
    BOOL bIsWow64 = FALSE;
 
    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            // handle error
        }
    }
    return bIsWow64;
}

CStringA CSystem::FromUnicode(CStringW const & Name)
{
  int len = Name.GetLength();
  CStringA result;

  if (len)
  {
    int bufSize = WideCharToMultiByte(CP_UTF8, 0, Name, len, NULL, 0, NULL, NULL);
    if (bufSize)
    {
      bufSize = WideCharToMultiByte(CP_UTF8, 0, Name, len, result.GetBuffer(bufSize + 2), bufSize, NULL, NULL);
      if (bufSize)
        result.ReleaseBuffer(bufSize);
      else
        result.Empty();
    }
  }

  return result;
}

CStringW CSystem::ToUnicode(CStringA const & Name)
{
  int len = Name.GetLength();
  CStringW result;

  if (len)
  {
    int bufSize = MultiByteToWideChar(CP_UTF8, 0, Name, len, NULL, 0);
    if (bufSize)
    {
      bufSize = MultiByteToWideChar(CP_UTF8, 0, Name, len, result.GetBuffer(bufSize + 2), bufSize);
      if (bufSize)
        result.ReleaseBuffer(bufSize);
      else
        result.Empty();
    }
  }

  return result;
}

CStringA CSystem::Encrypt (CStringW const & Passw)
{
	CStringA strTemp = FromUnicode (Passw);
	CStringA strReturn;
	unsigned char	ch;

	for (int a = 0; a < strTemp.GetLength (); a++)
	{
		ch = strTemp [a];
		ch ^= 0xAA;
		strReturn.AppendFormat ("%2x", ch);
	}
	return strReturn;
}

CStringW CSystem::Decrypt (CStringA const & Passw)
{
	CStringA strTemp;
	CStringA strReturn;
	unsigned char	ch;
	char	*Buff;

	for (int a = 0; a < Passw.GetLength (); a = a + 2)
	{
		strTemp = Passw.Mid (a, 2);
		ch = char (strtol (strTemp, &Buff, 16));
		ch ^= 0xAA;
		strReturn += ch;
	}

	return ToUnicode (strReturn);
}

CString CSystem::BuildClientName (const CUngNetSettings &settings)
{
	CString str;
	CString strTemp;
	CString strDesk;

	strTemp = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_HUB_NAME]);

	if (strTemp.GetLength ())
	{
		str  = strTemp;
		str += _T(": ");
	}
	str += settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_USB_PORT]);
	strTemp = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_NAME]);
	strDesk = settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_DESK]);
	if (strDesk.GetLength ())
	{
		str += _T("(");
		str += strDesk;
		if (strTemp.GetLength ())
		{
			str += _T(" / ");
			str += strTemp;
		}
		str += _T(")");
	}
	else
	{
		if (strTemp.GetLength ())
		{
			if (str.GetLength ())
			{
				str += _T("(");
				str += strTemp;
				str += _T(")");
			}
			else
			{
				str = strTemp;
			}
		}
	}

	return str;
}

bool CSystem::ParseNetSettings (LPCTSTR szParam, CString &strServer, int &iPort)
{
	return CSystem::ParseNetSettings (CString (szParam), strServer, iPort);	
}

bool CSystem::ParseNetSettings (const CString &strParam, CString &strServer, int &iPort)
{
	int iPos;

	iPos = strParam.Find (_T(":"));

	if (iPos == -1)
	{
		// only port
		strServer = _T("");
		iPort = _ttoi (strParam);
		return false;
	}
	else
	{
		// server:port
		strServer = strParam.Left (iPos);
		iPort = _ttoi (strParam.Mid (iPos + 1));
		return true;
	}
}

CStringA CSystem::EchoSpecSymbol (CStringA strText)
{
    strText.Replace ("@", "@2");
	strText.Replace (",", "@6");
	strText.Replace ("^", "@5");
	strText.Replace (":", "@4");
    strText.Replace ("/", "@3");
    strText.Replace ("!", "@1");
    return strText;
}

CStringA CSystem::UnEchoSpecSymbol (CStringA strText)
{
    strText.Replace ("@1", "!");
    strText.Replace ("@3", "/");
	strText.Replace ("@4", ":");
	strText.Replace ("@5", "^");
	strText.Replace ("@6", ",");
    strText.Replace ("@2", "@");
    return strText;
}

CString CSystem::EchoSpecSymbolU (CString strText)
{
    strText.Replace (_T("@"), _T("@2"));
	strText.Replace (_T(","), _T("@6"));
	strText.Replace (_T("^"), _T("@5"));
	strText.Replace (_T(":"), _T("@4"));
    strText.Replace (_T("/"), _T("@3"));
    strText.Replace (_T("!"), _T("@1"));
    return strText;
}

CString CSystem::UnEchoSpecSymbolU (CString strText)
{
    strText.Replace (_T("@1"), _T("!"));
    strText.Replace (_T("@3"), _T("/"));
	strText.Replace (_T("@4"), _T(":"));
	strText.Replace (_T("@5"), _T("^"));
	strText.Replace (_T("@6"), _T(","));
    strText.Replace (_T("@2"), _T("@"));
    return strText;
}

bool CSystem::IsGuiExe ()
{
	LPTSTR lptstrCmdLine = ::GetCommandLine ();

	CString strCmdLine = CString (lptstrCmdLine);
	CStringW wstrCmdLine = CStringW (strCmdLine);

	LPWSTR *szArgList;
	int argCount;

	szArgList = ::CommandLineToArgvW (wstrCmdLine, &argCount);
	if (szArgList == NULL || argCount <= 0)
	{
		return false;
	}

	CStringW wstrExePath = CStringW (szArgList [0]);
	// get only base name
	const int ixBackSlash = wstrExePath.ReverseFind (L'\\');
	const int ixFwdSlash = wstrExePath.ReverseFind (L'/');
	int ixSlash = ixBackSlash;
	if (ixBackSlash < ixFwdSlash)
	{
		ixSlash = ixFwdSlash;
	}

	CStringW wstrExe = wstrExePath;
	if (ixSlash >= 0)
	{
		wstrExe = wstrExePath.Mid (ixSlash+1);
	}

	const bool result = wstrExe.CompareNoCase (L"UsbConfig.exe") == 0;

	LocalFree (szArgList);
	
	return result;
}

CString CSystem::ErrorToString (int iError)
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
		str = (TCHAR*)lpMessageBuffer;
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

#ifndef NO_NETWORK

CString CSystem::GetHostName (in_addr *in, int Size)
{
	CString ret;
	struct hostent* hs = gethostbyaddr((const char*)in, Size, AF_INET);
	if (hs)
	{
		ret = CString (hs->h_name);
	}
	else
	{
		ret = CString (inet_ntoa (*in));
	}

	return ret;
}

#endif
