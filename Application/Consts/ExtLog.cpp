/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   extlog.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include <atlstr.h>
#include <atlcomtime.h>
#include "consts.h"
#include "extlog.h"

CString CExtLog::m_strFileName;
HANDLE  CExtLog::m_hPipeLog = INVALID_HANDLE_VALUE;
CRITICAL_SECTION CExtLog::m_csLog;

CExtLog::CExtLog() {

}

CExtLog::~CExtLog() {

}

// add string to file log
void CExtLog::AddLogFile (LPCTSTR String) {
	CString sTmp;
	CStringA sTmpA;
	CTime   time = CTime::GetCurrentTime();
	sTmp = time.Format(L"%d.%m.%Y %H:%M:%S  - ") + String;
	// save to pipe
	AddLogPipe (sTmp);

	// save to file
	sTmp += _T("\r\n");

	if (m_strFileName.IsEmpty())	{
		return;
	}

	EnterCriticalSection (&m_csLog);
	HANDLE  hFile;
	DWORD   dwRead;
	hFile = CreateFile (m_strFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}

	sTmpA = sTmp;
	SetFilePointer (hFile, 0, 0, FILE_END);
	dwRead = (DWORD)(sTmpA.GetLength());
	WriteFile (hFile, sTmpA, dwRead, &dwRead, NULL); 
	CloseHandle (hFile);
	LeaveCriticalSection (&m_csLog);
}

void CExtLog::AddLogPipe (LPCTSTR String)
{
	DWORD cbWritten; 

	if (m_hPipeLog == INVALID_HANDLE_VALUE)
	{
		if (!OpenLogPipe (Consts::szPipeNameLog))
		{
			return;
		}
	}

	for (int a = 0; a < 2; a++)
	{
		if (!WriteFile( 
			m_hPipeLog,                  // pipe handle 
			String,             // message 
			DWORD((_tcslen(String)+ 1)*sizeof (TCHAR)), // message length 
			&cbWritten,             // bytes written 
			NULL))                  // not overlapped 
		{
			if (OpenLogPipe (Consts::szPipeNameLog))
			{
				continue;
			}
		}
		break;

	}

	return; 
}

bool CExtLog::OpenLogPipe (LPCTSTR szPipe)
{
	BOOL fSuccess; 
	DWORD dwMode; 

	// Try to open a named pipe; wait for it, if necessary. 

	m_hPipeLog = CreateFile( 
		szPipe,   // pipe name 
		GENERIC_READ |  // read and write access 
		GENERIC_WRITE, 
		0,              // no sharing 
		NULL,           // no security attributes
		OPEN_EXISTING,  // opens existing pipe 
		0,              // default attributes 
		NULL);          // no template file 


	// The pipe connected; change to message-read mode. 

	dwMode = PIPE_READMODE_MESSAGE; 
	fSuccess = SetNamedPipeHandleState( 
		m_hPipeLog,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 

	if (!fSuccess) 
	{
		//printf("SetNamedPipeHandleState failed"); 
		m_hPipeLog = INVALID_HANDLE_VALUE;
		return false;
	}

	return true;
}

void CExtLog::CloseLogPipe ()
{
	if (m_hPipeLog != INVALID_HANDLE_VALUE)
	{
		CloseHandle (m_hPipeLog);
		m_hPipeLog = INVALID_HANDLE_VALUE;
	}
}
