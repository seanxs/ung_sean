/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Tcp.h

Abstract:

	TCP socket client/server.

Environment:

    User mode

--*/

#ifndef TCP_TRASPORT_H
#define TCP_TRASPORT_H

#include <WinSock2.h>
#include "ITransport.h"
#include "Consts/Buffer.h"
#include "Consts/consts.h"

namespace Transport
{
class CTcp : public CTransport
{
public:
	CTcp (const CUngNetSettings &settings) ;
	virtual ~CTcp () ;

public:
	virtual bool Connect(TPtrEvent pEvent);
	virtual bool Disconnect ();

	// io
	virtual int Read (LPVOID pBuff, int iSize);
	virtual int Write (LPVOID pBuff, int iSize);

	// options
	virtual bool HasCompressSupport () const {return true;}
	virtual bool AuthSupport () const {return true;}
	virtual bool CryptSupport () const {return true;}

	virtual ULONG GetIpIndex () const {return m_uIndef;}
	virtual bool IsRdpConnected () const {return false;}

	virtual HANDLE GetHandle () const {return (HANDLE)m_sock;}

	CString GetRemoteHost () const {return m_strRemoteHostName;}

	// only rdp
	virtual CString GetUserDosDevice () const {return CString ();}
	virtual CString GetUserSid () const {return CString ();}
	virtual CBuffer GetSid () const {return CBuffer ();}

protected:
	static DWORD WINAPI stThreadWait (LPVOID lpParam);
	void ThreadWait ();

	bool Connect ();
	bool ClientConnect ();
	bool ServerConnect ();
	static SOCKET Listen (u_short uPort);
	void WaitRecv ();

	bool IsTcpSettings ();

	bool SetKeepAlive (SOCKET s, long lKeepAlive, long lRepeat);

private:
	SOCKET		m_sock;
	SOCKET		m_Listen;
	CBuffer		m_bufRead;
	CBuffer		m_bufWrite;
	HANDLE		m_hWait;
	DWORD		m_dwWaitId;
	ULONG		m_uIndef;
	CString		m_strRemoteHostName;
};

typedef boost::intrusive_ptr <CTcp> TPtrTcp;

} // namespace Transport

#endif
