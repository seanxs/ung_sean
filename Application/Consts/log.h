/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   log.h

Environment:

    User mode

--*/

#pragma once

#define LOG_FILENAME L"c:\\usb4rdp.log"

typedef enum _LOG_DEST {
	Console,
	DebuggerDest,
	FileDest,
	MessageBoxDest,
	Nirvana
}	LOG_DEST, *PLOG_DEST;

typedef enum _LOG_LEVEL {
	LogError,
	LogWarning,
	LogTrace,
	LogInfo,
	LogVerbose
} LOG_LEVEL, *PLOG_LEVEL;

#define LOG_NONE		(0)
#define LOG_GENERAL		(1 << 0)
#define LOG_CONSTR		(1 << 1)
#define LOG_UTIL		(1 << 2)
#define LOG_AUX			(1 << 3)
#define LOG_THREAD		(1 << 4)
#define LOG_DATA		(1 << 5)
#define LOG_PARAM		(1 << 6)
#define LOG_DBG			(1 << 7)
#define LOG_CMD			(1 << 8)
#define LOG_ENTEREXIT	(1 << 9)
#define LOG_RESULT		(1 << 10)
#define LOG_INIT		(1 << 11)
#define LOG_CALLBACK	(1 << 12)
#define LOG_EXCLUDE		(1 << 13)
#define LOG_SESSIONS	(1 << 14)
#define LOG_CONNECT		(1 << 15)
#define LOG_RDC		(1 << 16)

#define LOG_ALL					0xFFFFFFFF

class CLog 
{
protected:
	LOG_DEST	  m_LogDest;
	LOG_LEVEL		m_LogLevel;
	ULONG				m_LogArea;
	CRITICAL_SECTION m_LogCriticalSection;
	LPTSTR m_strLogFilePath;
	bool m_bCustomLogFile;

public:
	CLog();
	CLog(LOG_DEST LogDest, LOG_LEVEL LogLevel, ULONG LogArea);
	CLog(LPCTSTR strLogFilePath);
	~CLog();

	void SetLogDest(LOG_DEST LogDest);
	LOG_DEST GetLogDest();

	void SetLogLevel(LOG_LEVEL LogLevel);
	LOG_LEVEL GetLogLevel();

	void SetLogArea(ULONG LogArea);
	ULONG GetLogArea();
	void AddLogArea(ULONG Mask);
	void SubLogArea(ULONG Mask);

	void LogString(ULONG LogArea, LOG_LEVEL LogLevel, PCHAR Format, ...);
	void LogStringDest(LOG_DEST LogDest, ULONG LogArea, LOG_LEVEL LogLevel, PCHAR Format, ...);
};
