/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    CryptApi.cpp

Abstract:

	Working with CryptApi class. 

Environment:

    User mode

--*/

#include "stdafx.h"
#include "CryptApi.h"

//*************************************************************
//*******  Create MD5 of used key  ****************************
//*************************************************************
BOOL CreateMD5(BYTE* Buf, int SizeBuf, char MD5Key[16])
{
	HCRYPTPROV   hCryptProv = NULL;
	HCRYPTHASH   hOriginalHash;
	DWORD        dwHashLen = 16;//sizeof(DWORD);

	char t[100];
	memset(t, 0, 100);

	if(!CryptAcquireContext(
	   &hCryptProv, 
	   NULL, 
	   NULL, 
	   PROV_RSA_FULL, 
	   0) && 
	   !CryptAcquireContext(
	   &hCryptProv, 
	   NULL, 
	   NULL, 
	   PROV_RSA_FULL, 
	   CRYPT_NEWKEYSET)) 
	{
		return FALSE;
	}

	if(!CryptCreateHash(
		hCryptProv, 
		CALG_MD5, 
		0, 
		0,
		&hOriginalHash))
		return FALSE;

	// Load key
	if(!CryptHashData(
		hOriginalHash, 
		Buf, 
		SizeBuf, 0)) 
	{
		CryptReleaseContext(hCryptProv,0);
		return FALSE;
	}

	if(!CryptGetHashParam(
	   hOriginalHash,
	   HP_HASHVAL, 
	   (BYTE*)&MD5Key[0], 
	   &dwHashLen, 
	   0)) 
	{
		CryptDestroyHash(hOriginalHash);
		CryptReleaseContext(hCryptProv,0);
		return FALSE;
	}

	CryptDestroyHash(hOriginalHash);
	CryptReleaseContext(hCryptProv,0);

	return TRUE;
}

//*************************************************************
//*******  Create access code  ********************************
//*************************************************************
BOOL CreateAccessCode(BYTE* Buf, int SizeBuf, char AccessCode[16])
{
	char	*Temp;
	unsigned int a;

	Temp = (char*)calloc(strlen((char*)Buf),1);
	for(a = 0; a < strlen((char*)Buf)/2; a++)
	{
		Temp[a*2] = Buf[a*2];
		Temp[a*2 + 1] = (Buf[a*2]^Buf[a*2+1])|Buf[strlen((char*)Buf)-1];
	}
	Temp[strlen((char*)Buf)-1] = Buf[strlen((char*)Buf)-2]^Buf[strlen((char*)Buf)-3];

	return CreateMD5((LPBYTE)Temp, (int)strlen((char*)Buf), AccessCode);
}

//*************************************************************
//*******  Create X and Y random keys  ************************
//*************************************************************
BOOL RndArray(uint128 &X)
{
	int		a;

	for(a = 0; a < 16; a++)
		X.Byte[a] = rand()/256;

	return TRUE;
}

//*************************************************************
//*******  Create MD5 from X and Y  ***************************
//*************************************************************
BOOL CreateMD5(char	X[16], char	Y[16], char	MD5Key[16])
{
	char	Buf[32];

	CopyMemory(Buf, X, 16);
	CopyMemory(Buf + 16, Y, 16);

	return CreateMD5((LPBYTE)&Buf[0], 32, MD5Key);
}

//-----------------------------------------------------------------------------------
//---------------  Class Security   -------------------------------------------------
//-----------------------------------------------------------------------------------


//*******************************************************************
//*************  Constructor  ***************************************
//*******************************************************************

CCryptApi::CCryptApi(void)
{
	hProv = NULL;				// Handle Context 
	hKeyRSA = NULL;				// Handle RSA private key
	hKeyRSAPublic = NULL;		// Handle RSA public key
	UseCrypt = false;
   	hKeyRC4_1 = NULL;
	hKeyRC4_2 = NULL;

}

//*******************************************************************
//***********  Destructor  ******************************************
//*******************************************************************

CCryptApi::~CCryptApi(void)
{
	if(hProv != NULL)
		CloseSession();
}

//*******************************************************************
//***********  Create or open crypt context  ************************
//*******************************************************************

int	CCryptApi::Initialization()
{
	if(!CryptAcquireContext(&hProv, NULL,MS_ENHANCED_PROV,PROV_RSA_FULL,0) && !CryptAcquireContext(&hProv,NULL,MS_ENHANCED_PROV,PROV_RSA_FULL,CRYPT_NEWKEYSET)) //Creating or opening crypt context
        return GetLastError();

	return 0;
}

//*******************************************************************
//***********  Create RSA Key  **************************************
//*******************************************************************
int	CCryptApi::CreateRSAKey()
{
	if(!CryptGenKey(hProv,AT_KEYEXCHANGE,1024<<16|AT_KEYEXCHANGE,&hKeyRSA)) //generating 1024-bit exchange key
		return GetLastError();

	return 0;
}

//*******************************************************************
//***********  Export RSA public key to Buf  ************************
//*******************************************************************

int	CCryptApi::ExportRSAPublicKey(RSAPubKey1024	&key)
{
	DWORD	dwLen = sizeof(RSAPubKey1024);
	if(!CryptExportKey(hKeyRSA,NULL,PUBLICKEYBLOB,0,(BYTE *)&key,&dwLen)) //Exporting Public Key
		return GetLastError();

	return 0;
}

//*******************************************************************
//***********  Import RSA public key to Buf  ************************
//*******************************************************************

int	CCryptApi::ImportRSAPublicKey(RSAPubKey1024	key)
{
	if(!CryptImportKey(hProv,(BYTE *)&key,sizeof(RSAPubKey1024),NULL,0,&hKeyRSAPublic)) //Importing Public Key
		return GetLastError();

	return 0;
}

//*******************************************************************
//***********  Create RC4 key ***************************************
//*******************************************************************

int	CCryptApi::CreateRC4Key(int NumberKey)
{
	int err;
	char t[25];
	memset(t, 0, 25);

    if(NumberKey == 1)
	{
		if(!CryptGenKey(hProv,CALG_RC4,CRYPT_EXPORTABLE,&hKeyRC4_1)) //generating the key for 3DES
		{
			err = GetLastError();
			
			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hProv = %p\n", hProv);
			ATLTRACE("hKeyRC4_1 = %p\n", hKeyRC4_1);			
			ATLTRACE("\n");

			return err;
		}
	}
	else if(NumberKey == 2)
	{
		if(!CryptGenKey(hProv,CALG_RC4,CRYPT_EXPORTABLE,&hKeyRC4_2)) //generating the key for 3DES
		{
			err = GetLastError();
			
			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hProv = %p\n", hProv);
			ATLTRACE("hKeyRC4_2 = %p\n", hKeyRC4_2);
			ATLTRACE("\n");

			return err;
		}
	}
	
	return 0;
}

//*******************************************************************
//***********  Export RC4 key with encrypt public key RSA  **********
//*******************************************************************

int	CCryptApi::ExportRC4Key(RC4Key	&key, int NumberKey)
{
	int err;
	char t[25];
	memset(t, 0, 25);

	DWORD	dwLen = 140;
	if(NumberKey == 1)
	{
		if(!CryptExportKey(hKeyRC4_1,hKeyRSAPublic, SIMPLEBLOB,0 , (BYTE*)key.Byte,&dwLen)) //exporting 3DES key
		{
			err = GetLastError();
			
			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hProv = %p\n", hProv);
			ATLTRACE("hKeyRSAPublic = %p\n", hKeyRSAPublic);
			ATLTRACE("hKeyRC4_1 = %p\n", hKeyRC4_1);

			return err;
		}
	}
	else
	{
		if(!CryptExportKey(hKeyRC4_2, hKeyRSAPublic, SIMPLEBLOB,0 , (BYTE*)key.Byte,&dwLen)) //exporting 3DES key
		{
			err = GetLastError();

			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hProv = %p\n", hProv);
			ATLTRACE("hKeyRSAPublic = %p\n", hKeyRSAPublic);
			ATLTRACE("hKeyRC4_2 = %p\n", hKeyRC4_2);

			return err;
		}
	}
	return 0;

}

//*******************************************************************
//***********  Import RC4 key with encrypt private RSA key  *********
//*******************************************************************

int	CCryptApi::ImportRC4Key(RC4Key	key, int NumberKey)
{
	int err;
	char t[25];
	memset(t, 0, 25);

	if(NumberKey == 1)
	{
		if(!CryptImportKey(hProv, (BYTE*)key.Byte, 140, hKeyRSAPublic ,0, &hKeyRC4_1))
		{
			err = GetLastError();
			
			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hProv = %p\n", hProv);
			ATLTRACE("hKeyRSAPublic = %p\n", hKeyRSAPublic);
			ATLTRACE("hKeyRC4_1 = %p\n", hKeyRC4_1);

			return err;
		}
	}
	else
	{
		if(!CryptImportKey(hProv, (BYTE*)key.Byte, 140, hKeyRSAPublic, 0, &hKeyRC4_2))
		{
			err = GetLastError();
			
			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hProv = %p\n", hProv);
			ATLTRACE("hKeyRSAPublic = %p\n", hKeyRSAPublic);
			ATLTRACE("hKeyRC4_2 = %p\n", hKeyRC4_2);

			return err;
		}
	}

	return 0;
}

//*******************************************************************
//***********  Encrypt  *********************************************
//*******************************************************************

int	CCryptApi::Encrypt(BYTE*	Buf, DWORD& SizeBuf, int NumberKey)
{
	int err;
	char t[25];
	memset(t, 0, 25);

	if(NumberKey == 1)
	{
		if(!CryptEncrypt(hKeyRC4_1, 0, false, 0, (BYTE*)Buf, &SizeBuf, SizeBuf))
		{
			err = GetLastError();
			
			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hKeyRC4_1 = %p\n", hKeyRC4_1);
			ATLTRACE("SizeBuf = %d\n", SizeBuf);

			return err;
		}
	}
	else
	{
		if(!CryptEncrypt(hKeyRC4_2, 0, false, 0, (BYTE*)Buf, &SizeBuf, SizeBuf))
		{
			err = GetLastError();
			
			ATLTRACE("EncryptError = %d\n", err);
			ATLTRACE("hKeyRC4_2 = %p\n", hKeyRC4_2);
			ATLTRACE("SizeBuf = %d\n", SizeBuf);

			return GetLastError();
		}
	}
	return 0;
}

//*******************************************************************
//***********  Decrypt  *********************************************
//*******************************************************************

int	CCryptApi::Decrypt(BYTE*	Buf, DWORD& SizeBuf, int NumberKey)
{
	if(NumberKey == 1)
	{
		if(!CryptDecrypt(hKeyRC4_1 ,0,false,0,(BYTE*)Buf,&SizeBuf))
			return GetLastError();
	}
	else
	{
		if(!CryptDecrypt(hKeyRC4_2 ,0,false,0,(BYTE*)Buf,&SizeBuf))
			return GetLastError();
	}
	return 0;
}

//*******************************************************************
//***********  Close crypt session  *********************************
//*******************************************************************

int	CCryptApi::CloseSession()
{
	if(hKeyRSA != NULL)
		CryptDestroyKey(hKeyRSA);

	if(hKeyRSAPublic != NULL)
		CryptDestroyKey(hKeyRSAPublic);

	if(hKeyRC4_1 != NULL)
		CryptDestroyKey(hKeyRC4_1);

	if(hKeyRC4_2 != NULL)
		CryptDestroyKey(hKeyRC4_2);

	if(hProv != NULL)
		CryptReleaseContext(hProv,0);

	hKeyRSA = NULL;
	hKeyRSAPublic = NULL;
	hKeyRC4_1 = NULL;
	hKeyRC4_2 = NULL;
	hProv = NULL;
	UseCrypt = false;

	return 0;
}