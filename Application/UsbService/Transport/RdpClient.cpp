/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RdpClient.cpp

Abstract:

	RDP connection transport.

Environment:

    User mode

--*/

#include "stdafx.h"
#include "RdpClient.h"
#include "..\Manager.h"

namespace Transport
{

CRdpClient::CRdpClient (TPtrRemoteSession pSession, const char *strRemoteHost)
	: m_pSession (pSession)
{
	m_uLocalIp = CManager::GetLocalIP();
	m_Buffer.SetReserv (sizeof (DATA_PACKET));
	if (strRemoteHost)
		m_strRemoteHost = CString (strRemoteHost);
}

CRdpClient::~CRdpClient ()
{
	SetStatus(offline);
}

bool CRdpClient::RequestConnect ()
{
	return SendCommandToUsbServer(USB4RDP_CONNECT_REQUEST);
}

bool CRdpClient::SendCommandToUsbServer(PCHAR pCommand)
{
	CAutoLock	lock (&m_lock);
	ULONG		id = GetId();
	m_Buffer.SetSize(0);

	m_Buffer.AddArray((LPBYTE)pCommand, (int)strlen(pCommand));
	m_Buffer.AddArray((LPBYTE)&id, sizeof (ULONG));
	m_Buffer.AddArray((LPBYTE)&m_uLocalIp, sizeof (ULONG));

	if (!GetSession()->SendRequest((PCHAR)m_Buffer.GetData(), m_Buffer.GetCount()))
	{
		return false;
	}
	return true;
}

bool CRdpClient::Connect(TPtrEvent pEvent)
{
	SetEvent(pEvent);

	SetStatus(connecting);
	RequestConnect();

	return true;
}

bool CRdpClient::Disconnect ()
{
	//m_strRemoteHost.Empty();
	SetStatus(offline);

	SendCommandToUsbServer (USB4RDP_DISCONNECT_REQUEST);

	return true;
}

bool CRdpClient::EstablishConnection (ULONG iIp)
{
	if (GetStatus() == connecting)
	{
		SetStatus (ITransport::online);

		TPtrEvent pEv = GetEvent();
		if (pEv)
		{
			pEv->OnConnected(this);
		}

		const CString strIp = CString (inet_ntoa (*((in_addr*)&iIp)));
		const CString strHostname = CSystem::GetHostName ((in_addr*)&iIp, sizeof (ULONG));
		if (strIp != strHostname)
		{
			m_strRemoteHost.Format (_T("%s (%s)"), strIp, strHostname);
		}
		else
		{
			m_strRemoteHost = strIp.IsEmpty() ? strHostname : strIp;
		}
	}

	return true;
}

bool CRdpClient::AbortEstablishConnection ()
{
	if (GetStatus() == connecting)
	{
		RequestConnect();
	}

	return true;
}

CRdpClient* CRdpClient::GetRdp (ITransport *pBase)
{
	return dynamic_cast <CRdpClient*> (pBase);
}

bool CRdpClient::OnReadOut (LPVOID pBuff, int iSize)
{
	TPtrEvent pEv = GetEvent();
	if (pEv)
	{
		pEv->OnRead(pBuff, iSize);
		return true;
	}
	return false;
}

int CRdpClient::Read (LPVOID pBuff, int iSize)
{
	throw Transport::CException (ERROR_NO_DATA);
};

int CRdpClient::Write (LPVOID pBuff, int iSize)
{
	CAutoLock	lock (&m_lock);

	HANDLE		hMutex = GetSession ()->GetMutex();

	m_Buffer.SetSize(iSize);
	RtlCopyMemory (m_Buffer.GetData(), pBuff, iSize);

	if (GetSession()->SendDataToFile (hMutex, m_Buffer, GetId()))
	{
		return iSize;
	}
	return 0;
}

} // namespace Transport
