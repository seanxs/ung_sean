/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

	UsbHub.cpp
	
--*/

#include "stdafx.h"
#include "UsbHub.h"

std::auto_ptr <CUsbHub>	CUsbHub::m_pBase;

CUsbHub::CUsbHub(void)
{
	InitializeCriticalSection (&m_cs);
}

CUsbHub::~CUsbHub(void)
{
	DeleteCriticalSection (&m_cs);
}

CString CUsbHub::GetUsbHubName (LPCTSTR szHubGuid)
{
	CString strRet;
	if (!m_pBase.get ())
	{
		Init ();
	}

	if (m_pBase.get ())
	{
		EnterCriticalSection ( &m_pBase->m_cs );
		const CUsbDevInfo* pHub = m_pBase->m_UsbDev.FindHub (szHubGuid);

		if (pHub)
		{
			strRet = pHub->m_strHubDev;
		}

		// if it isn't found to reenum
		if (strRet.IsEmpty ())
		{
			CUsbDevInfo::EnumerateUsbTree (m_pBase->m_UsbDev);
			pHub = m_pBase->m_UsbDev.FindHub (szHubGuid);
			if (pHub)
			{
				strRet = pHub->m_strHubDev;
			}
		}
		LeaveCriticalSection ( &m_pBase->m_cs );
	}

	return strRet;	
}

void CUsbHub::Init ()
{
	m_pBase.reset (new CUsbHub ());
	CUsbDevInfo::EnumerateUsbTree (m_pBase->m_UsbDev);
}
