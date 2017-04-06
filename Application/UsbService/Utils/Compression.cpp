/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Compression.cpp

Abstract:

    Support for different compression algorithms.

Environment:

    User mode

--*/

#include "stdafx.h"
#include <mstcpip.h>	// for ASSERT

#include "Compression.h"
#include "Consts/consts_helper.h"
#include "../lz4/lz4.h"
#include "zlib/zlib.h"


ICompression* ICompression::Create (Consts::UngCompressionType comprType)
{
	switch (comprType)
	{
	case Consts::UCT_COMPRESSION_NONE:	return new CCompressionNone;
	case Consts::UCT_COMPRESSION_ZLIB:	return new CCompressionZlib;
	case Consts::UCT_COMPRESSION_LZ4:	return new CCompressionLz4;
	default:							return NULL;
	}
}

LPCTSTR ICompression::CompressionTypeAsStr (Consts::UngCompressionType comprType)
{
	if (comprType < Consts::UNG_COMPRESSION_TYPES_MAX)
		return Consts::szUngCompressionType [comprType];
	else
		return _T("???");
}


CCompressionZlib::CZipParam CCompressionZlib::m_zipParam;

ECompressResult CCompressionZlib::Compress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const
{
	if (srcLength < m_zipParam.MIN_ZIP_SIZE)
	{
		return ECR_NOT_COMPRESSED;
	}

	//const long max_len = srcLength * m_zipParam.ZIP_RATIO_MUL / m_zipParam.ZIP_RATIO_DIV; // immediately reject poorly compressed data, see also compressBound
	const long max_len = compressBound(srcLength);
	if (bufOutput.GetCount () < max_len)
	{
		bufOutput.SetSize (max_len);
	}
	
	uLongf destLen = max_len;
	const int err = /*zlib*/::compress((BYTE*)bufOutput.GetData (), &destLen, pSource, srcLength);
	//const int err = /*zlib*/::compress2((BYTE*)bufOutput.GetData(), &destLen, pSource, srcLength, 9);
	switch (err)
	{
	case Z_OK:
		bufOutput.SetSize (destLen);
		ASSERT (bufOutput.GetCount () == destLen);
		return ECR_COMPRESSED;

	case Z_BUF_ERROR:
		return ECR_NOT_COMPRESSED;

	case Z_MEM_ERROR:
	default:
		ASSERT (!"zlib compress error");
		return ECR_ERROR;
	}
}

bool CCompressionZlib::Uncompress2 (const BYTE *pSource, const LONG srcLength, BYTE *pDest, LONG *srcDest) const
{
	uLongf destLen = *srcDest;

	const int err = /*zlib*/::uncompress(pDest, &destLen, pSource, srcLength);
	switch (err)
	{
	case Z_OK:
		*srcDest = destLen;
		return true;

	case Z_BUF_ERROR:
		// increase the buffer and try again
		*srcDest = destLen;
		break; // continue loop

	case Z_MEM_ERROR:
	case Z_DATA_ERROR:
		*srcDest = 0;
		ASSERT (!"zlib uncompress error");
		return false;
	}

	return true;
}

bool CCompressionZlib::Uncompress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const
{
	size_t sz = max(bufOutput.GetCount (), (m_zipParam.MIN_RATIO_CEILED + 1)*srcLength);

	if (int (sz) > bufOutput.GetCount ())
	{
		bufOutput.SetSize (int (sz));
	}
	
	while (true) 
	{
		uLongf destLen = bufOutput.GetBufferSize ();

		const int err = /*zlib*/::uncompress((BYTE*)bufOutput.GetData (), &destLen, pSource, srcLength);
		switch (err)
		{
		case Z_OK:
			bufOutput.SetSize (destLen);
			ASSERT (bufOutput.GetCount () == destLen);
			return true;

		case Z_BUF_ERROR:
			// increase the buffer and try again
			bufOutput.SetSize (3 * bufOutput.GetBufferSize ());
			break; // continue loop

		case Z_MEM_ERROR:
		case Z_DATA_ERROR:
			ASSERT (!"zlib uncompress error");
			return false;
		}
	}
	
	return false;
}


ECompressResult CCompressionLz4::Compress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const
{
	enum {MIN_BLOCK_SIZE = 128}; // the value based on m_zipParam.MIN_ZIP_SIZE, Linux version has 160 as limit

	if (srcLength < MIN_BLOCK_SIZE)
	{
		return ECR_NOT_COMPRESSED;
	}

	const int max_len = LZ4_compressBound (srcLength);
	if (bufOutput.GetCount () < max_len)
	{
		bufOutput.SetSize (max_len);
	}

	const int compressedSize = LZ4_compress_limitedOutput (
		(const char*)pSource, (char*)bufOutput.GetData (), srcLength, max_len);
	
	if (compressedSize == 0)
	{
		// compression fails, should we return ECR_NOT_COMPRESSED instead?
		return ECR_ERROR;
	}

	bufOutput.SetSize (compressedSize);
	return ECR_COMPRESSED;
}

bool CCompressionLz4::Uncompress2 (const BYTE *pSource, const LONG srcLength, BYTE *pDest, LONG *srcDest) const
{
	const int maxSize = *srcDest;
	const int resultSize = LZ4_decompress_safe (
		(const char*)pSource, (char*)pDest, srcLength, maxSize);

	// NOTE: LZ4_decompress_safe does not differentiate between various errors,
	// both insufficient memory error or stream error indicated by result value < 0.
	// So we try to increase output buffer several times 
	// in order to eliminate insufficient memory error.
	if (resultSize >= 0)
	{
		// success
		*srcDest = resultSize;
		//bufOutput.SetSize (resultSize);
		return true;
	}

	*srcDest = 0;
	return false;
}

bool CCompressionLz4::Uncompress (const BYTE *pSource, const LONG srcLength, CBuffer &bufOutput) const
{
	enum {
		MIN_RATIO = 2,			// some value to start guessing decompressed size
		MAX_ITERATIONS = 16,	// try to decompress N times with increased buffer every time
	};
	const int startSize = max (bufOutput.GetCount (), (MIN_RATIO * srcLength));
	if (startSize > bufOutput.GetCount ())
	{
		bufOutput.SetSize (startSize);
	}

	for (int i=1; i <= MAX_ITERATIONS; ++i)
	{
		const int maxSize = bufOutput.GetBufferSize ();
		const int resultSize = LZ4_decompress_safe (
			(const char*)pSource, (char*)bufOutput.GetData (), srcLength, maxSize);
		
		// NOTE: LZ4_decompress_safe does not differentiate between various errors,
		// both insufficient memory error or stream error indicated by result value < 0.
		// So we try to increase output buffer several times 
		// in order to eliminate insufficient memory error.
		if (resultSize >= 0)
		{
			// success
			bufOutput.SetSize (resultSize);
			return true;
		}

		const size_t newSize = 3 * (size_t)maxSize;
		if (i < MAX_ITERATIONS && newSize < LZ4_MAX_INPUT_SIZE)
		{
			bufOutput.SetSize (int (newSize));
		}
		else
		{
			break;
		}
	}

	// we got max iteration count or maybe too big buffer size, and still got decompression error
	ASSERT (!"lz4: uncompress error");
	return false;	
}
