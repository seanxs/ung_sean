/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   usb4rdp.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "usb4rdp.h"
#include "..\..\Consts\UngNetSettings.h"

CLog logger(L"c:\\usb4rdp.dll.log");
CHANNEL_ENTRY_POINTS SavedEntryPoints = {0};
HANDLE ClientHandle = NULL;
DWORD  ChannelHandle = NULL;

CServiceManager ServiceManagerInstance;

HANDLE hDataPipe = INVALID_HANDLE_VALUE;


BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints) 
{
	logger.LogString(LOG_CALLBACK, LogTrace, "\n--------------DLL ENTRY POINT--------------");
	logger.LogString(LOG_CALLBACK, LogTrace, __FUNCTION__"(): ENTER");
	
	CHANNEL_DEF VirtualChannelDef;
	strcpy_s(VirtualChannelDef.name, sizeof VirtualChannelDef.name, VIRTUAL_CHANNEL_NAME);
	VirtualChannelDef.options = CHANNEL_OPTION_ENCRYPT_RDP;

	logger.LogString(LOG_CALLBACK, LogInfo, "Initialization virtual channel \"%s\".", VIRTUAL_CHANNEL_NAME);

	UINT retval = pEntryPoints->pVirtualChannelInit(&ClientHandle, 
													&VirtualChannelDef, 
													1, 
													1, 
													VirtualChannelInitEvent);

	if(retval == CHANNEL_RC_OK) 
	{
		logger.LogString(LOG_CALLBACK, LogInfo, "VirtualChannelInit return CHANNEL_RC_OK.");
	}
	else 
	{
		logger.LogString(LOG_CALLBACK, LogError, "VirtualChannelInit return %d.", retval);
	}

	memcpy(&SavedEntryPoints, pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS));

	logger.LogString(LOG_CALLBACK, LogTrace, __FUNCTION__": EXIT");
	return TRUE;
}

VOID VCAPITYPE VirtualChannelInitEvent(LPVOID pInitHandle, UINT Event, LPVOID pData, UINT dataLength) 
{
	ULONG FuncLogArea = LOG_THREAD;
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__": ENTER");

	ServiceManagerInstance.SetCharEnd (CUngNetSettings::GetEnd ());
	switch(Event) 
	{

		case CHANNEL_EVENT_INITIALIZED: 
			{
				logger.LogString(FuncLogArea, LogInfo, "CHANNEL_EVENT_INITIALIZED event received.");
				break;
			}
		case CHANNEL_EVENT_CONNECTED: 
			{ 
				logger.LogString(FuncLogArea, LogInfo, "CHANNEL_EVENT_CONNECTED event received. Connected to \"%ws\"", pData);

				DWORD dwCurrentProcessId = GetCurrentProcessId();

				CString strPipeNameWithId;
				strPipeNameWithId.Format(_T("%ws_%d"), g_wsPipeName, dwCurrentProcessId);

				hDataPipe = CreateDataPipe((LPCWSTR)strPipeNameWithId);

				if (hDataPipe != INVALID_HANDLE_VALUE)
				{
					logger.LogString(FuncLogArea, LogInfo, "Named pipe \"%ws\" created successful", (LPCWSTR)strPipeNameWithId);

					CStringA strCmd;
					CUngNetSettings settings;
					settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand[Consts::CP_ADD_RDC_CONNECTION]);
					settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM], dwCurrentProcessId);

					strCmd = settings.BuildSetttings ();
					strCmd += CUngNetSettings::GetEnd ();

					if (!ServiceManagerInstance.SendCommand(strCmd, strCmd.GetLength()))
					{
						CloseHandle(hDataPipe);
						hDataPipe = INVALID_HANDLE_VALUE;
					}
				}
				else
				{
					logger.LogString(FuncLogArea, LogError, "Cannot create named pipe \"%ws\" (GLE=%d)", (LPCWSTR)strPipeNameWithId, GetLastError());
					break;
				}
				
				if(ChannelHandle == NULL) 
				{
					// open virtual chanel
					logger.LogString(FuncLogArea, LogInfo, "Try to open virtual channel \"%s\".", VIRTUAL_CHANNEL_NAME);

					UINT ret = SavedEntryPoints.pVirtualChannelOpen(pInitHandle, &ChannelHandle, 
																VIRTUAL_CHANNEL_NAME, VirtualChannelOpenEvent);

					if (ret == CHANNEL_RC_OK)	
					{
						logger.LogString(FuncLogArea, LogInfo, "Open virtual channel \"%s\" success.", VIRTUAL_CHANNEL_NAME);
					}
					else 
					{
						logger.LogString(FuncLogArea, LogError, "Cannot open virtual channel \"%s\".", VIRTUAL_CHANNEL_NAME);
						CloseHandle(hDataPipe);
						hDataPipe = INVALID_HANDLE_VALUE;
						break;
					}
				}
				
				if (hDataPipe != INVALID_HANDLE_VALUE)
				{
					// thread read from pipe
					CloseHandle(CreateThread(NULL, 0, ThreadReadServicePipe, NULL, 0, NULL));
				}

				break;
			} // case CHANNEL_EVENT_CONNECTED

		case CHANNEL_EVENT_DISCONNECTED: 
		{
			logger.LogString(FuncLogArea, LogInfo, "CHANNEL_EVENT_DISCONNECTED event received.");
 			
			if(ChannelHandle) 
			{
 				SavedEntryPoints.pVirtualChannelClose(ChannelHandle);
 				ChannelHandle = NULL;
 			}

			if(hDataPipe != INVALID_HANDLE_VALUE) 
			{
				CloseHandle(hDataPipe);
				hDataPipe = INVALID_HANDLE_VALUE;
			}

			CStringA strCmd;
			DWORD dwCurrentProcessId = GetCurrentProcessId();
			CUngNetSettings settings;
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_COMMAND], Consts::szpPipeCommand[Consts::CP_REM_RDC_CONNECTION]);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM], dwCurrentProcessId);

			strCmd = settings.BuildSetttings ();
			strCmd += CUngNetSettings::GetEnd ();
			ServiceManagerInstance.SendCommand(strCmd, strCmd.GetLength());

			break;
		}

		case CHANNEL_EVENT_TERMINATED: 
			{
				logger.LogString(FuncLogArea, LogInfo, "CHANNEL_EVENT_TERMINATED event received.");

				if (ChannelHandle) 
				{
					SavedEntryPoints.pVirtualChannelClose(ChannelHandle);
					ChannelHandle = NULL;
				}

				if (hDataPipe != INVALID_HANDLE_VALUE) 
				{
					CloseHandle(hDataPipe);
					hDataPipe = INVALID_HANDLE_VALUE;
				}
				break;
			}
	}

	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__": EXIT");
}

BYTE *lpBuffer = NULL;
DWORD dwOffset = 0;
DWORD dwTick;

VOID VCAPITYPE VirtualChannelOpenEvent(DWORD openHandle, UINT Event, LPVOID pData, 
									   UINT32 dataLength, UINT32 totalLength, UINT32 dataFlag) 
{
	ULONG FuncLogArea = LOG_NONE;
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	switch (Event) 
	{
		case CHANNEL_EVENT_DATA_RECEIVED: 
		{
			logger.LogString(FuncLogArea, LogInfo, "CHANNEL_EVENT_DATA_RECEIVED event received.");
			logger.LogString(FuncLogArea, LogInfo, "\"dataLength\" = %d, \"totalLength\" = %d", dataLength, totalLength);

			DWORD dwToWrite = 0;
			
			if (dataFlag != CHANNEL_FLAG_ONLY) 
			{
				if(dataFlag & CHANNEL_FLAG_FIRST) 
				{
					dwTick = GetTickCount ();
					lpBuffer = new BYTE[totalLength];
					dwOffset = 0;
				}

				RtlCopyMemory(lpBuffer + dwOffset, pData, dataLength);
				dwOffset += dataLength;

				if(!(dataFlag & CHANNEL_FLAG_LAST))
				{
					// more data
					break; 
				}

				dwToWrite = dwOffset;
			}
			else  
			{
				dwTick = GetTickCount ();
				lpBuffer = new BYTE[dataLength];
				RtlCopyMemory(lpBuffer, pData, dataLength);
				dwToWrite = dataLength;
			}

			if(hDataPipe != INVALID_HANDLE_VALUE) 
			{
				DWORD dwWrite;
				logger.LogString(FuncLogArea, LogInfo, "-> Writing to pipe %d bytes", dwToWrite);

				OVERLAPPED Overlapped = {0};
				Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

				BOOL bRes = WriteFile(hDataPipe, lpBuffer, dwToWrite, &dwWrite, &Overlapped);

				if (!bRes && GetLastError() == ERROR_IO_PENDING) 
				{
					bRes = GetOverlappedResult(hDataPipe, &Overlapped, &dwWrite, TRUE);
				}
				AtlTrace (_T("rdp2dll - T%d - D%d\n"), GetTickCount () - dwTick, dwToWrite);

				if ((bRes == FALSE) || (dwWrite != dwToWrite))	
				{
					logger.LogString(FuncLogArea, LogError, "WriteFile() error \"bRes \" = %s, \"dwWrite\" = %d, LastError = %d", 
																				bRes?"true":"false", dwWrite, GetLastError());
				}
				else 
				{
					logger.LogString(FuncLogArea, LogInfo, "WriteFile() SUCCESS!");
				}

				CloseHandle(Overlapped.hEvent);
			}

			if ( lpBuffer )  
			{
				delete lpBuffer;
				lpBuffer = NULL;
			}

			break;
		}

		case CHANNEL_EVENT_WRITE_CANCELLED:
			// the write operation has been canceled.
			logger.LogString(FuncLogArea, LogInfo, "CHANNEL_EVENT_WRITE_CANCELLED event received.");
			delete[] (BYTE*)pData;
			break;
		case CHANNEL_EVENT_WRITE_COMPLETE:
			// the write operation has been completed. The client ID data
			// was received by the server.
			logger.LogString(FuncLogArea, LogInfo, "CHANNEL_EVENT_WRITE_COMPLETE event received.");
			delete[] (BYTE*)pData;
			break;
	}
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
	return;
}
