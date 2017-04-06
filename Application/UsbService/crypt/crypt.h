/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    crypt.h

Abstract:

	Encryption plugin. It is used for RSA keys transfer, which is encrypted by RC4

Environment:

    User mode

--*/
#ifndef BASE_CRYPT_H
#define BASE_CRYPT_H

#define SECURITY_WIN32

#include "stdafx.h"
#include <winsock2.h>
#include "CryptApi.h"

#ifdef CRYPT_EXPORTS
#define FUNC_API __declspec(dllexport)
#else
#define FUNC_API __declspec(dllimport)
#endif
 
#pragma comment( lib, "ws2_32.lib" )

extern "C" 
{

FUNC_API int BeginConnection (SOCKET sock, WCHAR *szInitString, void **ppContext);
FUNC_API int CloseConnection (SOCKET sock, void *pContext);
FUNC_API int PreSendData (void *pContext, char *pIn, int iIn, char *pOut, int *piOut);
FUNC_API int PostRecvData (void *pContext, char *pIn, int iIn, char *pOut, int *piOut);

}

typedef struct
{
	CCryptApi	Crypt;
} SECURUTY_CONTEXT;

enum ERROR_CRYPT
{
	EC_SUCCESS,
	EC_SOCKET_ERROR,
	EC_CRYPT_API_INIT_ERROR,
	EC_CRYPT_ERROR,
};

int InitServer (SOCKET sock, SECURUTY_CONTEXT *ppContext);
int InitClient (SOCKET sock, SECURUTY_CONTEXT *ppContext);

extern int iRand;
extern CRITICAL_SECTION	cs;
#endif