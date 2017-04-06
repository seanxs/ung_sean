/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    SendEcho.cpp

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "SendEcho.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <Ws2ipdef.h>
#include "Buffer.h"

#pragma comment(lib, "IPHLPAPI.lib")

int CSendEcho::Send (SOCKET sockBase, int iPort, ULONG family, const char *pBuff, int iSize)
{
	int			iCount = 0;
	CBuffer		buffer;
	SOCKET	sock = sockBase;
	
	if (AF_INET == family)
	{
		ULONG dwRetVal = 0;
		MIB_IPADDRTABLE* pIPAddrTable;
		DWORD dwSize = 0;

		dwSize = 2000;
		buffer.SetSize (dwSize);
		pIPAddrTable = (MIB_IPADDRTABLE*)buffer.GetData ();

		if ((dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0 )) == NO_ERROR) 
		{ 
			ULONG i;
			for (i = 0; i < pIPAddrTable->dwNumEntries; i++) 
			{
				ULONG addr = pIPAddrTable->table[i].dwAddr;
				ULONG mask = pIPAddrTable->table[i].dwMask;

				if ((mask == 0) || ((addr & 0xff) == 127))
					continue;

				if (sock == INVALID_SOCKET)
				{
					sock = InitSock (family);
				}

				{
					SOCKADDR_IN ipv4;

					ZeroMemory (&ipv4, sizeof (ipv4));

					ipv4.sin_family = AF_INET;
					ipv4.sin_port = htons (iPort); 
					ipv4.sin_addr.S_un.S_addr = addr | (~mask);
					
					int iRet = sendto (sock, pBuff, iSize, 0, (struct sockaddr *) &ipv4, sizeof (SOCKADDR_IN));
					if (iRet != SOCKET_ERROR)
					{
						++iCount;
					}
				}
			}
		}
	}

	if (sock != INVALID_SOCKET && sockBase == INVALID_SOCKET)
	{
		closesocket (sock);
	}

	return iCount;
}

SOCKET CSendEcho::InitSock (ULONG family)
{
	SOCKET	sockTemp = INVALID_SOCKET;
	int iRet = INVALID_SOCKET;

	if ((sockTemp = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) != INVALID_SOCKET) 
	{  
		BOOL bBroadcast = TRUE; 

		iRet = setsockopt (sockTemp, SOL_SOCKET, SO_BROADCAST, (char*)&bBroadcast,sizeof(BOOL));
		if (iRet == SOCKET_ERROR)
		{
			return iRet;
		}		
	}

	return sockTemp;
}
