/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   RemoteSession.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include <atlstr.h>
#include <list>
#include <winioctl.h>
#include "Consts\consts.h"
#include "Consts\ExtLog.h"
#include "Consts\log.h"
#include "common\FileTransport.h"
#include "remotesession.h"
#include "public.h"
#include "Manager.h"
#include "Consts\Registry.h"
#include "Consts\system.h"
#include "usb4citrix\citrix.h"
#include "ApiRemoteSession.h"
#include <Sddl.h>
#include "Transport\RdpClient.h"
//#include <Ntifs.h>

#include <wfapi.h>

#include "Utils\UserLog.h"


CRemoteSession::CRemoteSession(DWORD dwId, CManager *pManager)
{
	SetLogArea(LOG_ALL & ~(LOG_UTIL | LOG_AUX));

	ULONG FuncLogArea = LOG_CONSTR;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	m_pManager = pManager;

	m_dwSessionId = dwId;

	InitializeCriticalSection(&m_ClientsListCriticalSection);
	InitializeCriticalSection(&m_EventCriticalSection);
	InitializeCriticalSection(&m_csReadVc);

	InitializeCriticalSection(&m_csPendingRequestsSection);

	m_hSendChannelMutex = CreateMutex(NULL, FALSE, NULL);

	m_hChannel        = NULL;
	m_hChannelFile    = NULL;
	m_hWorkedEvent    = NULL;

	m_hStartThreadHandle = NULL;
	m_dwStartThreadId = 0;

	HANDLE hToken;
	if (WTSQueryUserToken (dwId, &hToken))
	{
		TOKEN_STATISTICS stat;
		DWORD dwRet = sizeof (stat);

		if (GetTokenInformation (hToken, TokenStatistics, &stat, sizeof (stat), &dwRet))
		{
			m_strUserDosDevice.Format (_T("\\Sessions\\%d\\DosDevices\\%08x-%08x"), 0, stat.AuthenticationId.HighPart, stat.AuthenticationId.LowPart);
			AtlTrace (_T("%s\n"), m_strUserDosDevice);
		}

		char pBuff [1024];
		PTOKEN_USER pUser = (PTOKEN_USER)pBuff;
		LPTSTR szName;
		
		if (GetTokenInformation (hToken, TokenUser, pBuff, 1024, &dwRet))
		{
			if (ConvertSidToStringSid (pUser->User.Sid, &szName))
			{
				m_sid.SetSize(GetLengthSid (pUser->User.Sid));
				CopySid (GetLengthSid (pUser->User.Sid), m_sid.GetData(), pUser->User.Sid);
				m_strUserSid = szName;
				LocalFree (szName);
			}
			AtlTrace (_T("SID - %s\n"), m_strUserSid);
		}

		CloseHandle(hToken);
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

CRemoteSession::~CRemoteSession() 
{
	ULONG FuncLogArea = LOG_CONSTR;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	DeleteCriticalSection(&m_ClientsListCriticalSection);
	DeleteCriticalSection(&m_EventCriticalSection);
	DeleteCriticalSection(&m_csReadVc);

	DeleteCriticalSection(&m_csPendingRequestsSection);
	
	if(m_hSendChannelMutex) 
	{
		CloseHandle(m_hSendChannelMutex);
		m_hSendChannelMutex = NULL;
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

DWORD CRemoteSession::GetSessionId() 
{
	return m_dwSessionId;
}

void CRemoteSession::CreateStartThread() 
{
	TPtrRemoteSession pSave (this);
	ULONG FuncLogArea = LOG_INIT;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	m_hStartThreadHandle = CreateThread(NULL, 0, StartThread, this, 0, &m_dwStartThreadId);
	Sleep (0);

	if(m_hStartThreadHandle) 
	{
		LogString(FuncLogArea, LogInfo, __FUNCTION__"(): StartThread creating successful. TID = %d", m_dwStartThreadId);
	}
	else 
	{
		LogString(FuncLogArea, LogError, __FUNCTION__"(): CreateThread() failed! GLE = %d", GetLastError());
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

DWORD WINAPI CRemoteSession::StartThread(LPVOID lParam) 
{
	ULONG FuncLogArea = LOG_INIT;
	DWORD dwLastThreadError = 0;

	TPtrRemoteSession pClassInstance = ((CRemoteSession*)(lParam));

	pClassInstance->LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	if (!pClassInstance->Start())
	{
		pClassInstance->m_pManager->RemoveSession(pClassInstance->m_dwSessionId);
	}

	if(pClassInstance->m_hStartThreadHandle) {
		CloseHandle(pClassInstance->m_hStartThreadHandle);
		pClassInstance->m_hStartThreadHandle = NULL;
		pClassInstance->m_dwStartThreadId = 0;
	}

	pClassInstance->LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - dwLastThreadError = %d", dwLastThreadError);
	return dwLastThreadError;
}


typedef enum _ACTION_TYPE 
{
	AddSessionAction,
	RemoveSessionAction,
	SessionSelfStop
} ACTION_TYPE, *PACTION_TYPE;

typedef struct _PARAMS_TO_WRAP 
{
	ACTION_TYPE ActionType;
	DWORD dwSessionId;
} PARAMS_TO_WRAP, *PPARAMS_TO_WRAP;

BOOL CRemoteSession::Start() {
	ULONG FuncLogArea = LOG_INIT;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = FALSE;

	////////////////////////////////////////////////////////////////////////////////////
// 	LogString(FuncLogArea, LogInfo, "Enumerating windows sessions...");
// 	EnumSessions();
	///////////////////

	while (true)	
	{
		LogString(FuncLogArea, LogTrace, __FUNCTION__"(): Creating worked event...");
		m_hWorkedEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

		if(m_hWorkedEvent == NULL) 
		{
			LogString(FuncLogArea, LogError, "CreateEvent() failed! GLE = %d", GetLastError());
			_ASSERT(FALSE);
			break;
		}

		// Create Virtual Chanel
		LogString(FuncLogArea, LogInfo, "Creating Virtual channel \"%s\"...", VIRTUAL_CHANNEL_NAME);

		if(!CreateVirtualChannel(VIRTUAL_CHANNEL_NAME, &m_hChannel, &m_hChannelFile)) 
		{
			LogString(FuncLogArea, LogError, "Create \"%s\" virtual channel failed (ERROR#%d).", VIRTUAL_CHANNEL_NAME, GetLastError());
			_ASSERT(FALSE);
			break;
		}
		LogString(FuncLogArea, LogInfo, "Create virtual channel \"%s\" successful.", VIRTUAL_CHANNEL_NAME);

		// thread read from chanel
		LogString(FuncLogArea, LogInfo, "Creating thread ThreadReadVC()...");
		EnterCriticalSection (&m_EventCriticalSection);
		m_arrThreadReadVc.push_back (CreateThread(NULL, 0, ThreadReadVC, this, 0, NULL));
		LeaveCriticalSection (&m_EventCriticalSection);
		

		if(m_arrThreadReadVc.back () != NULL) 
		{
			LogString(FuncLogArea, LogInfo, "Thread ThreadReadVC() created success! ");
		}
		else 
		{
			LogString(FuncLogArea, LogError, "CreateThread() failed! GLE = %d", GetLastError());
			_ASSERT(FALSE);
			break;
		}

		// thread update client list
		LogString(FuncLogArea, LogInfo, "Creating thread ThreadUpdateClientsList()...");
		EnterCriticalSection (&m_EventCriticalSection);
		m_arrThreadReadVc.push_back (CreateThread(NULL, 0, ThreadUpdateClientsList, this, 0, NULL));
		LeaveCriticalSection (&m_EventCriticalSection);

		if(m_arrThreadReadVc.back () != NULL)
		{
			LogString(FuncLogArea, LogInfo, "Thread ThreadUpdateClientsList() created success!");
		}
		else 
		{
			LogString(FuncLogArea, LogError, "CreateThread() failed! GLE = %d", GetLastError());
			_ASSERT(FALSE);
			break;
		}
	
		WaitStopThreads (m_arrThreadReadVc, INFINITE, &m_EventCriticalSection);

		if (m_hChannelFile)
		{
			PPARAMS_TO_WRAP pParams = new PARAMS_TO_WRAP;
			pParams->ActionType = RemoveSessionAction;
			pParams->dwSessionId = m_dwSessionId;
			m_pManager->CreateWrapThread (pParams);
		}

		bFuncResult = TRUE;
		break;
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}

BOOL CRemoteSession::Stop() 
{
	ULONG FuncLogArea = LOG_INIT;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = TRUE;

	do 
	{
		if(m_hWorkedEvent) 
		{
			LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Reset worked event...");
			ResetEvent(m_hWorkedEvent);
			CloseHandle(m_hWorkedEvent);
			m_hWorkedEvent = NULL;
		}
		else 
		{
			LogString(FuncLogArea, LogWarning, __FUNCTION__"(): m_hWorkedEvent = NULL!");
			break;
		}

		// Close virtual chanel
		if (m_hChannel)
		{
			LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Closing virtual channel...");
			CloseVirtualChannel(&m_hChannel, &m_hChannelFile);
		}

		DWORD dwTimeout = 10000; // time finishing thread

		WaitStopThreads (m_arrThreadReadVc, dwTimeout, &m_EventCriticalSection);

		//ClearPendingRequestsList();
		ClearClientsList();

		if (m_hStartThreadHandle && (m_hStartThreadHandle != INVALID_HANDLE_VALUE) )
		{
			LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Waiting %d ms for StartThread()", dwTimeout);

			if ( !WaitForSingleObject(m_hStartThreadHandle, dwTimeout) == WAIT_OBJECT_0 ) 
			{
				int nLastError = GetLastError();
				LogString(FuncLogArea, LogWarning, __FUNCTION__"(): Wait %d ms for StartThread() failed! GLE = %d", dwTimeout, nLastError);
				TerminateThread(m_hStartThreadHandle, 0);
				AtlTrace ("TerminateThread m_hStartThreadHandle\n");
			}
			else 
			{
				LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Waiting success!");
			}

			CloseHandle(m_hStartThreadHandle);
			m_hStartThreadHandle = NULL;
			m_dwStartThreadId = 0;
		}
	} while(false);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}


//#pragma comment(lib, "wfapi.lib")

BOOL CRemoteSession::CreateVirtualChannel(PCHAR ChannelName, PHANDLE phChannel, PHANDLE phFile) {
	ULONG FuncLogArea = LOG_UTIL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	DWORD  len;
	DWORD  result = ERROR_SUCCESS;
	HANDLE hChannel = NULL;
	HANDLE hFile = NULL;
	PVOID  pHandle;
	BOOL ReturnResult = FALSE;

	LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Opening \"%s\" channel with SessionID = %d", VIRTUAL_CHANNEL_NAME, m_dwSessionId);

	hChannel = CApiRemoteSession::impl ().VirtualChannelOpen (WTS_CURRENT_SERVER_HANDLE, m_dwSessionId, VIRTUAL_CHANNEL_NAME);

	if (hChannel == NULL) {
		result = GetLastError();
		LogString(FuncLogArea, LogError, "WTSVirtualChannelOpen() failed! ERROR#%d", result);
	}

	if (result == ERROR_SUCCESS) {
		// get citrix info
		{
			ICO_C2H *pHeader;
			DWORD	dwSize = sizeof (ICO_C2H);
			if (CApiRemoteSession::impl ().VirtualChannelQuery(hChannel,
						WFVirtualClientData,
						(PVOID*)&pHeader,
						&dwSize ))
			{
				if (pHeader->usMaxDataSize)
				{
					SetMaxSizeTranfer (pHeader->usMaxDataSize);
				}
				CApiRemoteSession::impl ().FreeMemory(pHeader);
			}
		}

		if (!WTSVirtualChannelQuery(hChannel, WTSVirtualFileHandle, &pHandle, &len)) {
			result = GetLastError();
			LogString(FuncLogArea, LogError, "WTSVirtualChannelQuery() failed! ERROR#%d", result);
		}
	}

	if (result == ERROR_SUCCESS) {
		memcpy(phFile, pHandle, sizeof(HANDLE));
		WTSFreeMemory(pHandle);
		memcpy(phChannel, &hChannel, sizeof(phChannel));
		//memcpy(phFile, &hFile, sizeof(phFile));
		ReturnResult = TRUE;
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT; return %s", ReturnResult?"TRUE":"FALSE");
	return ReturnResult;
}

void CRemoteSession::CloseVirtualChannel(PHANDLE phChannel, PHANDLE phFile) {
	ULONG FuncLogArea = LOG_UTIL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	if (*phChannel != NULL) 
	{
		CApiRemoteSession::impl ().VirtualChannelClose (*phChannel);
		*phChannel = NULL;
		*phFile = NULL;
	}
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

BOOL CRemoteSession::SendRequest(PCHAR pRequest, DWORD dwRequestSize) 
{
	return SendToFile(m_hSendChannelMutex, pRequest, dwRequestSize);
}


BOOL CRemoteSession::ReceiveAnswer(CBuffer &pBuffer) 
{
	return ReceiveFromFile(pBuffer);
}

BOOL CRemoteSession::AnalyseAnswer(const CBuffer &pBuffer) 
{
	ULONG FuncLogArea = LOG_UTIL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = FALSE;

	char*	pAnswer = (char*)pBuffer.GetData();
	DWORD	dwAnswerSize = pBuffer.GetCount();

	do 
	{
		if((strlen(USB4RDP_ANSWER_SHARES_LIST) <= dwAnswerSize) && (_strnicmp(pAnswer, USB4RDP_ANSWER_SHARES_LIST, strlen(USB4RDP_ANSWER_SHARES_LIST)) == 0))  
		{
			DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_ANSWER_SHARES_LIST);
			bFuncResult = ParseSharesString (pAnswer + dwHeaderSize, dwAnswerSize - dwHeaderSize);
			break;
		}

		if((strlen(USB4RDP_DATA_PACKET) <= dwAnswerSize) && (_strnicmp(pAnswer, USB4RDP_DATA_PACKET, strlen(USB4RDP_DATA_PACKET)) == 0))  
		{
			DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_DATA_PACKET);
			bFuncResult = DeliverDataToSpecificClient (pAnswer + dwHeaderSize, dwAnswerSize - dwHeaderSize);
			break;
		}

		if((strlen(USB4RDP_CONNECT_ESTABLISHED_ANSWER) <= dwAnswerSize) && (_strnicmp(pAnswer, USB4RDP_CONNECT_ESTABLISHED_ANSWER, strlen(USB4RDP_CONNECT_ESTABLISHED_ANSWER)) == 0))  
		{
			DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_CONNECT_ESTABLISHED_ANSWER);
			bFuncResult = DeliverConnectEstablishedAnswerToClient (pAnswer + dwHeaderSize, dwAnswerSize - dwHeaderSize);
			break;
		}

		if((strlen(USB4RDP_CONNECT_ERROR_ANSWER) <= dwAnswerSize) && (_strnicmp(pAnswer, USB4RDP_CONNECT_ERROR_ANSWER, strlen(USB4RDP_CONNECT_ERROR_ANSWER)) == 0))  
		{
			DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_CONNECT_ERROR_ANSWER);
			bFuncResult = DeliverConnectErrorAnswerToClient (pAnswer + dwHeaderSize, dwAnswerSize - dwHeaderSize);
			break;
		}

		if((strlen(USB4RDP_MANUAL_DISCONNECT_COMMAND) <= dwAnswerSize) && (_strnicmp(pAnswer, USB4RDP_MANUAL_DISCONNECT_COMMAND, strlen(USB4RDP_MANUAL_DISCONNECT_COMMAND)) == 0))  
		{
			DWORD dwHeaderSize = (DWORD)strlen(USB4RDP_MANUAL_DISCONNECT_COMMAND);
			bFuncResult = DeliverManualDisconnectCommandToClient (pAnswer + dwHeaderSize, dwAnswerSize - dwHeaderSize);
			break;
		}

		if((strlen(USB4RDP_GETNAME_ANSWER) <= dwAnswerSize) && (_strnicmp(pAnswer, USB4RDP_GETNAME_ANSWER, strlen(USB4RDP_GETNAME_ANSWER)) == 0))
		{
			ATLASSERT (false);
			break;
		}
	} while (false);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}

BOOL CRemoteSession::DeliverManualDisconnectCommandToClient (PCHAR pDataBuffer, DWORD dwDataSize)
{
	ULONG FuncLogArea = LOG_AUX;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = FALSE;

	do 
	{
		ULONG uClientID = 0;
		ULONG dwServerIP = 0;
		bool  bStop = ((*pDataBuffer) == '1')?true:false;

		ParseId (pDataBuffer + 1, dwDataSize - 1, uClientID, dwServerIP);

		TPtrUsbClient pUsbRdpClient = FindClientByID(uClientID);

		if(pUsbRdpClient)
		{
			LogString(FuncLogArea, LogTrace, __FUNCTION__"(): Client find success (pClient = 0x%.8X). Disconnecting client...", pUsbRdpClient);
			StopClient(uClientID, FALSE);
			if (pUsbRdpClient->GetAutoReconnect() && !bStop)
			{
				bFuncResult = StartClient(uClientID, pUsbRdpClient->GetAutoReconnect());
			}
		}
		else
		{
			LogString(FuncLogArea, LogError, __FUNCTION__"(): Could not find client with ID = %d", uClientID);
		}

	} while (false);


	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}

void CRemoteSession::ParseId (PCHAR pDataBuffer, DWORD dwDataSize, ULONG &uClientID, ULONG &dwServerIP)
{
	if (dwDataSize >= sizeof(ULONG))
	{
		RtlCopyMemory(&uClientID, pDataBuffer, sizeof(ULONG));
	}
	if (dwDataSize >= (sizeof(ULONG) * 2))
	{
		RtlCopyMemory(&dwServerIP, pDataBuffer + sizeof(ULONG), sizeof (ULONG));
	}
}

BOOL CRemoteSession::DeliverConnectEstablishedAnswerToClient (PCHAR pDataBuffer, DWORD dwDataSize)
{
	ULONG FuncLogArea = LOG_AUX;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = FALSE;

	ULONG uClientID = 0;
	ULONG dwServerIP = 0;

	ParseId (pDataBuffer, dwDataSize, uClientID, dwServerIP);

	TPtrUsbClient pUsbRdpClient = FindClientByID(uClientID);

	if(pUsbRdpClient)
	{
		Transport::TPtrRdpClient pRdp (Transport::CRdpClient::GetRdp (pUsbRdpClient->GetTrasport().get()));
		if (pRdp)
		{
			return pRdp->EstablishConnection(dwServerIP);
		}
	}

	return false;
}

BOOL CRemoteSession::DeliverConnectErrorAnswerToClient (PCHAR pDataBuffer, DWORD dwDataSize)
{
	ULONG uClientID = 0;
	ULONG dwServerIP = 0;

	ParseId (pDataBuffer, dwDataSize, uClientID, dwServerIP);

	TPtrUsbClient pUsbRdpClient = FindClientByID((DWORD)uClientID);

	if(pUsbRdpClient)
	{
		Transport::TPtrRdpClient pRdp (Transport::CRdpClient::GetRdp (pUsbRdpClient->GetTrasport().get()));
		if (pRdp)
		{
			return pRdp->AbortEstablishConnection ();
		}
	}

	return false;
}

BOOL CRemoteSession::DeliverDataToSpecificClient(PCHAR pDataBuffer, DWORD dwDataSize) 
{
	DWORD dwID = 0;
	RtlCopyMemory(&dwID, pDataBuffer, sizeof(DWORD));

	TPtrUsbClient pUsbClient = FindClientByID(dwID);

	if(pUsbClient) 
	{
		Transport::TPtrRdpClient pRdp (Transport::CRdpClient::GetRdp (pUsbClient->GetTrasport().get()));
		if (pRdp)
		{
			PCHAR pData = pDataBuffer + sizeof(DWORD);
			DWORD dwDataSizeWithoutID = dwDataSize - sizeof(DWORD);
			return pRdp->OnReadOut (pData, dwDataSizeWithoutID);
		}
	}

	return false;
}


BOOL CRemoteSession::ParseSharesString(PCHAR pSharesString, DWORD dwAnswerSize) 
{
	ULONG FuncLogArea = LOG_UTIL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = TRUE;
	DWORD		iPos = 0;
	std::vector <DWORD>	arrIds;
	std::vector <DWORD>::iterator itemId;

	do 
	{
		if (!pSharesString) break;

		// Get cur ticker
		m_ul64ClientsListTimestamp = GetTickCount();

		ULONG ulAddr = 0; 
		// read ip
		RtlCopyMemory(&ulAddr, pSharesString, sizeof(ULONG));
		iPos = sizeof(ULONG);

		while (iPos < dwAnswerSize)
		{
			CUngNetSettings settings;
			if (*(pSharesString + iPos) == CUngNetSettings::GetEnd ())
			{
				break;
			}
			if (settings.ParsingSettings (CStringA (pSharesString + iPos)))
			{
				// name
				//CString strName = CSystem::BuildClientName (settings);
				arrIds.push_back (settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]));

				//bFuncResult = AddClient(strName, arrIds.back (), ulAddr);
				bFuncResult = AddClient(settings, arrIds.back (), ulAddr);

				if (bFuncResult == FALSE) 
				{
					LogString(FuncLogArea, LogError, "Cannot add client! \"strShareName\" = %ws, dwID = %d", CSystem::BuildClientName (settings), arrIds.back ());
				}
				else
				{

				}
			}
			else
			{
				break;
			}

			iPos += int (strlen (pSharesString + iPos)) + 1;
		}

		EnterCriticalSection (&m_ClientsListCriticalSection);

		TListRdpClients::iterator ClientItem;

		bool bClientFinded;
		for (ClientItem = m_ClientsList.begin(); ClientItem != m_ClientsList.end (); ++ClientItem) 
		{
			bClientFinded = false;
			for (itemId = arrIds.begin (); itemId != arrIds.end (); itemId++)
			{
				if((*itemId == (*ClientItem)->GetTcpPort ())) 
				{
					// exist client
					bClientFinded = true;
					break;
				}
			}

			if(!bClientFinded) 
			{
				// delete rdp client
				DeleteClient((*ClientItem)->GetTcpPort());
				if(m_ClientsList.size() == 0) 
				{
					break;
				}
				ClientItem = m_ClientsList.begin();
			}
		}

		LeaveCriticalSection (&m_ClientsListCriticalSection);   
	}
	while(0);


	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}

BOOL CRemoteSession::AddClient(CUngNetSettings &settings, DWORD dwID, ULONG uRemoteShareAddr)
{
	ULONG FuncLogArea = LOG_UTIL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	BOOL bFuncResult = FALSE;

	do
	{
		TPtrUsbClient pClient = FindClientByID(dwID);

		EnterCriticalSection (&m_ClientsListCriticalSection);

		if(pClient == NULL) 
		{ 
			// remember IP of server in transport
			const char *strRemoteAddr = inet_ntoa (*((struct in_addr*)&uRemoteShareAddr));
			pClient = TPtrUsbClient (new CUsbClient(settings, new Transport::CRdpClient (this, strRemoteAddr)));

			if (pClient) 
			{
				// add client
				m_ClientsList.push_back(pClient);

				// add to log
				eltima::ung::UserLog::Client::AddRdp (settings);

				BOOL bAutoconnect = FALSE; // auto connect default disable

				CRegistry reg (Consts::szRegistry_usb4rdp);

				if(reg.ExistsValue(Consts::szAutoConnectParam))
				{
					bAutoconnect = reg.GetBool(Consts::szAutoConnectParam);
				}
				else
				{
					reg.SetBool(Consts::szAutoConnectParam, bAutoconnect ? true : false);
				}

				if(bAutoconnect) 
				{
					bFuncResult = StartClient(dwID, true);
				}
			}
		}
		else {
			LogString(FuncLogArea, LogInfo, "Client with ID = %d already added.", dwID);
			LogString(FuncLogArea, LogInfo, "New share name = \"%S\"", CSystem::BuildClientName (settings));
			// update name
			pClient->UpdateSettings (settings);
			bFuncResult = TRUE;
		}

		LeaveCriticalSection (&m_ClientsListCriticalSection);
	} 
	while (false);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE" : "FALSE");
	return bFuncResult;
}

BOOL CRemoteSession::StartClient(DWORD dwID, bool bAutoReconnect) 
{
	DWORD dwFuncArea = LOG_INIT;
	BOOL bFuncResult = FALSE;

	LogString(dwFuncArea, LogTrace, __FUNCTION__"(): ENTER - dwID = %d", dwID);

	EnterCriticalSection (&m_ClientsListCriticalSection);
	TPtrUsbClient pClient = FindClientByID(dwID);
	LeaveCriticalSection (&m_ClientsListCriticalSection);   

	if(pClient) 
	{
		LogString(dwFuncArea, LogTrace, __FUNCTION__"(): Client found. Call Start()...");
		pClient->SetAutoReconnect (bAutoReconnect);
		bFuncResult = pClient->Connect();
	}


	LogString(dwFuncArea, LogTrace, __FUNCTION__"(): EXIT - bFuncResult = \"%s\"", bFuncResult?"TRUE":"FALSE");
	return bFuncResult;
}

BOOL CRemoteSession::StopClient(DWORD dwID, BOOL bServerNotify) 
{
	DWORD dwFuncArea = LOG_INIT;
	BOOL bFuncResult = FALSE;

	LogString(dwFuncArea, LogTrace, __FUNCTION__"(): ENTER - dwID = %d", dwID);

	EnterCriticalSection (&m_ClientsListCriticalSection);
	TPtrUsbClient pClient = FindClientByID(dwID);
	LeaveCriticalSection (&m_ClientsListCriticalSection);   

	if (pClient) 
	{
		LogString(dwFuncArea, LogTrace, __FUNCTION__"(): Client found. Call Stop()...");
		bFuncResult = pClient->Disconnect(/*bServerNotify?true:false*/);
	}


	LogString(dwFuncArea, LogTrace, __FUNCTION__"(): EXIT - bFuncResult = \"%s\"", bFuncResult?"TRUE":"FALSE");
	return bFuncResult;
}

TPtrUsbClient CRemoteSession::FindClientByID(DWORD dwID) 
{
	ULONG FuncLogArea = LOG_UTIL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	TPtrUsbClient pClient;

	EnterCriticalSection (&m_ClientsListCriticalSection);

	TListRdpClients::iterator ClientItem;

	for (ClientItem = m_ClientsList.begin(); ClientItem != m_ClientsList.end (); ++ClientItem) 
	{
		if ((*ClientItem)->GetTcpPort() == dwID) 
		{
			pClient = *ClientItem;
			break;
		}
	}

	LeaveCriticalSection (&m_ClientsListCriticalSection);   

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"pClient\" = %p", pClient);
	return pClient;
}

void CRemoteSession::DeleteClient (DWORD dwID) 
{

	TListRdpClients::iterator	item;
	TPtrUsbClient				pClient;

	EnterCriticalSection (&m_ClientsListCriticalSection);

	for (item = m_ClientsList.begin (); item != m_ClientsList.end (); ++item) 
	{
		if (((*item)->GetTcpPort() == dwID))	
		{
			eltima::ung::UserLog::Client::DeleteRdp (*item);

			pClient = (*item);
			pClient->Release();
			m_ClientsList.erase (item);
			LeaveCriticalSection (&m_ClientsListCriticalSection);

			pClient->Disconnect ();
			pClient->Release();
			return;
		}
	}

	LeaveCriticalSection (&m_ClientsListCriticalSection);

}

BOOL CRemoteSession::GetClientsList (CBuffer &buffer) 
{
	ULONG FuncLogArea = LOG_NONE;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	CUngNetSettings	settins;
	BOOL bFuncResult = true;
	CStringA str;

	EnterCriticalSection (&m_ClientsListCriticalSection); 

	TListRdpClients::iterator ClientItem;

	for (ClientItem = m_ClientsList.begin(); ClientItem != m_ClientsList.end (); ++ClientItem) 
	{
		TPtrUsbClient pClient = *ClientItem;

		settins.Clear ();

		settins = pClient->GetSettings();

		settins.AddParamInt (Consts::szUngNetSettings [Consts::UNS_ID], pClient->GetTcpPort());
		settins.AddParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS], pClient->GetStatusForDll());

		settins.AddParamInt (Consts::szUngNetSettings [Consts::UNS_SESSION_ID], GetSessionId());

		str = settins.BuildSetttings ();
		buffer.AddArray (LPBYTE (LPCSTR (str)), str.GetLength ());
		buffer.Add (CUngNetSettings::GetSeparatorConfig ());
	}

	LeaveCriticalSection (&m_ClientsListCriticalSection);   

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"bFuncResult\" = %s", bFuncResult ? "TRUE":"FALSE");
	return bFuncResult;
}


// thread update rdp cleint list
DWORD WINAPI CRemoteSession::ThreadUpdateClientsList(LPVOID lParam) 
{
	ULONG FuncLogArea = LOG_THREAD;
	CRemoteSession *pBase = (CRemoteSession*)lParam;
	pBase->LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");
	DWORD dwLastThreadError = 0;

	HANDLE hEventCopy = NULL;

	EnterCriticalSection(&pBase->m_EventCriticalSection);

	DuplicateHandle (GetCurrentProcess(), pBase->m_hWorkedEvent, 
						GetCurrentProcess(), &hEventCopy, 
						0, FALSE, DUPLICATE_SAME_ACCESS);

	LeaveCriticalSection(&pBase->m_EventCriticalSection);

	while(WaitForSingleObject(hEventCopy, 0) == WAIT_OBJECT_0) 
	{
		BOOL bSendResult = pBase->SendRequest(USB4RDP_REQUEST_GET_SHARES_LIST, (DWORD)strlen(USB4RDP_REQUEST_GET_SHARES_LIST) + sizeof(CHAR));

		if (bSendResult == FALSE)
		{
 			ResetEvent(hEventCopy);
 
			pBase->m_pManager->RemoveSession(pBase->GetSessionId());
 			break;
		}

		Sleep(500);

#ifdef DEBUG
		int iTimeout = 30000000;
#else
		int iTimeout = 10000;
#endif

		// validate connection
		if((GetTickCount() - pBase->m_ul64ClientsListTimestamp > iTimeout) && 
			(!pBase->m_ClientsList.empty())) 
		{ 
			pBase->LogString(FuncLogArea, LogInfo, "CLIENT LIST REFRESH TIMEOUT expired and list not empty. Clear list.");
			pBase->ClearClientsList();
		}

	} 

	if(hEventCopy)
	{
		CloseHandle(hEventCopy);
		hEventCopy = NULL;
	}

	pBase->LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"dwLastThreadError\" = %d", dwLastThreadError);
	return dwLastThreadError;
}

void CRemoteSession::ClearClientsList()
{
	ULONG FuncLogArea = LOG_AUX;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	EnterCriticalSection(&m_ClientsListCriticalSection);

	if(!m_ClientsList.empty())
	{
		TListRdpClients::iterator item;

		for (item = m_ClientsList.begin (); item != m_ClientsList.end (); item = m_ClientsList.begin ()) 
		{
			(*item)->Disconnect();
			(*item)->Release();
			m_ClientsList.erase (item);

			if(m_ClientsList.empty()) 
			{
				break;
			}
		}
	}

	LeaveCriticalSection(&m_ClientsListCriticalSection);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

DWORD WINAPI CRemoteSession::ThreadReadVC(LPVOID lParam) 
{
	ULONG FuncLogArea = LOG_THREAD;
	CRemoteSession *pBase = (CRemoteSession*)lParam;

	pBase->LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	DWORD dwLastThreadError = 0;

	HANDLE hEventCopy = NULL;

	EnterCriticalSection(&pBase->m_EventCriticalSection);
	DuplicateHandle(GetCurrentProcess(), pBase->m_hWorkedEvent, 
		GetCurrentProcess(), &hEventCopy, 
		0, FALSE, DUPLICATE_SAME_ACCESS);
	LeaveCriticalSection(&pBase->m_EventCriticalSection);

	CBuffer Answer;
	while(WaitForSingleObject(hEventCopy, 0) == WAIT_OBJECT_0) 
	{
		EnterCriticalSection (&pBase->m_csReadVc);
		BOOL bReceiveAnswer = pBase->ReceiveAnswer(Answer);
		LeaveCriticalSection (&pBase->m_csReadVc);

		if(bReceiveAnswer) 
		{
			BOOL bAnalyseAnswerResult = pBase->AnalyseAnswer(Answer);

			if(bAnalyseAnswerResult == FALSE) 
			{
				pBase->LogString(FuncLogArea, LogWarning, __FUNCTION__"(): AnalyseAnswer() return FALSE");
			}

		}
		else 
		{
			break;
		}

	}

	CloseHandle(hEventCopy);

	pBase->LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - \"dwLastThreadError\" = %d", dwLastThreadError);
	return dwLastThreadError;
}

void CRemoteSession::EnumSessions() {
	ULONG FuncLogArea = LOG_DBG;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	PWTS_SESSION_INFO pWtsSessionInfo = NULL;
	DWORD dwCount = 0;
	BOOL bEnumResult = WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pWtsSessionInfo, &dwCount);

	if(bEnumResult && pWtsSessionInfo && dwCount) {
		LogString(FuncLogArea, LogInfo, "WTSEnumerateSessions() SUCCESS. Printing...");

		for(DWORD i = 0; i < dwCount; i++, pWtsSessionInfo++){

			LogString(FuncLogArea, LogInfo, "SessionId = %d, WinStaName = \"%ws\", State = \"%ws\"", 
				pWtsSessionInfo->SessionId,	
				pWtsSessionInfo->pWinStationName,	
				(LPCTSTR)SessionStateString(pWtsSessionInfo->State));

			LPWSTR pBuffer = NULL;
			DWORD dwRet = 0;

			PrintSessionInfo(pWtsSessionInfo->SessionId, WTSUserName);
			PrintSessionInfo(pWtsSessionInfo->SessionId, WTSWinStationName);
			PrintSessionInfoUlong(pWtsSessionInfo->SessionId, WTSClientBuildNumber); // ULONG 
			PrintSessionInfo(pWtsSessionInfo->SessionId, WTSClientName);
			PrintSessionInfoUshort(pWtsSessionInfo->SessionId, WTSClientProtocolType); // USHORT
			LogString(FuncLogArea, LogInfo, "----------------\n");

		} // for

		WTSFreeMemory(pWtsSessionInfo);
	}
	else {
		LogString(FuncLogArea, LogInfo, "WTSEnumerateSessions() failed. ERROR#%d !", GetLastError());
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

void CRemoteSession::PrintSessionInfo(DWORD SessionId, WTS_INFO_CLASS WtsClass) {
	LPWSTR pBuffer = NULL;
	DWORD dwRet = 0;
	ULONG FuncLogArea = LOG_DBG;

	if(WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, SessionId, WtsClass, &pBuffer, &dwRet)) {
		LogString(FuncLogArea, LogInfo, "%ws = \"%ws\"", (LPCTSTR)InfoClassNameString(WtsClass), pBuffer);
		WTSFreeMemory(pBuffer);
	}
}

void CRemoteSession::PrintSessionInfoUlong(DWORD SessionId, WTS_INFO_CLASS WtsClass) {
	LPWSTR pBuffer = NULL;
	DWORD dwRet = 0;
	ULONG FuncLogArea = LOG_DBG;

	if(WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, SessionId, WtsClass, &pBuffer, &dwRet)) {
		LogString(FuncLogArea, LogInfo, "%ws = %d", (LPCTSTR)InfoClassNameString(WtsClass), (ULONG)*pBuffer);
		WTSFreeMemory(pBuffer);
	}
}

void CRemoteSession::PrintSessionInfoUshort(DWORD SessionId, WTS_INFO_CLASS WtsClass) {
	LPWSTR pBuffer = NULL;
	DWORD dwRet = 0;
	ULONG FuncLogArea = LOG_DBG;

	if(WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, SessionId, WtsClass, &pBuffer, &dwRet)) {
		LogString(FuncLogArea, LogInfo, "%ws = %d", (LPCTSTR)InfoClassNameString(WtsClass), (USHORT)*pBuffer);
		WTSFreeMemory(pBuffer);
	}
}

CString CRemoteSession::InfoClassNameString(WTS_INFO_CLASS WtsClass) {
	CString str;
	switch (WtsClass) {
		case WTSInitialProgram: 
			str = "InitialProgram";
			break;
		case WTSApplicationName: 
			str = "ApplicationName";
			break;
		case WTSWorkingDirectory: 
			str = "WorkingDirectory";
			break;
		case WTSOEMId: 
			str = "OEMId";
			break;
		case WTSSessionId: 
			str = "SessionId";
			break;
		case WTSUserName:
			str = "UserName";
			break;
		case WTSWinStationName: 
			str = "WinStationName";
			break;
		case WTSDomainName: 
			str = "DomainName";
			break;
		case WTSConnectState: 
			str = "ConnectState";
			break;
		case WTSClientBuildNumber: 
			str = "ClientBuildNumber";
			break;
		case WTSClientName: 
			str = "ClientName";
			break;
		case WTSClientDirectory: 
			str = "ClientDirectory";
			break;
		case WTSClientProductId: 
			str = "ClientProductId";
			break;
		case WTSClientHardwareId: 
			str = "ClientHardwareId";
			break;
		case WTSClientAddress: 
			str = "ClientAddress";
			break;
		case WTSClientDisplay: 
			str = "ClientDisplay";
			break;
		case WTSClientProtocolType: 
			str = "ClientProtocolType";
			break;
		default:
			str = "Unknown";
	}
	return str;
}

CString CRemoteSession::SessionStateString(WTS_CONNECTSTATE_CLASS state) {
	CString str;
	switch (state) {
		case WTSActive: 
			str = "Active";
			break;
		case WTSConnected: 
			str = "Connected";
			break;
		case WTSConnectQuery: 
			str = "ConnectQuery";
			break;
		case WTSShadow: 
			str = "Shadow";
			break;
		case WTSDisconnected: 
			str = "Disconnected";
			break;
		case WTSIdle: 
			str = "Idle";
			break;
		case WTSReset: 
			str = "Reset";
			break;
		case WTSDown: 
			str = "Down";
			break;
		case WTSInit: 
			str = "Init";
			break;
		default:
			str = "Unknown";
	}
	return str;
}
