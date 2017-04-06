/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    reg_token_array.h

Abstract:

	Add/delete tokens to persistent storage in windows registry.
	Token is a short string value without spaces inside.

Environment:

   user mode

--*/

#pragma once

#include <list>

// Our type to work with array of tokens (strings). Token is a short string value without spaces inside.
// NOTE: using list (not vector) because we need to delete items from any position.
typedef std::list<std::string> TokenArray;

// Read tokens if value exists and has appropriate type into container `result`.
// Return true if successful.
bool GetTokens(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, TokenArray &result);

// Check whether at least one token exists (value exists, has appropriate type). 
bool HasTokens(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue);

// Try to add token to value. Create the value if not exists. Check value has appropriate type if exists.
// Return number of tokens in value after operation.
size_t AddToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const std::string &token);
size_t AddToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const TCHAR *token);

// Try to delete token from value. Don't create the value if not exists. Check value has appropriate type if exists.
// Return number of tokens in value after operation.
size_t DeleteToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const std::string &token);
size_t DeleteToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const TCHAR *token);
