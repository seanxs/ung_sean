/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    auth.h

Abstract:

	Authorization plugin

Environment:

    User mode

--*/
#ifndef BASE_AUTH_H
#define BASE_AUTH_H

#include <winsock2.h>
#include "stdafx.h"
#include "..\Crypt\CryptApi.h"

#pragma comment( lib, "ws2_32.lib" )

#ifdef AUTH_EXPORTS
#define FUNC_API __declspec(dllexport)
#else
#define FUNC_API __declspec(dllimport)
#endif

extern "C" {

FUNC_API int BeginConnectionAuth (SOCKET sock, WCHAR *szInitString);

}

extern int iRand;
extern CRITICAL_SECTION	cs;

enum ERROR_AUTH
{
	EA_SUCCESS,
	EA_SOCKET_ERROR,
	EA_CRYPT_API_INIT_ERROR,
	EA_CRYPT_ERROR,
};

int AuthServer (SOCKET sock, char MD5Key[16], CCryptApi	&auth);
int AuthClient (SOCKET sock, char MD5Key[16], CCryptApi	&auth);

#endif