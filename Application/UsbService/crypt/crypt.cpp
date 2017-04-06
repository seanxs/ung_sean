/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    crypt.cpp

Abstract:

	Encryption plugin. It is used for RSA keys transfer, which is encrypted by RC4

Environment:

    User mode

--*/

#include "stdafx.h"
#include "crypt.h"
//#include <time.h>
#include <ATLComTime.h>

#ifdef _MANAGED
#pragma managed(push, off)
#endif

int iRand = 340;
CRITICAL_SECTION	cs;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
        iRand = GetTickCount ();
		InitializeCriticalSection (&cs);
		break;
	case DLL_PROCESS_DETACH:
		DeleteCriticalSection (&cs);
		break;
	};
    return TRUE;
}

FUNC_API int BeginConnection (SOCKET sock, WCHAR *szInitString, void **ppContext)
{
	int	iFirst;
	int iSecond;
	int iCount;
	SECURUTY_CONTEXT *pContext;


	pContext = new SECURUTY_CONTEXT;
	if (pContext->Crypt.Initialization ())
	{
		delete pContext;
		return EC_CRYPT_API_INIT_ERROR;
	}

	EnterCriticalSection (&cs);
	iRand = (45*iRand + 5000) ^ 54321;
	srand (iRand);
	ATLTRACE ("%d\n", iRand);
	LeaveCriticalSection (&cs);
	
	do 
	{
		iFirst = rand ();
		iCount = send (sock, (char*)&iFirst, sizeof (int), 0);
		if (iCount == SOCKET_ERROR || iCount == 0)
		{
			return EC_SOCKET_ERROR;
		}

		iCount = recv (sock, (char*)&iSecond, sizeof (int), 0);
		if (iCount == SOCKET_ERROR || iCount != sizeof (int))
		{
			return EC_SOCKET_ERROR;
		}
	}
	while (iFirst == iSecond);

	if (iFirst > iSecond)
	{
		iCount = InitServer (sock, pContext);
	}
	else
	{
		iCount = InitClient (sock, pContext);
	}

	if (iCount == EC_SUCCESS)
	{
		*ppContext = pContext;
	}
	else
	{
		delete pContext;
		*ppContext = NULL;
	}

	return iCount;
}

FUNC_API int CloseConnection (SOCKET sock, void *pContext)
{
	delete (SECURUTY_CONTEXT *)pContext;
	return 0;
}

FUNC_API int PreSendData (void *pContext, char *pIn, int iIn, char *pOut, int *piOut)
{
	SECURUTY_CONTEXT *pSecury = (SECURUTY_CONTEXT*)pContext;
	
	*piOut = iIn;
	if (pOut && pContext)
	{
		DWORD	dw = *piOut;
		RtlCopyMemory (pOut, pIn, iIn);
		if (pSecury->Crypt.Encrypt ((LPBYTE)pOut, dw, 1))
		{
			return EC_CRYPT_ERROR;
		}
		*piOut = dw;
	}
	return 0;
}

FUNC_API int PostRecvData (void *pContext, char *pIn, int iIn, char *pOut, int *piOut)
{
	SECURUTY_CONTEXT *pSecury = (SECURUTY_CONTEXT*)pContext;

	*piOut = iIn;
	if (pOut && pContext)
	{
		DWORD	dw = *piOut;
		RtlCopyMemory (pOut, pIn, iIn);
		if (pSecury->Crypt.Decrypt ((LPBYTE)pOut, dw, 2))
		{
			return EC_CRYPT_ERROR;
		}
		*piOut = dw;
	}
	return 0;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif


int InitServer (SOCKET sock, SECURUTY_CONTEXT *pContext)
{
	RSAPubKey1024 pubKey;
	int			iCount;
	RC4Key		keyRc4;


	do 
	{
		// Create RSA key
		if (pContext->Crypt.CreateRSAKey ())
		{
			break;
		}

		if (pContext->Crypt.ExportRSAPublicKey (pubKey))
		{
			break;
		}

		iCount = send (sock, (char*)&pubKey, sizeof (RSAPubKey1024), 0);
		if (iCount != sizeof (RSAPubKey1024))
		{
			break;
		}

		// Get RC4 first key
		iCount = recv (sock, (char*)&keyRc4, sizeof (RC4Key), 0);
		if (iCount != sizeof (RC4Key))
		{
			break;
		}

		if (pContext->Crypt.ImportRC4Key (keyRc4, 1))
		{
			break;
		}
		// Get RC4 second key
		iCount = recv (sock, (char*)&keyRc4, sizeof (RC4Key), 0);
		if (iCount != sizeof (RC4Key))
		{
			break;
		}

		if (pContext->Crypt.ImportRC4Key (keyRc4, 2))
		{
			break;
		}
		return 0;
	}
	while (false);

	return EC_CRYPT_API_INIT_ERROR;
}

int InitClient (SOCKET sock, SECURUTY_CONTEXT *pContext)
{
	RSAPubKey1024 pubKey;
	int			iCount;
	RC4Key		keyRc4;


	do 
	{
		// Get RSA key
		iCount = recv (sock, (char*)&pubKey, sizeof (RSAPubKey1024), 0);
		if (iCount != sizeof (RSAPubKey1024))
		{
			break;
		}

		if (pContext->Crypt.ImportRSAPublicKey (pubKey))
		{
			break;
		}

		// Get RC4 first key
		if(pContext->Crypt.CreateRC4Key(2))
		{
			break;
		}

		if(pContext->Crypt.ExportRC4Key (keyRc4, 2))
		{
			break;
		}
		iCount = send (sock, (char*)&keyRc4, sizeof (RC4Key), 0);
		if (iCount != sizeof (RC4Key))
		{
			break;
		}
		// Get RC4 second key
		if(pContext->Crypt.CreateRC4Key(1))
		{
			break;
		}

		if(pContext->Crypt.ExportRC4Key (keyRc4, 1))
		{
			break;
		}
		iCount = send (sock, (char*)&keyRc4, sizeof (RC4Key), 0);
		if (iCount != sizeof (RC4Key))
		{
			break;
		}
		return 0;
	}
	while (false);

	return EC_CRYPT_API_INIT_ERROR;
}
