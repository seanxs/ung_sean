/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   RemoteSession.h

Environment:

    User mode

--*/

#pragma once
class CUsbClient;

#include <list>
#include <WtsApi32.h>
#include "Common\threadwrapper.h"
#include "common\FileTransport.h"
#include <boost/intrusive_ptr.hpp>
#include "Consts\Buffer.h"
#include "Consts\UngNetSettings.h"
#include "Consts\RefCounter.h"
#include <vector>
#include "Usb\UsbClient.h"

class CRemoteSession: public CLog, public CFileTransport, public Common::CRefCounter
{
	friend class CManager;
	
	HANDLE m_hStartThreadHandle;
	DWORD  m_dwStartThreadId;

	HANDLE m_hWorkedEvent;
	CRITICAL_SECTION m_EventCriticalSection;

	// list rdp client
	typedef std::list <TPtrUsbClient> TListRdpClients;
	TListRdpClients m_ClientsList;
	
	CRITICAL_SECTION m_ClientsListCriticalSection;

	BOOL AddClient(CUngNetSettings &settings, DWORD dwID, ULONG uRemoteShareAddr);
	void DeleteClient (DWORD dwID);
	void ClearClientsList();

	// unic id
	DWORD m_dwSessionId;

	CString m_strUserDosDevice;
	CString m_strUserSid;
	CBuffer	m_sid;

public:
	virtual HANDLE GetMutex () {return m_hSendChannelMutex;}
	virtual HANDLE GetHandle () const {return m_hChannelFile;}

private:
	CManager *m_pManager;

	HANDLE m_hChannel;
	HANDLE m_hChannelFile;

	HANDLE m_hSendChannelMutex;


	BOOL CreateVirtualChannel(PCHAR ChannelName, PHANDLE phChannel, PHANDLE phFile);
	void CloseVirtualChannel(PHANDLE phChannel, PHANDLE phFile);
	
public:
	BOOL SendRequest(PCHAR pRequest, DWORD dwRequestSize);
private:
	BOOL ReceiveAnswer(CBuffer &pBuffer);
	BOOL AnalyseAnswer(const CBuffer &pBuffer);
	BOOL ParseSharesString(PCHAR pSharesString, DWORD dwAnswerSize);
	BOOL DeliverDataToSpecificClient(PCHAR pDataBuffer, DWORD dwDataSize);
	BOOL DeliverConnectEstablishedAnswerToClient(PCHAR pDataBuffer, DWORD dwDataSize);
	BOOL DeliverConnectErrorAnswerToClient(PCHAR pDataBuffer, DWORD dwDataSize);
	BOOL DeliverManualDisconnectCommandToClient (PCHAR pDataBuffer, DWORD dwDataSize);

	void ParseId (PCHAR pDataBuffer, DWORD dwDataSize, ULONG &uClientID, ULONG &dwServerIP);

	CRITICAL_SECTION	m_csReadVc;
	std::vector <HANDLE>	m_arrThreadReadVc;
	static DWORD WINAPI ThreadReadVC(LPVOID lParam);


	static DWORD WINAPI ThreadUpdateClientsList(LPVOID lParam);
	ULONGLONG m_ul64ClientsListTimestamp;

	static DWORD WINAPI StartThread(LPVOID lParam);

	void EnumSessions(); 

	void PrintSessionInfo(DWORD SessionId, WTS_INFO_CLASS WtsClass);
	void PrintSessionInfoUlong(DWORD SessionId, WTS_INFO_CLASS WtsClass);
	void PrintSessionInfoUshort(DWORD SessionId, WTS_INFO_CLASS WtsClass);
	CString InfoClassNameString(WTS_INFO_CLASS WtsClass);
	CString SessionStateString(WTS_CONNECTSTATE_CLASS state);

	// pending requests section BEGIN
public:
	typedef struct _PENDING_REQUEST {
		HANDLE hEvent;
		DWORD  dwRequestID;
		BOOL bSuccess;
		PCHAR pszRdpShareString;
	} PENDING_REQUEST, *PPENDING_REQUEST;

private:

	std::list <PENDING_REQUEST*> m_pRequestsList;

	CRITICAL_SECTION m_csPendingRequestsSection;

public:
	void CreateStartThread();
	CRemoteSession(DWORD dwId, CManager *pManager);
	virtual ~CRemoteSession();
	const CString& GetUserDosDevice () const {return m_strUserDosDevice;}
	const CString& GetUserSid () const {return m_strUserSid;}
	const CBuffer& GetSid () const {return m_sid;}

	BOOL Start();
	BOOL Stop();
	DWORD GetSessionId();
	BOOL GetClientsList (CBuffer &buffer);
	BOOL StartClient(DWORD dwID, bool bAutoReconnect);
	BOOL StopClient(DWORD dwID, BOOL bServerNotify);
	TPtrUsbClient FindClientByID(DWORD dwID);
};

typedef boost::intrusive_ptr <CRemoteSession> TPtrRemoteSession;