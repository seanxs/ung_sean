/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    base64.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include <atlenc.h>
#include "base64.h"

namespace eltima {
namespace base64 {

const DWORD dwFlags = ATL_BASE64_FLAG_NOCRLF;

bool encode(const TByteVector &binaryData, std::string &base64Data)
{
	if (binaryData.empty())
		return false;

	const int nBinary = (int)binaryData.size();
	int nB64 = ::Base64EncodeGetRequiredLength(nBinary, dwFlags);
	char *buf = new char[nB64];

	const BOOL ok = ::Base64Encode(
		&binaryData[0],
		nBinary,
		buf,
		&nB64,
		dwFlags);

	if (!ok) {
		delete[] buf;
		return false;
	}

	base64Data.assign(buf, nB64);
	delete[] buf;
	return true;
}

bool encode(const std::string &originalData, std::string &base64Data)
{
	if (originalData.empty())
		return false;

	const int nBinary = (int)originalData.size();
	int nB64 = ::Base64EncodeGetRequiredLength(nBinary, dwFlags);
	char *buf = new char[nB64];

	const BOOL ok = ::Base64Encode(
		(const BYTE*)originalData.c_str(),
		nBinary,
		buf,
		&nB64,
		dwFlags);

	if (!ok) {
		delete[] buf;
		return false;
	}

	base64Data.assign(buf, nB64);
	delete[] buf;
	return true;
}

bool decode(const std::string &base64Data, TByteVector &binaryData)
{
	if (base64Data.empty())
		return false;

	const int nB64 = (int)base64Data.size();
	int nBytes = nB64;
	binaryData.resize(nBytes);

	const BOOL ok = ::Base64Decode(
		base64Data.c_str(),
		nB64,
		&binaryData[0],
		&nBytes);

	if (!ok)
		return false;

	binaryData.resize(nBytes);
	return true;
}

} // namespace base64
} // namespace eltima
