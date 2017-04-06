/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    base64.h

Environment:

    User mode

--*/

#pragma once

#include <string>
#include <vector>

typedef std::vector<BYTE> TByteVector;

namespace eltima {
namespace base64 {

// Encode binary data to base64 string.
bool encode(const TByteVector &binaryData, std::string &base64Data);
bool encode(const std::string &originalData, std::string &base64Data);

// Decode base64 string to binary data.
bool decode(const std::string &base64Data, TByteVector &binaryData);

} // namespace base64
} // namespace eltima
