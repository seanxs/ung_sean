/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    SettingsInRegistry.cpp

Abstract:

	Helper classes to keep sharing settings in registry.

Environment:

    User mode

--*/

#include "stdafx.h"
#include "SettingsInRegistry.h"
#include "Consts/consts.h"
#include "Consts/System.h"


//-----------------------------------------------------------
// base class CSettingsInRegistry

void CSettingsInRegistry::SaveSettings (const CUngNetSettings &settings)
{
	CRegString value = toCRegString (CSystem::ToUnicode (settings.BuildSetttings ()));
	m_registry.SetString (m_regName, value);
}

void CSettingsInRegistry::DeleteSettings ()
{
	if (m_registry.ExistsValue (m_regName))
	{
		m_registry.DeleteValue (m_regName);
	}
}

bool CSettingsInRegistry::UpdateSettingsStringParam (const CStringA &paramName, 
													 const CString &strParam)
{
	// get old settings from registry
	CRegString regstrSettings = m_registry.GetStringMaybe (m_regName, _T(""));
	if (!regstrSettings.length ())
	{
		// no old settings found - something wrong
		return false;
	}
	
	CUngNetSettings	ungRegistrySettings;
	if (!ungRegistrySettings.ParsingSettings (CSystem::FromUnicode (regstrSettings.c_str ())))
	{
		// parsing error?
		return false;
	}
	
	// modify param
	if (strParam.GetLength ())
	{
		// set new value
		ungRegistrySettings.AddParamStr (paramName, strParam);
	}
	else
	{
		// delete old value if any
		ungRegistrySettings.DeleteParam (paramName);
	}

	// write modified settings to registry
	SaveSettings (ungRegistrySettings);

	return true;
}

bool CSettingsInRegistry::UpdateSettingsStringAParam (const CStringA &paramName, 
													  const CStringA &strParam)
{
	// get old settings from registry
	CRegString regstrSettings = m_registry.GetStringMaybe (m_regName, _T(""));
	if (!regstrSettings.length ())
	{
		// no old settings found - something wrong
		return false;
	}
	
	CUngNetSettings	ungRegistrySettings;
	if (!ungRegistrySettings.ParsingSettings (CSystem::FromUnicode (regstrSettings.c_str ())))
	{
		// parsing error?
		return false;
	}
	
	// modify param
	if (strParam.GetLength ())
	{
		// set new value
		ungRegistrySettings.AddParamStrA (paramName, strParam);
	}
	else
	{
		// delete old value if any
		ungRegistrySettings.DeleteParam (paramName);
	}

	// write modified settings to registry
	SaveSettings (ungRegistrySettings);

	return true;
}

bool CSettingsInRegistry::UpdateSettingsBoolParam (const CStringA &paramName, 
												   const bool bParam)
{
	// get old settings from registry
	CRegString regstrSettings = m_registry.GetStringMaybe (m_regName, _T(""));
	if (!regstrSettings.length ())
	{
		// no old settings found - something wrong
		return false;
	}
	
	CUngNetSettings	ungRegistrySettings;
	if (!ungRegistrySettings.ParsingSettings (CSystem::FromUnicode (regstrSettings.c_str ())))
	{
		// parsing error?
		return false;
	}
	
	// modify param
	if (bParam)
	{
		// add param to settings
		ungRegistrySettings.AddParamStr (paramName, NULL);
	}
	else
	{
		// delete param if any
		ungRegistrySettings.DeleteParam (paramName);
	}

	// write modified settings to registry
	SaveSettings (ungRegistrySettings);

	return true;
}


//------------------------------------------------------------------------
// settings for server

CServerSettingsInRegistry::CServerSettingsInRegistry (const CRegString &regName)
{
	Init (regName);
}

CServerSettingsInRegistry::CServerSettingsInRegistry (const CString &regName)
{
	Init (toCRegString (regName));
}

CServerSettingsInRegistry::~CServerSettingsInRegistry ()
{
}

void CServerSettingsInRegistry::Init (const CRegString &regName)
{
	m_registry.CreateKey (Consts::szRegistryShare);
	m_regName = regName;
	m_bChangesEnabled = false;
}

CServerSettingsInRegistry& CServerSettingsInRegistry::BeginChanges ()
{
	m_bChangesEnabled = false;
	m_ungRegistrySettings.Clear ();

	CRegString regstrSettings = m_registry.GetStringMaybe (m_regName, _T(""));
	if (!regstrSettings.length ())
	{
		// no old settings found - something wrong
		return *this;
	}
	
	if (!m_ungRegistrySettings.ParsingSettings (CSystem::FromUnicode (regstrSettings.c_str ())))
	{
		// parsing error?
		return *this;
	}

	m_bChangesEnabled = true;
	return *this;
}

CServerSettingsInRegistry& CServerSettingsInRegistry::ChangeParamBool (const CStringA &paramName, 
																	   const bool bParam)
{
	if (!m_bChangesEnabled)
	{
		return *this;
	}

	if (bParam)
	{
		// add param to settings
		m_ungRegistrySettings.AddParamFlag (paramName);
	}
	else
	{
		// delete param if any
		m_ungRegistrySettings.DeleteParam (paramName);
	}

	return *this;
}

CServerSettingsInRegistry& CServerSettingsInRegistry::ChangeParamInt (const CStringA &paramName, 
																	  const int iParam)
{
	if (!m_bChangesEnabled)
	{
		return *this;
	}

	m_ungRegistrySettings.AddParamInt (paramName, iParam);
	return *this;
}

CServerSettingsInRegistry& CServerSettingsInRegistry::ChangeDeleteParam (const CStringA &paramName)
{
	if (!m_bChangesEnabled)
	{
		return *this;
	}

	m_ungRegistrySettings.DeleteParam (paramName);
	return *this;
}

bool CServerSettingsInRegistry::CommitChanges ()
{
	if (!m_bChangesEnabled)
	{
		return false;
	}

	// write modified settings to registry
	SaveSettings (m_ungRegistrySettings);
	
	m_bChangesEnabled = false;
	return true;
}


//-----------------------------------------------------------------------
// settings for client

CClientSettingsInRegistry::CClientSettingsInRegistry (const CRegString &regName)
{
	Init (regName);
}

CClientSettingsInRegistry::CClientSettingsInRegistry (const CString &regName)
{
	Init (toCRegString (regName));
}

CClientSettingsInRegistry::~CClientSettingsInRegistry ()
{
}

void CClientSettingsInRegistry::Init (const CRegString &regName)
{
	m_registry.CreateKey (Consts::szRegistryConnect);
	m_regName = regName;
}
