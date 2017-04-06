/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Manager.h

Abstract:

	Manager class. Allows managing the service: adding, deleting disconnected devices,
	sharing, unsharing USB devices. Shows information about current connections and shared
	USB devices

Environment:

    User mode

--*/

#pragma once

#include <list>
#include "..\Consts\log.h"
#include "rdp_files\Common\FileTransport.h"
#include "rdp_files\RemoteSession.h"
#include "rdp_files\OutgoingRDCConnection.h"
#include "RemoteControl.h"
#include "RemoteManager.h"
#include "..\Consts\UngNetSettings.h"
#include "..\Consts\UsbDevInfo.h"
#include "Utils\Timeouts.h"

#include "Usb\UsbClient.h"
#include "Usb\UsbServer.h"

class CService;


class CManager : public CRemoteControl , public CRemoteManager, public CThreadWrapper, public CLog
{
	friend class CServerUsbToTcp;

	CLog m_Log;

	bool GetRdpSharesList (CUngNetSettings &settings, SOCKET sock);
	bool GetListOfClient (CUngNetSettings &settings, SOCKET sock);
	bool GetUsbTree (SOCKET sock);
	bool GetNumUsbTree (CUngNetSettings &settings, CUngNetSettings &answer);
	bool UpdateUsbTreeNow (CUngNetSettings &settings, CUngNetSettings &answer);
	bool DeleteAll (CUngNetSettings &settings, CUngNetSettings &answer);

	void FillHub (CUsbDevInfo *pHub, CBuffer &buffer);

	bool DoDisconnectServer (TPtrUsbServer pServer);

	std::list <TPtrRemoteSession> m_SessionsList;

	std::list <TPtrOutgoingConnection> m_OutgoingRDCConnectionsList;
	CRITICAL_SECTION m_CriticalSectionForOutgoingRDCConnectionsList;

protected:
    mutable CRITICAL_SECTION m_cs;

	typedef std::list <TPtrUsbServer> TListServer;
    TListServer    m_arrServer;
	typedef std::list <TPtrUsbClient> TListClient;
    TListClient    m_arrClient;

    USHORT          m_uUdpPort;
	// local usb tree
	CUsbDevInfo		m_devsUsb;
	static int		m_iCountUpdateUsb;

public:
    CManager ();
    ~CManager ();

	// rdp
	TPtrUsbServer FindServerByIDForRdp(DWORD dwID);
	void StopAllConnectionServers(DWORD dwConnectionId);

	static ULONG GetLocalIP();
	static void ClearCountUpdateUsb () {m_iCountUpdateUsb++;}
	static DWORD WINAPI FinishInitServer (LPVOID lpParam);

	void UpdateUsbTree ();
    //void UpdateLic ();

    void LoadConfig ();
    void StopItems (CService *pSrv);
    static CString GetHubName (LPCTSTR szHubId);
    CString ConvertAuth (CStringA strBase);

	TPtrUsbServer AddServer (const CUngNetSettings &settings, CUngNetSettings &answer, bool bAutoStart);
    TPtrUsbClient AddClient (const CUngNetSettings &settings);
    void DeleteServer (LPCTSTR szHubName, ULONG uUsbPort);
    void DeleteClient (TPtrUsbClient pClient, bool bSave);
    // Find
    TPtrUsbServer FindItemServer (const CUngNetSettings &settings);
    TPtrUsbServer FindServer (LPCTSTR szHubName, ULONG uUsbPort);
    TPtrUsbClient FindClient (LPCTSTR szHostName, ULONG uTcpPort);
    // pipe command
    virtual bool ParseCommand (LPCSTR pBuff, SOCKET sock);
    bool AddItem (CUngNetSettings &settings, CUngNetSettings &answre);

	// add
	bool AddItemServer (CUngNetSettings &settings, CUngNetSettings &answer);
	bool AddItemClient (CUngNetSettings &settings, CUngNetSettings &answer);
	
	// addition
	TPtrUsbServer GetServer (const CUngNetSettings &settings);
	TPtrUsbServer GetServerNetSettings (const CUngNetSettings &settings);
	TPtrUsbClient GetClient (const CUngNetSettings &settings);

    bool DelItem (CUngNetSettings &settings, CUngNetSettings &answre);
    bool InfoItem (CUngNetSettings &settings, CUngNetSettings &answre);

	// Server-side settings for shared device
    bool ChangeItemDescription (const CUngNetSettings &settings, CUngNetSettings &answer);
    bool SetCompressOption (const CUngNetSettings &settings, CUngNetSettings &answer);
    bool SetCryptOption (const CUngNetSettings &settings, CUngNetSettings &answer);
    bool SetAuthOption (const CUngNetSettings &settings, CUngNetSettings &answer);

    bool StartClient (CUngNetSettings &settings, CUngNetSettings &answre);
    bool StopClient (CUngNetSettings &settings, CUngNetSettings &answre);
    bool SetHubName (CUngNetSettings &settings, CUngNetSettings &answre);
    bool DisconnectServer (CUngNetSettings &settings, CUngNetSettings &answre);
	bool GetDevId (CUngNetSettings &settings, CUngNetSettings &answre);
	bool GetConnectedClientInfo (const CUngNetSettings &settings, CUngNetSettings &answer);
	bool SetAllowRDisconnect (const CUngNetSettings &settings, CUngNetSettings &answer);
	// remote sessions implementation
	void WrapRoutine(LPVOID pCallerParam);
	BOOL AddSession(DWORD dwSessionId);
	TPtrRemoteSession FindSessionById(DWORD dwSessionId);
	BOOL IsSessionInList(TPtrRemoteSession);// not used
	BOOL RemoveSession(DWORD dwSessionId);

	bool SetAutoconnect(CUngNetSettings	&settings);
	bool SetRdpIsolation(CUngNetSettings	&settings, CUngNetSettings &answer);

	// Outgoing RDC (Remote Desktop Client) connections management
	bool AddRDCConnection(CUngNetSettings	&settings);
	bool RemoveRDCConnection(CUngNetSettings	&settings);
	bool RemoveRDCConnection(DWORD dwSessionId);

	TPtrOutgoingConnection FindRDCConnectionById(DWORD dwSessionId);

    // Get config
    virtual void ExistConnect (SOCKET sock, const SOCKADDR_IN &ip);
    virtual bool Start (USHORT uTcpPort, USHORT uTcpUdp);
    virtual void Stop (CService *pSrv);
	static DWORD WINAPI ThreadUdp (LPVOID lpParam);

	void GetStringShare (CBuffer &buffer, bool bRdp=false);
	static void BuildStringShare (TPtrUsbServer pServer, CBuffer &buffer, bool bRdp, const CUsbDevInfo *pUsbDevInfo=NULL);
	// method to automatically pick usb dev info from manager's data, and fill in buffer
	bool BuildStringShareEx (TPtrUsbServer pServer, CBuffer &buffer, bool bRdp);
	// Get USB port location (as N:M:X:Y:Z) for local port.
	CString GetUsbLoc (const CUngNetSettings &settings) const;

	CStringA GetDeviceUsbClass(LPCTSTR szHubName, ULONG uUsbPort);
	size_t GetSharedCount() {return m_arrServer.size();}

    // Log
    static void AddLogFile (LPCTSTR String);
    static void AddLogPipe (LPCTSTR String);
    static bool OpenLogPipe (LPCTSTR szPipe);
    static void CloseLogPipe ();
    static HANDLE   m_hPipeLog;
    static CString  m_strFileName;
    static int      bInitLog;
    static CRITICAL_SECTION m_csLog;

private:
	// helper method(s)
	TPtrUsbServer GetServerPtr (const CUngNetSettings &settings);
	void DoSetHubName (const CString &strId, const CString &strName); // helper for SetHubName
	bool IsUsbHub (TPtrUsbServer pServer) const;

	eltima::utils::ShptrTimeout m_shptrProtectorTimer;
};
