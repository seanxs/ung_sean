/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UsbHub.h

--*/

#pragma once

#include "..\Consts\UsbDevInfo.h"

class CUsbHub;

class CUsbHub
{
private:
	CUsbHub(void);

public:
	~CUsbHub(void);

	static void Init ();
	static CString GetUsbHubName (LPCTSTR szHubGuid);
	static std::auto_ptr <CUsbHub>	m_pBase;

private:
	CUsbDevInfo	m_UsbDev;
	CRITICAL_SECTION m_cs;
};
