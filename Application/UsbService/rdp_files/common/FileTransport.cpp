/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   FileTransport.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "consts.h"
#include "FileTransport.h"
#include <boost\scoped_array.hpp>

#include <ATLComTime.h>

CFileTransport::CFileTransport()
	: m_dwCurrentPacketNumber (0)
	, m_maxSize (-1)
{
	ZeroMemory(&m_WriteOverlapped, sizeof(m_WriteOverlapped));
	m_WriteOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

CFileTransport::~CFileTransport() 
{
	if (m_WriteOverlapped.hEvent != NULL)
	{
		CloseHandle(m_WriteOverlapped.hEvent);
		m_WriteOverlapped.hEvent = NULL;
	}
}

BOOL CFileTransport::SendToFile(HANDLE hMutex, LPVOID pBuffer, DWORD dwBufferSize) 
{
	WaitForSingleObject(hMutex, INFINITE);

	BOOL bSendResult = FALSE;
	HEADER_PACKET HeaderPacket;
	HeaderPacket.NextPacketType = Type1;
	HeaderPacket.NextPacketSize = dwBufferSize;
	HeaderPacket.PacketNumber = m_dwCurrentPacketNumber++;

	do
	{
		// Sending header
		
		bSendResult = SendToFileInt(&HeaderPacket, sizeof(HEADER_PACKET));

		if(bSendResult == FALSE) 
		{
			// Cannot send header packet! 
			break;
		}
		// Header send success


		LPBYTE lpBuffer = (LPBYTE)pBuffer;

		bSendResult = SendToFileInt(pBuffer, dwBufferSize);

	} while(false);

	ReleaseMutex(hMutex);

	return bSendResult;
}

BOOL CFileTransport::ReceiveFromFile(CBuffer &pBuffer) 
{
	BOOL bReceiveResult = FALSE;
	HEADER_PACKET HeaderPacket;
	DWORD dwRead;
	DWORD dwReadAll = 0;
	DWORD dwTailSize;
	PBYTE pCurPtr;

	while (true)	
	{
		DWORD dw = GetTickCount ();
		RtlZeroMemory (&HeaderPacket, sizeof(HEADER_PACKET));

		dwReadAll = 0;
		while(dwReadAll < sizeof(HEADER_PACKET)) 
		{
			pCurPtr = (PBYTE)(&HeaderPacket) + dwReadAll;

			dwTailSize = sizeof(HEADER_PACKET) - dwReadAll;

			bReceiveResult = ReceiveFromFileInt(pCurPtr, dwTailSize, &dwRead);

			if(bReceiveResult) 
			{
				dwReadAll += dwRead;
			}
			else 
			{
				break;
			}
		}

		if(bReceiveResult == FALSE) 
		{
			// Cannot receive header packet!
			break;
		}

		if (dwReadAll != sizeof(HEADER_PACKET))
		{
			ATLASSERT (false);
		}

		// read data
		pBuffer.SetSize(HeaderPacket.NextPacketSize);

		dw = GetTickCount ();
		dwReadAll = 0;
		while(dwReadAll < HeaderPacket.NextPacketSize) 
		{
			PBYTE pCurPtr = (PBYTE)(pBuffer.GetData()) + dwReadAll;

			dwTailSize = HeaderPacket.NextPacketSize - dwReadAll;

			bReceiveResult = ReceiveFromFileInt(pCurPtr, dwTailSize, &dwRead);

			if(bReceiveResult) 
			{
				dwReadAll += dwRead;
			}
			else 
			{
				break;
			}
		}

		break;
	} // while(true)

	return bReceiveResult;
}


BOOL CFileTransport::SendDataToFile(HANDLE hMutex, CBuffer &pBuffer, DWORD dwID) 
{
	BOOL bFuncResult = FALSE;

	PDATA_PACKET pPacket = (PDATA_PACKET)(pBuffer.GetFullData());

	RtlCopyMemory (pPacket->Name, USB4RDP_DATA_PACKET, sizeof (pPacket->Name));
	pPacket->Id = dwID;

	bFuncResult = SendToFile(hMutex, pPacket, pBuffer.GetCount() + pBuffer.GetReservSize());

	return bFuncResult;
}

BOOL CFileTransport::SendToFileInt(LPVOID pBuffer, DWORD dwBufferSize)
{
	DWORD dwWrite = 0;
	DWORD dwWriteTotal = 0;
	BOOL bWriteResult = FALSE;

	while (dwWriteTotal < dwBufferSize)
	{
		DWORD dwCurSize;
		if (m_maxSize == -1)
		{
			dwCurSize = dwBufferSize - dwWriteTotal;
		}
		else
		{
			dwCurSize = (dwBufferSize - dwWriteTotal < ULONG (m_maxSize))?(dwBufferSize - dwWriteTotal):m_maxSize;
		}
		bWriteResult = WriteFile(GetHandle (), LPBYTE (pBuffer) + dwWriteTotal, dwCurSize, &dwWrite, &m_WriteOverlapped);

		int nLastError = GetLastError();

		if (!bWriteResult && ( nLastError == ERROR_IO_PENDING)) 
		{
			bWriteResult = GetOverlappedResult(GetHandle(), &m_WriteOverlapped, &dwWrite, TRUE);
		}

		if(bWriteResult) 
		{
			dwWriteTotal += dwWrite;
		}
		else 
		{ 
			nLastError = GetLastError();
			break;
		}
	}

	return bWriteResult;
}

BOOL CFileTransport::ReceiveFromFileInt(LPVOID pBuffer, DWORD dwBufferSize, DWORD *pdwRead)
{
	OVERLAPPED ReadOverlapped = {0};
	ReadOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	BOOL bReadResult = FALSE;

	while(true) 
	{
		DWORD dwReadCur = 0;

		bReadResult = ReadFile(GetHandle (), pBuffer, dwBufferSize, pdwRead, &ReadOverlapped);

		DWORD dwLastError = GetLastError();

		if(!bReadResult && dwLastError == ERROR_IO_PENDING) 
		{
			bReadResult = GetOverlappedResult(GetHandle (), &ReadOverlapped, pdwRead, TRUE);
		}
		if (!bReadResult)
		{
			if (GetLastError () == ERROR_MORE_DATA)
			{
				bReadResult = true;
				if (*pdwRead == 0)
				{
					*pdwRead = dwBufferSize;
				}
			}
		}

		break;
	}

	CloseHandle(ReadOverlapped.hEvent);

	return bReadResult;
}

bool CFileTransport::WaitStopThreads (std::vector<HANDLE> &arrHandle, DWORD dwTime, CRITICAL_SECTION *cs)
{
	if (arrHandle.empty ())
	{
		return true;
	}

	DWORD dwRet = WaitForMultipleObjects (DWORD (arrHandle.size ()), &arrHandle [0], true, dwTime);

	if (dwRet == WAIT_OBJECT_0)
	{
		ClearThreads (arrHandle, cs);
		return true;
	}
	return false;
}

bool CFileTransport::ClearThreads (std::vector<HANDLE> &arrHandle, CRITICAL_SECTION *cs)
{
	if (cs)
	{
		EnterCriticalSection (cs);
	}
	std::vector<HANDLE>::iterator item;
	for (item = arrHandle.begin (); item != arrHandle.end (); item++)
	{
		CloseHandle (*item);
	}
	arrHandle.clear ();
	if (cs)
	{
		LeaveCriticalSection (cs);
	}
	return true;
}
