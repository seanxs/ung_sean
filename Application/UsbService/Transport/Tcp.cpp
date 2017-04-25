/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Tcp.cpp

Abstract:

	TCP socket client/server.

Environment:

    User mode

--*/

#include "stdafx.h"
#include <mstcpip.h>
#include "Tcp.h"

namespace Transport
{

CTcp::CTcp (const CUngNetSettings &settings)
	: m_sock (INVALID_SOCKET)
	, m_Listen (INVALID_SOCKET)
	, m_hWait (INVALID_HANDLE_VALUE)
	, m_uIndef (0)
	, m_dwWaitId (0)
{
	SetSettings (settings);
}

CTcp::~CTcp ()
{
	switch (GetStatus())
	{
	case connecting:
	case online:
		Disconnect();
		break;
	}
}

int CTcp::Read (LPVOID pBuff, int iSize)
{
	CSecurity	*pSec;
	int iRead = 0;
	int iReadTotal = 0;

	if (Status () == online || m_sock == INVALID_SOCKET)
	{
		throw Transport::CException (ERROR_DEV_NOT_EXIST);
	}

	while (iReadTotal < iSize)
	{
		iRead = recv (m_sock, ((char*)pBuff) + iReadTotal, iSize - iReadTotal, 0);
		if (iRead > 0)
		{
			iReadTotal += iRead;
			iRead = 0;
		}
		else
		{
			throw Transport::CException (WSAGetLastError());
		}
	}

	// get security settings
	{
		TPtrEvent pEvent = GetEvent();
		if (pEvent)
		{
			pSec = pEvent->GetSecuritySettings();
		}
	}

	if (pSec && pSec->GetIsCrypt())
	{
		if (!pSec->m_pContext)
		{
			throw Transport::CException (ERROR_INVALID_SECURITY_DESCR);
		}

		m_bufRead.SetSize(iReadTotal);
		if (pSec->cryptPostRecvData (pSec->m_pContext, (char*)pBuff, iReadTotal, (char*)m_bufRead.GetData(), &iRead))
		{
			throw Transport::CException (ERROR_INVALID_SECURITY_DESCR);
		}
		RtlCopyMemory (pBuff, m_bufRead.GetData(), min (iRead, iSize));
		iReadTotal = min (iRead, iSize);
	}

	return iReadTotal;
}

int CTcp::Write (LPVOID pBuff, int iSize) 
{
	CSecurity	*pSec;
	int			iTotalSent;

	if (GetStatus () != online || m_sock == INVALID_SOCKET)
	{
		throw Transport::CException (ERROR_DEV_NOT_EXIST);
	}

	// get security settings
	{
		TPtrEvent pEvent = GetEvent();
		if (pEvent)
		{
			pSec = pEvent->GetSecuritySettings();
		}
	}

	if (pSec && pSec->GetIsCrypt())
	{
		if (!pSec->m_pContext)
		{
			throw Transport::CException (ERROR_INVALID_SECURITY_DESCR);
		}

		m_bufWrite.SetSize(iSize);
		if (pSec->PreSendData((char*)pBuff, iSize, (char*)m_bufWrite.GetData(), &iTotalSent))
		{
			throw Transport::CException (ERROR_INVALID_SECURITY_DESCR);
		}

		iTotalSent = ::send (m_sock, (char*)m_bufWrite.GetData(), iTotalSent, 0);

		return iTotalSent;
	}

	iTotalSent = send (m_sock, (char*)pBuff, iSize, 0);
	return iTotalSent;
}

bool CTcp::Disconnect ()
{
	switch (GetStatus())
	{
	case connecting:
	case online:
		SetStatus (offline);
		{
			CAutoLock lock (&m_lock);
			if (m_sock != INVALID_SOCKET)
			{
				closesocket(m_sock);
				m_sock = INVALID_SOCKET;
			}

			if (m_Listen != INVALID_SOCKET)
			{
				closesocket(m_Listen);
				m_Listen = INVALID_SOCKET;
			}
		}

		if (m_hWait != INVALID_HANDLE_VALUE)
		{
			if (GetCurrentThreadId () != m_dwWaitId)
			{
				WaitForSingleObject(m_hWait, WaitMax ());
			}
			CloseHandle(m_hWait);
			m_hWait = INVALID_HANDLE_VALUE;
			m_dwWaitId = 0;
		}

		return true;
	}

	return false;
}

bool CTcp::Connect(TPtrEvent pEvent)
{
	SetEvent(pEvent);

	if (GetStatus() == offline && IsTcpSettings ())
	{
		ATLASSERT (m_hWait == INVALID_HANDLE_VALUE);

		SetStatus(connecting);
		m_hWait = CreateThread(NULL, 0, stThreadWait, this, 0, &m_dwWaitId);
		Sleep (0);
		return true;
		
	}
	return false;
}

DWORD WINAPI CTcp::stThreadWait (LPVOID lpParam)
{
	TPtrTcp pTcp ((CTcp*)lpParam);
	pTcp->ThreadWait();

	return 0;
}

void CTcp::ThreadWait ()
{
	if (GetStatus () == connecting)
	{
		if (!Connect ())
		{
			//CAutoLock lock(&m_lock);
			TPtrEvent pEvent = GetEvent();
			if (pEvent)
			{
				pEvent->OnError(ERROR_CONNECTION_INVALID);
			}
			return;
		}

		{
			//CAutoLock lock (&m_lock); 
			TPtrEvent pEvent = GetEvent();
			if (pEvent)
			{
				pEvent->OnConnected (this);
			}
			else
			{
				return;
			}
		}

		if (GetStatus () == online)
		{
			WaitRecv ();			
		}
		else
		{
			ATLTRACE ("GetStatus () != online");
		}
	}
	else
	{
		ATLTRACE ("GetStatus () != connecting");
	}
}

bool CTcp::Connect ()
{
	// connect to server
	if (GetHostName().GetLength())
	{
		return ClientConnect();
	}
	else // listen tcp port
	{
		return ServerConnect();
	}
	return false;
}

bool CTcp::ClientConnect ()
{
	SOCKET socketFuncResult = INVALID_SOCKET;

	LPHOSTENT		hostEntry;
	int				rVal = SOCKET_ERROR;
	SOCKADDR_IN		serverInfo;
	CString			str;
	CStringA		strHost;

	strHost		= CStringA(GetHostName ());
	hostEntry	= (LPHOSTENT)gethostbyname(strHost);

	serverInfo.sin_port = htons(GetTcpPort());
	if (hostEntry)
	{
		m_uIndef = ((LPIN_ADDR)*hostEntry->h_addr_list)->S_un.S_addr;

		serverInfo.sin_family = PF_INET;
		serverInfo.sin_addr = *((LPIN_ADDR)*hostEntry->h_addr_list);

		socketFuncResult = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

		u_long iMode = 1;
		fd_set	fdWrite;
		fd_set	fdError;
		struct timeval Time;

		Time.tv_sec = 0;
		Time.tv_usec = 500;
		ioctlsocket(socketFuncResult, FIONBIO, &iMode); // non-blocked

		FD_ZERO (&fdWrite);
		FD_ZERO (&fdError);

		while (GetStatus () == connecting)
		{
			rVal = connect(socketFuncResult, (LPSOCKADDR)&serverInfo, sizeof(serverInfo));

			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				while (GetStatus () == connecting)
				{
					FD_SET(socketFuncResult, &fdWrite);
					FD_SET(socketFuncResult, &fdError);

					if (select (0, NULL, &fdWrite, &fdError, &Time))
					{
						if (FD_ISSET (socketFuncResult, &fdWrite))
						{
							rVal = 0;
						}
						else
						{
							Sleep (200);
						}
						break;
					}
				}
			}

			if (rVal == SOCKET_ERROR)
			{
				int nError = WSAGetLastError();

				if (nError == WSAENOTSOCK)
				{
					break;
				}
			}
			else
			{
				break;
			}
		}

		iMode = 0;
		ioctlsocket(socketFuncResult, FIONBIO, &iMode); // blocked

		int val = 1;
		if (setsockopt(socketFuncResult, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val)) != SOCKET_ERROR)
		{
			OutputDebugString(_T("TCP_NODELAY succeed on client!\n"));
		}

		if (rVal == SOCKET_ERROR && socketFuncResult != INVALID_SOCKET)
		{
			closesocket (socketFuncResult);
			return false;
		}
	}
	else
	{
		return false;
	}

	SetKeepAlive (socketFuncResult, 10000, 1000);

	{
		TPtrEvent pEvent = GetEvent();
		if (pEvent)
		{
			m_strRemoteHostName = strHost;
			m_sock = socketFuncResult;
			SetStatus(online);
			//pEvent->OnConnected(this);
			return true;
		}
	}

	return false;
}

SOCKET CTcp::Listen (u_short uPort)
{
	int rVal;
	SOCKADDR_IN sin;
	SOCKET	sock;

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock != INVALID_SOCKET)
	{
		sin.sin_family = PF_INET;
		sin.sin_port = htons(uPort);

		sin.sin_addr.s_addr = INADDR_ANY;
		//binding the socket
		rVal = bind(sock, (LPSOCKADDR)&sin, sizeof(sin));
		if (rVal != SOCKET_ERROR)
		{
			rVal = listen(sock, 1);
			if (rVal != SOCKET_ERROR)
			{
				int val = 1;
				if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val)) != SOCKET_ERROR)
				{
					OutputDebugString(_T("TCP_NODELAY succeed on server!\n"));
				}
				return sock;
			}
		}
		closesocket (sock);
	}

	return INVALID_SOCKET;
}

bool CTcp::ServerConnect ()
{
	//SOCKET			sockListen = INVALID_SOCKET;
	SOCKET			sockAccept = INVALID_SOCKET;
	SOCKADDR_IN		serverInfo;
	int				iSize = sizeof (SOCKADDR_IN);

	{
		CAutoLock lock (&m_lock);
		m_Listen = Listen (GetTcpPort());
	}

	if (m_Listen == INVALID_SOCKET)
	{
		return false;
	}
	else 
	{
		// waiting for incoming connections
		sockAccept = accept (m_Listen, (sockaddr*)&serverInfo, &iSize);
		{
			CAutoLock lock (&m_lock);
			closesocket (m_Listen);
			m_Listen = INVALID_SOCKET;
		}

		// connect
		if (sockAccept == INVALID_SOCKET)
		{
			return false;
		}
		else {
			// get host 
			struct hostent* hs = gethostbyaddr((const char*)&serverInfo.sin_addr.S_un.S_addr, 4, AF_INET);
			if (hs)
			{
				m_strRemoteHostName = CString (hs->h_name);
			}
			else
			{
				m_strRemoteHostName = CString (inet_ntoa (serverInfo.sin_addr));
			}
		}
	}

	SetKeepAlive (sockAccept, 10000, 1000);
	int val = 1;
	if (setsockopt(sockAccept, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val)) != SOCKET_ERROR)
	{
		OutputDebugString(_T("TCP_NODELAY succeed on server!\n"));
	}

	TPtrEvent	pEvent = GetEvent();
	if (pEvent)
	{
		CAutoLock lock (&m_lock);
		//m_strRemoteHostName = strHost;
		m_sock = sockAccept;
		SetStatus(online);
		//pEvent->OnConnected(this);
		return true;	
	}

	if (sockAccept != INVALID_SOCKET)
	{
		closesocket(sockAccept);
	}

	return false;
}

void CTcp::WaitRecv()
{
	fd_set	fdRead;
	struct timeval Time;

	Time.tv_sec = 0;
	Time.tv_usec = 100;

	FD_ZERO (&fdRead);

	try
	{
		while (GetStatus() == online)
		{
			FD_SET (m_sock, &fdRead);
			if (select (0, &fdRead, NULL, NULL, &Time))
			{
				TPtrEvent pEvent = GetEvent();
				if (pEvent)
				{
					pEvent->OnRead(NULL, 0);
				}
			}
		}
	}
	catch (Transport::CException &pEx)
	{
		(void)pEx;
		ATLTRACE ("%s\n", pEx.GetError());
		TPtrEvent pEvent = GetEvent();
		if (pEvent)
		{
			pEvent->OnError(WSAGetLastError());
		}
	}
}

bool CTcp::IsTcpSettings ()
{
	if (m_settings.ExistParam(Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]) || 
		m_settings.ExistParam(Consts::szUngNetSettings [Consts::UNS_TCP_PORT]))
	{
		return true;
	}
	return false;
}

bool CTcp::SetKeepAlive(SOCKET s, long lKeepAlive, long lRepeat)
{
	struct tcp_keepalive alive;
	DWORD dwRet = 0, dwSize = 0;

	alive.onoff = 1;
	alive.keepalivetime = lKeepAlive;
	alive.keepaliveinterval = lRepeat;

	dwRet = WSAIoctl(s, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, reinterpret_cast<DWORD*>(&dwSize), NULL, NULL);

	return (dwRet == 0);
}

//-----------------------------------------------------------------------------

const char* CTransport::GetStatusStr() const
{
	switch (m_state)
	{
	case offline:		return "offline";
	case connecting:	return "connecting";
	case online:		return "online";
	default:			return "<???>";
	}
}

} // namespace Transport
