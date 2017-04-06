/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    ITransport.h

Abstract:

	Interface for transport classes.

Environment:

    User mode

--*/

#ifndef ITRANSPORT_H
#define ITRANSPORT_H

#include <Winsock2.h>
#include <boost/intrusive_ptr.hpp>
#include "Consts/System.h"
#include "Consts/RefCounter.h"
#include "Consts/consts.h"
#include "Utils/Security.h"
#include "Consts/Buffer.h"
#include "Consts/lock.h"

namespace Transport
{
enum {  // uint16_t, flags
	HANDSHAKE_AUTH			= 0x001,
	HANDSHAKE_COMPR_ZLIB	= 0x002,
	HANDSHAKE_COMPR_LZ4		= 0x004,
	HANDSHAKE_ENCR			= 0x100,

	HANDSHAKE_COMPR = (HANDSHAKE_COMPR_ZLIB | HANDSHAKE_COMPR_LZ4),
	HANDSHAKE_ALL	= (HANDSHAKE_AUTH | HANDSHAKE_COMPR | HANDSHAKE_ENCR),
};

enum TypeTcpPackage
{
	IrpToTcp,
	IrpAnswerTcp,
	ControlPing,
	ManualDisconnect,
	DeviceId,
	TYPE_PeerName = 0x40,
	TYPE_CompressedFlag = 1 << 31, // | TYPE_XXX
};

typedef struct _TCP_PACKAGE
{
	LONG	Type;
	LONG	SizeBuffer;
}TCP_PACKAGE,*PTCP_PACKAGE ;

class ITransport;
typedef boost::intrusive_ptr <ITransport> TPtrTransport;

class CException : public std::exception
{
public:
	CException () : m_iError (0) {}
	CException (CString strErr)  : m_strError (strErr), m_iError (WSAESHUTDOWN) {}
	CException (int iError) : m_iError (iError)
	{
		m_strError = CSystem::ErrorToString(m_iError);
	}

	virtual ~CException () {}

	CString GetError () const {return m_strError;}
	int GetErrorInt () const {return m_iError;}

private:
	CString m_strError;
	int		m_iError;
};

class IEvent : public virtual Common::CRefCounter
{
public:
	IEvent () {}
	virtual ~IEvent (){}

public:
	virtual void OnConnected (TPtrTransport pConnection) = 0;
	virtual void OnRead (LPVOID lpData, int iData) = 0;
	virtual void OnError (int iError) = 0;

	//virtual int GetCompressionType () const = 0;
	virtual CSecurity* GetSecuritySettings () = 0;
};

typedef boost::intrusive_ptr <IEvent> TPtrEvent;

class ITransport : public Common::CRefCounter
{
public:
	ITransport () {}
	virtual ~ITransport () {}

	enum Status
	{
		offline,
		connecting,
		online,
	};

public:
	// event
	virtual bool Connect(TPtrEvent pEvent) = 0;
	virtual bool Disconnect () = 0;
	virtual void ClearEvent () = 0;
	virtual bool SetEvent(TPtrEvent pEvent) = 0;

	// io
	virtual int Read (LPVOID pBuff, int iSize) = 0;
	virtual int Write (LPVOID pBuff, int iSize) = 0;

	// options
	virtual bool HasCompressSupport () const = 0;
	virtual bool AuthSupport () const = 0;
	virtual bool CryptSupport () const = 0;
	virtual Status GetStatus () const = 0;
	virtual bool SetStatus (Status New) = 0;
	virtual bool SetSettings (const CUngNetSettings &settings) = 0;

	virtual ULONG GetIpIndex () const = 0;
	virtual bool IsRdpConnected () const = 0;
	virtual CString GetRemoteHost () const = 0;

	virtual HANDLE GetHandle () const = 0;

	virtual CString GetHostName () const = 0;
	virtual int GetTcpPort ()  const = 0;

	virtual CString GetUserDosDevice () const = 0;
	virtual CString GetUserSid () const = 0;
	virtual CBuffer GetSid () const = 0;

protected:
	virtual DWORD WaitMax () const = 0;
	virtual TPtrEvent GetEvent() const = 0;
};

class CTransport : public ITransport
{
public:
	CTransport () : m_state (offline), m_pEvent (NULL) {}
	~CTransport () {}

	virtual Status GetStatus () const {return m_state;}
	virtual bool SetStatus (Status New) {m_state = New;return true;}
	virtual bool SetSettings (const CUngNetSettings &settings) {m_settings = settings;return true;}
	virtual void ClearEvent() { SetEvent(NULL); }
	virtual bool SetEvent(TPtrEvent pEvent) { CAutoLock lock(&m_lock); m_pEvent = pEvent; return true; }

	virtual CString GetHostName () const {return m_settings.GetParamStr(Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);}
	virtual int GetTcpPort () const {return m_settings.GetParamInt(Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);}

	// for debugging: return status as string.
	const char* GetStatusStr() const;

protected:
	DWORD WaitMax () const {return 1000;}

	TPtrEvent GetEvent() const { CAutoLock lock(&m_lock); return m_pEvent; }

protected:
	Status			m_state;
	CUngNetSettings	m_settings;
	mutable CLock			m_lock;
private:
	TPtrEvent		m_pEvent;
};

} // namespace Transport

#endif
