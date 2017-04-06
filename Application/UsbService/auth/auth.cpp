/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    auth.cpp

Abstract:

	Authorization plugin. It uses CrypApi/MD5

Environment:

    User mode

--*/

#include "stdafx.h"
#include "auth.h"


#ifdef _MANAGED
#pragma managed(push, off)
#endif

int iRand = 23423;
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

FUNC_API int BeginConnectionAuth (SOCKET sock, WCHAR *szInitString)
{
	int	iFirst;
	int iSecond;
	int iCount;
	CCryptApi	auth;
	char MD5Key[16];

	if (auth.Initialization ())
	{
		return EA_CRYPT_API_INIT_ERROR;
	}

	if (!CreateMD5 ((LPBYTE)szInitString, int (wcslen (szInitString) * sizeof (WCHAR)), MD5Key))
	{
		return EA_CRYPT_API_INIT_ERROR;
	}

	EnterCriticalSection (&cs);
	iRand = (45*iRand + 5000) ^ 54321;
	srand (iRand);
	LeaveCriticalSection (&cs);
	
	do 
	{
		iFirst = rand ();
		iCount = send (sock, (char*)&iFirst, sizeof (int), 0);
		if (iCount == SOCKET_ERROR && iCount == 0)
		{
			return EA_SOCKET_ERROR;
		}

		iCount = recv (sock, (char*)&iSecond, sizeof (int), 0);
		if (iCount == SOCKET_ERROR && iCount != sizeof (int))
		{
			return EA_SOCKET_ERROR;
		}
	}
	while (iFirst == iSecond);

	if (iFirst > iSecond)
	{
		return AuthServer (sock, MD5Key, auth);
	}
	else
	{
		return AuthClient (sock, MD5Key, auth);
	}


	return 0;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

int AuthServer (SOCKET sock, char MD5Key[16], CCryptApi	&auth)
{
	RSAPubKey1024 pubKey;
	int			iCount;
	DWORD		SizeBuf;
	char		MD5Key_clint[128];


	do 
	{
		// Create RSA key
		if (auth.CreateRSAKey ())
		{
			break;
		}

		if (auth.ExportRSAPublicKey (pubKey))
		{
			break;
		}

		iCount = send (sock, (char*)&pubKey, sizeof (RSAPubKey1024), 0);
		if (iCount != sizeof (RSAPubKey1024))
		{
			break;
		}

		iCount = recv (sock, (char*)MD5Key_clint, 128, 0);
		if (iCount != 128)
		{
			break;
		}

		// decrypt MD5
		SizeBuf = 128;
		if(!CryptDecrypt (auth.hKeyRSA, 0, false, 0,(BYTE*)MD5Key_clint, &SizeBuf))
		{
			return EA_CRYPT_ERROR;
		}		
		// Compare
		iCount = RtlEqualMemory (MD5Key_clint, MD5Key, 16);
		SizeBuf = send (sock, (char*)&iCount, sizeof (int), 0);
		if (SizeBuf != sizeof (int))
		{
			break;
		}

		if (!iCount)
		{
			return EA_CRYPT_ERROR;
		}
		
		return 0;
	}
	while (false);

	return EA_CRYPT_API_INIT_ERROR;
}

int AuthClient (SOCKET sock, char MD5Key[16], CCryptApi	&auth)
{
	RSAPubKey1024 pubKey;
	int			iCount;
	DWORD		SizeBuf;
	char		MD5KetCrypt [128];


	do 
	{
		// Get RSA key
		iCount = recv (sock, (char*)&pubKey, sizeof (RSAPubKey1024), 0);
		if (iCount != sizeof (RSAPubKey1024))
		{
			break;
		}

		if (auth.ImportRSAPublicKey (pubKey))
		{
			break;
		}

		// Crypt MD5
		RtlCopyMemory (MD5KetCrypt, MD5Key, 16);
		SizeBuf = 16;
		if(!CryptEncrypt (auth.hKeyRSAPublic ,0,false,0,(BYTE*)MD5KetCrypt, &SizeBuf, 128))
		{
			return EA_CRYPT_ERROR;
		}

		iCount = send (sock, (char*)&MD5KetCrypt, 128, 0);
		if (iCount != 128)
		{
			break;
		}

		SizeBuf = recv (sock, (char*)&iCount, sizeof (int), 0);
		if (SizeBuf != sizeof (int))
		{
			break;
		}

		if (!iCount)
		{
			break;
		}

		return 0;
	}
	while (false);

	return EA_CRYPT_API_INIT_ERROR;
}
