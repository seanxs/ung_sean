/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    SendEcho.h

Environment:

    User mode

--*/

#pragma once

class CSendEcho
{
public:
	static int Send (SOCKET sock, int iPort, ULONG family, const char *pBuff, int iSize);

private:
	static SOCKET InitSock (ULONG family);
};
