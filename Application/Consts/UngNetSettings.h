/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UngNetSettings.h

Abstract:

	Settings dictionary with serialization/deserialization support.

Environment:

    User mode

--*/

#pragma once

#include <map>
#include <atlstr.h>

class CUngNetSettings
{
public:
	CUngNetSettings(void);
	~CUngNetSettings(void);

	void AddParamStrA (CStringA strName, CStringA strValue);
	void AddParamStr (CStringA strName, CString strValue);
	void AddParamInt (CStringA strName, int iValue);
	void AddParamBoolean (CStringA strName, bool bValue);
	void AddParamFlag (CStringA strName);
	void AddParamFlag (CStringA strName, BOOL bFlag);

	CStringA GetParamStrA (CStringA strName, CStringA strDefault = "") const;
	CString GetParamStr (CStringA strName, CString strDefault = _T("")) const;
	int GetParamInt (CStringA strName, int iDefault = 0) const;
	bool GetParamBoolean (CStringA strName, bool bDefault = false) const;
	bool GetParamFlag (CStringA strName) const {return ExistParam (strName);}
	bool ExistParam (CStringA strName) const;
	bool DeleteParam (CStringA strName);  // removes parameter if it exists and returns true; returns false if no such parameter exists

	CStringA BuildSetttings () const;
	CStringA ComposeCommand () const;
	bool ParsingSettings (CStringA &strSettings, bool bClean = true);

	void Clear ();

public:
	static char GetSeparator () {return ',';}
	static char GetSeparatorConfig () {return 0;}
	static char GetEnd () {return char (0xff);}

private:
	std::map <CStringA, CStringA> m_mapParam;
};
