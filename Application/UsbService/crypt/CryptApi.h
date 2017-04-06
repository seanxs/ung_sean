/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    CryptApi.h

Abstract:

	Working with CryptApi class. 

Environment:

    User mode

--*/

#ifndef	SECURITY_H
#define SECURITY_H

#include "stdafx.h"
#include <wincrypt.h>

// struct
template<int bitlen> struct RSAPubKey
{
	PUBLICKEYSTRUC  publickeystruc ;
	RSAPUBKEY rsapubkey;
	BYTE modulus[bitlen/8];
};
template<int bitlen> struct RSAPrivKey
{
	RSAPubKey<bitlen> pubkey;
	BYTE prime1[bitlen/16];
	BYTE prime2[bitlen/16];
	BYTE exponent1[bitlen/16];
	BYTE exponent2[bitlen/16];
	BYTE coefficient[bitlen/16];
	BYTE privateExponent[bitlen/8];
};

template<int bitlen> struct RSAKeyExchBLOB
{
	PUBLICKEYSTRUC publickeystruc ;
	ALG_ID algid;
	BYTE encryptedkey[bitlen/8];
};

typedef RSAKeyExchBLOB<1024> RSA1024KeyExchBLOB;
typedef struct
{
	RSA1024KeyExchBLOB kb;
	unsigned __int64 fSize;
} EncFileHeader;

typedef RSAPubKey<1024> RSAPubKey1024;
typedef RSAPrivKey<1024> RSAPrivKey1024;
typedef	struct _RC4Key{char Byte[140];} RC4Key;
typedef	struct _uint128{char Byte[16];} uint128;
// Function for MD5 key creation
BOOL CreateMD5(BYTE* Buf, int SizeBuf, char	MD5Key[16]);
BOOL CreateAccessCode(BYTE*	Buf, int SizeBuf, char AccessCode[16]);
BOOL CreateMD5(char X[16], char Y[16], char MD5Key[16]);
BOOL RndArray(uint128 &X);


// Class for crypt
class CCryptApi
{
	public:
	// Initialization
		int	Initialization();
		int	CloseSession();
	// RSA
		int	CreateRSAKey();
		int	ExportRSAPublicKey(RSAPubKey1024	&key);
		int	ImportRSAPublicKey(RSAPubKey1024	key);
	// RC4
		int	CreateRC4Key(int NumberKey);
		int	ExportRC4Key(RC4Key	&key, int NumberKey);
		int	ImportRC4Key(RC4Key	key, int NumberKey);
	// Encrypt and Decrypt
		int	Encrypt(BYTE*	Buf, DWORD &SizeBuf, int NumberKey);
		int	Decrypt(BYTE*	Buf, DWORD &SizeBuf, int NumberKey);

		CCryptApi(void);
		~CCryptApi(void);

		bool	UseCrypt;
	public:
		HCRYPTPROV	hProv;				// Handle Context 
		HCRYPTKEY	hKeyRSA;			// Handle RSA private key
		HCRYPTKEY	hKeyRSAPublic;		// Handle RSA public key
		HCRYPTKEY	hKeyRC4_1;			// Handle RC4 public key
		HCRYPTKEY	hKeyRC4_2;			// Handle RC4 public key
		RSAPubKey1024	key;			// Buf for Export and Import of RSA public key
		EncFileHeader	fh;				// Buf for Import and Export of RC4 key

};

#endif