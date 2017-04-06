/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   SettingsInRegistry.h

Abstract:

	Helper classes to keep sharing settings in registry.
	NOTE: Use only dedicated subclasses: CServerSettingsInRegistry and CClientSettingsInRegistry.

Environment:

    User mode

--*/

#pragma once

#include "Consts/Registry.h"
#include "Consts/UngNetSettings.h"

class CSettingsInRegistry
{
public:
	CSettingsInRegistry () {}
	~CSettingsInRegistry () {}

	void SaveSettings (const CUngNetSettings &settings);
	void DeleteSettings ();

	bool UpdateSettingsStringParam (const CStringA &paramName,
									const CString &strParam);
	bool UpdateSettingsStringAParam (const CStringA &paramName,
									 const CStringA &strParam);
	bool UpdateSettingsBoolParam (const CStringA &paramName,
								  const bool bParam);

protected:
	CRegistry m_registry;
	CRegString m_regName;
};

class CServerSettingsInRegistry: public CSettingsInRegistry
{
public:
	CServerSettingsInRegistry (const CRegString &regName);
	CServerSettingsInRegistry (const CString &regName);
	~CServerSettingsInRegistry ();

private:
	void Init (const CRegString &regName);

public:
	CServerSettingsInRegistry& BeginChanges ();
	CServerSettingsInRegistry& ChangeParamBool (const CStringA &paramName, const bool bParam);
	CServerSettingsInRegistry& ChangeParamInt (const CStringA &paramName, const int iParam);
	CServerSettingsInRegistry& ChangeDeleteParam (const CStringA &paramName);
	bool                       CommitChanges ();

private:
	CUngNetSettings m_ungRegistrySettings;
	bool m_bChangesEnabled;
};

class CClientSettingsInRegistry: public CSettingsInRegistry
{
public:
	CClientSettingsInRegistry (const CRegString &regName);
	CClientSettingsInRegistry (const CString &regName);
	~CClientSettingsInRegistry ();

private:
	void Init (const CRegString &regName);
};


static inline CRegString toCRegString (const CString &str)
{
	return (LPCTSTR)str;
}
