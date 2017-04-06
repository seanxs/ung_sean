/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Compression.h

Abstract:

    Support for different compression algorithms.

Environment:

    User mode

--*/

#pragma once
#include <math.h>
#include "Consts/Buffer.h"
#include "Consts/consts.h"


typedef enum ECompressResult {
	ECR_NOT_COMPRESSED,
	ECR_COMPRESSED,
	ECR_ERROR,
} ECompressResult;

class ICompression
{
public:
	static ICompression* Create (Consts::UngCompressionType comprType);
	static LPCTSTR CompressionTypeAsStr (Consts::UngCompressionType comprType);

public:
	virtual ~ICompression () {}
	// returns compression type (e.g. for introspection or to hold info about current compression type)
	virtual Consts::UngCompressionType GetCompressionType () const = 0;
	// attempt to compress source into buffer
	virtual ECompressResult  Compress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const = 0;
	// attempt to decompress source into buffer, returns true if successfully decompressed, otherwise false
	virtual bool             Uncompress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const = 0;
	virtual bool             Uncompress2 (const BYTE *pSource, const LONG srcLength, BYTE *pDest, LONG *srcDest) const = 0;
};

// special no-op class
class CCompressionNone: public ICompression
{
public:
	CCompressionNone () {}
	virtual Consts::UngCompressionType GetCompressionType () const {return Consts::UCT_COMPRESSION_NONE;}
	virtual ECompressResult  Compress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const {return ECR_NOT_COMPRESSED;}
	virtual bool             Uncompress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const {return false;}
	virtual bool             Uncompress2 (const BYTE *pSource, const LONG srcLength, BYTE *pDest, LONG *srcDest) const {return false;}
};

class CCompressionZlib: public ICompression
{
public:
	CCompressionZlib () {}
	virtual Consts::UngCompressionType GetCompressionType () const {return Consts::UCT_COMPRESSION_ZLIB;}
	virtual ECompressResult  Compress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const;
	virtual bool             Uncompress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const;
	virtual bool             Uncompress2 (const BYTE *pSource, const LONG srcLength, BYTE *pDest, LONG *srcDest) const;

private:
	class CZipParam 
	{
	public:
		CZipParam () : MIN_ZIP_SIZE  (128), ZIP_RATIO_MUL (2), ZIP_RATIO_DIV (3), MIN_RATIO_CEILED (1)
		{
			CRegKey reg;

			if (reg.Open (HKEY_LOCAL_MACHINE, Consts::szRegistry, KEY_READ) == ERROR_SUCCESS)
			{
				DWORD dw;
				if (reg.QueryDWORDValue (_T("MIN_ZIP_SIZE"), dw) == ERROR_SUCCESS) MIN_ZIP_SIZE = dw;
				if (reg.QueryDWORDValue (_T("ZIP_RATIO_MUL"), dw) == ERROR_SUCCESS) ZIP_RATIO_MUL = dw;
				if (reg.QueryDWORDValue (_T("ZIP_RATIO_DIV"), dw) == ERROR_SUCCESS) ZIP_RATIO_DIV = dw;
			}

			// update min ratio
			MIN_RATIO_CEILED = int (ceil (double (ZIP_RATIO_MUL) / double (ZIP_RATIO_DIV)));
		}

		int MIN_ZIP_SIZE;
		int ZIP_RATIO_MUL;
		int ZIP_RATIO_DIV;

		// used for decompression
		int	MIN_RATIO_CEILED;
	};

	static CZipParam m_zipParam;
};

class CCompressionLz4: public ICompression
{
public:
	CCompressionLz4 () {}
	virtual Consts::UngCompressionType GetCompressionType () const {return Consts::UCT_COMPRESSION_LZ4;}
	virtual ECompressResult  Compress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const;
	virtual bool             Uncompress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const;
	virtual bool             Uncompress2 (const BYTE *pSource, const LONG srcLength, BYTE *pDest, LONG *srcDest) const;
};
