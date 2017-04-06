#include "stdafx.h"
#include "log.h"

CLog::CLog(): m_LogDest(FileDest), m_LogLevel(LogInfo), m_LogArea(LOG_ALL) {
	InitializeCriticalSection (&m_cs);
}

CLog::CLog(LOG_DEST LogDest, LOG_LEVEL LogLevel, ULONG LogArea){
	m_LogDest = LogDest;
	m_LogLevel = LogLevel;
	m_LogArea = LogArea;
	InitializeCriticalSection (&m_cs);
}

CLog::~CLog() {
	DeleteCriticalSection (&m_cs);
}

void CLog::SetLogDest(LOG_DEST LogDest) {
	m_LogDest = LogDest;
}

LOG_DEST CLog::GetLogDest() {
	return m_LogDest;
}

void CLog::SetLogLevel(LOG_LEVEL LogLevel) {
	m_LogLevel = LogLevel;
}

LOG_LEVEL CLog::GetLogLevel() {
	return m_LogLevel;
}

void CLog::SetLogArea(ULONG LogArea) {
	m_LogArea = LogArea;
}

ULONG CLog::GetLogArea() {
	return m_LogArea;
}

void CLog::AddLogArea(ULONG Mask) {
	m_LogArea |= Mask;
}

void CLog::SubLogArea(ULONG Mask) {
	m_LogArea |= ~Mask;
}

void CLog::LogString(ULONG LogArea, LOG_LEVEL LogLevel, PCHAR Format, ...) {

	if((LogLevel <= m_LogLevel) && (LogArea & m_LogArea)) {
		va_list Arguments;
		va_start(Arguments, Format);

		int len = _vscprintf(Format, Arguments);
		
		PCHAR Buffer = (PCHAR)malloc(len+100);
		ZeroMemory(Buffer, len+100);
		
		vsprintf_s(Buffer, len+100, Format, Arguments);

		PCHAR Buffer2 = (PCHAR)malloc(len+100);
		ZeroMemory(Buffer2, len+100);

		SYSTEMTIME SystemTime;
		GetLocalTime(&SystemTime);
		COleDateTime OleDateTime(SystemTime);
		CString DateTimeString = OleDateTime.Format(L"%d.%m.%y.%H:%M:%S.");
		
		CHAR ms[4];
		ZeroMemory(ms, sizeof ms);
		if (SystemTime.wMilliseconds < 10) 
			sprintf(ms, "%d%d%d", 0, 0, SystemTime.wMilliseconds);
		else if(SystemTime.wMilliseconds < 100)
			sprintf(ms, "%d%d", 0, SystemTime.wMilliseconds);
		else 
			sprintf(ms, "%d", SystemTime.wMilliseconds);

		switch(LogLevel) {
			case LogError: {
				sprintf(Buffer2, "[usb4rdp(%ws%s)<ERROR>  ] %s\n", DateTimeString.GetBuffer(), ms, Buffer);
				break;
			}
			case LogWarning: {
				sprintf(Buffer2, "[usb4rdp(%ws%s)<WARNING>] \t%s\n", DateTimeString.GetBuffer(), ms, Buffer);
				break;
			}
			case LogTrace: {
				sprintf(Buffer2, "[usb4rdp(%ws%s)<TRACE>  ] \t\t%s\n", DateTimeString.GetBuffer(), ms, Buffer);
				break;
			}
			case LogInfo: {
				sprintf(Buffer2, "[usb4rdp(%ws%s)<INFO>   ] \t\t\t%s\n", DateTimeString.GetBuffer(), ms, Buffer);
				break;
			}
		}
		DateTimeString.ReleaseBuffer();
		free(Buffer);

		switch (m_LogDest) {
			case Console: {
				printf(Buffer2);
			}
			case DebuggerDest: {
				OutputDebugStringA(Buffer2);
				break;
			}
			case FileDest: {
				EnterCriticalSection (&m_cs);
				HANDLE  hLogFile = CreateFile(LOG_FILENAME, FILE_WRITE_DATA, NULL, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				SetFilePointer(hLogFile, GetFileSize(hLogFile, NULL), NULL, FILE_BEGIN);
				if(hLogFile != INVALID_HANDLE_VALUE) {
					ULONG dwWrite = (ULONG)strlen(Buffer2);
					WriteFile(hLogFile, Buffer2, dwWrite, &dwWrite, NULL);
					CloseHandle(hLogFile);
				}
				LeaveCriticalSection(&m_cs);
				break;
			}
			case MessageBoxDest: {
				MessageBoxA(0, Buffer2, "usb4rdp.log", MB_OK);
				break;
		 }
		}
		free(Buffer2);
		va_end(Arguments);
	}

}

void CLog::LogStringDest(LOG_DEST LogDest, ULONG LogArea, LOG_LEVEL LogLevel, PCHAR Format, ...) {
	LOG_DEST SavedLogDest = GetLogDest();
	SetLogDest(LogDest);

	va_list Arguments;
	va_start(Arguments, Format);
	LogString(LogArea, LogLevel, Format, Arguments);
	va_end(Arguments);

	SetLogDest(SavedLogDest);
}


