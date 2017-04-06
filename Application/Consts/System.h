/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    System.h

Abstract:
	
	Class with additional utilities functions

Environment:

    User mode

--*/

#pragma once
#include "atlstr.h"
#include "UngNetSettings.h"

class CSystem
{
public:
	// WinXP64
	static BOOL IsWow64();
	static CStringA FromUnicode(CStringW const & Name);
	static CStringW ToUnicode(CStringA const & Name);
	static CStringA Encrypt (CStringW const & Passw);
	static CStringW Decrypt (CStringA const & Passw);
	static CString BuildClientName (const CUngNetSettings &settings);
	// parses "host:port" or just "port" string and stores separate parts; 
	// returns true if both parts (host and port) are present in source string.
	static bool ParseNetSettings (LPCTSTR szParam, CString &strServer, int &iPort);
	static bool ParseNetSettings (const CString &strParam, CString &strServer, int &iPort);

    static CStringA EchoSpecSymbol (CStringA strText);
    static CStringA UnEchoSpecSymbol (CStringA strText);
    static CString EchoSpecSymbolU (CString strText);
    static CString UnEchoSpecSymbolU (CString strText);

    static bool IsGuiExe ();
	static CString ErrorToString (int iError);

#ifndef NO_NETWORK
	static CString GetHostName (in_addr *in, int Size);
#endif

};
