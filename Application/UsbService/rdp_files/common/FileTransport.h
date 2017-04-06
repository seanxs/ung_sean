/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   FileTransport.h

Environment:

    User mode

--*/

#pragma once
#include <atlstr.h>
#include "consts/consts.h"
#include "Consts/Buffer.h"
#include <vector>

typedef enum _PACKET_TYPE {
	Type1 = 0x8888,
	Type2
} PACKET_TYPE, *PPACKET_TYPE;

typedef struct _HEADER_PACKET {
	DWORD NextPacketSize;
	DWORD PacketNumber;
	PACKET_TYPE NextPacketType;
} HEADER_PACKET, *PHEADER_PACKET;

typedef struct _DATA_PACKET {
	BYTE Name [sizeof (USB4RDP_DATA_PACKET) - 1];
	DWORD Id;
} DATA_PACKET, *PDATA_PACKET;

class CFileTransport 
{
public:
	void SetMaxSizeTranfer (LONG Max){m_maxSize = Max;}
protected:
	LONG	m_maxSize;
protected:
	CFileTransport();
	virtual ~CFileTransport();

public:
	BOOL SendToFile(HANDLE hMutex, LPVOID pBuffer, DWORD dwBufferSize);
	BOOL ReceiveFromFile(CBuffer &pBuffer);
	BOOL SendDataToFile(HANDLE hMutex, CBuffer &pBuffer, DWORD dwID);

	virtual BOOL SendToFileInt(LPVOID pBuffer, DWORD dwBufferSize);
	virtual BOOL ReceiveFromFileInt(LPVOID pBuffer, DWORD dwBufferSize, DWORD *pdwRead);

	virtual HANDLE GetHandle () const = 0;

	static bool WaitStopThreads (std::vector<HANDLE> &arrHandle, DWORD dwTime, CRITICAL_SECTION *cs);
	static bool ClearThreads (std::vector<HANDLE> &arrHandle, CRITICAL_SECTION *cs);

private:
	//CLog logger;
	DWORD m_dwCurrentPacketNumber;
	OVERLAPPED	m_WriteOverlapped;
};