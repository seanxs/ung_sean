#include "Spy.h"

DBG_LEVEL CurrentDebugLevel = DebugInfo;
//ULONG     CurrentDebugArea  = (DBG_ALL) & ~(DBG_MOUSE|DBG_TCP|DBG_KEYBOARD);
ULONG     CurrentDebugArea  = DBG_;

VOID _SpyDebugPrint(ULONG DebugArea, DBG_LEVEL DebugLevel, PCHAR Format, ...) {
	if((DebugLevel <= CurrentDebugLevel) && (DebugArea & CurrentDebugArea)) {
	  va_list Arguments;
	 	CHAR Buffer[512];
	  va_start(Arguments, Format);
		vsprintf(Buffer, Format, Arguments);
		switch(DebugLevel) {
			case DebugError: {
				DbgPrint("[SPY -  <ERROR> ] %s\n", Buffer);
				break;
			}
			case DebugWarning: {
				DbgPrint("[SPY - <WARNING>] \t%s\n", Buffer);
				break;
			}
			case DebugTrace: {
				DbgPrint("[SPY -  <TRACE> ] \t\t%s\n", Buffer);
				break;
			}
			case DebugInfo: {
				DbgPrint("[SPY -  <INFO>  ] \t\t\t%s\n", Buffer);
				break;
			}
			case DebugVerbose: {
				DbgPrint("[SPY - <VERBOSE>] \t\t\t\t%s\n", Buffer);
				break;
			}
		}
	  va_end(Arguments);
	}
}

VOID FileDebugPrint(ULONG DebugArea, DBG_LEVEL DebugLevel, PCHAR Format, ...) {
	if((DebugLevel <= CurrentDebugLevel) && (DebugArea & CurrentDebugArea)) {
	  va_list Arguments;
	 	CHAR Buffer[512], Buffer2[512];
		PCHAR OutBuffer = NULL;
		DWORD OutBufferLength = 0;
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		IO_STATUS_BLOCK IoStatus = {0};

	  va_start(Arguments, Format);
		vsprintf(Buffer, Format, Arguments);
		switch(DebugLevel) {
			case DebugError: {
				sprintf(Buffer2, "[SPY -  <ERROR> ] %s\n", Buffer);
				break;
			}
			case DebugWarning: {
				sprintf(Buffer2, "[SPY - <WARNING>] \t%s\n", Buffer);
				break;
			}
			case DebugTrace: {
				sprintf(Buffer2, "[SPY -  <TRACE> ] \t\t%s\n", Buffer);
				break;
			}
			case DebugInfo: {
				sprintf(Buffer2, "[SPY -  <INFO>  ] \t\t\t%s\n", Buffer);
				break;
			}
			case DebugVerbose: {
				sprintf(Buffer2, "[SPY - <VERBOSE>] \t\t\t\t%s\n", Buffer);
				break;
			}
			default: goto exit;
		}

		status = Base64EncodeBuffer(Buffer2, strlen(Buffer2), &OutBuffer, &OutBufferLength); CHECK;
		if (!Globals.hDbgLog) {
			OBJECT_ATTRIBUTES ObjAttr;
			UNICODE_STRING	usFileName;
			RtlInitUnicodeString(&usFileName, DBGLOG_FILENAME);
			InitializeObjectAttributes(&ObjAttr, &usFileName, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL, NULL);
			ZwCreateFile(&Globals.hDbgLog, FILE_READ_DATA|FILE_WRITE_DATA, &ObjAttr, &IoStatus,
								NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
		}
		
		ZwWriteFile(Globals.hDbgLog, NULL, NULL, NULL, &IoStatus, OutBuffer, OutBufferLength, NULL, NULL);

exit:
		if (OutBuffer) ExFreePool(OutBuffer);
	  va_end(Arguments);
	}
}
