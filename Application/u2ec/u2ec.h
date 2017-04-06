/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   u2ec.h

Abstract:

	Additional functions for working with server

Environment:

    Kernel mode

--*/

#ifndef U2EC_H_000000002
#define U2EC_H_000000002

#include "stdafx.h"
#include <boost/shared_ptr.hpp>
#include "../Consts/UsbDevInfo.h"
#include <map>
#include <atlstr.h>
#include "..\Consts\consts.h"
#include "EventWindow.h"
#include "..\Consts\UngNetSettings.h"
#include "..\Consts\Buffer.h"
#include <stack>

class CCriticalSectionDll
{
public:
	CCriticalSectionDll () {InitializeCriticalSection (&m_cs);}
	~CCriticalSectionDll () {DeleteCriticalSection (&m_cs);}

	void Lock (int i) 
	{
		EnterCriticalSection (&m_cs);
	}
	void UnLock (int i) 
	{
		LeaveCriticalSection (&m_cs);
	}

protected:
	std::stack <int> stack;
	CRITICAL_SECTION m_cs;
};

// server
typedef boost::shared_ptr <CUsbDevInfo>		TPtrUsbDevInfo;
typedef std::map <PVOID,  TPtrUsbDevInfo>	TMAPUSBDEV;
extern TMAPUSBDEV g_mapEnum;
extern CString g_strServer;
extern CString g_strRemoteHost;
extern CCriticalSectionDll	g_csClient;
extern CCriticalSectionDll	g_csServer;

typedef std::map <PVOID, CStringA> TMapContextError;
extern TMapContextError g_mapServerLastError;

// client
struct CLIENT_ELMENT
{
	CLIENT_ELMENT () 
		: bAuth (false)
		, bCrypt (false)
		, bCompress (false) 
		, bRdp (false)
		, bRDisconn (false)
		, uSession (0)
		, iStatus (0)
		, bAutoAdded (false)
		, nUsbBaseClass (0), nUsbSubClass (0), nUsbProtocol (0)
		, bcdUSB(0)
		, idVendor(0)
		, idProduct(0)
		, bcdDevice(0)
	{}
	CString strName;
	CString strNetSettings;
	CString strRemoteServer;
	bool	bAuth;
	bool	bCrypt;
	bool	bCompress;
	bool	bRdp;
	bool	bRDisconn;
	ULONG	uSession;
	// strName (above) is actually composition of real name, nick, usb hub & port
	// below those values separately
	CString strRealName; // just real name of remote shared device
	CString strNick;     // optional user description
	CString strUsbHub;   // optional USB hub name where device connected on server side
	CString strUsbPort;  // optional USB port name where device connected on server side
	CString strUsbLoc;   // optional USB port location as N:M:X:Y:Z
	// "cached" status/remote host values
	int     iStatus;       // shared device status
	CString strSharedWith; // if status >= 2 - this is connected client ip/name 
	bool    bAutoAdded;
	BYTE    nUsbBaseClass; // usb class triple: base class
	BYTE    nUsbSubClass;  // usb class triple: sub class
	BYTE    nUsbProtocol;  // usb class triple: protocol
	//// USB descriptor data
	USHORT	bcdUSB;
	USHORT	idVendor;
	USHORT	idProduct;
	USHORT	bcdDevice;
	CString	strSerialNumber;

	CStringA UsbClassAsHexStrA () const {return CUsbDevInfo::UsbClassAsHexStrA (nUsbBaseClass,nUsbSubClass,nUsbProtocol);}
};

typedef std::vector <CLIENT_ELMENT> TVECCLEINT;
typedef struct 
{
	CString		strServer;
	TVECCLEINT	Clients;
}TVECCLEINTS;

typedef boost::shared_ptr <TVECCLEINTS>	TPtrVECCLEINTS;
typedef std::vector <TPtrVECCLEINTS> TVECCLIENT;
extern TVECCLIENT g_Clinet;


// server
bool SendCommand (LPCTSTR szServer, USHORT uPort/* = U2ECConsts::uTcpControl*/, LPCSTR szCommand, CBuffer &answer);
bool SendCommandToUsbService (LPCSTR szCommand, CBuffer &answer, bool bCheckAnswer=true); // Send command to localhost:5475, re-use the same connection if possible
bool ParseAnsver (const CBuffer &answer);
CStringA GetErrorCodeFromAnswer (const CBuffer &answer);
SOCKET OpenConnection (LPCTSTR szHost, USHORT uPort);
SOCKET OpenConnectionWithTimeout (LPCTSTR szHost, USHORT uPort, DWORD dwTimeoutMs);
USHORT CalcTcpPort (LPCTSTR HubName, ULONG lUsbPort);
CString GetNetSettingsForServerDevice (const CUsbDevInfo *pDev);
bool GetNetSettingsFromVariant (const VARIANT &varNetSettings, CString &strServer, int &iTcpPort);
bool GetPswdEncyptedFromVariant (const VARIANT &varValue, CStringA &strPswdEncrypted);
bool GetBoolFromVariant (const VARIANT &varValue, bool &bValue);
bool GetCompressFromVariant (const VARIANT &varValue, bool &bCompress, int &iCompressType);
bool GetIntFromVariant (const VARIANT &varValue, int &iValue);
CString ServerDevDesc (LPCTSTR HubName, ULONG lUsbPort);
bool AddHubName (LPCTSTR szHubId, LPCTSTR szHubName);
bool SetUserDescriptionForSharedDevice (const CUsbDevInfo *pDev, const CString &strUserDescription);
bool SetAuthForSharedDevice (const CUsbDevInfo *pDev, const CStringA &strPswdEncrypted);
bool SetCryptForSharedDevice (const CUsbDevInfo *pDev, const bool bCrypt);
bool SetCompressForSharedDevice (const CUsbDevInfo *pDev, const bool bCompress, int iCompressType=-1);

// client
PVOID GetCleintContext (SOCKET sock, LPCTSTR szServer);
bool ParseClientConfiguration (boost::shared_ptr <TVECCLEINTS> &Clients, CBuffer &answer);

bool ParseUsbHub (CUsbDevInfo *pDev, CBuffer &answer, int &iPos, int iDev = 20);

CString ConvertNetSettingsServerToClient (const CString &strServer, const CString &strNetSettings);
CString ConvertNetSettingsClientToServer (const CString &strServer, const CString &strNetSettings, const CString &strRemoteServer);

bool GetShareDevInfoEx (const CString &strServer, 
                        const CString &strNetSettings,
                        CLIENT_ELMENT *pElement,
                        CString *strShared,
                        LONG *pState);

bool SendRemoteDisconnect (	const CString &strServer, 
							const CString &strNetSettings,
							const CLIENT_ELMENT *pElement);

void FillOutClientStructFromParsedAnswer (CLIENT_ELMENT *pElement, 
                                          const CUngNetSettings &settings);
bool GetShareDevClientDesk (const CString &strServer, 
                            const CString &strNetSettings,
                            CString *strDesk);


CString GetRegistry (CString strFullPath);

extern CEventWindow window;

bool RecvData (SOCKET socket, CBuffer &buffer);

TPtrVECCLEINTS FindClient (PVOID pContext);

void SetServerLastError (PVOID pContext, const CStringA &strError);
CStringA GetServerLastError (PVOID pContext);

// Persistent socket to USB service
bool CheckSocketToUsbService ();
void CloseSocketToUsbService ();


#endif
