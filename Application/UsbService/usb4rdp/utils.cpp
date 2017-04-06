/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    utils.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "usb4rdp.h"

HANDLE CreateDataPipe(LPCWSTR wsPipeName) 
{
	ULONG FuncLogArea = LOG_UTIL;
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__": ENTER");

	HANDLE hPipe = INVALID_HANDLE_VALUE;

	do 
	{
		wchar_t * wzSecurityDescriptor =
			L"D:"                  
			L"(A;OICI;GA;;;WD)"     // Deny access to network service
			L"(A;OICI;GA;;;CO)"		// Allow read/write/execute to creator/owner
			L"(A;OICI;GA;;;BA)";    // Allow full control to administrators

		SECURITY_ATTRIBUTES sa;
		sa.bInheritHandle = TRUE; 
		sa.nLength = sizeof(sa);   

		ConvertStringSecurityDescriptorToSecurityDescriptorW 
			(wzSecurityDescriptor,
			SDDL_REVISION_1,
			&sa.lpSecurityDescriptor, // <-- sa = SECURITY_ATTRIBUTES
			NULL ); 

		hPipe = CreateNamedPipeW (wsPipeName,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,       // read/write access 
			PIPE_TYPE_MESSAGE |       // message type pipe 
			PIPE_READMODE_MESSAGE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			BUFSIZE_1K*8,                  // output buffer size 
			BUFSIZE_1K*8,                  // input buffer size 
			NMPWAIT_USE_DEFAULT_WAIT, // client time-out 
			&sa);                   

		if (hPipe == INVALID_HANDLE_VALUE)	
		{
			logger.LogString(FuncLogArea, LogError, "Cannot create named pipe \"%ws\"(Error#%d).", wsPipeName, GetLastError());
		}
		else 
		{
			logger.LogString(FuncLogArea, LogInfo, "Pipe \"%ws\" created successful.", wsPipeName);
		}

		LocalFree (sa.lpSecurityDescriptor);

	} while(false);

	logger.LogString(FuncLogArea, LogInfo, "RESULT \"hPipe\" = %x", hPipe);
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__": EXIT");
	return hPipe;
}
