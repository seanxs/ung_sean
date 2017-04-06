/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Security.cpp

Abstract:

	Class responsible for auth and crypt initialization

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "Security.h"
#include "System.h"

CSecurity::CSecurity(void)
	: m_hCrypt (NULL)
	, m_hAuth (NULL)
	, cryptCloseConnection (NULL)
{
	Clear ();
}

CSecurity::~CSecurity(void)
{
	Clear ();
}

bool CSecurity::GetIsCrypt () const
{
	return m_bIsCrypt;
}

bool CSecurity::GetIsAuth () const
{
	return m_bIsAuth;
}

void CSecurity::ClearAuth ()
{
	m_bIsAuth = false;

	if (m_hAuth)
	{
		FreeLibrary (m_hAuth);
		m_hAuth = NULL;
	}
	authBeginConnection = NULL;
	m_strPassw = _T("");
}

void CSecurity::ClearCrypt ()
{
	m_bIsCrypt = false;

	if (m_hCrypt)
	{
		if (cryptCloseConnection)
		{
			cryptCloseConnection (INVALID_SOCKET, m_pContext);
		}
		
		FreeLibrary (m_hCrypt);
		m_hCrypt = NULL;
	}

	cryptBeginConnection = NULL;
	cryptCloseConnection = NULL;
	cryptPreSendData = NULL;
	cryptPostRecvData = NULL;
	m_pContext = NULL;
}

void CSecurity::Clear ()
{
    ClearAuth ();
    ClearCrypt ();
}

// crypt.dll:auth.dll^PASSWORD
bool CSecurity::SetAuth (CString strPassw, bool bEncrypt)
{
	CString strLib;

#ifdef WIN64
	strLib = "auth64.dll";
#else
	strLib = "auth.dll";
#endif

    m_bIsAuth = false;
    m_strPassw = _T("");

    if (strPassw.GetLength ())
    {
	    // loading library
	    m_hAuth = LoadLibrary (strLib);
	    if (!m_hAuth)
	    {
		    return false;
	    }
	    // getting functions
	    authBeginConnection = (BeginConnectionAuthFn)GetProcAddress (m_hAuth, "BeginConnectionAuth");
	    if (!authBeginConnection)
	    {
		    return false;
	    }

	    // Getting Password
	    m_strPassw = strPassw;
	    if (bEncrypt)
	    {
			m_strPassw = CSystem::Decrypt (CStringA (m_strPassw));
	    }

	    m_bIsAuth = true;
        return true;
    }

    return false;
}

bool CSecurity::SetCrypt ()
{
	CString strLib;

	// loading library
#ifdef WIN64
	strLib = "crypt64.dll";
#else
	strLib = "crypt.dll";
#endif

	m_hCrypt = LoadLibrary (strLib);
	if (!m_hCrypt)
	{
		return false;
	}
	// getting fuctions
	cryptBeginConnection = (BeginConnectionFn)GetProcAddress (m_hCrypt, "BeginConnection");
	if (!cryptBeginConnection)
	{
		return false;
	}
	cryptCloseConnection = (CloseConnectionFn)GetProcAddress (m_hCrypt, "CloseConnection");
	if (!cryptCloseConnection)
	{
		return false;
	}
	cryptPreSendData = (PreSendDataFn)GetProcAddress (m_hCrypt, "PreSendData");
	if (!cryptPreSendData)
	{
		return false;
	}
	cryptPostRecvData = (PostRecvDataFn)GetProcAddress (m_hCrypt, "PostRecvData");
	if (!cryptPostRecvData)
	{
		return false;
	}
	m_bIsCrypt = true;
    return true;
}
