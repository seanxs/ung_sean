/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   threads.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "usb4rdp.h"

DWORD WINAPI ThreadReadServicePipe(LPVOID lParam) 
{
	ULONG FuncLogArea = LOG_UTIL;
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	DWORD dwLastThreadError = 0;

	OVERLAPPED Overlapped = {0};
	Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	DWORD dwDefaultBufferSize = BUFSIZE_2K;
	DWORD dwBufferSize = 0;
	DWORD dwTotalRead = 0;
	BYTE *lpBuffer = NULL;
	DWORD dwTick;

	while (hDataPipe != INVALID_HANDLE_VALUE)
	{
		// Read from pipe
		if (dwBufferSize == 0)
		{
			if( PeekNamedPipe (hDataPipe, NULL, 0, NULL, &dwBufferSize, NULL) == FALSE) 
			{
				int err = GetLastError();
				logger.LogString(FuncLogArea, LogInfo, "PeekNamedPipe() failed GLE = %d", err);
				dwBufferSize = dwDefaultBufferSize;
			}
			else
			{
				logger.LogString(FuncLogArea, LogInfo, "PeekNamedPipe() success dwBufferSize = %d", dwBufferSize);
			}

			if(dwBufferSize == 0)
			{
				dwBufferSize = dwDefaultBufferSize;
			}

			logger.LogString(FuncLogArea, LogInfo, "Allocating %d bytes for reading buffer.", dwBufferSize);
			lpBuffer = new BYTE [dwBufferSize];
			ZeroMemory (lpBuffer, dwBufferSize);
			dwTick = GetTickCount ();
		}

		DWORD dwRead = 0;
		logger.LogString(FuncLogArea, LogInfo, "Reading from pipe...");
		BOOL bRes = ReadFile (hDataPipe, lpBuffer + dwTotalRead, dwBufferSize, &dwRead, &Overlapped);

		if (bRes)
		{
			dwTotalRead += dwRead;
		}
		else 
		{
			int nReadFileLastError = GetLastError();

			logger.LogString(FuncLogArea, LogInfo, "Call ReadFile() FAILED! \"bRes\" = %s, \"dwRead\" = %d, GLE = %d", bRes ? "TRUE":"FALSE", dwRead, nReadFileLastError);

			if (nReadFileLastError == ERROR_IO_PENDING)
			{
				logger.LogString(FuncLogArea, LogError, "GLE = ERROR_IO_PENDING. Call GetOverlappedResult()...");
				bRes = GetOverlappedResult(hDataPipe, &Overlapped, &dwRead, TRUE);
				nReadFileLastError = GetLastError();
			}

			if (bRes == FALSE) 
			{

				if (nReadFileLastError == ERROR_BROKEN_PIPE) 
				{
					break;
				}

				if (nReadFileLastError == ERROR_MORE_DATA)
				{
					logger.LogString(FuncLogArea, LogInfo, "GLE = ERROR_MORE_DATA. Some part of data received. dwRead = %d", dwRead);

					if(lpBuffer)
					{
						logger.LogString(FuncLogArea, LogError, "Call PeekNamedPipe()...");

						if (dwRead == 0)
						{
							dwRead = dwBufferSize;
						}

						if ( PeekNamedPipe (hDataPipe, NULL, 0, NULL, &dwBufferSize, NULL) )
						{
							logger.LogString(FuncLogArea, LogError, "PeekNamedPipe() return \"dwBufferSize\" = %d", dwBufferSize);
							
							PBYTE pNewSizeBuffer = new BYTE[dwRead + dwBufferSize];

							ZeroMemory(pNewSizeBuffer, dwRead + dwBufferSize);
							CopyMemory(pNewSizeBuffer, lpBuffer, dwRead);
							
							delete [] lpBuffer;

							dwTotalRead += dwRead;
							lpBuffer = pNewSizeBuffer;
							continue;
						}
					}
				}
				DebugBreak();
			}
			else 
			{ 
				// read block completed
				logger.LogString(FuncLogArea, LogInfo, "Read success. \"dwRead\" = %d", dwRead);
				dwTotalRead = dwRead;
			}
		}

		AtlTrace (_T("rdp2dll read - T%d - D%d\n"), GetTickCount () - dwTick, dwTotalRead);
		// save to virtual chanel
		logger.LogString(FuncLogArea, LogInfo, "-> Writing to channel...");
		UINT ret = SavedEntryPoints.pVirtualChannelWrite(ChannelHandle, lpBuffer, dwTotalRead, lpBuffer);
		
		if (ret != CHANNEL_RC_OK) 
		{
			logger.LogString(FuncLogArea, LogError, "Write error! VirtualChannelWrite() return %d. BREAK", ret);
			break;
		}
		else 
		{
			logger.LogString(FuncLogArea, LogInfo, "VirtualChannelWrite() success!");
		}

		dwBufferSize = 0;
		dwTotalRead = 0;
		lpBuffer = NULL;

	} // while(hDataPipe != INVALID_HANDLE_VALUE) {

	if(Overlapped.hEvent)
	{
		CloseHandle(Overlapped.hEvent);
	}

	logger.LogString(FuncLogArea, LogTrace, "RESULT \"dwLastThreadError\" = %d", dwLastThreadError);
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
	return dwLastThreadError;
}
