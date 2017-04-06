/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    BaseUsb.h

Abstract:

	Base class for shared USB device server 
	and connected USB device client.

Environment:

    User mode

--*/

#ifndef USB_BASE
#define USB_BASE

#include "IBase.h"
#include "Transport/ITransport.h"
#include "Utils/PoolList.h"
#include "Consts/Buffer.h"
#include "Consts/RefCounter.h"
#include "Utils/Compression.h"
#include "lock.h"
#include <boost/intrusive_ptr.hpp>
#include "Utils/Security.h"


class CBaseUsb : public Transport::IEvent, public virtual Common::CRefCounter
{
public:
	CBaseUsb (const CUngNetSettings &Settings);
	virtual ~CBaseUsb();

public:
	enum Status
	{
		Created,
		Connecting,
		Handshake,
		Connected,
		Empty,
		Deleting,
		Disconnecting,	// usb client
		Reconnecting,	// usb client / usb server
	};
	// event transport
public:
	virtual void OnConnected (Transport::TPtrTransport pConnection) = 0;
	virtual void OnRead (LPVOID lpData, int iSize);
	virtual void OnError (int iError) = 0;

	// base function
public:
	virtual bool Connect () = 0;
	virtual bool Disconnect () = 0;
	virtual void Release();
	//virtual bool Stop () = 0;

	Status GetStatus () const {return m_State;}
	int    GetStatusForDll () const; // see dllmain.h constants U2EC_STATE_xxx
	void SetStatus (Status st) {m_State = st;}

	// for debugging: return status as string
	const char* GetStatusStr() const;

	// return true if status is either wait/establish new connection, or already connected
	bool IsConnectionActive () const;

	Transport::TPtrTransport GetTrasport () const;
	virtual bool SetTrasport (Transport::TPtrTransport tr) ;

	CString GetDescription () const {return m_settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_DESK]);}

	CString GetVaribleName () const;

	virtual void LogCompressionError (Transport::TPtrTransport pTransport) const = 0;

	bool CheckVersionDriver (HANDLE hDriver, DWORD dwIoCtrl);

	// settings
public:
	bool IsCompressionEnabled () const {return (m_aptrCompressor->GetCompressionType () != Consts::UCT_COMPRESSION_NONE);}
	bool IsAuthEnable () const {return m_security.GetIsAuth();}
	bool IsCryptEnable () const {return m_security.GetIsCrypt ();}

	bool Auth (Transport::TPtrTransport pTr);
	bool Crypt (Transport::TPtrTransport pTr);

	virtual int GetCompressionType () const ;
	virtual CSecurity* GetSecuritySettings ();

	virtual CString GetDeviceName () const = 0;
	virtual int GetTcpPort () const {return m_settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);}
	virtual CString GetHostName () const {return m_settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);}
	virtual CString GetNetSettings () const;

	CString GetHubName () const {return m_settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_HUB_NAME]);}
	int GetUsbPort () const {return m_settings.GetParamInt (Consts::szUngNetSettings [Consts::UNS_USB_PORT]);}
	virtual CString GetUsbLoc () const {return _T("");}

	const CUngNetSettings& GetSettings () const {return m_settings;}

	// Return string of HEX-ASCII representation of USB class triple as KKSSPP, where KK - value of class, SS - subclass, PP - protocol
	CStringA GetUsbClass () const {return m_settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_DEV_USBCLASS]);}

	// addition function
protected:
	static bool InitPoolList (HANDLE hHandle, DWORD IoCtrl, CPoolList *list);
	static bool IsPackageCompressed (const Transport::TCP_PACKAGE *pPackage);
	bool Compress (Transport::TCP_PACKAGE *pPackage, CBuffer &Buffer) const;
	bool Uncompress (LPBYTE lpSource, int iSource, LPBYTE lpDest, int *iDest) const;
	LONG GetType (const Transport::TCP_PACKAGE *pPackage);

	virtual bool ReadyToReadData (const Transport::TCP_PACKAGE *pPacket, int iSize) = 0;

	virtual void SendDataToTransport ();
	virtual LONG GetTypeDataSend () const = 0;

	virtual void ClearStatistics ();

	virtual void ReadDataFromDevice ();
	static DWORD WINAPI stReadDataFromDevice (LPVOID lpParam);
	bool StartReadDataFromDevice ();
	bool WaitStoppedReadDataFromDevice ();

	HANDLE GetHandleThreadReadFromDriver () const {return m_hReadDataFromDriver;}

	DWORD GetWaitThreadTimeMs () {return 3*1000/*ms*/;}

protected:
	CPoolList					m_poolRead;
	CPoolList					m_poolWrite;
	HANDLE						m_hDriver;
	Transport::TPtrTransport		m_pTrasport;
	CUngNetSettings				m_settings;
	Status						m_State;
	// sync
	mutable CLock						m_cs;
	mutable CLock						m_csTr;
	// security
	CSecurity					m_security;

	HANDLE						m_hReadDataFromDriver;
	DWORD						m_dwReadDataFromDriver;

	typedef std::auto_ptr<ICompression> AptrCompression;
	AptrCompression	m_aptrCompressor;

	OVERLAPPED		m_Overlapped;

	// stat
	LONG						m_iSizeInZip;
	LONG						m_iSizeOutZip;
	LONG						m_iSizeIn;
	LONG						m_iSizeOut;
};

typedef boost::intrusive_ptr <CBaseUsb> TPtrUsbBase;

#endif
