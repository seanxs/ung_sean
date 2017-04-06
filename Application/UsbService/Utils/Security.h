/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Security.h

Abstract:

	Class responsible for auth and crypt initialization

Environment:

    User mode

--*/

#pragma once

#include <atlstr.h>

typedef int (*BeginConnectionFn) (SOCKET sock, WCHAR *szInitString, void **ppContext);
typedef int (*CloseConnectionFn) (SOCKET sock, void *pContext);
typedef int (*PreSendDataFn) (void *pContext, char *pIn, int iIn, char *pOut, int *piOut);
typedef int (*PostRecvDataFn) (void *pContext, char *pIn, int iIn, char *pOut, int *piOut);
typedef int (*BeginConnectionAuthFn) (SOCKET sock, WCHAR *szInitString);


class CSecurity
{
public:
	CSecurity(void);
	~CSecurity(void);

	void Clear ();
    void ClearAuth ();
    void ClearCrypt ();

// crypt
	BeginConnectionFn cryptBeginConnection;
	CloseConnectionFn cryptCloseConnection;
	PreSendDataFn cryptPreSendData;
	PostRecvDataFn cryptPostRecvData;

	int PostRecvData (char *pIn, int iIn, char *pOut, int *piOut)
	{
		return cryptPostRecvData (m_pContext, pIn, iIn, pOut, piOut);
	}

	int PreSendData (char *pIn, int iIn, char *pOut, int *piOut)
	{
		return cryptPreSendData (m_pContext, pIn, iIn, pOut, piOut);
	}

	HMODULE m_hCrypt;

	bool	m_bIsCrypt;
	bool	GetIsCrypt () const;
	void	*m_pContext;

// auth
	BeginConnectionAuthFn authBeginConnection;
	HMODULE m_hAuth;
	bool	m_bIsAuth;
	bool	GetIsAuth () const;
	CString	m_strPassw;

	// methods
    bool SetAuth (CString strPassw, bool bEncrypt = true);
    bool SetCrypt ();
};
