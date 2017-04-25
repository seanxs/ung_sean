/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbClient.h

Abstract:

	USB device client implementation.

Environment:

    User mode

--*/

#ifndef USB_CLIENT
#define USB_CLIENT

#include "BaseUsb.h"
#include "..\..\Drivers\common\public.h"

class CUsbClient : public CBaseUsb
{
public:
	CUsbClient (const CUngNetSettings &settings, Transport::TPtrTransport pTransport);
	~CUsbClient () {}

public:
	virtual void OnConnected (Transport::TPtrTransport pConnection);
	virtual void OnRead (LPVOID lpData, int iSize);
	virtual void OnError (int iError);

	// base function
public:
	virtual bool Connect ();
	virtual bool Disconnect ();

	virtual void ReadDataFromDevice ();

	bool GetAutoReconnect () const {return m_settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_PARAM]);}
	bool IsAutoAdded() const {return m_settings.GetParamBoolean (Consts::szUngNetSettings [Consts::UNS_AUTO_ADDED]);}

	bool SetAutoReconnect (bool bSet);
	bool SetAuth (const CUngNetSettings &settings);

	CString GetDeviceName () const {return m_settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_NAME]);}

	bool UpdateSettings (const CUngNetSettings& settings);
	void SaveSettingsToRegistry () const;

	virtual void LogCompressionError (Transport::TPtrTransport pTransport) const;

protected:
	bool ConnectToVuh ();
	bool DisconnectToVuh ();
	void ClearResource ();

	void ClearAutoReconnect () {m_settings.DeleteParam(Consts::szUngNetSettings [Consts::UNS_PARAM]);}

	virtual bool ReadyToReadData (const Transport::TCP_PACKAGE *pPacket, int iSize);

	bool InitSecurityConnection (Transport::TPtrTransport pTr);

	void EnableCompression (Consts::UngCompressionType comprType);

	virtual bool SetTrasport (Transport::TPtrTransport tr) {m_pTrasport = tr;return true;}
	virtual LONG GetTypeDataSend () const {return Transport::IrpToTcp;}

	// insulation
	bool SetUserSid ();
	bool SetDosDevPref ();

	// control thread for disconnect/reconnect
	void StartDisconnectInThread (const CBaseUsb::Status newStatus);
	static DWORD WINAPI stDisconnectThread (LPVOID lpParam);
	void DisconnectThread ();
	HANDLE m_hDisconnectThread;

private:
	ULONG			m_uIpIndex;
	CBuffer			m_Buffer;
	USB_DEV			m_usbDev;
	BOOL			m_bIsolation;
};

typedef boost::intrusive_ptr <CUsbClient> TPtrUsbClient;

#endif
