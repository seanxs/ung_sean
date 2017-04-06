/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Manager.cpp

Abstract:

	Manager class. Allows managing the service: adding, deleting disconnected devices,
	sharing, unsharing USB devices. Shows information about current connections and shared
	USB devices


Environment:

    User mode

--*/

#include "stdafx.h"
#include <atlstr.h>
#include <atltime.h>
#include <atltrace.h>
#include <exception>
#include <sddl.h>
#include <winioctl.h>
#include <WtsApi32.h>
#include "rdp_files\common\filetransport.h"
#include "rdp_files\RemoteSession.h"
#include "Manager.h"
#include "CommandService.h"
#include "Service.h"
#include "SettingsInRegistry.h"
#include "usbioctl.h"
#include "Consts\consts.h"
#include "Consts\consts_helper.h"
#include "Consts\base64.h"
#include "Consts\Registry.h"
#include "Consts\System.h"
#include "Consts\VersionInfo.hpp"
#include "Transport\RdpServer.h"
#include "Transport\Tcp.h"
#include "u2ec\dllmain.h"
#include "Utils\UserLog.h"

CString CManager::m_strFileName;
HANDLE  CManager::m_hPipeLog = INVALID_HANDLE_VALUE;
int     CManager::bInitLog = 0;
int		CManager::m_iCountUpdateUsb = 0;
CRITICAL_SECTION        CManager::m_csLog;

PCHAR GetVendorString (USHORT     idVendor);

CManager::CManager ()
    : m_uUdpPort (0)
{
	//SetLogArea(LOG_ALL & ~(LOG_EXCLUDE));
	SetLogArea(LOG_ALL);
    // Init Log
    try
    {
        CRegistry   reg (Consts::szRegistry);

        //m_strFileName = reg.GetString (Consts::szRegLogFile).c_str ();
		m_strFileName = _T("c:\\ung_sean.log");
    }
    catch (...)
    {
        m_strFileName = _T("");
		//m_strFileName = _T("c:\\ung_sean.log");
    }

    bInitLog++;
    if (bInitLog == 1)
    {
        InitializeCriticalSection (&m_csLog);
    }

    InitializeCriticalSection (&m_cs);
	InitializeCriticalSection (&m_CriticalSectionForOutgoingRDCConnectionsList);
	//InitializeCriticalSection (m_csConnect);

	// protector initialization should be done in CManager::Start - that is real starting point of normal work for UsbService, not here.
}

CManager::~CManager ()
{
    if (m_shptrProtectorTimer)
        m_shptrProtectorTimer->Cancel ();
    StopItems (NULL);
    DeleteCriticalSection (&m_cs);
	DeleteCriticalSection (&m_CriticalSectionForOutgoingRDCConnectionsList);
}

DWORD WINAPI CManager::FinishInitServer (LPVOID lpParam)
{
	CManager *pBase = (CManager*)lpParam;
	TListServer arrServer = pBase->m_arrServer;

	for (TListServer::iterator item = arrServer.begin (); item != arrServer.end (); item++)
    {
		if (pBase->FindServer ((*item)->GetHubName(), (*item)->GetUsbPort()))
		{
			if ((*item)->Connect ())
			{
				eltima::ung::UserLog::Server::Add (*item);
				continue;
			}
		}
		
		// not found or not started
		eltima::ung::UserLog::Server::NotStarted (*item);
	}

	return 0;
}

void CManager::LoadConfig()
{
    CRegistry       reg (Consts::szRegistryShare);
    std::vector <CRegString> arrValue;
    CString temp;
    int             iPos = 0;

    // load shared ports
    CString                 strName, strTemp;
    arrValue = reg.GetValueNames ();
	
	CUngNetSettings	settings;
	CUngNetSettings	answer;

    #ifdef LOG_FULL
    CString str;
    str.Format (_T("Loading config"));
    CManager::AddLogFile (str);

    str.Format (_T("Number of servers - %d"), arrValue.size ());
    CManager::AddLogFile (str);
    #endif

	// servers are restored in 2 steps (to make this operation fast):
	// 1st step: create objects but don't start connect and usb device sharing process
    for (int a = 0; a < (int)arrValue.size (); a++)
    {
		CStringA str;
		settings.ParsingSettings (CSystem::FromUnicode (reg.GetString (arrValue [a]).c_str ()));

		if (!AddServer (settings, answer, false))
        {
			// delete server
			TPtrUsbServer pDelServer = CUsbServer::Create (settings);
			eltima::ung::UserLog::Server::NotStarted (pDelServer);
			CRegistry reg (Consts::szRegistryShare);
			reg.DeleteValue ((LPCTSTR)pDelServer->GetVaribleName ());
        }
	}
	// 2nd step: in separate thread we start connect and sharing
	CloseHandle (CreateThread (NULL, 0, FinishInitServer, this, 0, NULL));

	// load remote clients
    reg.CreateKey (Consts::szRegistryConnect);
    arrValue = reg.GetValueNames ();

    for (int a = 0; a < (int)arrValue.size (); a++)
    {
		CStringA str;
		settings.ParsingSettings (CSystem::FromUnicode (reg.GetString (arrValue [a]).c_str ()));

		AddItemClient (settings, answer);
    }
}

void CManager::StopItems (CService *pSrv)
{
    int iCount = 1;
    TListClient::iterator itemClient;
    for (itemClient = m_arrClient.begin (); itemClient != m_arrClient.end (); itemClient = m_arrClient.begin ())
    {
        DeleteClient (*itemClient, false);
        if (pSrv)
        {
            pSrv->CheckWait (30000 * iCount);
            iCount++;
        }
        Sleep (1000);
    }

    TListServer::iterator itemServer;
    for (itemServer = m_arrServer.begin (); itemServer != m_arrServer.end (); itemServer = m_arrServer.begin ())
    {
        DeleteServer ((*itemServer)->GetHubName(), (*itemServer)->GetUsbPort());
        if (pSrv)
        {
            pSrv->CheckWait (30000 * iCount);
            iCount++;
        }
        Sleep (1000);
    }
}

// Share/unshare device 
TPtrUsbServer CManager::AddServer (const CUngNetSettings &settings, CUngNetSettings &answer, bool bAutoStart)
{
    TPtrUsbServer pServer = CUsbServer::Create (settings);
	pServer->m_strUsbLoc = GetUsbLoc (settings);

	bool bOk = true;

	if (bAutoStart && !pServer->Connect ())
	{
		bOk = false;
	}

	if (bOk)
	{
		EnterCriticalSection (&m_cs);
		m_arrServer.push_back (pServer);
		LeaveCriticalSection (&m_cs);
	}
	else
	{
		pServer = NULL;
	}

    return pServer;
}

TPtrUsbClient CManager::AddClient (const CUngNetSettings &settings)
{
    TPtrUsbClient pClient;
    EnterCriticalSection (&m_cs);
    pClient = new CUsbClient (settings, new Transport::CTcp (settings));
    m_arrClient.push_back (pClient);
    LeaveCriticalSection (&m_cs);
    return pClient;
}

TPtrUsbServer CManager::GetServerNetSettings (const CUngNetSettings &settings)
{
	return FindItemServer (settings);
}

TPtrUsbServer CManager::FindItemServer (const CUngNetSettings &settings)
{
	ULONG uTcpPort = settings.GetParamInt(Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);
	CString strHost =settings.GetParamStr(Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);

    TListServer::iterator item;
    EnterCriticalSection (&m_cs);
    for (item = m_arrServer.begin (); item != m_arrServer.end (); ++item)
    {
        if ((*item)->GetTcpPort() == uTcpPort && 
			(*item)->GetHostName().Compare(strHost) == 0)
        {
            LeaveCriticalSection (&m_cs);
            return (*item);
        }
    }
    LeaveCriticalSection (&m_cs);
    return NULL;
}

void CManager::DeleteServer (LPCTSTR szHubName, ULONG uUsbPort)
{
    TListServer::iterator item;

    EnterCriticalSection (&m_cs);
    for (item = m_arrServer.begin (); item != m_arrServer.end (); ++item)
    {
        if ((_tcsicmp (szHubName, (*item)->GetHubName()) == 0) && ((*item)->GetUsbPort() == uUsbPort))
        {
            eltima::ung::UserLog::Server::Delete (*item);

            (*item)->Disconnect ();
			(*item)->Release();
            m_arrServer.erase (item);
            break;
        }
    }
    LeaveCriticalSection (&m_cs);
}

// Parameter bSave enables saving current state of client in registry.
// When DeleteClient called by user command to Delete client 
// (e.g. dllmain's ClientRemoveRemoteDev) then we should update registry.
// But DeleteClient is also called when service is stopped, 
// in this case we only need stop client and don't change registry.
void CManager::DeleteClient (TPtrUsbClient pClient, bool bSave)
{
    TListClient::iterator item;
    EnterCriticalSection (&m_cs);
    for (item = m_arrClient.begin (); item != m_arrClient.end (); ++item)
    {
        if (*item == pClient)
        {
            eltima::ung::UserLog::Client::Delete (*item);

            (*item)->Disconnect();//Stop (bSave);
			(*item)->Release();
            if (bSave)
			{
				CClientSettingsInRegistry regset ((*item)->GetVaribleName ());
				regset.DeleteSettings ();
			}

            m_arrClient.erase (item);
            break;
        }
    }
    LeaveCriticalSection (&m_cs);
}

bool CManager::GetListOfClient(CUngNetSettings &settings, SOCKET sock)
{
	ULONG FuncLogArea = LOG_EXCLUDE;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	CBuffer		buffer;

	EnterCriticalSection(&m_cs);

	CUngNetSettings	answerSettins;
	CStringA str;

	TListClient::iterator ClientItem;

	for (ClientItem = m_arrClient.begin(); ClientItem != m_arrClient.end (); ++ClientItem) 
	{
		TPtrUsbClient pClient = *ClientItem;
		Transport::TPtrTransport pTr = pClient->GetTrasport();
		answerSettins.Clear ();

		answerSettins.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], pClient->GetHostName());
		answerSettins.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], pClient->GetTcpPort());
		answerSettins.AddParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS], pClient->GetStatusForDll());
		if (pTr)
		{
			answerSettins.AddParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH], pTr->GetRemoteHost());
		}

		if (pClient->IsAutoAdded())
		{
			answerSettins.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_AUTO_ADDED]);
		}

		str = answerSettins.BuildSetttings ();
		buffer.AddArray (LPBYTE (LPCSTR (str)), str.GetLength ());
		buffer.Add (CUngNetSettings::GetSeparatorConfig ());
	}

	LeaveCriticalSection(&m_cs);
	buffer.Add (CUngNetSettings::GetEnd ());

	// send answer
	send (sock, (char*)buffer.GetData(), buffer.GetCount (), 0);

	return true;
}

bool CManager::GetUsbTree (SOCKET sock)
{
	CBuffer		buffer;

	EnterCriticalSection(&m_cs);
	FillHub (&m_devsUsb, buffer);
	LeaveCriticalSection(&m_cs);
	buffer.Add (CUngNetSettings::GetEnd ());

	// send answer
	send (sock, (char*)buffer.GetData(), buffer.GetCount (), 0);
	return true;
}

void CManager::FillHub (CUsbDevInfo *pHub, CBuffer &buffer)
{
	CUngNetSettings		settings;
	CStringA			str;
	std::vector <CUsbDevInfo>::iterator item;

	for (item = pHub->m_arrDevs.begin(); item != pHub->m_arrDevs.end (); ++item) 
	{
		settings.Clear ();

		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], item->m_strGuid);
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_PARAM], item->m_strHubDev);
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_NAME], item->m_strName);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], item->m_iPort);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_CONN], item->m_bConnected);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_HUB], item->m_bHub);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_COUNT], (int)item->m_arrDevs.size ());
		if (!item->m_strUsbLoc.IsEmpty ())
		{
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_USB_LOC], item->m_strUsbLoc);
		}

		// usb dev info
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_CLASS], item->m_bBaseClass);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_SUBCLASS], item->m_bSubClass);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_PROTOCOL], item->m_bProtocol);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_BCDUSB], item->m_bcdUSB);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_VID], item->m_idVendor);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_PID], item->m_idProduct);
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_REV], item->m_bcdDevice);
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DEV_SN], item->m_strSerialNumber);
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DEV_UNIC_ID], item->m_strDevUnicId);
		
		// if shared
		TPtrUsbServer pServer;
		pServer = FindServer (pHub->m_strGuid, item->m_iPort);
		if (pServer)
		{
			Transport::TPtrTransport pTr = pServer->GetTrasport();
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_SHARED], true);
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_COMPRESS], pServer->IsCompressionEnabled ());
			if (pServer->IsCompressionEnabled ())
			{
				settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE],
					pServer->GetCompressionType ());
			}
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_AUTH], pServer->IsAuthEnable());
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_CRYPT], pServer->IsCryptEnable());
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], pServer->GetDescription());
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], pServer->GetHostName());
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], pServer->GetTcpPort());
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON], pServer->AllowRemoteDiscon());
			
			// connection status
			int iStatusForDll = pServer->GetStatusForDll ();
			if (pTr && iStatusForDll == U2EC_STATE_CONNECTED)
			{
				if (pTr->IsRdpConnected ())
				{
					iStatusForDll = U2EC_STATE_CONNECTED_FROM_RDP;
					settings.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_RDP]);
				}
				if (!pTr->GetRemoteHost().IsEmpty ())
				{
					settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH], pTr->GetRemoteHost());
				}
			}
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS], iStatusForDll);
		}

		str = settings.BuildSetttings ();
		buffer.AddArray (LPBYTE (LPCSTR (str)), str.GetLength ());
		buffer.Add (CUngNetSettings::GetSeparatorConfig ());

		if (item->m_bHub)
		{
			DoSetHubName (item->m_strGuid, item->m_strName);
			FillHub (&(*item), buffer);
		}
	}
}

bool CManager::GetRdpSharesList(CUngNetSettings &settings, SOCKET sock) 
{
	ULONG FuncLogArea = LOG_EXCLUDE;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	CBuffer		Buffer;
	ULONG		SessionId = -1;
	SessionId = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM]);


	EnterCriticalSection(&m_cs);

	std::list <TPtrRemoteSession>::iterator SessionItem;
	CStringA strSessionClients;

	for(SessionItem = m_SessionsList.begin(); SessionItem != m_SessionsList.end(); ++SessionItem) 
	{
		if ((*SessionItem)->m_dwSessionId == SessionId || SessionId == -1)
		{
			(*SessionItem)->GetClientsList(Buffer);
		}
	}
	LeaveCriticalSection(&m_cs);
	Buffer.Add (CUngNetSettings::GetEnd ());

	// send answer
	send (sock, (char*)Buffer.GetData(), Buffer.GetCount (), 0);

	return true;
}


// ***************************************************
//                 pipe command
// ***************************************************
bool CManager::ParseCommand (LPCSTR pBuffData, SOCKET sock)
{
	bool bRet = false;
	CUngNetSettings	settings;
	CUngNetSettings	answer;
	CStringA        strRet;
	CStringA		strCommand;
	int             iRet;

	//static std::list <int>	arrCount;

	DWORD FuncLogArea = LOG_NONE;

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER - pBuff = \"%s\"", pBuffData);

	settings.ParsingSettings (CStringA (pBuffData));

	strCommand = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_COMMAND]);

	const int ixCommand = Consts::PipeCommandFromStr (strCommand);

	try 
	{
		switch (ixCommand)
		{
		case Consts::CP_SET_AUTOCONNECT:
			// Set autoconnect param to registry
			bRet = SetAutoconnect(settings);
			break;

		case Consts::CP_ADD_RDC_CONNECTION:
			// Add new RDC connection need
			bRet = AddRDCConnection(settings);
			break;

		case Consts::CP_REM_RDC_CONNECTION:
			// remove broken RDC connection 
			bRet = RemoveRDCConnection(settings);
			break;

		case Consts::CP_GETRDPSHARES:
			// get list of available shares
			bRet = GetRdpSharesList(settings, sock);
			//AtlTrace ("Leave Counter - %d", settings.GetParamInt ("counter"));
			//arrCount.push_back (settings.GetParamInt ("counter"));
			return true;

		case Consts::CP_ADD:
			// add item
			bRet = AddItem (settings, answer);
			break;

		case Consts::CP_DEL:
			// del item
			bRet = DelItem (settings, answer);
			break;

		case Consts::CP_INFO:
			// get item info
			bRet = InfoItem (settings, answer);
			break;

		case Consts::CP_START_CLIENT:
			// start client
			bRet = StartClient (settings, answer);
			break;

		case Consts::CP_STOP_CLIENT:
			// stop client
			bRet = StopClient (settings, answer);
			break;

		case Consts::CP_HUBNAME:
			// hub name
			bRet = SetHubName (settings, answer);
			break;

		case Consts::CP_SRVDISCONN:
			bRet = DisconnectServer (settings, answer);
			break;

		case Consts::CP_GETDEVID:
			// get device id
			bRet = GetDevId (settings, answer);
			break;

		case Consts::CP_GETLISTOFCLIENTS:
			// get list of clients
			bRet = GetListOfClient (settings, sock);
			return true;

		case Consts::CP_GETUSBTREE:
			// get USB list
			bRet = GetUsbTree (sock);
			return true;

		case Consts::CP_GETNUMUSBTREE:
			// get num build USB list
			bRet = GetNumUsbTree (settings, answer);
			break;

		case Consts::CP_UPDATEUSBTREE:
			// update usb tree
			bRet = UpdateUsbTreeNow (settings, answer);
			break;

		case Consts::CP_DEL_ALL:
			// delete all
			bRet = DeleteAll (settings, answer);
			break;

		case Consts::CP_CHANGEDESK:
			// update user description (nick) for shared device (server side)
			bRet = ChangeItemDescription (settings, answer);
			break;

		case Consts::CP_SET_COMPRESS:
			// change compress option for shared device (server side)
			bRet = SetCompressOption (settings, answer);
			break;

		case Consts::CP_SET_CRYPT:
			// change crypt option for shared device (server side)
			bRet = SetCryptOption (settings, answer);
			break;

		case Consts::CP_SET_AUTH:
			// change auth option for shared device (server side)
			bRet = SetAuthOption (settings, answer);
			break;

		case Consts::CP_GET_CONN_CLIENT_INFO:
			// get data from connected client
			bRet = GetConnectedClientInfo (settings, answer);
			break;

		case Consts::CP_SET_RDISCONNECT:
			// set param of remote disconnect
			bRet = SetAllowRDisconnect (settings, answer);
			break;

		case Consts::CP_SET_RDP_ISOLATION:
			bRet = SetRdpIsolation (settings, answer);
			break;

		case -1:
			// unknown command.
			break;

		default:
			// known command but we don't have handler for it.
			break;
		}
	}
	catch (const std::exception &e)
	{
		// http://bugtracker.de.eltima.com/index.php?do=details&task_id=20549&project=46
		m_Log.LogString (FuncLogArea, LogError, __FUNCTION__"(): EXCEPTION: %s", e.what ());
		bRet = false;
		answer.Clear ();
	}
	catch (...)
	{
		// http://bugtracker.de.eltima.com/index.php?do=details&task_id=20549&project=46
		m_Log.LogString (FuncLogArea, LogError, __FUNCTION__"(): Unknown EXCEPTION");
		bRet = false;
		answer.Clear ();
	}

	if (!answer.ExistParam (Consts::szUngNetSettings [Consts::UNS_ERROR]))
	{
		answer.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_ERROR],
			bRet ? Consts::szUngErrorCodes [Consts::UEC_OK] : Consts::szUngErrorCodes [Consts::UEC_ERROR]);
	}

    if (sock != INVALID_SOCKET)
    {
		strRet = answer.BuildSetttings ();
		strRet += answer.GetEnd ();
        iRet = send (sock, strRet.GetBuffer (), strRet.GetLength (), 0);
        iRet = WSAGetLastError ();
    }

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - bRet = \"%s\"", bRet?"true":"false");
    return bRet;
}

bool CManager::SetHubName (CUngNetSettings &settings, CUngNetSettings &answre)
{
	CString strNameId = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME]);
	CString strNameName = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_PARAM]);

	DoSetHubName (strNameId, strNameName);

	return true;
}

void CManager::DoSetHubName (const CString &strId, const CString &strName)
{
	try
	{
		CRegistry       reg (Consts::szRegistryHubName);
		reg.SetString (LPCTSTR(strId), LPCTSTR(strName));
	}
	catch (...)
	{
	}
}

CString CManager::ConvertAuth (CStringA strBase)
{
    CString strRet;
    CStringA strTemp;
    int         iPosTemp = 0;
    if (strBase.GetLength ())
    {
        strTemp = strBase.Tokenize ("^", iPosTemp);
        if (strTemp.GetLength ())
        {
            strRet.Format (_T("%S^"), strTemp);

            // Get Password
            strTemp = strBase.Tokenize ("^", iPosTemp);
            if (strTemp.GetLength ())
            {
				strRet += CSystem::Decrypt (strTemp);
            }
        }
    }

    return strRet;
}

bool CManager::AddItemServer (CUngNetSettings &settings, CUngNetSettings &answer)
{
    CString         strName;
	CString			strNetSettings;
    ULONG           uUsbPort;

	strName =			settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME]);
	uUsbPort =			settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT]);
	strNetSettings  =	settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);
	strNetSettings +=	":";
	strNetSettings +=	settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);

	// verify exist net settings
	if (FindItemServer (settings))
	{
		eltima::ung::UserLog::Server::NotShared (strName, uUsbPort, strNetSettings);

		answer.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_PARAM], 
			Consts::szUngErrorCodes [Consts::UEC_TCP_PORT]);

		return false;
	}

    if (!FindServer (strName, uUsbPort))
    {
        TPtrUsbServer pServer;
        if (pServer = AddServer (settings, answer, true))
        {
			eltima::ung::UserLog::Server::Add (pServer);

			// adding to registry
			pServer->SaveSettingsInRegistry();

            return true;
        }
        else
        {
			eltima::ung::UserLog::Server::NotShared (strName, uUsbPort, strNetSettings);
        }
    }
    else
    {
        return true;
    }

	return false;
}

bool CManager::AddItemClient (CUngNetSettings &settings, CUngNetSettings &answer)
{
    CString         strName;
    ULONG           uTcpPort;

	strName  = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);
	uTcpPort = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);

    if (!FindClient (strName, uTcpPort))
    {
        TPtrUsbClient pClient;
        if (pClient = AddClient (settings))
        {
			eltima::ung::UserLog::Client::Add (pClient);

			pClient->SaveSettingsToRegistry ();

			// auto start
			if (settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_PARAM]))
			{
				pClient->Connect ();
			}

            return true;
        }
        else
        {
			eltima::ung::UserLog::Client::NotAdded (strName, uTcpPort);
        }
    }
	return false;
}

bool CManager::AddItem (CUngNetSettings &settings, CUngNetSettings &answre)
{
	CStringA str;

	str = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TYPE]);

	if (str.CompareNoCase (Consts::szpTypeItem [Consts::TI_SERVER]) == 0)
	{
		return AddItemServer (settings, answre);
	}
	else if (str.CompareNoCase (Consts::szpTypeItem [Consts::TI_CLIENT]) == 0)
	{
		return AddItemClient (settings, answre);
	}

	return false;
}

TPtrUsbServer CManager::GetServer (const CUngNetSettings &settings)
{
    CString         strName;
    ULONG           uUsbPort;

    // Usb Hub Name
	strName = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME]);
    // USB port
	uUsbPort = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT]);

    return FindServer (strName, uUsbPort);
}

TPtrUsbClient CManager::GetClient (const CUngNetSettings &settings)
{
	bool bRdp;
	TPtrUsbClient pCleint = NULL;

	// TCP port
	int uTcpPort = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);
	bRdp = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_RDP])?true:false;

	if (bRdp)
	{
		std::list <TPtrRemoteSession>::iterator Session;
		int iSessionId = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_SESSION_ID], -1);

		EnterCriticalSection (&m_cs);

		for(Session = m_SessionsList.begin(); Session != m_SessionsList.end(); ++Session) 
		{
			if (((*Session)->GetSessionId () == iSessionId) || (iSessionId == -1))
			{
				pCleint = (*Session)->FindClientByID (uTcpPort).get ();
			}
		}

		LeaveCriticalSection (&m_cs);

	}
	else
	{
		// Host name
		CString strName (settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]));

		pCleint = FindClient (strName, uTcpPort);
	}
	return pCleint;
}

bool CManager::DeleteAll (CUngNetSettings &settings, CUngNetSettings &answer)
{
    CStringA        strTemp;

    // Item type
	strTemp = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TYPE]);
    if (!strTemp.GetLength ())
    {
        return false;
    }

    if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_SERVER]) == 0)
    {
        // Server
		TListServer::iterator item;
		EnterCriticalSection (&m_cs);
		while (m_arrServer.size ())
		{
			LeaveCriticalSection (&m_cs);   

            CRegistry reg (Consts::szRegistryShare);
            reg.DeleteValue((LPCTSTR)m_arrServer.front ()->GetVaribleName ());

			DeleteServer (m_arrServer.front ()->GetHubName(), m_arrServer.front ()->GetUsbPort());
			EnterCriticalSection (&m_cs);
		}
		LeaveCriticalSection (&m_cs);   
	}
    else
    {
        // Unknown item type
        return false;
    }

    return true;

}

bool CManager::DelItem (CUngNetSettings &settings, CUngNetSettings &answre)
{
    CStringA        strTemp;

    // Item type
	strTemp = settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TYPE]);
    if (!strTemp.GetLength ())
    {
        return false;
    }

    if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_SERVER]) == 0)
    {
        // Server
        TPtrUsbServer pServer = GetServer (settings);
        if (pServer)
        {
            // deleting from registry
			CServerSettingsInRegistry regset (pServer->GetVaribleName ());
			regset.DeleteSettings ();

	        DeleteServer (pServer->GetHubName(), pServer->GetUsbPort());
        }
		else
		{
			return false;
		}
    }
    else if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_CLIENT]) == 0)
    {
        // Client
        TPtrUsbClient pClient = GetClient (settings);
        if (pClient)
        {
	        DeleteClient (pClient, true);
        }
    }
    else
    {
        // Unknown item type
        return false;
    }

    return true;
}

bool CManager::DisconnectServer (CUngNetSettings &settings, CUngNetSettings &answre)
{
    TPtrUsbServer pServer = GetServer (settings);
    if (pServer)
    {
		return DoDisconnectServer (pServer);
    }

    return false;
}

bool CManager::DoDisconnectServer (TPtrUsbServer pServer)
{
	if (pServer->IsConnectionActive ())
	{
		Transport::TCP_PACKAGE	tcpPackage;
		Transport::TPtrTransport pTr = pServer->GetTrasport();

		if (pTr)
		{
			tcpPackage.Type = Transport::ManualDisconnect;
			tcpPackage.SizeBuffer = 0;
			pTr->Write(&tcpPackage, sizeof (tcpPackage));
		}
	}
	return true;
}

TPtrUsbServer CManager::GetServerPtr (const CUngNetSettings &settings)
{
	// Item type
	CStringA strType = settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_TYPE]);
	if (strType.CompareNoCase (Consts::szpTypeItem [Consts::TI_SERVER]) == 0)
	{
		// Server
		TPtrUsbServer pServer = GetServer (settings);
		return pServer;
	}

	return TPtrUsbServer(NULL);
}


bool CManager::InfoItem (CUngNetSettings &settings, CUngNetSettings &answre)
{
    CString         strName;
	bool			bRet		= false;

	CStringA strTemp (settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TYPE]));
    if (!strTemp.GetLength ())
    {
        return bRet;
    }
    if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_SERVER]) == 0)
    {
        // Server
		TPtrUsbServer pServer = GetServer (settings);
		if (pServer)
		{
			int iStatusForDll = pServer->GetStatusForDll();
			Transport::TPtrTransport pTr = pServer->GetTrasport();
			if (pTr && iStatusForDll == U2EC_STATE_CONNECTED)
			{
				if (pTr->IsRdpConnected ())
				{
					iStatusForDll = U2EC_STATE_CONNECTED_FROM_RDP;
					answre.AddParamFlag (Consts::szUngNetSettings [Consts::UNS_RDP]);
				}
				if (!pTr->GetRemoteHost().IsEmpty ())
				{
					answre.AddParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH], pTr->GetRemoteHost());
				}
			}

			answre.AddParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS], iStatusForDll);

			bRet = true;
		}
    }
    else if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_CLIENT]) == 0)
    {
		EnterCriticalSection (&m_cs);
		TPtrUsbClient pClient = GetClient (settings);
		if (pClient)
		{
			answre.AddParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS], pClient->GetStatusForDll());
			answre.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], pClient->GetHostName());
			bRet = true;
		}
		LeaveCriticalSection (&m_cs);
    }

	return bRet;
}

bool CManager::SetAllowRDisconnect (const CUngNetSettings &settings, CUngNetSettings &answer)
{
	TPtrUsbServer pServer = GetServerPtr (settings);
	if (pServer)
	{
		// change description
		pServer->SetAllowRemoteDiscon (settings.GetParamInt(Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON])?true:false);

		return true;
		// ^ we can get false from UpdateSettingsStringParam only if old settings are invalid
		// is it really an error for CManager::ChangeItemDescription ?
	}

	return false;
}

bool CManager::ChangeItemDescription (const CUngNetSettings &settings, CUngNetSettings &answer)
{
	TPtrUsbServer pServer = GetServerPtr (settings);
	if (pServer)
	{
		// change description
		const CStringA strParamName = Consts::szUngNetSettings [Consts::UNS_DESK];
		const CString newDesc = settings.GetParamStr (strParamName);
		pServer->SetDescription (newDesc);

		// update registry
		CServerSettingsInRegistry regset (pServer->GetVaribleName ());
		return regset.UpdateSettingsStringParam (strParamName, newDesc);
		// ^ we can get false from UpdateSettingsStringParam only if old settings are invalid
		// is it really an error for CManager::ChangeItemDescription ?
	}

	return false;
}

bool CManager::SetCompressOption (const CUngNetSettings &settings, CUngNetSettings &answer)
{
	TPtrUsbServer pServer = GetServerPtr (settings);
	if (pServer)
	{
		if (pServer->EnableCompression(settings))
		{
			// update registry settings
			pServer->SaveSettingsInRegistry();		
			// restart current connection
			pServer->OnError(0);
			return true;
		}
	}

	return false;
}

bool CManager::SetCryptOption (const CUngNetSettings &settings, CUngNetSettings &answer)
{
	TPtrUsbServer pServer = GetServerPtr (settings);
	if (pServer)
	{
		// update registry settings
		if (pServer->SetCrypt (settings))
		{
			pServer->SaveSettingsInRegistry();
			// restart current connection
			pServer->OnError(0);
			return true;
		}
	}

	return false;
}

bool CManager::SetAuthOption (const CUngNetSettings &settings, CUngNetSettings &answer)
{
	TPtrUsbServer pServer = GetServerPtr (settings);
	if (pServer)
	{
		if (pServer->SetAuth(settings))
		{
			pServer->SaveSettingsInRegistry();
			// restart current connection
			pServer->OnError(0);
			return true;
		}
	}

	return false;
}

bool CManager::GetNumUsbTree(CUngNetSettings &settings, CUngNetSettings &answer)
{
	answer.AddParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM], m_iCountUpdateUsb);
	return true;
}

bool CManager::UpdateUsbTreeNow (CUngNetSettings &settings, CUngNetSettings &answer)
{
	CService *pServ = CService::GetService ();
	if (pServ->IsEnableTimer ())
	{
		pServ->ResetTimer ();
		//GetUsbTree (settings);
		return true;
	}
	CService::GetService ()->UpdateUsbTree ();
	return true;
}


bool CManager::StartClient (CUngNetSettings &settings, CUngNetSettings &answre)
{
	TPtrUsbClient pClient;
	bool bRet = false;

	EnterCriticalSection (&m_cs);
	pClient = GetClient (settings);
	if (pClient)
	{
		pClient->SetAuth (settings);
		pClient->SetAutoReconnect (settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_PARAM]));
		pClient->SaveSettingsToRegistry ();		

		bRet = pClient->Connect ();
	}
	LeaveCriticalSection (&m_cs);
	return bRet;
}

bool CManager::StopClient (CUngNetSettings &settings, CUngNetSettings &answre)
{
	TPtrUsbClient pClient;

	EnterCriticalSection (&m_cs);
	pClient = GetClient (settings);
	if (pClient)
	{
		pClient->SetAutoReconnect (false);
		pClient->Disconnect();
	}
	LeaveCriticalSection (&m_cs);

	return true;
}

TPtrUsbServer CManager::FindServerByIDForRdp(DWORD dwID) 
{
	TListServer::iterator item;
	EnterCriticalSection (&m_cs);

	if (! (dwID & 0x80000000))
	{
		ATLASSERT (FALSE);
	}

	dwID = (dwID & (~0x80000000));

	for (item = m_arrServer.begin (); item != m_arrServer.end (); ++item)
	{
		if ((*item)->GetTcpPort () == dwID)
		{
			LeaveCriticalSection (&m_cs);
			return (*item);
		}
	}
	LeaveCriticalSection (&m_cs);
	return NULL;
}

ULONG CManager::GetLocalIP() 
{
	hostent* pHostEntry = gethostbyname("");
	SOCKADDR_IN AddrIn;
	AddrIn.sin_addr = *((LPIN_ADDR)*pHostEntry->h_addr_list);
	
	if(AddrIn.sin_addr.S_un.S_un_b.s_b1  == 0)
	{
		AddrIn.sin_addr	= *((LPIN_ADDR)pHostEntry->h_addr_list[1]);
	}

	return AddrIn.sin_addr.S_un.S_addr;
}

void CManager::Stop (CService *pSrv)
{
    StopItems (pSrv);
    CRemoteManager::Stop ();
    WaitForSingleObject (m_hThread, 2000);
    //StopProtector ();
}

// Thread for find shared
DWORD WINAPI CManager::ThreadUdp (LPVOID lpParam)
{
    CManager *pBase = (CManager *)lpParam;
    SOCKADDR_IN     servAddr;
    SOCKADDR_IN     clientAddr;
    SOCKET          sock;
    int iAddSize = sizeof (struct sockaddr_in); 
    timeval time = { 0, 100 };
    fd_set  fdRead;
    char    Buf[100];
    int             iRead;

    servAddr.sin_family = AF_INET; 
    servAddr.sin_port = htons (pBase->m_uUdpPort); 
    servAddr.sin_addr.s_addr = INADDR_ANY; 

    if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) 
    { 
        return 0; 
    } 

	BOOL bBroadcast = TRUE; 
	setsockopt (sock, SOL_SOCKET, SO_BROADCAST, (char*)&bBroadcast,sizeof(BOOL));

    if (bind (sock, (struct sockaddr *) &servAddr, iAddSize) == SOCKET_ERROR) 
    { 
        closesocket (sock); 
        return 0;
    } 

    FD_ZERO (&fdRead);
    while (WaitForSingleObject (pBase->m_hEvent, 100) == WAIT_OBJECT_0)
    {
        FD_SET (sock, &fdRead);
        if (select (0, &fdRead, NULL, NULL, &time))
        {
            iRead = recvfrom (sock, Buf, 100, 0, (struct sockaddr *) &clientAddr, &iAddSize);
            if (iRead == SOCKET_ERROR || iRead == 0)
            {
                continue;
            }
            Sleep (10);
            Buf [iRead] = 0;
            if (strcmp (Consts::szpPipeCommand [Consts::CP_GET_IP], Buf) == 0)
            {
				SOCKET          sockTemp;

				if ((sockTemp = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) != SOCKET_ERROR) 
				{ 
					USHORT portOut = htons (clientAddr.sin_port);
					if (portOut > pBase->m_uUdpPort && portOut < pBase->m_uUdpPort + 200)
					{
						clientAddr.sin_port = clientAddr.sin_port;
					}
					else
					{
						clientAddr.sin_port = htons (pBase->m_uUdpPort + 1);
					}
					for (int a = 0; a < 10; a++)
					{
						sendto (sockTemp, Buf, iRead, 0, (struct sockaddr *) &clientAddr, iAddSize);
					}
					closesocket (sockTemp);
				}
            }
        }
    }

    return 0;
}
// *******************************************************************
//                              remote connection
// *******************************************************************
// get addition info about shared list and shared device
// Consts::szpPipeCommand [Consts::CP_GET_CONFIG]        - list shared devices
// Consts::szpPipeCommand [Consts::CP_GET_NAME_DEVICE]   - Get info about shared device
void CManager::ExistConnect (SOCKET sock, const SOCKADDR_IN &ip)
{
    timeval         wait = { 10, 0 };
    fd_set          fdRead;
    char            pChar;
    CStringA        str;
    int             rVal;
	CUngNetSettings	settings;

    FD_ZERO (&fdRead);
    // receiving command
    FD_SET (sock, &fdRead);
    FD_ZERO (&fdRead);
    if (select (0, &fdRead, NULL, NULL, &wait))
    {
        wait.tv_sec = 1;
        // Wait only 10 sec
        for (int a = 0; a < 10; a++)
        {
            FD_ZERO (&fdRead);
            FD_SET (sock, &fdRead);
            if (select (0, &fdRead, NULL, NULL, &wait))
            {
                rVal = recv (sock, &pChar, 1, 0);
                if (rVal == INVALID_SOCKET || rVal == 0)
                {
                    return;
                }
				if (pChar == CUngNetSettings::GetEnd ())
                {
                    break; 
                }
				if (pChar == '!') // old protocol
                {
                    return;
                }
                str += pChar;
                a--;
            }
        }

		if (!settings.ParsingSettings (str))
		{
			return;
		}
        // Get shared ports
        if (settings.ExistParam (Consts::szpPipeCommand [Consts::CP_GET_CONFIG]))
        {
			CBuffer buffer;
            GetStringShare (buffer, false);

			buffer.Add (CUngNetSettings::GetEnd ());
            FD_SET (sock, &fdRead);
            FD_ZERO (&fdRead);
            if (select (0, &fdRead, NULL, NULL, &wait))
            {
                send (sock, (LPCSTR)buffer.GetData (), buffer.GetCount (), 0);
            }
            return;
        }
        // Get shared ports names
        else if (settings.ExistParam (Consts::szpPipeCommand [Consts::CP_GET_NAME_DEVICE]))
        {
			CBuffer	buffer;
			TPtrUsbServer pItem = GetServerNetSettings (settings);
            if (BuildStringShareEx (pItem, buffer, false))
            {
				buffer.Add (CUngNetSettings::GetEnd ());
				FD_SET (sock, &fdRead);
				FD_ZERO (&fdRead);
				if (select (0, &fdRead, NULL, NULL, &wait))
				{
					send (sock, (LPCSTR)buffer.GetData (), buffer.GetCount (), 0);
				}
			}
			return;
		}
		// Get shared ports names
		else if (settings.ExistParam (Consts::szpPipeCommand [Consts::CP_RDISCONNECT]))
		{
			TPtrUsbServer pItem = GetServerNetSettings (settings);
			CBuffer	buffer;
			if (pItem && pItem->AllowRemoteDiscon ())
			{
				DoDisconnectServer (pItem);
				settings.Clear();
				settings.AddParamStrA(Consts::szUngNetSettings [Consts::UNS_ERROR], Consts::szUngErrorCodes [Consts::UEC_OK]);
			}
			else
			{
				settings.Clear();
				settings.AddParamStrA(Consts::szUngNetSettings [Consts::UNS_ERROR], Consts::szUngErrorCodes [Consts::UEC_ERROR]);
			}
			buffer.AddString (settings.BuildSetttings());
			buffer.Add (CUngNetSettings::GetEnd ());
			send (sock, (LPCSTR)buffer.GetData (), buffer.GetCount (), 0);
		}
    }
}

bool CManager::BuildStringShareEx (TPtrUsbServer pServer, CBuffer &buffer, bool bRdp)
{
	if (pServer)
	{
		EnterCriticalSection (&m_cs);
		const CUsbDevInfo *pUsbDevInfo = m_devsUsb.FindDevice (pServer->GetHubName(), pServer->GetUsbPort());
		BuildStringShare (pServer, buffer, bRdp, pUsbDevInfo);
		LeaveCriticalSection (&m_cs);
		return true;
	}
	return false;
}

/*static*/
void CManager::BuildStringShare (TPtrUsbServer pServer, CBuffer &buffer, bool bRdp, const CUsbDevInfo *pUsbDevInfo)
{
	CUngNetSettings	settings;
	CString			strTemp;

	strTemp = pServer->GetDeviceName();
	if (pServer->GetDevicePresentCachedFlag() && (strTemp.IsEmpty() || (strTemp == Consts::szUnknowDevice)))
	{
		pServer->FillDevName ();
	}

	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], GetHubName (pServer->GetHubName()));
	
	// usb location
	{
		const CString strUsbLoc = pServer->GetUsbLoc ();
		if (!strUsbLoc.IsEmpty ())
		{
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_USB_LOC], strUsbLoc);
		}
	}

	if (bRdp)
	{
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], pServer->GetRdpPort ());
	}
	else
	{
		settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT], pServer->GetTcpPort());
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST], pServer->GetHostName());
	}

	// name
	{
	CStringA str;
	str.Format ("Port%d", pServer->GetUsbPort());
	settings.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_USB_PORT], str);
	}

	strTemp = pServer->GetDeviceName();

	if (!strTemp.IsEmpty ())
	{
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_NAME], strTemp);
	}
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], pServer->GetDescription());
	if (pServer->IsAuthEnable())
	{
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_AUTH], NULL);
	}
	if (pServer->IsCryptEnable())
	{
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_CRYPT], NULL);
	}
	if (pServer->IsCompressionEnabled ())
	{
		settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_COMPRESS], NULL);
	}
	
	CString strSharedWith;
	Transport::TPtrTransport pTr = pServer->GetTrasport();
	if (pTr)
	{
		strSharedWith = pTr->GetRemoteHost ();
	}
	if (strSharedWith.IsEmpty ())
	{
		strSharedWith = pServer->GetHostName ();
	}
	settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH], strSharedWith);
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS], pServer->GetStatusForDll());
	settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON], pServer->AllowRemoteDiscon());

	if (pUsbDevInfo)
	{
		if (pUsbDevInfo->m_bBaseClass != 0 || pUsbDevInfo->m_bSubClass != 0 || pUsbDevInfo->m_bProtocol != 0)
		{
			// send USBCLASS info only if usb class triple is not all zero.
			settings.AddParamStrA (
				Consts::szUngNetSettings [Consts::UNS_DEV_USBCLASS],
				pUsbDevInfo->UsbClassAsHexStrA ());
		}

		// send other data from device descriptor if not zero
		if (pUsbDevInfo->m_idVendor != 0)
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_VID], pUsbDevInfo->m_idVendor);

		if (pUsbDevInfo->m_idProduct != 0)
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_PID], pUsbDevInfo->m_idProduct);

		if (pUsbDevInfo->m_bcdDevice != 0)
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_REV], pUsbDevInfo->m_bcdDevice);

		if (pUsbDevInfo->m_bcdUSB != 0)
			settings.AddParamInt (Consts::szUngNetSettings [Consts::UNS_DEV_BCDUSB], pUsbDevInfo->m_bcdUSB);

		if (!pUsbDevInfo->m_strSerialNumber.IsEmpty())
			settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DEV_SN], pUsbDevInfo->m_strSerialNumber);
	}

	buffer.AddString (settings.BuildSetttings ());
}

void CManager::GetStringShare (CBuffer &buffer, bool bRdp)
{
    CStringA        str;
    CStringA        strHub;
	CStringA        strNetSettings;

    TListServer::iterator item;


    EnterCriticalSection (&m_cs);
    for (item = m_arrServer.begin (); item != m_arrServer.end (); ++item)
    {
		TPtrUsbServer pServer = *item;
		if (IsUsbHub (pServer))
		{
			// don't show USB hubs to external world.
			continue;
		}
		const CUsbDevInfo *pUsbDevInfo = m_devsUsb.FindDevice (pServer->GetHubName(), pServer->GetUsbPort());
		BuildStringShare (pServer, buffer, bRdp, pUsbDevInfo);
		buffer.Add (CUngNetSettings::GetSeparatorConfig ());
    }
    LeaveCriticalSection (&m_cs);   
}

bool CManager::IsUsbHub (TPtrUsbServer pServer) const
{
	const CUsbDevInfo* pHub = m_devsUsb.FindHub (pServer->GetHubName ());
	if (pHub)
	{
		const int ix = pServer->GetUsbPort ();	// ix starts from 1, not zero
		if (ix < 1 || (size_t)ix > pHub->m_arrDevs.size ())
			return false;

		if (pHub->m_arrDevs.at (ix-1).m_bHub)
			return true;
	}
	return false;
}


// Find
TPtrUsbServer CManager::FindServer (LPCTSTR szHubName, ULONG uUsbPort)
{
    TListServer::iterator item;

    EnterCriticalSection (&m_cs);
    for (item = m_arrServer.begin (); item != m_arrServer.end (); ++item)
    {
        if (_tcsicmp ((*item)->GetHubName(), szHubName) == 0 &&
            (*item)->GetUsbPort() == uUsbPort)
        {
            LeaveCriticalSection (&m_cs);
            return *item;
        }

    }
    LeaveCriticalSection (&m_cs);   
    return NULL;
}

TPtrUsbClient CManager::FindClient (LPCTSTR szHostName, ULONG uTcpPort)
{
    TListClient::iterator item;
    EnterCriticalSection (&m_cs);
    for (item = m_arrClient.begin (); item != m_arrClient.end (); ++item)
    {
        if (_tcsicmp ((*item)->GetHostName(), szHostName) == 0 &&
            (*item)->GetTcpPort() == uTcpPort)
        {
            LeaveCriticalSection (&m_cs);
            return *item;
        }
    }
    LeaveCriticalSection (&m_cs);   
    return NULL;
}

CString CManager::GetHubName (LPCTSTR szHubId)
{
    CString str;
    try
    {
        CRegistry    reg (Consts::szRegistryHubName);
        str = reg.GetString (szHubId).c_str ();
    }
    catch (...)
    {
        str = szHubId;
    }
    return str;
}

CStringA CManager::GetDeviceUsbClass(LPCTSTR szHubName, ULONG uUsbPort)
{
	CStringA usb_class;

    EnterCriticalSection (&m_cs);   
	const CUsbDevInfo *pUsbDevInfo = m_devsUsb.FindDevice (szHubName, uUsbPort);
	if (pUsbDevInfo)
	{
		usb_class = pUsbDevInfo->UsbClassAsHexStrA ();
	}
    LeaveCriticalSection (&m_cs);   

	return usb_class;
}

void CManager::AddLogFile (LPCTSTR String)
{
    CString sTmp;
    CStringA sTmpA;
    CTime   time = CTime::GetCurrentTime();
    sTmp = time.Format(L"%d.%m.%Y %H:%M:%S  - ") + String;
    // save to pipe
    AddLogPipe (sTmp);

    // save to file
    sTmp += _T("\r\n");

    if (m_strFileName.IsEmpty())
    {
        return;
    }

    EnterCriticalSection (&m_csLog);
    HANDLE  hFile;
    DWORD   dwRead;
    hFile = CreateFile (m_strFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
		LeaveCriticalSection (&m_csLog);
        return;
    }

    sTmpA = sTmp;
    SetFilePointer (hFile, 0, 0, FILE_END);
    dwRead = (DWORD)(sTmpA.GetLength());
    WriteFile (hFile, sTmpA, dwRead, &dwRead, NULL); 
    CloseHandle (hFile);
    LeaveCriticalSection (&m_csLog);
}

void CManager::AddLogPipe (LPCTSTR String)
{
    DWORD cbWritten; 

    if (m_hPipeLog == INVALID_HANDLE_VALUE)
    {
        if (!OpenLogPipe (Consts::szPipeNameLog))
        {
            return;
        }
    }

    for (int a = 0; a < 2; a++)
    {
        if (!WriteFile( 
            m_hPipeLog,                  // pipe handle 
            String,             // message 
            DWORD((_tcslen(String)+ 1)*sizeof (TCHAR)), // message length 
            &cbWritten,             // bytes written 
            NULL))                  // not overlapped 
        {
            if (OpenLogPipe (Consts::szPipeNameLog))
            {
                continue;
            }
        }
        break;

    }

    return; 
}

bool CManager::OpenLogPipe (LPCTSTR szPipe)
{
    BOOL fSuccess; 
    DWORD dwMode; 

    // Try to open a named pipe; wait for it, if necessary. 

    m_hPipeLog = CreateFile( 
        szPipe,   // pipe name 
        GENERIC_READ |  // read and write access 
        GENERIC_WRITE, 
        0,              // no sharing 
        NULL,           // no security attributes
        OPEN_EXISTING,  // opens existing pipe 
        0,              // default attributes 
        NULL);          // no template file 


    // The pipe connected; change to message-read mode. 

    dwMode = PIPE_READMODE_MESSAGE; 
    fSuccess = SetNamedPipeHandleState( 
        m_hPipeLog,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 

    if (!fSuccess) 
    {
        //printf("SetNamedPipeHandleState failed"); 
        m_hPipeLog = INVALID_HANDLE_VALUE;
        return false;
    }

    return true;
}

void CManager::CloseLogPipe ()
{
    if (m_hPipeLog != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_hPipeLog);
        m_hPipeLog = INVALID_HANDLE_VALUE;
    }
}

bool CManager::GetDevId (CUngNetSettings &settings, CUngNetSettings &answre)
{
    CString         strName;
	bool			bRet		= false;

	CStringA strTemp (settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TYPE]));
    if (!strTemp.GetLength ())
    {
        return bRet;
    }
    if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_SERVER]) == 0)
	{
		TPtrUsbServer pServer = GetServer (settings);
		if (pServer)
		{
			answre.AddParamStr (Consts::szUngNetSettings [Consts::UNS_NAME], pServer->GetDeviceName());
			bRet = true;
		}
	}
	else if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_CLIENT]) == 0)
	{
		TPtrUsbClient pClient = GetClient (settings);
		if (pClient)
		{
			answre.AddParamStr (Consts::szUngNetSettings [Consts::UNS_NAME], pClient->GetDeviceName ());
			bRet = true;
		}
	}
	return bRet;
}

bool CManager::GetConnectedClientInfo (const CUngNetSettings &settings, CUngNetSettings &answer)
{
	CStringA strTemp (settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TYPE]));
	if (strTemp.CompareNoCase (Consts::szpTypeItem [Consts::TI_CLIENT]) != 0)
		return false;
	
	TPtrUsbClient pClient = GetClient (settings);
	if (pClient)
	{
		Transport::TPtrTransport pTr = pClient->GetTrasport();

		answer.AddParamStr (Consts::szUngNetSettings [Consts::UNS_NAME], 
			pClient->GetDeviceName());
		answer.AddParamStr (Consts::szUngNetSettings [Consts::UNS_DESK], 
			pClient->GetDescription());
		answer.AddParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME], 
			pClient->GetHubName ());
		answer.AddParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT], 
			pClient->GetUsbPort());
		{
			const CString strUsbLoc = pClient->GetUsbLoc ();
			if (!strUsbLoc.IsEmpty ())
				answer.AddParamStr (Consts::szUngNetSettings [Consts::UNS_USB_LOC], strUsbLoc);
		}
		answer.AddParamInt (Consts::szUngNetSettings [Consts::UNS_STATUS], 
			pClient->GetStatusForDll());
		if (pTr)
		{
			answer.AddParamStr (Consts::szUngNetSettings [Consts::UNS_SHARED_WITH], 
				pTr->GetRemoteHost());
		}
		answer.AddParamBoolean (Consts::szUngNetSettings [Consts::UNS_AUTH], 
			pClient->IsAuthEnable());
		answer.AddParamBoolean (Consts::szUngNetSettings [Consts::UNS_COMPRESS], 
			pClient->IsCompressionEnabled());
		answer.AddParamBoolean (Consts::szUngNetSettings [Consts::UNS_CRYPT],
			pClient->IsCryptEnable());

		const CStringA usbClass = pClient->GetUsbClass ();
		if (!usbClass.IsEmpty())
			answer.AddParamStrA (Consts::szUngNetSettings [Consts::UNS_DEV_USBCLASS], usbClass);

		return true;
	}

	return false;
}

void CManager::WrapRoutine(LPVOID pCallerParam) 
{
	ULONG FuncLogArea = LOG_AUX;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	PPARAMS_TO_WRAP pParams = (PPARAMS_TO_WRAP)pCallerParam;

	switch(pParams->ActionType) {
		case AddSessionAction:
			AddSession(pParams->dwSessionId);
			break;
		case RemoveSessionAction:
			RemoveSession(pParams->dwSessionId);
			break;
	}

	delete pParams;

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT");
}

BOOL CManager::AddSession(DWORD dwSessionId) {
	ULONG FuncLogArea = LOG_SESSIONS;
	BOOL bFuncResult = FALSE;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER, dwSessionId = %d", dwSessionId);

	if(FindSessionById(dwSessionId)) 
	{
		LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Session with ID = %d already exist.", dwSessionId);
	}
	else 
	{
		EnterCriticalSection(&m_cs);

		TPtrRemoteSession pNewRemoteSession = new CRemoteSession(dwSessionId, this);

		if(pNewRemoteSession)	 
		{
			LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Session created successful (0x%.8X)", pNewRemoteSession);
			m_SessionsList.push_back(pNewRemoteSession);
			pNewRemoteSession->CreateStartThread();
			bFuncResult = TRUE;
		}
		else 
		{
			LogString(FuncLogArea, LogError, __FUNCTION__"(): Cannot create remote session object!");
		}

		LeaveCriticalSection(&m_cs);
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, bFuncResult = %s", bFuncResult ? "TRUE":"FALSE");
	return bFuncResult;
}

TPtrRemoteSession CManager::FindSessionById(DWORD dwSessionId) 
{
	ULONG FuncLogArea = LOG_SESSIONS;
	TPtrRemoteSession pFuncResultSession = NULL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER, dwSessionId = %d", dwSessionId);

	EnterCriticalSection(&m_cs);

	std::list <TPtrRemoteSession>::iterator SessionItem;

	for (SessionItem = m_SessionsList.begin(); SessionItem != m_SessionsList.end (); ++SessionItem) 
	{
		if ((*SessionItem)->GetSessionId() == dwSessionId) 
		{
			pFuncResultSession = *SessionItem;
			break;
		}
	}

	LeaveCriticalSection(&m_cs);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, pFuncResultSession  = 0x%.8X", pFuncResultSession);
	return pFuncResultSession;
}

BOOL CManager::IsSessionInList(TPtrRemoteSession pSession) 
{
	ULONG FuncLogArea = LOG_SESSIONS;
	BOOL bFuncResult = FALSE;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER, pSession = 0x%.8X", pSession);

	EnterCriticalSection(&m_cs);

	std::list <TPtrRemoteSession>::iterator SessionItem;

	for (SessionItem = m_SessionsList.begin(); SessionItem != m_SessionsList.end (); ++SessionItem) 
	{
		if ((*SessionItem) == pSession) 
		{
			bFuncResult = TRUE;
			break;
		}
	}

	LeaveCriticalSection(&m_cs);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, bFuncResult = %s", bFuncResult ? "TRUE":"FALSE");
	return bFuncResult;
}

BOOL CManager::RemoveSession(DWORD dwSessionId) 
{
	ULONG FuncLogArea = LOG_SESSIONS;
	BOOL bFuncResult = FALSE;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER, dwSessionId = %d", dwSessionId);

	EnterCriticalSection(&m_cs);

	std::list <TPtrRemoteSession>::iterator SessionItem;

	for (SessionItem = m_SessionsList.begin(); SessionItem != m_SessionsList.end (); ++SessionItem) 
	{
		if ((*SessionItem)->GetSessionId() == dwSessionId) 
		{
			TPtrRemoteSession pSession = *SessionItem;
			// delete sessiob from list
			m_SessionsList.erase(SessionItem);
			LeaveCriticalSection(&m_cs);

			// stop session
			pSession->Stop();
			bFuncResult = TRUE;

			EnterCriticalSection(&m_cs);
			break;
		}
	}

	LeaveCriticalSection(&m_cs);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, bFuncResult = %s", bFuncResult ? "TRUE":"FALSE");
	return bFuncResult;
}

bool CManager::SetAutoconnect(CUngNetSettings	&settings)
{
	bool bFuncResult = true;

	CRegistry reg (Consts::szRegistry_usb4rdp);
	reg.SetBool(Consts::szAutoConnectParam, settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM]) ? true : false);

	return bFuncResult;
}

bool CManager::SetRdpIsolation(CUngNetSettings	&settings, CUngNetSettings &answer)
{
	bool bFuncResult = true;

	CRegistry reg (Consts::szRegistry_usb4rdp);
	reg.SetBool(Consts::szIsolation, settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM]) ? true : false);

	return bFuncResult;
}

bool CManager::AddRDCConnection(CUngNetSettings	 &settings)
{
	bool bFuncResult = false;
	ULONG FuncLogArea = LOG_RDC;

	int iPos = 0;

	DWORD dwRemoteClientProcessId = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM]);

	if(FindRDCConnectionById(dwRemoteClientProcessId)) 
	{
		LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Connection with ID = %d already exist.", dwRemoteClientProcessId);
	}
	else 
	{
		EnterCriticalSection(&m_CriticalSectionForOutgoingRDCConnectionsList);

		TPtrOutgoingConnection pNewRDCConnection = new COutgoingRDCConnection(dwRemoteClientProcessId, this);

		if(pNewRDCConnection)	 
		{
			LogString(FuncLogArea, LogInfo, __FUNCTION__"(): RDC Connection object created successful (0x%.8X)", pNewRDCConnection);
			m_OutgoingRDCConnectionsList.push_back(pNewRDCConnection);
			pNewRDCConnection->StartConnection();
			bFuncResult = true;
		}
		else 
		{
			LogString(FuncLogArea, LogError, __FUNCTION__"(): Cannot create RDC Connection object!");
		}

		LeaveCriticalSection(&m_CriticalSectionForOutgoingRDCConnectionsList);
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, bFuncResult = %s", bFuncResult ? "true":"false");
	return bFuncResult;
}

TPtrOutgoingConnection CManager::FindRDCConnectionById(DWORD dwId)
{
	ULONG FuncLogArea = LOG_RDC;
	TPtrOutgoingConnection pFuncResultRDCConnection = NULL;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER, dwId = %d", dwId);

	EnterCriticalSection(&m_CriticalSectionForOutgoingRDCConnectionsList);

	std::list <TPtrOutgoingConnection>::iterator RDCConnectionItem;

	for (RDCConnectionItem = m_OutgoingRDCConnectionsList.begin(); RDCConnectionItem != m_OutgoingRDCConnectionsList.end (); ++RDCConnectionItem) 
	{
		if ((*RDCConnectionItem)->GetRDCConnectionId() == dwId) 
		{
			pFuncResultRDCConnection = *RDCConnectionItem;
			break;
		}
	}

	LeaveCriticalSection(&m_CriticalSectionForOutgoingRDCConnectionsList);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, pFuncResultSession  = 0x%.8X", pFuncResultRDCConnection);
	return pFuncResultRDCConnection;
}

bool CManager::RemoveRDCConnection(CUngNetSettings	&settings)
{
	ULONG FuncLogArea = LOG_RDC;
	bool bFuncResult = false;

	DWORD dwRemoteClientProcessId = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_PARAM]);

	if(FindRDCConnectionById(dwRemoteClientProcessId)) 
	{
		LogString(FuncLogArea, LogInfo, __FUNCTION__"(): Connection with ID = %d find successful. Removing...", dwRemoteClientProcessId);
		bFuncResult = RemoveRDCConnection(dwRemoteClientProcessId);
	}

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, bFuncResult = %s", bFuncResult ? "true":"false");
	return bFuncResult;
}

bool CManager::RemoveRDCConnection(DWORD dwId)
{
	ULONG FuncLogArea = LOG_RDC;
	bool bFuncResult = false;
	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER, dwId = %d", dwId);

	EnterCriticalSection(&m_CriticalSectionForOutgoingRDCConnectionsList);

	std::list <TPtrOutgoingConnection>::iterator RDCConnectionItem;

	for (RDCConnectionItem = m_OutgoingRDCConnectionsList.begin(); RDCConnectionItem != m_OutgoingRDCConnectionsList.end (); ++RDCConnectionItem) 
	{
		if ((*RDCConnectionItem)->GetRDCConnectionId() == dwId) 
		{
			StopAllConnectionServers(dwId);
			(*RDCConnectionItem)->StopConnection();
			m_OutgoingRDCConnectionsList.erase(RDCConnectionItem);
			bFuncResult = true;
			break;
		}
	}

	LeaveCriticalSection(&m_CriticalSectionForOutgoingRDCConnectionsList);

	LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT, bFuncResult = %s", bFuncResult ? "true":"false");
	return bFuncResult;
}

void CManager::StopAllConnectionServers(DWORD dwConnectionId)
{
	TListServer::iterator item;

	EnterCriticalSection (&m_cs);

	for (item = m_arrServer.begin (); item != m_arrServer.end (); ++item)
	{
		Transport::TPtrTransport pTr = (*item)->GetTrasport();
		if (pTr && pTr->IsRdpConnected ())
		{
			Transport::CRdpServer *pRdp = dynamic_cast <Transport::CRdpServer*> (pTr.get());

			if (pRdp->GetRdpConnection ()->GetRDCConnectionId() == dwConnectionId)
			{
				pTr->Disconnect ();
			}
		}
	}

	LeaveCriticalSection (&m_cs);
}

void CManager::UpdateUsbTree ()
{
	CUsbDevInfo temp;

	CUsbDevInfo::EnumerateUsbTree (temp);

	EnterCriticalSection (&m_cs);
	++m_iCountUpdateUsb;
	m_devsUsb = temp;
	LeaveCriticalSection (&m_cs);
}

CString CManager::GetUsbLoc(const CUngNetSettings &settings) const
{
	const CString hubName =	settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME]);
	const int usbPort = settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT]);

	CString strUsbLoc;

	EnterCriticalSection (&m_cs);
	const CUsbDevInfo *pInfo = m_devsUsb.FindDevice (hubName, usbPort);
	if (pInfo)
		strUsbLoc = pInfo->m_strUsbLoc;
	LeaveCriticalSection (&m_cs);

	return strUsbLoc;
}

bool CManager::Start (USHORT uTcpPort, USHORT uTcpUdp)
{
	DWORD FuncLogArea = LOG_INIT;
	bool bFuncResult = false;

	m_Log.LogString(FuncLogArea, LogTrace, "");
	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): ENTER");

	do
	{
		if (!StartServer (Consts::uTcpControl))
		{
			AddLogFile (Consts::szpLogMes [Consts::LM_ERR_5475]);
			break;
		}

		// Note: there used to be call to LoadConfig,
		// but now we should call LoadConfig only after activator is properly check and update its status
		// (and probably makes re-activation). See onActivatorUpdate. See FS#22352

		CRemoteManager::Start (uTcpPort);
		m_uUdpPort = uTcpUdp;
		CloseHandle (CreateThread (NULL, 0, ThreadUdp, this, 0, NULL));

		bFuncResult = true;
	} 
	while (false);

	m_Log.LogString(FuncLogArea, LogTrace, __FUNCTION__"(): EXIT - bFuncResult = \"%s\"", bFuncResult?"true":"false");
    return bFuncResult;
}
