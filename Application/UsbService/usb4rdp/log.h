#define LOG_FILENAME L"c:\\usb4rdp.log"

typedef enum _LOG_DEST {
	Console,
	DebuggerDest,
	FileDest,
	MessageBoxDest
}	LOG_DEST, *PLOG_DEST;

typedef enum _LOG_LEVEL {
	LogError,
	LogWarning,
	LogTrace,
	LogInfo
} LOG_LEVEL, *PLOG_LEVEL;

#define LOG_GENERAL		  (1 << 0)
#define LOG_CALLBACK		(1 << 1)
#define LOG_SERVER			(1 << 2)
#define LOG_ALL					0xFFFFFFFF

class CLog {

protected:
	
	LOG_DEST	  m_LogDest;
	LOG_LEVEL		m_LogLevel;
	ULONG				m_LogArea;
	CRITICAL_SECTION m_cs;

public:

	CLog();
	CLog(LOG_DEST LogDest, LOG_LEVEL LogLevel, ULONG LogArea);
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