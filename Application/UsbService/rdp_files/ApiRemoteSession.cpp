/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ApiRemoteSession.cpp

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "ApiRemoteSession.h"


CApiRemoteSession::CApiRemoteSession(void)
	: m_hCitrix (NULL)
{
#ifdef WIN64
	m_hCitrix = LoadLibrary (L"wfapi64.dll");
#else
	m_hCitrix = LoadLibrary (L"wfapi.dll");
#endif
	if (m_hCitrix)
	{
		m_pVirtualChannelQuery = (PFUNCVirtualChannelQuery)GetProcAddress (m_hCitrix, "WFVirtualChannelQuery");
		m_pFreeMemory = (PFUNCFreeMemory)GetProcAddress (m_hCitrix, "WFFreeMemory");
		m_pVirtualChannelOpen = (FUNCVirtualChannelOpen)GetProcAddress (m_hCitrix, "WFVirtualChannelOpen");
		m_pVirtualChannelClose = (PFUNCVirtualChannelClose)GetProcAddress (m_hCitrix, "WFVirtualChannelClose");
	}
	else
	{
		m_pVirtualChannelQuery = NULL;
		m_pFreeMemory = WTSFreeMemory;
		m_pVirtualChannelOpen = WTSVirtualChannelOpen;
		m_pVirtualChannelClose = WTSVirtualChannelClose;
	}
}

CApiRemoteSession::~CApiRemoteSession(void)
{
	if (m_hCitrix)
	{
		FreeLibrary (m_hCitrix);
		m_hCitrix = NULL;
	}
}


HANDLE CApiRemoteSession::VirtualChannelOpen (HANDLE hServer, DWORD  SessionId, LPSTR  pVirtualName)
{
	HANDLE hRet;

	hRet = WTSVirtualChannelOpen (hServer, SessionId, pVirtualName);

	if (hRet)
	{
		CAutoLock AutoLock (&m_lock);
		setChannelRdp.insert (hRet);
	}
	else if (m_pVirtualChannelOpen)
	{
		hRet = m_pVirtualChannelOpen (hServer, SessionId, pVirtualName);
	}
	return hRet;
}

BOOL CApiRemoteSession::VirtualChannelQuery (HANDLE hChannelHandle, int VirtualClass, PVOID *ppBuffer, DWORD *pBytesReturned)
{
	if (m_pVirtualChannelQuery)
	{
		return m_pVirtualChannelQuery (hChannelHandle, (WF_VIRTUAL_CLASS)VirtualClass, ppBuffer, pBytesReturned);
	}
	return false;
}

VOID CApiRemoteSession::FreeMemory (IN PVOID pMemory)
{
	if (m_pFreeMemory)
	{
		m_pFreeMemory (pMemory);
	}
}

BOOL CApiRemoteSession::VirtualChannelClose (HANDLE hChannelHandle)
{
	BOOL bRet = false;
	if (m_pVirtualChannelClose)
	{
		m_lock.Lock ();
		SETHANDLE::iterator find = setChannelRdp.find (hChannelHandle);
		if (find != setChannelRdp.end ())
		{
			setChannelRdp.erase (find);
			m_lock.UnLock ();
			bRet = WTSVirtualChannelClose (hChannelHandle);
		}
		else
		{
			m_lock.UnLock ();
			bRet = m_pVirtualChannelClose (hChannelHandle);
		}
	}
	return bRet;
}

