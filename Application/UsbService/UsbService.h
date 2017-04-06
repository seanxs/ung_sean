/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbService.h

Abstract:

	Main module. Processing of service's console commands

Environment:

    User mode

--*/

#include "stdafx.h"
#include "SflBase.h"
#include <atlstr.h>

class CUsbServiceApp: public CServiceAppT<CUsbServiceApp>
{
public:
    BOOL PreMain( int argc, LPTSTR* argv );
	CString ErrorToString (int iError);
	void PrintErrorUtf8 (LPCTSTR formatStr, int iError);
};
