/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    reg_token_array.cpp

Abstract:

	Add/delete tokens to persistent storage in windows registry.
	Token is a short string value without spaces inside.

Environment:

   user mode

--*/

#include "stdafx.h"
#include "reg_token_array.h"
#include <algorithm>
#include <iostream>
#include "StringUtils.h"


bool GetRegistryString(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, std::string &result);
bool PutRegistryString(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const std::string &str);

void SplitStringToArray(const std::string &str, TokenArray &ta);
std::string StringFromArray(const TokenArray &ta);


bool GetTokens(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, TokenArray &result)
{
    std::string reg_str;
    if (GetRegistryString(hKey, lpSubkey, lpValue, reg_str)) {
        SplitStringToArray(reg_str, result);
        return true;
    }
    else {
        result.clear();
        return true;
    }
}

bool HasTokens(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue)
{
	TokenArray ta;
	if (!GetTokens(hKey, lpSubkey, lpValue, ta))
		return false;
	return !ta.empty();
}

size_t AddToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const std::string &token)
{
    TokenArray ta;
    std::string reg_str;
    if (GetRegistryString(hKey, lpSubkey, lpValue, reg_str)) {
        SplitStringToArray(reg_str, ta);
    }

    // find token in array
    for (TokenArray::const_iterator it = ta.begin(); it != ta.end(); ++it) {
        if (*it == token) {
            // found! no need to add anything
            return ta.size();
        }
    }

    // this is new token, append it and save array back to registry
    ta.push_back(token);

    reg_str = StringFromArray(ta);

    if (PutRegistryString(hKey, lpSubkey, lpValue, reg_str))
        return ta.size();

    // else we have problems with registry
    return 0;
}

size_t AddToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const TCHAR *token)
{
    return AddToken(hKey, lpSubkey, lpValue, StringFromTchar(token));
}

size_t DeleteToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const std::string &token)
{
    TokenArray ta;
    std::string reg_str;
    if (GetRegistryString(hKey, lpSubkey, lpValue, reg_str)) {
        SplitStringToArray(reg_str, ta);
    }

    // remove token from array
    const size_t size_before = ta.size();
    if (size_before == 0)
        return size_before;

    ta.remove(token);
    const size_t size_after = ta.size();

    if (size_before == size_after)
        // token not found
        return size_after;

    // update registry
    reg_str = StringFromArray(ta);
    if (PutRegistryString(hKey, lpSubkey, lpValue, reg_str))
        return size_after;

    // else we have problems with registry
    return size_before;
}

size_t DeleteToken(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const TCHAR *token)
{
    return DeleteToken(hKey, lpSubkey, lpValue, StringFromTchar(token));
}

//---------------------------------------------
// Registry dirty work

bool GetRegistryString(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, std::string &result)
{
    HKEY my_key;

    // open key
    if (::RegOpenKeyEx(hKey, lpSubkey, 0, KEY_READ, &my_key) != ERROR_SUCCESS)
        return false;

    bool bRet = true;

    do {

    // check value exists and its type
    DWORD valueType = 0;
    DWORD valueSize = 0;
    if (::RegQueryValueEx(my_key, lpValue, 0, &valueType, NULL, &valueSize) != ERROR_SUCCESS) {
        // value does not exists
        bRet = false;
        break;
    }

    if (valueType != REG_SZ) {
        // wrong type of value;
        bRet = false;
        break;
    }

    const size_t buf_size = (valueSize/sizeof(TCHAR))+1;
    TCHAR *pBuf = new TCHAR[buf_size];
    pBuf[buf_size-1] = 0;

    if (::RegQueryValueEx(my_key, lpValue, 0, &valueType, (LPBYTE)pBuf, &valueSize) != ERROR_SUCCESS) {
        // can't read value?
        delete[] pBuf;
        bRet = false;
        break;
    }

    // convert TCHAR to ascii/utf8 std::string
    result = StringFromTchar(pBuf);
    delete[] pBuf;

    } while (0);

    ::RegCloseKey(my_key);
    return bRet;
}

bool PutRegistryString(HKEY hKey, LPCTSTR lpSubkey, LPCTSTR lpValue, const std::string &str)
{
    HKEY my_key;

    // create/open key
    DWORD disposition;
    if (::RegCreateKeyEx(hKey, lpSubkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &my_key, &disposition) != ERROR_SUCCESS)
        return false;

    bool bRet = true;

    do {
        if (disposition == REG_OPENED_EXISTING_KEY) {
            // check existence of value and its type
            DWORD valueType = 0;
            if (::RegQueryValueEx(my_key, lpValue, 0, &valueType, NULL, NULL) == ERROR_SUCCESS) {
                // value exists
                if (valueType != REG_SZ) {
                    // wrong type
                    bRet = false;
                    break;
                }
            }
        }

        size_t buf_size = (str.size()+1)*3;
        TCHAR *pBuf = new TCHAR[buf_size]; // buffer with some extra space
        do {
            pBuf[0] = 0; pBuf[buf_size-1] = 0;

            if (!StringToTchar(str, pBuf, &buf_size)) {
                // something wrong with conversion
                bRet = false;
                break;
            }

            // write value
            if (::RegSetValueEx(my_key, lpValue, 0, REG_SZ, (LPBYTE)pBuf, (DWORD)(buf_size*sizeof(TCHAR))) != ERROR_SUCCESS) {
                bRet = false;
                break;
            }
        } while(0);

        delete[] pBuf;

    } while(0);

    ::RegCloseKey(my_key);
    return bRet;
}

//----------------------------------------------------------
// TokenArray <-> string representation

void SplitStringToArray(const std::string &str, TokenArray &ta)
{
    ta.clear();
    size_t old_pos = 0;
    for (;;) {
        const size_t pos = str.find(' ', old_pos);
        if (pos == std::string::npos) {
            // tail of the string is the last token
            std::string token = str.substr(old_pos, std::string::npos);
            if (!token.empty())
                ta.push_back(token);
            break;
        }
        if (pos > old_pos) {
            std::string token = str.substr(old_pos, pos-old_pos);
            if (!token.empty())
                ta.push_back(token);
        }

        old_pos = pos + 1;
    }
}

std::string StringFromArray(const TokenArray &ta)
{
    std::string result;
    for (TokenArray::const_iterator it = ta.begin(); it != ta.end(); ++it) {
        if (it != ta.begin())
            result += " ";
        result += *it;
    }
    return result;
}
