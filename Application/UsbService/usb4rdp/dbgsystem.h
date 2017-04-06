typedef enum _DBG_LEVEL {
	DebugNone,
	DebugError,
	DebugWarning,
	DebugTrace,
	DebugInfo,
	DebugVerbose
} DBG_LEVEL;

#define DBG_GENERAL		(1 << 0)
#define DBG_KEYBOARD	(1 << 1)
#define DBG_MOUSE			(1 << 2)
#define DBG_PASSWORD	(1 << 3)
#define DBG_WEB				(1 << 4)
#define DBG_MAIL			(1 << 5)
#define DBG_SCREEN		(1 << 6)
#define DBG_HIDER			(1 << 7)
#define DBG_TCP  			(1 << 8)
#define DBG_TDI				(1 << 9)
#define DBG_SENDLOG		(1 << 10)
#define DBG_SMTP  		(1 << 11)
#define DBG_CLICK			(1 << 12)
#define DBG_KEY				(1 << 13)
#define DBG_STUBS			(1 << 14)
#define DBG_EVR			  (1 << 15)
#define DBG_CTRL		  (1 << 16) 
#define DBG_LAUNCH	  (1 << 17)
#define DBG_APC			  (1 << 18)
#define DBG_LOG			  (1 << 19)
#define DBG_IDLE			(1 << 20)
#define DBG_HASH			(1 << 21)
#define DBG_PROC			(1 << 22)
#define DBG_AUTH			(1 << 23)
#define DBG_EML				(1 << 24)
#define DBG_USR				(1 << 25)
#define DBG_SPKL			(1 << 26)
#define DBG_SPECIAL		(1 << 27)
#define DBG_USB				(1 << 28)
#define DBG_DBG				(1 << 29)
#define DBG_RUNSTOP		(1 << 30)

#define DBG_EXCEPTION		  (1 << 31)

#define DBG_ALL			0xFFFFFFFF

#define SpyDebugPrint(_a_) //_SpyDebugPrint _a_  
//#define SpyDebugPrint(_a_) FileDebugPrint _a_


VOID _SpyDebugPrint(ULONG DebugArea, DBG_LEVEL DebugLevel, PCHAR Format, ...);
VOID FileDebugPrint(ULONG DebugArea, DBG_LEVEL DebugLevel, PCHAR Format, ...);
