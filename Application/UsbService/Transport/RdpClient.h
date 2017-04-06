/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RdpClient.h

Abstract:

	RDP connection transport.

Environment:

    User mode

--*/

#ifndef RDP_CLIENT_TRASPORT_H
#define RDP_CLIENT_TRASPORT_H

#include "ITransport.h"
#include "Consts/Lock.h"
#include "rdp_files/RemoteSession.h"
#include "Consts/Buffer.h"

namespace Transport
{

class CRdpClient;
class CRdpClient : public CTransport
{
public:
	CRdpClient (TPtrRemoteSession pSession, const char *strRemoteHost);
	~CRdpClient ();

public:
	// event
	virtual bool Connect(TPtrEvent pEvent);
	virtual bool Disconnect ();

	// io
	virtual int Read (LPVOID pBuff, int iSize);
	virtual int Write (LPVOID pBuff, int iSize) ;

	// options
	virtual bool HasCompressSupport () const {return false;}
	virtual bool AuthSupport () const {return false;}
	virtual bool CryptSupport () const {return false;}

	virtual ULONG GetIpIndex () const {return m_uLocalIp;};
	virtual bool IsRdpConnected () const {return true;}
	virtual CString GetRemoteHost () const {return m_strRemoteHost;}

	virtual HANDLE GetHandle () const {return m_pSession->GetHandle();}

	ULONG GetId () {return m_settings.GetParamInt(Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);}

	virtual CString GetUserDosDevice () const {return m_pSession->GetUserDosDevice ();}
	virtual CString GetUserSid () const {return m_pSession->GetUserSid ();}
	virtual CBuffer GetSid () const {return m_pSession->GetSid ();}

public:
	TPtrRemoteSession GetSession () {return m_pSession;}

	bool EstablishConnection (ULONG iIp);
	bool AbortEstablishConnection ();

	bool OnReadOut (LPVOID pBuff, int iSize);

	static CRdpClient* GetRdp (ITransport *pBase);

#ifdef DEBUG
	virtual DWORD WaitMax () {return -1;}
#endif

protected:
	bool SendCommandToUsbServer(PCHAR pCommand/*, bool bWaitAnswer*/);
	bool RequestConnect ();

protected:
	CLock		m_lock;
	CBuffer		m_Buffer;
	ULONG		m_uLocalIp;
	HANDLE		m_hWait;
	CString		m_strRemoteHost;

private:
	TPtrRemoteSession m_pSession;
};


typedef boost::intrusive_ptr <CRdpClient> TPtrRdpClient;

} // namespace Transport

#endif
