/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   OutgoingRDCConnection.h

Environment:

    User mode

--*/

#pragma once

#include <atlstr.h>
#include <setupapi.h>
#include "common\filetransport.h"
#include "RemoteSession.h"
#include <vector>
#include "..\..\Consts\log.h"
#include "..\..\Consts\consts.h"
#include "..\..\Consts\Buffer.h"

class COutgoingRDCConnection: public CFileTransport, public Common::CRefCounter
{
public:
	COutgoingRDCConnection(DWORD dwId, LPVOID pManager);
	~COutgoingRDCConnection();
	DWORD GetRDCConnectionId();
	BOOL StartConnection();
	void StopConnection();
	HANDLE GetDataPipeHandle();
	HANDLE GetSendDataPipeMutexHandle();

	bool SendServerCommand (ULONG dwId, PCHAR pAnswer);
protected:
	BOOL AnalyseRequest(CBuffer &Buffer, CBuffer &Analyze, OVERLAPPED *pOverlapped);
	BOOL DeliverDataToSpecificServer(PCHAR pDataBuffer, DWORD dwDataSize, OVERLAPPED *pOverlapped);
	BOOL StartSpecificRDPServer(PCHAR pDataBuffer, DWORD dwDataSize);
	BOOL StopSpecificRDPServer(PCHAR pDataBuffer, DWORD dwDataSize);
	BOOL BuildSharesListPacket(CBuffer &Buffer);

private:
	CLog m_Log;
	DWORD m_dwID;
	HANDLE m_hDataPipe;
	HANDLE m_hSendDataPipeMutex;
	LPVOID m_pManager;
	CRITICAL_SECTION m_csRead;

	std::vector <HANDLE>	m_arrThredReadFromRdcDll;

	static DWORD WINAPI ThreadReadFromRDCDLL(LPVOID lParam);
	DWORD ThreadReadFromRDCDLLRoutine();

	BOOL GetRequest(CBuffer	&pBuffer);
	BOOL SendAnswer(CBuffer	&pBuffer);

	virtual HANDLE GetHandle () const {return m_hDataPipe;}
};

typedef boost::intrusive_ptr <COutgoingRDCConnection> TPtrOutgoingConnection;