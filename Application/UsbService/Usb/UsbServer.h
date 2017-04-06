/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbServer.h

Abstract:

	USB device server implementation.

Environment:

    User mode

--*/

#ifndef USB_SERVER
#define USB_SERVER

#include <boost\intrusive_ptr.hpp>
#include "BaseUsb.h"
#include "Consts\UngNetSettings.h"
#include "Consts\Count.h"
#include "Utils\Security.h"
#include "Utils\Timeouts.h"


class CUsbServer;
typedef boost::intrusive_ptr <CUsbServer> TPtrUsbServer;

class CUsbServer : public CBaseUsb
{
	friend class CAdvancedUsb;

public:
	CUsbServer (const CUngNetSettings &settings);
	~CUsbServer () {}

	static TPtrUsbServer Create (const CUngNetSettings &settings);

	// event transport
public:
	virtual void OnConnected (Transport::TPtrTransport pConnection);
	virtual void OnError (int iError);
	virtual void OnRead (LPVOID lpData, int iSize);

	virtual void ReadDataFromDevice ();
	
	static CCount	m_cRestart;

	// base function
public:
	virtual bool Connect ();
	virtual bool Disconnect ();
	virtual bool Stop ();		// stop all thread

	virtual CString GetDeviceName () const {return m_strNameDev;}

	virtual CString GetUsbLoc () const;
	mutable CString m_strUsbLoc;

	virtual bool ReadyToReadData (const Transport::TCP_PACKAGE *pPacket, int iSize);
	bool DeviceIsPresent ();
	void FillDevName ();

	ULONG	GetRdpPort () {return (GetTcpPort() | 0x80000000);}

	virtual void LogCompressionError (Transport::TPtrTransport pTransport) const;

private:
	// work with driver
	bool ConnectToDriver ();
	bool CloseDriver ();
	bool CycleUsbPort ();
	CStringW GetDeviceId (); 
	bool ShareUsbPort (bool &bManulRestart);

	bool UnshareUsbPort ();
	bool HubDeviceRelations ();
	bool DeviceIsPresentCore ();
	bool WaitPluginDevice ();
	bool InitUsbDesc ();

	// fill name
	int StrDevIdToInt (CStringW strId);
	CString GetDevName ( int iParam, bool *pReset );
	CString GetHubLinkName (LPCTSTR HubName);
	CString GetDescriptionPort ( HANDLE hHubDevice, ULONG NumPorts, int iParam, bool *pReset );
	CString GetStringDescriptor (HANDLE  hHubDevice, ULONG ConnectionIndex, UCHAR DescriptorIndex, USHORT LanguageID);
	CString GetDriverKeyName (HANDLE  Hub, ULONG   ConnectionIndex);
	bool RestartDevice ();
	CString GetInstanceId ();

	void SendDeviceId ();

	bool InitSecurityConnection (Transport::TPtrTransport pTr);
	
	// param
public:
	void SetDescription (CString str);
	bool EnableCompression (const CUngNetSettings &settings);
	bool SetCrypt (const CUngNetSettings &settings);
	bool SetAuth (const CUngNetSettings &settings);
	void SetAllowRemoteDiscon (bool bAllow);
	bool AllowRemoteDiscon () {return m_settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_DEV_ALLOW_RDISCON]);}
	bool GetDevicePresentCachedFlag() const {return m_devicePresent;}

	void SaveSettingsInRegistry () const;

protected:
	virtual LONG GetTypeDataSend () const {return Transport::IrpAnswerTcp;}

private:
	static DWORD WINAPI stWaitDevice (LPVOID lpParam);
	void StartWaitDevice ();
	void WaitDevice ();

protected:
	CString			m_strNameDev;
	HANDLE			m_hWait;
	OVERLAPPED		m_OverlappedWait;

	// addition param
	// names map
	typedef std::map <int, CString> TMapIntToString;
	TMapIntToString	m_mapDevName;

protected:
	// track device presence
	bool m_devicePresent;
	eltima::utils::ShptrTimeout m_shptrDeviceTimer;
	void CheckDevicePresence ();
};

#endif
