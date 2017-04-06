/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RdpServer.cpp

Abstract:

	RDP connection transport.

Environment:

    User mode

--*/

#include "stdafx.h"
#include "RdpServer.h"
#include "..\Manager.h"

namespace Transport
{

CRdpServer::CRdpServer (TPtrOutgoingConnection pConn)
	: m_pConnection (pConn)
{
	m_uLocallIp = CManager::GetLocalIP();
	m_uIndef = ULONG (GetTcpPort()) | 0x80000000;
	m_bufCommand.SetReserv(sizeof (DATA_PACKET));
}

bool CRdpServer::SetSettings (const CUngNetSettings &settings)
{
	Transport::CTransport::SetSettings(settings);
	m_uLocallIp = CManager::GetLocalIP();
	m_uIndef = ULONG (GetTcpPort()) | 0x80000000;

	return true;
}

bool CRdpServer::Connect(TPtrEvent pEvent)
{
	SetEvent (pEvent);

	TPtrEvent	pEv = GetEvent();
	if (pEv)
	{
		SetStatus (ITransport::connecting);

		SendConnectionEstablishedAnswerToClient ();

		SetStatus (ITransport::online);

		pEv->OnConnected(this);

		return true;
	}
	return false;
}

bool CRdpServer::Disconnect () 
{
	TPtrEvent	pEv = GetEvent();
	if (pEv)
	{
		pEv->OnError(ERROR_CONNECTION_INVALID);
	}

	SetStatus (ITransport::offline);

	return SendManualDisconnectToClient (true);
}

bool CRdpServer::SendAnswerToUsbClient(PCHAR pAnswer)
{
	CAutoLock	lock (&m_lock);
	HANDLE		hDll = m_pConnection->GetDataPipeHandle();
	HANDLE		hMutex = m_pConnection->GetSendDataPipeMutexHandle();


	m_bufCommand.SetSize (0);

	if (hDll != INVALID_HANDLE_VALUE && hMutex != INVALID_HANDLE_VALUE)
	{
		m_bufCommand.AddArray((LPBYTE)pAnswer, (int) strlen(pAnswer));
		m_bufCommand.AddArray((LPBYTE)&m_uIndef, sizeof(m_uIndef));
		m_bufCommand.AddArray((LPBYTE)&m_uLocallIp, sizeof(m_uLocallIp));
		return m_pConnection->SendToFile(hMutex, (PCHAR)m_bufCommand.GetData(), m_bufCommand.GetCount())?true:false;
	}
	
	return false;
}

bool CRdpServer::SendConnectionEstablishedAnswerToClient()
{
	return SendAnswerToUsbClient(USB4RDP_CONNECT_ESTABLISHED_ANSWER);
}

bool CRdpServer::SendManualDisconnectToClient(bool bStop)
{
	CStringA str;
	str.Format ("%s%s", USB4RDP_MANUAL_DISCONNECT_COMMAND, bStop?"1":"0");
	return SendAnswerToUsbClient(str.GetBuffer ());
}

int CRdpServer::Write (LPVOID pBuff, int iSize)
{
	CAutoLock	lock (&m_lock);
	m_bufCommand.SetSize(0);
	m_bufCommand.AddArray((LPBYTE)pBuff, iSize);
	HANDLE		hMutex = m_pConnection->GetSendDataPipeMutexHandle();

	if (/*hDll == INVALID_HANDLE_VALUE || */hMutex == INVALID_HANDLE_VALUE)
	{
		throw Transport::CException (WSA_INVALID_HANDLE);
		return 0;
	}
	// send to rdp pipe 
	if (m_pConnection->SendDataToFile(hMutex, m_bufCommand, m_uIndef))
	{
		return iSize;
	}
	DWORD dwLastThreadError = GetLastError();
	SendManualDisconnectToClient(false);

	return 0;
}

bool CRdpServer::OnReadOut (LPVOID lpData, int iSize)
{
	TPtrEvent	pEv = GetEvent();
	if (pEv)
	{
		pEv->OnRead(lpData, iSize);
		return true;
	}
	return false;
}

int CRdpServer::Read (LPVOID pBuff, int iSize)
{
	throw Transport::CException (ERROR_NO_DATA);
}

CString CRdpServer::GetRemoteHost () const
{
	if (GetStatus () == ITransport::online)
		return m_strRemoteHostName;
	else
		return _T("");
}

} // namespace Transport
