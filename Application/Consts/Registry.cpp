/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Regsitry.cpp

Abstract:

	Class for work with the registry

Environment:

    User mode

--*/

#include "stdafx.h"
#include "Registry.h"


CRegistryException::CRegistryException()
{
}

CRegistryException::CRegistryException(const char *message) : std::exception(message)
{
}

CRegistryException::CRegistryException(const CRegistryException& ex) : std::exception(ex)
{
}

CRegistryException& CRegistryException::operator=(const CRegistryException& ex)
{
	exception::operator=(ex);
	return *this;
}

CRegistryException::~CRegistryException()
{
}


CRegistry::CRegistry()
{
	Init();
}

CRegistry::CRegistry(HKEY rootKey, CRegString subKey, DWORD options, REGSAM samDesired, LPSECURITY_ATTRIBUTES pSecurityAttributes)
{
	Init();
	CreateKey(rootKey,subKey,options,samDesired,pSecurityAttributes);
}

CRegistry::CRegistry(CRegString key, DWORD options, REGSAM samDesired, LPSECURITY_ATTRIBUTES pSecurityAttributes)
{
	Init();
	CreateKey(key,options,samDesired,pSecurityAttributes);
}

CRegistry::~CRegistry()
{
	if(m_hKey)
		CloseKey(false);
}

void CRegistry::CloseKey(bool flush)
{
	if(m_hKey)
	{
		if(flush)
			Flush();
		::RegCloseKey(m_hKey);
		m_hKey=0;
		m_hRootKey=0;
		m_SubKey=_T("");
	}
}

void CRegistry::Flush()
{
	if(m_hKey)
		::RegFlushKey(m_hKey);
}

void CRegistry::CreateKey(HKEY rootKey, CRegString subKey, DWORD options, REGSAM samDesired, LPSECURITY_ATTRIBUTES pSecurityAttributes)
{
	DWORD dwDisposition=0;
	if(::RegCreateKeyEx(rootKey,subKey.c_str(),0,NULL,options,samDesired,pSecurityAttributes,&m_hKey,&dwDisposition) == ERROR_SUCCESS)
	{
		m_hRootKey=rootKey;
		m_Opened=(dwDisposition == REG_OPENED_EXISTING_KEY);
		m_Created=!m_Opened;
		m_SubKey=subKey;
	}
	else
	{
		m_hKey=0;
		m_hRootKey=0;
		m_Opened=false;
		m_Created=false;
		m_SubKey=_T("");
	}
}

void CRegistry::CreateKey(CRegString key, DWORD options, REGSAM samDesired, LPSECURITY_ATTRIBUTES pSecurityAttributes)
{
	CreateKey(GetRootKey(key),GetSubKey(key),options,samDesired,pSecurityAttributes);
}

void CRegistry::OpenKey(HKEY rootKey, CRegString subKey, DWORD options, REGSAM samDesired)
{
	if(::RegOpenKeyEx(rootKey,subKey.c_str(),options,samDesired,&m_hKey) == ERROR_SUCCESS)
	{
		m_hRootKey=rootKey;
		m_Opened=true;
		m_Created=false;
		m_SubKey=subKey;
	}
	else
	{
		m_hKey=0;
		m_hRootKey=0;
		m_Opened=false;
		m_Created=false;
		m_SubKey=_T("");
	}
}

void CRegistry::OpenKey(CRegString key, DWORD options, REGSAM samDesired)
{
	OpenKey(GetRootKey(key),GetSubKey(key),options,samDesired);
}

HKEY CRegistry::GetRootKey(CRegString key)
{
	CRegString::size_type lengthOfRoot=key.find(_T("\\"));
	if(lengthOfRoot == CRegString::npos)
		lengthOfRoot=key.length();
	else
		lengthOfRoot;
	CRegString rootKey=key.substr(0,lengthOfRoot);
	if(m_RootKeys.find(rootKey) == m_RootKeys.end())
		throw CRegistryException("Could not find root key.");
	return m_RootKeys[rootKey];
}

CRegString CRegistry::GetSubKey(CRegString key)
{
	CRegString::size_type lengthOfRoot=key.find(_T("\\"));
	CRegString subKey;
	if((lengthOfRoot != CRegString::npos) && (lengthOfRoot != key.length()-1))
		subKey=key.substr(lengthOfRoot+1);
	return subKey;
}

bool CRegistry::ExistsKey(HKEY hKey, CRegString subKey, DWORD options, REGSAM samDesired)
{
	CRegistry reg;
	reg.OpenKey(hKey,subKey,options,samDesired);
	return reg.IsOpened(); // destructor should close hKey.
}

bool CRegistry::ExistsKey(CRegString key, DWORD options, REGSAM samDesired)
{
	CRegistry reg;
	reg.OpenKey(key,options,samDesired);
	return reg.IsOpened(); // destructor should close hKey.
}

void CRegistry::InitRootKeysMap()
{
	m_RootKeys.insert(RootKeyPair(CRegString(_T("HKEY_CLASSES_ROOT")),HKEY_CLASSES_ROOT));
	m_RootKeys.insert(RootKeyPair(CRegString(_T("HKEY_CURRENT_CONFIG")),HKEY_CURRENT_CONFIG));
	m_RootKeys.insert(RootKeyPair(CRegString(_T("HKEY_CURRENT_USER")),HKEY_CURRENT_USER));
	m_RootKeys.insert(RootKeyPair(CRegString(_T("HKEY_LOCAL_MACHINE")),HKEY_LOCAL_MACHINE));
	m_RootKeys.insert(RootKeyPair(CRegString(_T("HKEY_USERS")),HKEY_USERS));
	m_RootKeys.insert(RootKeyPair(CRegString(_T("HKEY_PERFORMANCE_DATA")),HKEY_PERFORMANCE_DATA));
	m_RootKeys.insert(RootKeyPair(CRegString(_T("HKEY_DYN_DATA")),HKEY_DYN_DATA));
}

bool CRegistry::IsOpened()
{
	return m_Opened;
}

bool CRegistry::IsCreated()
{
	return m_Created;
}

HKEY CRegistry::GetHKEY()
{
	return m_hKey;
}

void CRegistry::Attach(HKEY hKey)
{
	CloseKey();
	m_hKey=hKey;
	m_hRootKey=0;
	m_SubKey=_T("");
	m_Opened=true;
	m_Created=false;
}

HKEY CRegistry::Detach()
{
	HKEY hKey=m_hKey;
	m_hKey=0;
	m_hRootKey=0;
	m_SubKey=_T("");
	m_Opened=false;
	m_Created=false;
	return hKey;
}

void CRegistry::SetString(CRegString name, CRegString value)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::SetString(name,value): m_hKey == NULL.");
	if(::RegSetValueEx(m_hKey,name.c_str(),0,REG_SZ,(const BYTE*)value.c_str(), (DWORD)value.length()*sizeof (TCHAR)) != ERROR_SUCCESS)
		throw CRegistryException("CRegistry::SetString(name,value): Could not set value.");
}

CRegString CRegistry::GetString(CRegString name)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::GetString(name): m_hKey == NULL.");

	CRegString value;
	DWORD size=0;
	DWORD type=REG_SZ;
	if(::RegQueryValueEx(m_hKey,name.c_str(),0,&type,NULL,&size) == ERROR_SUCCESS)
	{
		if(type != REG_SZ)
			throw CRegistryException("CRegistry::GetString(name): in the registry is not string(REG_SZ).");
		TCHAR* tempValue=new TCHAR[size+1];
		tempValue[size]='\0';
		if(::RegQueryValueEx(m_hKey,name.c_str(),0,&type,(LPBYTE)tempValue,&size) == ERROR_SUCCESS)
		{
			value=tempValue;
			delete[] tempValue;
		}
		else
		{
			delete[] tempValue;
			throw CRegistryException("CRegistry::GetString(name): Could not read string from the registry.");
		}
	}
	else
		throw CRegistryException("CRegistry::GetString(name): Could not read string from the registry.");
	return value;
}

CRegString CRegistry::GetStringMaybe(CRegString name, CRegString defaultValue)
{
    if (ExistsValue(name))
        return GetString(name);
    else
        return defaultValue;
}


DWORD CRegistry::GetUInt(CRegString name)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::GetUInt(name): m_hKey == NULL.");
	DWORD value;
	DWORD size=sizeof(value);
	DWORD type=REG_DWORD;
	if(::RegQueryValueEx(m_hKey,name.c_str(),0,&type,(LPBYTE)&value,&size) != ERROR_SUCCESS)
		throw CRegistryException("CRegistry::GetUInt(name): Could not set value in the registry.");
	if(type != REG_DWORD)
	{
		value=0;
		throw CRegistryException("CRegistry::GetUInt(name): Could not read DWORD from the registry. Type of data in the registry is not DWORD");
	}
	return value;
}

void CRegistry::SetUInt(CRegString name, DWORD value)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::SetUInt(name,value): m_hKey == NULL.");
	if(::RegSetValueEx(m_hKey,name.c_str(),0,REG_DWORD,(const BYTE*)&value,sizeof(DWORD)) != ERROR_SUCCESS)
		throw CRegistryException("CRegistry::SetUInt(name,value): Could not set the value.");
}

DWORD CRegistry::GetUIntMaybe(CRegString name, DWORD defaultValue)
{
	if (ExistsValue(name))
		return GetUInt(name);
	else
		return defaultValue;
}

bool CRegistry::GetBool(CRegString name)
{
	return (GetUInt(name) != 0);
}

bool CRegistry::GetBoolMaybe(CRegString name, bool defaultValue)
{
    if (ExistsValue(name))
        return GetBool(name);
    else
        return defaultValue;
}

void CRegistry::SetBool(CRegString name, bool value)
{
	SetUInt(name,value?1:0);
}

void CRegistry::DeleteValue(CRegString name)
{
	if(::RegDeleteValue(m_hKey,name.c_str()) != ERROR_SUCCESS) 
		throw CRegistryException("CRegistry::DeleteValue(name): Could not delete the value.");
}

void CRegistry::DeleteKey()
{
	if(!m_hKey || m_SubKey.empty())
		throw CRegistryException("CRegistry::DeleteKey(): m_hKey == NULL or m_SubKey is empty.");
	if(::RegDeleteKey(m_hRootKey,m_SubKey.c_str()) != ERROR_SUCCESS) 
		throw CRegistryException("CRegistry::DeleteKey(): Could not delete the key.");
}

void CRegistry::DeleteKey(HKEY hRootKey, CRegString subKey)
{
	if(::RegDeleteKey(hRootKey,subKey.c_str()) != ERROR_SUCCESS) 
		throw CRegistryException("CRegistry::DeleteKey(hRootKey,subKey): Could not delete the key.");
}

CRegString CRegistry::GetSubKey()
{
	return m_SubKey; 
}

HKEY CRegistry::GetRootKey()
{
	return m_hRootKey;
}

void CRegistry::Init()
{
	m_hKey=0;
	m_hRootKey=0;
	m_SubKey=_T("");
	m_Created=false;
	m_Opened=false;
	InitRootKeysMap();
}

bool CRegistry::ExistsValue(CRegString name)
{
	DWORD type=0;
	DWORD size=0;
	return (::RegQueryValueEx(m_hKey,name.c_str(),0,&type,NULL,&size) == ERROR_SUCCESS);
}


void CRegistry::SetBinaryData(CRegString name, const char* buffer, size_t size)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::SetString(name,value): m_hKey == NULL.");
	if(::RegSetValueEx(m_hKey,name.c_str(),0,REG_BINARY,(const BYTE*)buffer, (DWORD)size) != ERROR_SUCCESS)
		throw CRegistryException("CRegistry::SetString(name,value): Could not set the value.");
}

CStdBuffer  CRegistry::GetBinaryData(CRegString name)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::GetString(name): m_hKey == NULL.");

	CRegString value;
	DWORD size=0;
	DWORD type=REG_BINARY;
	if(::RegQueryValueEx(m_hKey,name.c_str(),0,&type,NULL,&size) == ERROR_SUCCESS)
	{
		if(type != REG_BINARY)
			throw CRegistryException("CRegistry::GetString(name): in the registry is not string(REG_SZ).");

		CStdBuffer Buff(size);
		if(::RegQueryValueEx(m_hKey,name.c_str(),0,&type,(LPBYTE)Buff.m_Buffer,&size) == ERROR_SUCCESS)
		{
			return Buff;
		}
		else
		{
			throw CRegistryException("CRegistry::GetString(name): Could not read string from the registry.");
		}
	}
	else
		throw CRegistryException("CRegistry::GetString(name): Could not read string from the registry.");

	CStdBuffer BuffFalse(1);
	BuffFalse.m_BufferSize = 0;
	return BuffFalse;
}

void CRegistry::SetExpandString(CRegString name, CRegString value)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::SetString(name,value): m_hKey == NULL.");
	if(::RegSetValueEx(m_hKey,name.c_str(),0,REG_EXPAND_SZ,(const BYTE*)value.c_str(), (DWORD)value.length()*sizeof (TCHAR)) != ERROR_SUCCESS)
		throw CRegistryException("CRegistry::SetString(name,value): Could not set the value.");
}

CRegString CRegistry::GetExpandString(CRegString name)
{
	if(!m_hKey)
		throw CRegistryException("CRegistry::GetString(name): m_hKey == NULL.");

	CRegString value;
	DWORD size=0;
	DWORD type=REG_EXPAND_SZ;
	if(::RegQueryValueEx(m_hKey,name.c_str(),0,&type,NULL,&size) == ERROR_SUCCESS)
	{
		if(type != REG_EXPAND_SZ)
			throw CRegistryException("CRegistry::GetString(name): in the registry is not string(REG_SZ).");
		TCHAR* tempValue=new TCHAR[size+1];
		tempValue[size]='\0';
		if(::RegQueryValueEx(m_hKey,name.c_str(),0,&type,(LPBYTE)tempValue,&size) == ERROR_SUCCESS)
		{
			value=tempValue;
			delete[] tempValue;
		}
		else
		{
			delete[] tempValue;
			throw CRegistryException("CRegistry::GetString(name): Could not read string from the registry.");
		}
	}
	else
		throw CRegistryException("CRegistry::GetString(name): Could not read string from the registry.");
	return value;
}

std::vector<CRegString> CRegistry::GetValueNames()
{
	std::vector <CRegString> ret;

	TCHAR	BufName[250];
	DWORD	dwLenName, dwLenDate;
	DWORD	dwDisposition = 0;
	DWORD	dwType;
	int		dwIndex = 0;


	if(!m_hKey)
		throw CRegistryException("CRegistry::GetString(name): m_hKey == NULL.");


	do
	{
		FillMemory (BufName, sizeof (BufName), 0);
		dwLenName = sizeof(BufName)/sizeof (TCHAR);
		dwLenDate = 0;
		if (RegEnumValue(m_hKey,   
				dwIndex,   
				BufName, &dwLenName,
				NULL,
				&dwType,
				(LPBYTE)NULL, &dwLenDate) != ERROR_SUCCESS
			)
		{
			break;
		}

		ret.push_back (BufName);

		dwIndex ++;
	}
	while (true);

	return ret;
}
