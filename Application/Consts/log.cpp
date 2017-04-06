/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   log.cpp

Environment:

    User mode

--*/

#include "stdafx.h"

#include <atlstr.h>
#include <atlcomtime.h>
#include "log.h"

CLog::CLog(): m_LogDest(FileDest), m_LogLevel(LogInfo), m_LogArea(LOG_ALL) 
{
	m_strLogFilePath = LOG_FILENAME;
	m_bCustomLogFile = false;
	InitializeCriticalSection (&m_LogCriticalSection);
}

CLog::CLog(LOG_DEST LogDest, LOG_LEVEL LogLevel, ULONG LogArea)
{
	m_LogDest = LogDest;
	m_LogLevel = LogLevel;
	m_LogArea = LogArea;
	m_strLogFilePath = LOG_FILENAME;
	m_bCustomLogFile = false;
	InitializeCriticalSection (&m_LogCriticalSection);
}

CLog::CLog(LPCTSTR strLogFilePath): 
	m_LogDest(FileDest), 
	m_LogLevel(LogInfo), 
	m_LogArea(LOG_ALL)
{
	InitializeCriticalSection (&m_LogCriticalSection);
	m_bCustomLogFile = true;
	const size_t n = wcslen(strLogFilePath) + 1;
	m_strLogFilePath = new TCHAR [n];
	memset(m_strLogFilePath, 0, n * sizeof(TCHAR));
	_tcscpy_s(m_strLogFilePath, n, strLogFilePath);
}

CLog::~CLog() 
{
	DeleteCriticalSection (&m_LogCriticalSection);
	
	if(m_bCustomLogFile) 
	{
		delete[] m_strLogFilePath;
	}
}

void CLog::SetLogDest(LOG_DEST LogDest) 
{
	m_LogDest = LogDest;
}

LOG_DEST CLog::GetLogDest() 
{
	return m_LogDest;
}

void CLog::SetLogLevel(LOG_LEVEL LogLevel) 
{
	m_LogLevel = LogLevel;
}

LOG_LEVEL CLog::GetLogLevel() 
{
	return m_LogLevel;
}

void CLog::SetLogArea(ULONG LogArea) 
{
	m_LogArea = LogArea;
}

ULONG CLog::GetLogArea() 
{
	return m_LogArea;
}

void CLog::AddLogArea(ULONG Mask) 
{
	m_LogArea |= Mask;
}

void CLog::SubLogArea(ULONG Mask) 
{
	m_LogArea |= ~Mask;
}

void CLog::LogString(ULONG LogArea, LOG_LEVEL LogLevel, PCHAR Format, ...) 
{
#ifdef DEBUG
	int nSavedLastError = GetLastError();

	if((LogLevel <= m_LogLevel) && (LogArea & m_LogArea)) 
	{
		va_list Arguments;
		va_start(Arguments, Format);

		int len = _vscprintf(Format, Arguments);
		const size_t n = len + 100;

		PCHAR Buffer = (PCHAR)malloc(n);

		_ASSERT(Buffer);
		
		ZeroMemory(Buffer, n);
		
		vsprintf_s(Buffer, n, Format, Arguments);

		PCHAR Buffer2 = (PCHAR)malloc(n);
		ZeroMemory(Buffer2, n);

		SYSTEMTIME SystemTime;
		GetLocalTime(&SystemTime);
		COleDateTime OleDateTime(SystemTime);
		CString DateTimeString = OleDateTime.Format(L"%d.%m.%y.%H:%M:%S.");
		
		CHAR ms[4];
		ZeroMemory(ms, sizeof ms);
		sprintf_s(ms, "%03d", SystemTime.wMilliseconds);

		const CHAR *format = NULL;
		switch(LogLevel) 
		{
			case LogError: 
				format = "[usb4rdp(%ws%s)<ERROR>  %.4d] %s\r\n";
				break;

			case LogWarning: 
				format = "[usb4rdp(%ws%s)<WARNING>%.4d]   %s\r\n";
				break;

			case LogTrace: 
				format = "[usb4rdp(%ws%s)<TRACE>  %.4d]     %s\r\n";
				break;

			case LogInfo: 
				format = "[usb4rdp(%ws%s)<INFO>   %.4d]       %s\r\n";
				break;
		}

		if (format) {
			sprintf_s(Buffer2, n, format, DateTimeString.GetBuffer(), ms, GetCurrentThreadId(), Buffer);
			DateTimeString.ReleaseBuffer();
		}
		free(Buffer);

		switch (m_LogDest) 
		{
			case Console: 
				{
					printf(Buffer2);
					break;
				}
			case DebuggerDest: 
				{
					OutputDebugStringA(Buffer2);
					break;
				}
			case FileDest: 
				{
					EnterCriticalSection (&m_LogCriticalSection);
					
					HANDLE  hLogFile = CreateFileW(m_strLogFilePath, FILE_WRITE_DATA, NULL, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

					do {

						int nCreateFileError = 0;

						if (hLogFile == INVALID_HANDLE_VALUE)
						{
							nCreateFileError = GetLastError();
							break;
						}

						SetFilePointer(hLogFile, GetFileSize(hLogFile, NULL), NULL, FILE_BEGIN);

						if(hLogFile != INVALID_HANDLE_VALUE) 
						{
							ULONG dwWrite = (ULONG)strlen(Buffer2);
							WriteFile(hLogFile, Buffer2, dwWrite, &dwWrite, NULL);
							CloseHandle(hLogFile);
						}

					} while (false);

					LeaveCriticalSection(&m_LogCriticalSection);
					break;
				}

			case MessageBoxDest: 
				{
					MessageBoxA(0, Buffer2, "usb4rdp.log", MB_OK);
					break;
				}

			case Nirvana : 
				{
					break;
				}

			default: 
				{
					break;
				}
		} // END of switch of log destination

		free(Buffer2);

		va_end(Arguments);
	}

	SetLastError(nSavedLastError);
#endif
}

void CLog::LogStringDest(LOG_DEST LogDest, ULONG LogArea, LOG_LEVEL LogLevel, PCHAR Format, ...) 
{
	LOG_DEST SavedLogDest = GetLogDest();
	SetLogDest(LogDest);

	va_list Arguments;
	va_start(Arguments, Format);
	LogString(LogArea, LogLevel, Format, Arguments);
	va_end(Arguments);

	SetLogDest(SavedLogDest);
}
