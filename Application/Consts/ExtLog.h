/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   extlog.h

Environment:

    User mode

--*/

class CExtLog {

	static HANDLE   m_hPipeLog;
	static CString  m_strFileName;
	static CRITICAL_SECTION m_csLog;
	static void AddLogPipe (LPCTSTR String);
	static bool OpenLogPipe (LPCTSTR szPipe);
	static void CloseLogPipe ();

public:
	static void AddLogFile (LPCTSTR String);
	CExtLog();
	virtual ~CExtLog();
};
