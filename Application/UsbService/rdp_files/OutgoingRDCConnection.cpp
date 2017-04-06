/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   OutgoingRDCConnection.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "OutgoingRDCConnection.h"
#include <WtsApi32.h>
#include "Manager.h"
#include "Transport\RdpServer.h"

COutgoingRDCConnection::COutgoingRDCConnection(DWORD dwID, LPVOID pManager)
{
	ULONG FuncLogArea = LOG_RDC;

	m_hSendDataPipeMutex = CreateMutex(NULL, FALSE, NULL);

	m_Log.SetLogLevel(LogError);

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	m_dwID = dwID;
	m_pManager = pManager;

	m_hDataPipe = INVALID_HANDLE_VALUE;

	InitializeCriticalSection (&m_csRead);

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

COutgoingRDCConnection::~COutgoingRDCConnection()
{
	ULONG FuncLogArea = LOG_RDC;
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	if (m_hSendDataPipeMutex)
	{
		CloseHandle(m_hSendDataPipeMutex); 
		m_hSendDataPipeMutex = NULL;
	}

	WaitStopThreads (m_arrThredReadFromRdcDll, 1000, NULL);
	LeaveCriticalSection (&m_csRead);

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

DWORD COutgoingRDCConnection::GetRDCConnectionId()
{
	return m_dwID;
}

BOOL COutgoingRDCConnection::StartConnection ()
{
	ULONG FuncLogArea = LOG_RDC;
	BOOL bFuncResult = FALSE;
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	CString strNamedPipeName;
	strNamedPipeName.Format(_T("%ws_%d"), g_wsPipeName, m_dwID);

	BOOL bWaitNamedPipeResult = WaitNamedPipe(strNamedPipeName, NMPWAIT_WAIT_FOREVER);

	if (bWaitNamedPipeResult == FALSE && GetLastError() != ERROR_SEM_TIMEOUT) 
	{
		m_Log.LogString(FuncLogArea, LogWarning, "No instances of the specified named pipe \"%s\" exist", strNamedPipeName);
	}
	else 
	{
		// open Named pipe created of RDP plugin
		m_hDataPipe = CreateFileW ( strNamedPipeName, 
			GENERIC_READ | GENERIC_WRITE, 
			NULL,	
			NULL, 
			OPEN_EXISTING, 
			FILE_FLAG_OVERLAPPED, 
			NULL );

		if(m_hDataPipe == INVALID_HANDLE_VALUE) 
		{
			m_Log.LogString(FuncLogArea, LogError, "Failed to OPEN_EXISTING named pipe \"%s\". GLE = %d", strNamedPipeName, GetLastError());
		}
		else 
		{
			m_Log.LogString(FuncLogArea, LogInfo, "OPEN_EXISTING named pipe \"%s\" success", strNamedPipeName);

			DWORD dwMode = PIPE_READMODE_MESSAGE; 
			BOOL bResult = SetNamedPipeHandleState(m_hDataPipe, &dwMode, NULL,	NULL);

			if(bResult)
			{
				m_Log.LogString(FuncLogArea, LogInfo, "SetNamedPipeHandleState() success");

				m_arrThredReadFromRdcDll.push_back (CreateThread(NULL, 0, ThreadReadFromRDCDLL, this, 0, NULL));

				if (m_arrThredReadFromRdcDll.back ())
				{
					m_Log.LogString(FuncLogArea, LogInfo, "Successful created reading thread \"ThreadReadFromRDCDLL\"");
					bFuncResult = TRUE;
				}
			}
		}
	}

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - bFuncResult = \"%s\"", bFuncResult?"true":"false");
	return bFuncResult;
}

void COutgoingRDCConnection::StopConnection()
{
	ULONG FuncLogArea = LOG_RDC;
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	if((m_hDataPipe != NULL) && (m_hDataPipe != INVALID_HANDLE_VALUE))
	{
		CloseHandle(m_hDataPipe);
		m_hDataPipe = INVALID_HANDLE_VALUE;
	}

	WaitStopThreads (m_arrThredReadFromRdcDll, 1000, NULL);

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

DWORD WINAPI COutgoingRDCConnection::ThreadReadFromRDCDLL(LPVOID lParam)
{
	return ((COutgoingRDCConnection*)(lParam))->ThreadReadFromRDCDLLRoutine();
}

DWORD COutgoingRDCConnection::ThreadReadFromRDCDLLRoutine()
{
	DWORD dwFuncResult = 0;
	DWORD FuncLogArea = LOG_RDC;
	OVERLAPPED			Overlapped;
	CBuffer				pBuffer;
	CBuffer				pAnswer;

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); 

	// loop command
	while (true) { 

		DWORD	dw = GetTickCount ();

		EnterCriticalSection (&m_csRead);
		BOOL bRequestResult = GetRequest(pBuffer);
		LeaveCriticalSection (&m_csRead);

		if(pBuffer.GetCount() && bRequestResult) 
		{
			// analyze command
			DWORD	dw = GetTickCount ();

			BOOL bAnalyseResult = AnalyseRequest(pBuffer, pAnswer, &Overlapped);

			if (pAnswer.GetCount ()) 
			{
				BOOL bSendResult = SendAnswer(pAnswer);
			}

			AtlTrace (_T("Command - T%d\n"), GetTickCount () - dw);

		}
		else 
		{ 
			//disconnct pipe
			DWORD dwLastError = GetLastError();
			m_Log.LogString(FuncLogArea, LogError, "GetRequest() failed! GLE = %d", dwLastError);

			break;
		}

	} // while(true)

	CloseHandle (Overlapped.hEvent);

	((CManager*)m_pManager)->RemoveRDCConnection (m_dwID);
	
	m_Log.LogString(FuncLogArea, LogInfo, __FUNCTION__"(): EXIT - dwFuncResult = %d", dwFuncResult);
	return dwFuncResult;
}

BOOL COutgoingRDCConnection::GetRequest(CBuffer	&pBuffer) 
{
	return ReceiveFromFile(pBuffer);
}

BOOL COutgoingRDCConnection::SendAnswer(CBuffer	&pBuffer) 
{
	return SendToFile(m_hSendDataPipeMutex, pBuffer.GetData(), pBuffer.GetCount());
}

HANDLE COutgoingRDCConnection::GetDataPipeHandle()
{
	return m_hDataPipe;
}

HANDLE COutgoingRDCConnection::GetSendDataPipeMutexHandle()
{
	return m_hSendDataPipeMutex;
}

bool COutgoingRDCConnection::SendServerCommand (ULONG dwId, PCHAR pAnswer)
{
	BOOL bFuncResult = FALSE;
	DWORD dwBufferSize = (DWORD)strlen(pAnswer) + sizeof(ULONG) + sizeof(ULONG);
	PCHAR pBuffer = new CHAR[dwBufferSize];

	if (pBuffer)
	{
		if (GetDataPipeHandle () != INVALID_HANDLE_VALUE)
		{
			RtlZeroMemory(pBuffer, dwBufferSize);

			// build package ( command + ID + IP )
			memcpy(pBuffer, pAnswer, strlen(pAnswer));
			memcpy(pBuffer + strlen(pAnswer), &dwId, sizeof(dwId));
			ULONG ulAddr = CManager::GetLocalIP();
			memcpy(pBuffer + strlen(pAnswer) + sizeof(dwId), &ulAddr, sizeof(ULONG));

			bFuncResult = CFileTransport::SendToFile(GetSendDataPipeMutexHandle (), (PCHAR)pBuffer, dwBufferSize);
		}

		delete pBuffer;
	}

	return bFuncResult?true:false;	
}

BOOL COutgoingRDCConnection::AnalyseRequest(CBuffer &Buffer, CBuffer &Analyze, OVERLAPPED *pOverlapped) 
{
	ULONG FuncLogArea = LOG_NONE;
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = FALSE;
	char	*pRequest = (char *)Buffer.GetData();
	DWORD	dwRequestSize = Buffer.GetCount ();

	while(true) 
	{

		if((strlen(USB4RDP_REQUEST_GET_SHARES_LIST) <= size_t (Buffer.GetCount())) &&
			(_strnicmp(pRequest, USB4RDP_REQUEST_GET_SHARES_LIST, strlen(USB4RDP_REQUEST_GET_SHARES_LIST)) == 0))  
		{
				bFuncResult = BuildSharesListPacket(Analyze);
				break;
		}

		if((strlen(USB4RDP_DATA_PACKET) <= dwRequestSize) &&
			(_strnicmp(pRequest, USB4RDP_DATA_PACKET, strlen(USB4RDP_DATA_PACKET)) == 0)) 
		{
				DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_DATA_PACKET);

				bFuncResult = DeliverDataToSpecificServer(pRequest + dwHeaderSize, dwRequestSize - dwHeaderSize, pOverlapped);
				break;
		}

		if((strlen(USB4RDP_CONNECT_REQUEST) <= dwRequestSize) &&
			(_strnicmp(pRequest, USB4RDP_CONNECT_REQUEST, strlen(USB4RDP_CONNECT_REQUEST)) == 0)) 
		{
			//COutgoingRDCConnection *pConnection = FindRDCConnectionById(dwRDConnectionId);

			DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_CONNECT_REQUEST);
			bFuncResult = StartSpecificRDPServer(pRequest + dwHeaderSize, dwRequestSize - dwHeaderSize);
			break;
		}

		if((strlen(USB4RDP_DISCONNECT_REQUEST) <= dwRequestSize) &&
			(_strnicmp(pRequest, USB4RDP_DISCONNECT_REQUEST, strlen(USB4RDP_DISCONNECT_REQUEST)) == 0)) 
		{
			DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_DISCONNECT_REQUEST);
			bFuncResult = StopSpecificRDPServer(pRequest + dwHeaderSize, dwRequestSize - dwHeaderSize);
			break;
		}

		if((strlen (Consts::szpPipeCommand [Consts::CP_GET_NAME_DEVICE]) + sizeof(DWORD) <= dwRequestSize) &&
		 (_strnicmp (pRequest + sizeof(DWORD), Consts::szpPipeCommand [Consts::CP_GET_NAME_DEVICE], 
			strlen (Consts::szpPipeCommand [Consts::CP_GET_NAME_DEVICE])) == 0))
		{
			ATLASSERT (false);
				//bFuncResult = GetDeviceNameString(pRequest, ppAnswer, pdwAnswerSize);
			break;
		}

		break;
	}

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}

BOOL COutgoingRDCConnection::DeliverDataToSpecificServer(PCHAR pDataBuffer, DWORD dwDataSize, OVERLAPPED *pOverlapped)
{
	DWORD dwID = 0;
	RtlCopyMemory(&dwID, pDataBuffer, sizeof(DWORD));

	TPtrUsbServer pUsbServer = ((CManager*)m_pManager)->FindServerByIDForRdp(dwID);

	if (pUsbServer)
	{
		PCHAR pData = pDataBuffer + sizeof(DWORD);
		DWORD dwDataSizeWithoutID = dwDataSize - sizeof(DWORD);
		// compress
		{
			Transport::TPtrTransport pTr = pUsbServer->GetTrasport();
			Transport::TCP_PACKAGE *pTcp = (Transport::TCP_PACKAGE *)pData;
			if (pTr && pTr->IsRdpConnected())
			{
				Transport::CRdpServer *pRdp = dynamic_cast <Transport::CRdpServer*> (pTr.get());
				if (pRdp)
				{
					return pRdp->OnReadOut(pData, dwDataSizeWithoutID) ? TRUE : FALSE;
				}
			}
		}
	}

	return false;
}

BOOL COutgoingRDCConnection::StartSpecificRDPServer(PCHAR pDataBuffer, DWORD dwDataSize)
{
	DWORD dwID = 0;
	ULONG ulAddr = 0;
	RtlCopyMemory(&dwID, pDataBuffer, sizeof(DWORD));
	RtlCopyMemory(&ulAddr, pDataBuffer + sizeof(DWORD), sizeof(ULONG));

	do
	{
		TPtrUsbServer				pUsbServer = ((CManager*)m_pManager)->FindServerByIDForRdp(dwID);
		Transport::TPtrRdpServer		pRdp(new Transport::CRdpServer(this));
		Transport::TPtrTransport		pOldTr;

		if (!pUsbServer)
		{
			break;
		}

		if (pUsbServer->GetStatus() != CBaseUsb::Connecting)
		{
			break;
		}

		if (!pUsbServer->DeviceIsPresent())
		{
			break;
		}
		pOldTr = pUsbServer->GetTrasport();

		if (!pOldTr)
		{
			// device is not finishing initializate. After device initialize it will have set default TCP interface
			break;
		}

		if (pUsbServer->SetTrasport(pRdp))
		{
			CString strRemoteHost;

			try
			{
				// disconnect old trasport
				if (pOldTr)
				{
					pOldTr->Disconnect();
				}
				// connect
				pRdp->SetSettings(pUsbServer->GetSettings());
				pRdp->SetRemoteHost (CSystem::GetHostName ((in_addr*)&ulAddr, sizeof (ulAddr)));
				if (pRdp->Connect(pUsbServer.get()))
				{
				}
				pUsbServer->SetStatus (CBaseUsb::Connected);
			}
			catch (Transport::CException *pEx)
			{
				(void)pEx;
				ATLTRACE("%S\n", pEx->GetError());
				break;
			}
			return true;
		}
	} while (false);

	SendServerCommand(dwID, USB4RDP_CONNECT_ERROR_ANSWER);

	return false;
}

BOOL COutgoingRDCConnection::StopSpecificRDPServer(PCHAR pDataBuffer, DWORD dwDataSize)
{
	ULONG FuncLogArea = LOG_UTIL;
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = FALSE;

	do
	{
		DWORD dwID = 0;
		ULONG ulAddr = 0;
		RtlCopyMemory(&dwID, pDataBuffer, sizeof(DWORD));
		RtlCopyMemory(&ulAddr, pDataBuffer + sizeof(DWORD), sizeof(ULONG));

		TPtrUsbServer pUsbServer = ((CManager*)m_pManager)->FindServerByIDForRdp(dwID);

		if (!pUsbServer)
		{
			m_Log.LogString(FuncLogArea, LogError, "FindServerByID(dwID = %d) failed!", dwID);
			break;
		}

		Transport::TPtrTransport pTr = pUsbServer->GetTrasport();
		if (!pTr || !pTr->IsRdpConnected())
		{
			m_Log.LogString(FuncLogArea, LogError, "FindServerByID(dwID = %d) no rdp!", dwID);
			break;
		}

		pUsbServer->SetTrasport (/*NULL*/ new Transport::CFakeRdpServer (pTr->GetRemoteHost ()) );
		pUsbServer->OnError(WSAECONNRESET);

		bFuncResult = true;

	} while (false);

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}

BOOL COutgoingRDCConnection::BuildSharesListPacket(CBuffer &Buffer)
{
	ULONG FuncLogArea = LOG_NONE;
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	BOOL bFuncResult = FALSE;

	while (true)
	{
		ULONG uAddr = ((CManager*)m_pManager)->GetLocalIP();

		Buffer.SetSize(0);
		// build packege - header + ip + share list
		Buffer.AddArray((BYTE*)USB4RDP_ANSWER_SHARES_LIST, (int)strlen(USB4RDP_ANSWER_SHARES_LIST));
		Buffer.AddArray((BYTE*)&uAddr, sizeof(uAddr));
		((CManager*)m_pManager)->GetStringShare(Buffer, true);
		Buffer.Add(CUngNetSettings::GetEnd());

		bFuncResult = TRUE;
		break;
	}

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}
