/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UngNetSettings.cpp

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "UngNetSettings.h"
#include "System.h"

CUngNetSettings::CUngNetSettings(void)
{
}

CUngNetSettings::~CUngNetSettings(void)
{
}

void CUngNetSettings::AddParamStrA (CStringA strName, CStringA strValue)
{
	strName.MakeUpper ();
	m_mapParam [strName] = strValue;
}

void CUngNetSettings::AddParamStr (CStringA strName, CString strValue)
{
	CStringA strTemp = CSystem::EchoSpecSymbol (CSystem::FromUnicode (strValue));
	strName.MakeUpper ();
	m_mapParam [strName] = strTemp;
}

void CUngNetSettings::AddParamInt (CStringA strName, int iValue)
{
	CStringA str;
	strName.MakeUpper ();
	str.Format ("%d", iValue);
	m_mapParam [strName] = str;
}

void CUngNetSettings::AddParamBoolean (CStringA strName, bool bValue)
{
	strName.MakeUpper ();
	m_mapParam [strName] = bValue ? "1" : "0";
}

void CUngNetSettings::AddParamFlag (CStringA strName)
{
	AddParamStrA (strName, "");
}

void CUngNetSettings::AddParamFlag (CStringA strName, BOOL bFlag)
{
	if (bFlag)
	{
		AddParamStrA (strName, "");
	}
}

CString CUngNetSettings::GetParamStr (CStringA strName, CString strDefault) const
{
	CStringA strRet;
	std::map <CStringA, CStringA>::const_iterator item;

	strName.MakeUpper ();

	item = m_mapParam.find (strName);
	if (item != m_mapParam.end ())
	{
		strRet = item->second;
	}

	if (strRet.GetLength ())
	{
		return CSystem::ToUnicode (CSystem::UnEchoSpecSymbol (strRet));
	}

	return strDefault;
}

CStringA CUngNetSettings::GetParamStrA (CStringA strName, CStringA strDefault) const
{
	CStringA strRet = strDefault;
	std::map <CStringA, CStringA>::const_iterator item;

	strName.MakeUpper ();

	item = m_mapParam.find (strName);
	if (item != m_mapParam.end ())
	{
		strRet = item->second;
	}

	return strRet;
}

int CUngNetSettings::GetParamInt (CStringA strName, int iDefault) const
{
	int iRet = iDefault;
	std::map <CStringA, CStringA>::const_iterator item;

	strName.MakeUpper ();

	item = m_mapParam.find (strName);
	if (item != m_mapParam.end ())
	{
		iRet = atoi ( item->second );
	}

	return iRet;
}

bool CUngNetSettings::GetParamBoolean (CStringA strName, bool bDefault) const
{
	bool bRet = bDefault;
	std::map <CStringA, CStringA>::const_iterator item;

	strName.MakeUpper ();

	item = m_mapParam.find (strName);
	if (item != m_mapParam.end ())
	{
		if (item->second.GetLength ())
		{
			bRet = atoi ( item->second )?true:false;
		}
		else
		{
			bRet = true;
		}
	}

	return bRet;
}

bool CUngNetSettings::ExistParam (CStringA strName) const
{
	bool bRet = false;
	std::map <CStringA, CStringA>::const_iterator item;

	strName.MakeUpper ();

	item = m_mapParam.find (strName);
	if (item != m_mapParam.end ())
	{
		bRet = true;
	}
	return bRet;
}

bool CUngNetSettings::DeleteParam (CStringA strName)
{
	bool bRet = false;
	std::map <CStringA, CStringA>::iterator item;

	strName.MakeUpper ();

	item = m_mapParam.find (strName);
	if (item != m_mapParam.end ())
	{
		m_mapParam.erase (item);
		bRet = true;
	}
	return bRet;
}

CStringA CUngNetSettings::BuildSetttings () const
{
	CStringA strSettings;
	std::map <CStringA, CStringA>::const_iterator item;

	for (item = m_mapParam.begin (); item != m_mapParam.end (); item++)
	{
		if (!strSettings.IsEmpty ())
		{
			strSettings += GetSeparator ();
		}
		strSettings += item->first;
		if (item->second.GetLength ())
		{
			strSettings += "=";
			strSettings += item->second;
		}
	}

	return strSettings;
}

CStringA CUngNetSettings::ComposeCommand () const
{
	CStringA strCmd = BuildSetttings () + GetEnd ();
	return strCmd;
}

bool CUngNetSettings::ParsingSettings (CStringA &strSettings, bool bClean)
{
	CStringA str;
	CStringA strValue;
	CStringA strSeparator (GetSeparator ());
	int iPos = 0;
	int iParam;

	if (bClean)
	{
		m_mapParam.clear ();
	}

	do
	{
		str = strSettings.Tokenize (strSeparator, iPos);
		if (str.GetLength ())
		{
			iParam = str.Find ("=", 0);

			if (iParam == -1)
			{
				//	avoid atl raising exception, please comment the following two lines
				str.MakeUpper ();
				m_mapParam [str] = "";
			}
			else
			{
				strValue = str.Right (str.GetLength () - iParam - 1);
				str = str.Left (iParam);

				str.MakeUpper ();
				m_mapParam [str] = strValue;
			}
		}
	}while (iPos != -1);

	return m_mapParam.size ()?true:false;
}

void CUngNetSettings::Clear ()
{
	m_mapParam.clear ();
}
