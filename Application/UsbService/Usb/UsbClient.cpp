/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    UsbClient.cpp

Abstract:

	USB device client implementation.

Environment:

    User mode

--*/

#include "stdafx.h"
#include "Consts\Registry.h"
#include "Manager.h"
#include "SettingsInRegistry.h"
#include "public.h"
#include "UsbClient.h"
#include "Utils\UserLog.h"

CUsbClient::CUsbClient (const CUngNetSettings &settings, Transport::TPtrTransport pTransport)
	: CBaseUsb (settings)
	, m_uIpIndex (0)
	, m_hDisconnectThread (INVALID_HANDLE_VALUE)
	, m_bIsolation(FALSE)
{
	SetTrasport(pTransport);
	GetTrasport()->SetSettings(settings);
	SetStatus(Created);
}

bool CUsbClient::ConnectToVuh ()
{
#ifdef NO_DRIVER
	m_hDriver = 0;
	return true;
#endif
	DWORD						Returned;
	Transport::TPtrTransport		pTrasport = GetTrasport();

	if (m_hDriver == INVALID_HANDLE_VALUE && pTrasport)
	{

		m_hDriver = CreateFile (_T("\\\\.\\VUHControl"), 0, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL, NULL);

		if (m_hDriver != INVALID_HANDLE_VALUE)
		{
			if (!CheckVersionDriver (m_hDriver, IOCTL_VUHUB_VERSION))
			{
				CloseHandle (m_hDriver);
				m_hDriver = INVALID_HANDLE_VALUE;

				eltima::ung::UserLog::Client::VuhubCtrlError (GetLastError ());

				return false;
			}

			CAutoLock lock (&m_cs);
			m_usbDev.HostIp = pTrasport->GetIpIndex();
			m_usbDev.DeviceAddress = m_uIpIndex = GetUsbPort();
			m_usbDev.ServerIpPort = GetTcpPort ();

			if (!DeviceIoControl(m_hDriver,
				IOCTL_VUHUB_ADD,
				&m_usbDev,
				sizeof (USB_DEV),
				NULL,
				0,
				&Returned,
				&m_Overlapped)) 
			{
				int DeviceIoControlError = GetLastError();

				if (DeviceIoControlError != ERROR_IO_PENDING)
				{
					CloseHandle (m_hDriver);
					m_hDriver = INVALID_HANDLE_VALUE;

					eltima::ung::UserLog::Client::VuhubAddError (DeviceIoControlError);

					ClearAutoReconnect ();
					return false;
				}
				else if (WaitForSingleObject(m_Overlapped.hEvent, -1) == WAIT_TIMEOUT)
				{
					CloseHandle (m_hDriver);
					m_hDriver = INVALID_HANDLE_VALUE;

					eltima::ung::UserLog::Client::VuhubAddError (GetLastError ());

					ClearAutoReconnect ();
					return false;
				}
			}
		}
		else
		{
			eltima::ung::UserLog::Client::VuhubCtrlError (GetLastError ());

			ClearAutoReconnect ();
		}
	}

	if (m_hDriver != INVALID_HANDLE_VALUE)
	{
		if (!InitPoolList(m_hDriver, IOCTL_VUHUB_IRP_TO_TCP_SET, &m_poolRead))
		{
			Disconnect ();
			return false;
		}
		if (!InitPoolList(m_hDriver, IOCTL_VUHUB_IRP_ANSWER_SET, &m_poolWrite))
		{
			Disconnect ();
			return false;
		}
	}


	return (m_hDriver != INVALID_HANDLE_VALUE);
}

bool CUsbClient::DisconnectToVuh ()
{
	DWORD			Returned;

	if (m_hDriver != INVALID_HANDLE_VALUE)
	{

		CAutoLock lock (&m_cs);

		if (!DeviceIoControl(m_hDriver,
			IOCTL_VUHUB_REMOVE,
			&m_usbDev,
			sizeof (USB_DEV),
			NULL,
			0,
			&Returned,
			&m_Overlapped)) 
		{
			int DeviceIoControlError = GetLastError();

			if (DeviceIoControlError != ERROR_IO_PENDING)
			{
				CloseHandle (m_hDriver);
				m_hDriver = INVALID_HANDLE_VALUE;
				return false;
			}
			else if (WaitForSingleObject(m_Overlapped.hEvent, -1) == WAIT_TIMEOUT)
			{
				CloseHandle (m_hDriver);
				m_hDriver = INVALID_HANDLE_VALUE;
				return false;
			}
		}
		CloseHandle (m_hDriver);
		m_hDriver = INVALID_HANDLE_VALUE;
	}
	return true;
}

bool CUsbClient::SetAutoReconnect (bool bSet)
{
	m_settings.DeleteParam(Consts::szUngNetSettings [Consts::UNS_PARAM]);
	m_settings.AddParamInt(Consts::szUngNetSettings [Consts::UNS_PARAM], bSet);
	if (bSet)
		m_settings.DeleteParam (Consts::szUngNetSettings [Consts::UNS_DONT_SAVE_REG]);
	return true;
}

// *********************************************************
//				event
// *********************************************************
void CUsbClient::OnConnected (Transport::TPtrTransport pConnection)
{
	ATLASSERT (pConnection == GetTrasport());

	CAutoLock	m_lockCreate (&m_cs);

	CAutoLock	lock (&m_cs);

	try
	{
		if (InitSecurityConnection (pConnection))
		{
			SetStatus(Connected);

			try
			{
				CRegistry reg (Consts::szRegistry_usb4rdp, 0, KEY_READ);
				if (reg.ExistsValue(Consts::szIsolation)) 
				{
					m_bIsolation = reg.GetBool (Consts::szIsolation);
				}
			}
			catch (...)
			{
			}

			if (!ConnectToVuh ())
			{
				OnError(ERROR_DEV_NOT_EXIST);
			}

			if (m_bIsolation)
			{
				SetDosDevPref ();
				SetUserSid ();
			}

			StartReadDataFromDevice ();

			eltima::ung::UserLog::Client::Connected (pConnection);
		}
		else
		{
			pConnection->Disconnect();
			OnError(ERROR_INVALID_SECURITY_DESCR);
		}
	}
	catch (Transport::CException &pEx)
	{
		pEx.GetError();
		pConnection->Disconnect();
		OnError(ERROR_INVALID_SECURITY_DESCR);
	}
}

void CUsbClient::OnRead (LPVOID lpData, int iSize)
{
	try
	{
		CBaseUsb::OnRead(lpData, iSize);
	}
	catch (Transport::CException &pEx)
	{
		OnError(pEx.GetErrorInt());
	}
}

void CUsbClient::OnError (int iError) 
{
	CAutoLock lock (&m_cs);

	switch (GetStatus())
	{
	case CBaseUsb::Created:
		break;

	case CBaseUsb::Deleting:
		Disconnect();
		break;

	case CBaseUsb::Handshake:
		StartDisconnectInThread (CBaseUsb::Disconnecting);
		break;

	case CBaseUsb::Connecting:
	case CBaseUsb::Connected:
		StartDisconnectInThread (CBaseUsb::Reconnecting);
		break;

	default:
		// ignore other states
		break;
	}
}

void CUsbClient::ReadDataFromDevice ()
{
	CBaseUsb::ReadDataFromDevice();
}

// *********************************************************
//				connect / disconnect
// *********************************************************
bool CUsbClient::Connect ()
{
	Transport::TPtrTransport pTransport = GetTrasport();
	CAutoLock lock (&m_cs);

	if (GetStatus() == CBaseUsb::Created)
	{
		eltima::ung::UserLog::Client::Start (pTransport);
		SetStatus(Connecting);
		pTransport->Connect(this);
		return true;
	}
	else
	{
		return false;
	}
}

void CUsbClient::ClearResource ()
{
	m_poolRead.Clear();
	m_poolWrite.Clear ();	
}

bool CUsbClient::Disconnect () 
{
	Transport::TPtrTransport pTransport = GetTrasport();
	CAutoLock lock (&m_cs);

	const bool isActive = IsConnectionActive();
	SetStatus(Created);

	if (isActive)
	{
		if (pTransport)
		{
			eltima::ung::UserLog::Client::Stop (pTransport);
			pTransport->Disconnect();
		}

		// stop all thread working with driver
		m_poolRead.Stop();
		WaitStoppedReadDataFromDevice ();

		DisconnectToVuh();
		ClearResource ();
		return true;
	}
	else
	{
		return false;
	}
}

bool CUsbClient::ReadyToReadData (const Transport::TCP_PACKAGE *pPacket, int iSize)
{
	LPBYTE					lpBuffer;
	Transport::TPtrTransport	pTr = GetTrasport();

	if (!pTr)
	{
		OnError(ERROR_NO_DATA);
	}

	switch (GetType(pPacket))
	{
	case Transport::DeviceId:
		{
			CAutoLock Lock (&m_cs);
			CUngNetSettings	settings;
			if (iSize) // data have already been read
			{
				lpBuffer = LPBYTE (pPacket) + sizeof (Transport::TCP_PACKAGE);
			}
			else       // data need to be read
			{
				m_Buffer.SetSize(pPacket->SizeBuffer);
				lpBuffer = (LPBYTE)m_Buffer.GetData();
				pTr-> Read (lpBuffer, pPacket->SizeBuffer);
			}

			//char* temp = (char*)lpBuffer;
			lpBuffer[pPacket->SizeBuffer + 1] = '\0';

			settings.ParsingSettings(CStringA ((LPCSTR)lpBuffer), false);
			settings.DeleteParam(Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);
			settings.DeleteParam(Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);
			UpdateSettings (settings);
		}
		return false;

	case Transport::IrpAnswerTcp:
		return true;

	case Transport::ManualDisconnect:
		ClearAutoReconnect();
		StartDisconnectInThread (CBaseUsb::Disconnecting);
		break;
	}

	return false;
}

bool CUsbClient::InitSecurityConnection (Transport::TPtrTransport pTr)
{
	if (pTr->AuthSupport() || pTr->CryptSupport())	
	{
		SetStatus (CBaseUsb::Handshake);

		USHORT	uHandshake;
		if (pTr->Read ((char*)&uHandshake, 2) == 2)
		{
			// check that handshake value does not have unknown bits set.
			if ((uHandshake & (~Transport::HANDSHAKE_ALL)) != 0)
			{
				eltima::ung::UserLog::Client::HandshakeBitsError (this);
				return false;
			}

			m_security.m_bIsAuth = (uHandshake & Transport::HANDSHAKE_AUTH)?true:false;
			m_security.m_bIsCrypt = (uHandshake & Transport::HANDSHAKE_ENCR)?true:false;

			// the code for compression settings should be executed in all cases,
			// even when compression is disabled,
			// because we could be auto-reconnected on compression settings changes.
			// (see FS#21939)
			if (true)
			{
				const bool comprZlib = (uHandshake & Transport::HANDSHAKE_COMPR_ZLIB) ? true : false;
				const bool comprLz4  = (uHandshake & Transport::HANDSHAKE_COMPR_LZ4) ? true : false;
				// check that only one compression type is selected
				if (comprZlib && comprLz4)
				{
					eltima::ung::UserLog::Client::HandshakeCompressionError (this);
					return false;
				}

				Consts::UngCompressionType comprType = Consts::UCT_COMPRESSION_NONE;
				if (comprZlib)
				{
					comprType = Consts::UCT_COMPRESSION_ZLIB;
				}
				else if (comprLz4)
				{
					comprType = Consts::UCT_COMPRESSION_LZ4;
				}
			
				EnableCompression (comprType);
			}

			// Authorization
			if (IsAuthEnable())
			{
				if (!Auth (pTr))
				{
					eltima::ung::UserLog::Client::HandshakeAuthError (this);
					return false;
				}
			}
			// 
			if (IsCryptEnable())
			{
				m_security.ClearCrypt ();
				if (m_security.SetCrypt ())
				{
					if (!Crypt (pTr))
					{
						eltima::ung::UserLog::Client::HandshakeCryptError (this);
						return false;
					}
				}
				else
				{
					eltima::ung::UserLog::Client::CantInitCrypt (this);
					return false;
				}
			}
			return true;
		}
	}

	return true;
}

void CUsbClient::EnableCompression (Consts::UngCompressionType comprType)
{
	m_aptrCompressor = AptrCompression (ICompression::Create (comprType));
	ATLASSERT (m_aptrCompressor.get ());
#ifdef DEBUG
	if (comprType != Consts::UCT_COMPRESSION_NONE)
	{
		CString str;
		str.Format (
			_T("Enable %s compression"), 
			ICompression::CompressionTypeAsStr (m_aptrCompressor->GetCompressionType ()));
		CManager::AddLogFile (str);
	}
#endif
}

bool CUsbClient::SetAuth (const CUngNetSettings &settings)
{
	const LPCSTR authParamName = Consts::szUngNetSettings [Consts::UNS_AUTH];
	m_settings.DeleteParam (authParamName);
	if (settings.ExistParam (authParamName))
		m_settings.AddParamStrA (authParamName, settings.GetParamStrA (authParamName));
	m_security.SetAuth (CSystem::Decrypt (settings.GetParamStrA (authParamName)), false);
	return true;
}

bool CUsbClient::UpdateSettings (const CUngNetSettings& settings)
{
	m_settings.DeleteParam(Consts::szUngNetSettings[Consts::UNS_HUB_NAME]);
	m_settings.AddParamStr(Consts::szUngNetSettings[Consts::UNS_HUB_NAME], settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_HUB_NAME]));

	m_settings.DeleteParam(Consts::szUngNetSettings[Consts::UNS_USB_PORT]);
	m_settings.AddParamStr(Consts::szUngNetSettings[Consts::UNS_USB_PORT], settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_USB_PORT]));

	{
		m_settings.DeleteParam (Consts::szUngNetSettings [Consts::UNS_USB_LOC]);
		if (settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_USB_LOC]))
			m_settings.AddParamStr (Consts::szUngNetSettings [Consts::UNS_USB_LOC], 
			  settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_USB_LOC]));
	}

	m_settings.DeleteParam(Consts::szUngNetSettings[Consts::UNS_NAME]);
	m_settings.AddParamStr(Consts::szUngNetSettings[Consts::UNS_NAME], settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_NAME]));

	m_settings.DeleteParam(Consts::szUngNetSettings[Consts::UNS_DESK]);
	m_settings.AddParamStr(Consts::szUngNetSettings[Consts::UNS_DESK], settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_DESK]));

	m_settings.DeleteParam(Consts::szUngNetSettings[Consts::UNS_DEV_USBCLASS]);
	m_settings.AddParamStr(Consts::szUngNetSettings[Consts::UNS_DEV_USBCLASS], settings.GetParamStr (Consts::szUngNetSettings[Consts::UNS_DEV_USBCLASS]));

	return true;
}

void CUsbClient::SaveSettingsToRegistry () const
{
	Transport::TPtrTransport pTr = GetTrasport();

	if (!pTr)
	{
		// client can't have transport
		return;
	}

	if (pTr->IsRdpConnected())
	{
		// don't save rdp settings
		return;
	}

	if (m_settings.GetParamFlag (Consts::szUngNetSettings [Consts::UNS_DONT_SAVE_REG]))
		// don't save to registry
		return;

	CClientSettingsInRegistry regset (GetVaribleName ());
	regset.SaveSettings (m_settings);
}

bool CUsbClient::SetUserSid ()
{
	OVERLAPPED		Overlapped;
	Transport::TPtrTransport	pTr = GetTrasport ();

	if (!pTr || !pTr->IsRdpConnected())
	{
		return false;
	}
	if (m_hDriver != INVALID_HANDLE_VALUE)
	{
		USB_DEV_SHARE	Share;
		DWORD			Returned;

		Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); 

		ZeroMemory (&Share, sizeof (USB_DEV_SHARE));
		wcscpy_s (Share.HubDriverKeyName, pTr->GetUserSid ());

		{
			LPBYTE pBuff = LPBYTE (Share.HubDriverKeyName) + (pTr->GetUserSid ().GetLength() + 1) * sizeof (TCHAR);

			if (IsValidSid (pTr->GetSid().GetData()))
			{
				CopySid(GetLengthSid (pTr->GetSid().GetData()), pBuff, pTr->GetSid ().GetData());
			}

		}

		if (!DeviceIoControl(m_hDriver,
			IOCTL_VUHUB_SET_USER_SID,
			&Share,
			sizeof (USB_DEV_SHARE),
			NULL,
			0,
			&Returned,
			&Overlapped)) 
		{
			int DeviceIoControlError = GetLastError();

			if (DeviceIoControlError == ERROR_IO_PENDING)
			{
				if (WaitForSingleObject(Overlapped.hEvent, 500) == WAIT_OBJECT_0)
				{
					CloseHandle(Overlapped.hEvent);
					return true;
				}
			}
		}
		else
		{
			CloseHandle(Overlapped.hEvent);
			return true;
		}
		CloseHandle(Overlapped.hEvent);
	}
	return false;
}

bool CUsbClient::SetDosDevPref ()
{
	OVERLAPPED		Overlapped;
	Transport::TPtrTransport	pTr = GetTrasport ();

	if (!pTr || !pTr->IsRdpConnected())
	{
		return false;
	}
	if (m_hDriver != INVALID_HANDLE_VALUE)
	{
		USB_DEV_SHARE	Share;
		DWORD			Returned;

		Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); 

		ZeroMemory (&Share, sizeof (USB_DEV_SHARE));
		wcscpy_s (Share.HubDriverKeyName, pTr->GetUserDosDevice ());

		if (!DeviceIoControl(m_hDriver,
			IOCTL_VUHUB_SET_DOSDEV_PREF,
			&Share,
			sizeof (USB_DEV_SHARE),
			NULL,
			0,
			&Returned,
			&Overlapped)) 
		{
			int DeviceIoControlError = GetLastError();

			if (DeviceIoControlError == ERROR_IO_PENDING)
			{
				if (WaitForSingleObject(Overlapped.hEvent, 500) == WAIT_OBJECT_0)
				{
					CloseHandle(Overlapped.hEvent);
					return true;
				}
			}
		}
		else
		{
			CloseHandle(Overlapped.hEvent);
			return true;
		}
		CloseHandle(Overlapped.hEvent);
	}
	return false;
}

// *********************************************************
//	disconnect/reconnect in separate thread
// *********************************************************

void CUsbClient::StartDisconnectInThread (const CBaseUsb::Status newStatus)
{
	if (GetStatus () == newStatus)
	{
		return;
	}

	SetStatus (newStatus);

	DWORD threadID;
	m_hDisconnectThread = ::CreateThread (NULL, 0, stDisconnectThread, this, 0, &threadID);
}

/*static*/
DWORD WINAPI CUsbClient::stDisconnectThread (LPVOID lpParam)
{
	TPtrUsbClient pClient ((CUsbClient*)lpParam);
	pClient->DisconnectThread ();
	return 0;
}

void CUsbClient::DisconnectThread ()
{
	// force disconnect first, see method CUsbClient::Disconnect above
	m_cs.Lock ();

	const bool requiredReconnect = (GetStatus () == CBaseUsb::Reconnecting) && GetAutoReconnect ();
	SetStatus (CBaseUsb::Created);

	Transport::TPtrTransport pTransport = GetTrasport ();
	if (pTransport)
	{
		eltima::ung::UserLog::Client::ConnectionBroken (pTransport);
		pTransport->Disconnect ();
		if (!requiredReconnect)
		{
			eltima::ung::UserLog::Client::Stop (pTransport);
		}
	}

	m_cs.UnLock ();

	// stop all thread working with driver
	m_poolRead.Stop ();
	WaitStoppedReadDataFromDevice ();

	DisconnectToVuh ();
	ClearResource ();

	// now we can reconnect if requested
	m_cs.Lock ();
	if (requiredReconnect)
	{
		Connect ();
	}
	m_cs.UnLock ();
}

void CUsbClient::LogCompressionError (Transport::TPtrTransport pTransport) const
{
	eltima::ung::UserLog::Client::CompressionError (pTransport);
}
