/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    RdpServer.h

Abstract:

	RDP connection transport.

Environment:

    User mode

--*/

#ifndef RDP_SERVER_TRASPORT_H
#define RDP_SERVER_TRASPORT_H

#include "ITransport.h"
#include "rdp_files\OutgoingRDCConnection.h"
#include "Consts\Buffer.h"
#include "Consts\Lock.h"

namespace Transport
{
class CRdpServer : public CTransport
{
public:
	CRdpServer (TPtrOutgoingConnection pConn);
	~CRdpServer () {}

public:
	// event
	virtual bool Connect(TPtrEvent pEvent);
	virtual bool Disconnect ();

	// io
	virtual int Read (LPVOID pBuff, int iSize);
	virtual int Write (LPVOID pBuff, int iSize);

	// options
	virtual bool HasCompressSupport () const {return false;}
	virtual bool AuthSupport () const {return false;}
	virtual bool CryptSupport () const {return false;}

	virtual ULONG GetIpIndex () const {return m_uLocallIp;}
	virtual bool IsRdpConnected () const {return true;}
	virtual CString GetRemoteHost () const;
	void SetRemoteHost (CString strName) {m_strRemoteHostName = strName;}

	virtual HANDLE GetHandle () const {return m_pConnection->GetDataPipeHandle();}

	bool OnReadOut (LPVOID lpData, int iSize);

	virtual bool SetSettings (const CUngNetSettings &settings);

	TPtrOutgoingConnection GetRdpConnection () {return m_pConnection;}

	virtual CString GetUserDosDevice () const {return CString ();}
	virtual CString GetUserSid () const {return CString ();}
	virtual CBuffer GetSid () const {return CBuffer ();}

protected:
	bool SendAnswerToUsbClient(PCHAR pAnswer);
	bool SendConnectionEstablishedAnswerToClient();
	bool SendManualDisconnectToClient(bool bStop);

#ifdef DEBUG
	virtual DWORD WaitMax () {return -1;}
#endif


protected:
	TPtrOutgoingConnection		m_pConnection;
	CString						m_strRemoteHostName;
	CBuffer						m_bufCommand;
	CLock						m_lock;
	ULONG						m_uLocallIp;
	ULONG						m_uIndef;
};

// fake rdp server required for disconnect procedure: we need to ask GetRemoteHost value from some object
class CFakeRdpServer: public CRdpServer
{
public:
	CFakeRdpServer (const CString &strRemoteHost): CRdpServer (NULL)
	{
		SetRemoteHost (strRemoteHost);
	}

	// event
	virtual bool Connect(TPtrEvent pEvent) {return false;}
	virtual bool Disconnect () {return false;}

	// io
	virtual int Read (LPVOID pBuff, int iSize) {return 0;}
	virtual int Write (LPVOID pBuff, int iSize) {return 0;}

	virtual HANDLE GetHandle () const {return INVALID_HANDLE_VALUE;}

	virtual CString GetRemoteHost () const {return m_strRemoteHostName;}
};

typedef boost::intrusive_ptr <CRdpServer> TPtrRdpServer;

} // namespace Transport

#endif
