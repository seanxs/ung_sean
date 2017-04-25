/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    BaseUsb.cpp

Abstract:

	Base class for shared USB device server 
	and connected USB device client.

Environment:

    User mode

--*/

#include "stdafx.h"
#include "BaseUsb.h"
#include "..\..\Drivers\common\public.h"
#include "u2ec/dllmain.h"
#include "Utils/UserLog.h"


CBaseUsb::CBaseUsb (const CUngNetSettings &Settings)
	: m_hDriver (INVALID_HANDLE_VALUE)
	, m_iSizeInZip (0)
	, m_iSizeOutZip (0)
	, m_iSizeIn (0)
	, m_iSizeOut (0)
	, m_State (Empty)
	, m_hReadDataFromDriver (INVALID_HANDLE_VALUE)
	, m_settings (Settings)
	, m_dwReadDataFromDriver (0)
{
	ZeroMemory (&m_Overlapped, sizeof (m_Overlapped));
	m_Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); 

	if (m_settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_COMPRESS]))
	{
		int iComprType = m_settings.GetParamInt (	Consts::szUngNetSettings [Consts::UNS_COMPRESS_TYPE], 
			Consts::UCT_COMPRESSION_DEFAULT);

		m_aptrCompressor = AptrCompression (ICompression::Create (Consts::UngCompressionType (iComprType)));
	}
	else
	{
		m_aptrCompressor = AptrCompression (ICompression::Create (Consts::UCT_COMPRESSION_NONE));
		//m_aptrCompressor = AptrCompression(ICompression::Create(Consts::UCT_COMPRESSION_ZLIB));
	}

	if (m_settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH]).GetLength())
	{
		CString strPassw = CSystem::Decrypt (m_settings.GetParamStrA (Consts::szUngNetSettings [Consts::UNS_AUTH]));
		m_security.SetAuth(strPassw, false);
	}

	if (m_settings.ExistParam (Consts::szUngNetSettings [Consts::UNS_CRYPT]))
	{
		m_security.SetCrypt();
	}
}

CBaseUsb::~CBaseUsb()
{
	SetTrasport(NULL);
	CloseHandle(m_Overlapped.hEvent);
}

bool CBaseUsb::InitPoolList (HANDLE hHandle, DWORD IoCtrl, CPoolList *pool)
{
	OVERLAPPED	Overlapped;
	LPBYTE		Buffer;
	DWORD		dwReturned;
	LPVOID		pInBuffer = NULL;

	if (!pool->IsEmpty ())
	{
		return false;
	}

	ZeroMemory (&Overlapped, sizeof(Overlapped));
	Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); 

	pool->Init(2, 1500000, sizeof (Transport::TCP_PACKAGE));

	pInBuffer = pool->GetInitData();

	if (!DeviceIoControl(hHandle,
		IoCtrl,
		pInBuffer,
		pool->GetInitSize(),
		&Buffer,
		sizeof (Buffer),
		&dwReturned,
		&Overlapped))
	{
		int nLastError = GetLastError();

		if (nLastError == ERROR_IO_PENDING)
		{
			if (!GetOverlappedResult(hHandle, &Overlapped, &dwReturned, true))
			{
				return false;
			}
		}
		else
			return false;
	}

	pool->InitBuffer(Buffer);

	CloseHandle(Overlapped.hEvent);

	return true;
}

bool CBaseUsb::CheckVersionDriver (HANDLE hDriver, DWORD dwIoCtrl)
{
	if (hDriver != INVALID_HANDLE_VALUE)
	{
		ULONG	uVer;
		DWORD	Returned;
		if (DeviceIoControl(hDriver,
			dwIoCtrl,
			NULL,
			0,
			&uVer,
			sizeof (uVer),
			&Returned,
			&m_Overlapped)) 
		{
			if (uVer >= 0x0700)
			{
				return true;
			}
		}
	}

	return false;
}

bool CBaseUsb::IsPackageCompressed (const Transport::TCP_PACKAGE *pPackage)
{
	return (pPackage->Type & Transport::TYPE_CompressedFlag)?true:false;
}

bool CBaseUsb::Compress (Transport::TCP_PACKAGE *pPackage, CBuffer &Buffer) const
{
	ATLASSERT (m_aptrCompressor.get ());

	BYTE *pData = ((BYTE*)pPackage) + sizeof (Transport::TCP_PACKAGE);
	const LONG dataLength = pPackage->SizeBuffer;

	const ECompressResult result = m_aptrCompressor->Compress (pData, dataLength, Buffer);
	switch (result)
	{
	case ECR_NOT_COMPRESSED:
		OutputDebugStringA(__FUNCTION__"()==>ECR_NOT_COMPRESSED!\n");
		// that's ok, don't change tcp package
		return true;

	case ECR_COMPRESSED:
		// update tcp package with compressed data, see below
		break;

	case ECR_ERROR:
	default:
		return false;
	}

	pPackage->Type |= Transport::TYPE_CompressedFlag;
	pPackage->SizeBuffer = Buffer.GetCount ();

	CopyMemory (pData, Buffer.GetData (), pPackage->SizeBuffer);

	return true;
}

bool CBaseUsb::Uncompress (LPBYTE lpSource, int iSource, LPBYTE lpDest, int *iDest) const
{
	ATLASSERT (m_aptrCompressor.get ());

	if (!lpSource || !lpDest)
	{
		return false;
	}
	LONG dataLength = *iDest;
	bool bRet = m_aptrCompressor->Uncompress2 (lpSource, iSource, lpDest, &dataLength);
	*iDest = dataLength ;
	return bRet;
}


void CBaseUsb::OnRead (LPVOID lpData, int iSize)
{
	LPBYTE						pBuff = NULL;
	Transport::TCP_PACKAGE		tcpPackage;
	Transport::TCP_PACKAGE		*pTcpPackage = &tcpPackage;
	CBuffer						Buffer;
	Transport::TPtrTransport	pIn = GetTrasport ();

	if (!pIn || m_hDriver == INVALID_HANDLE_VALUE)
	{
		OnError(WSANO_DATA);
		return;
	}

	Status status = GetStatus ();
	if (status != Connected)
	{
		if (status != Disconnecting && status != Reconnecting)
		{
			OnError(WSANO_DATA);
		}
		return;
	}

	if (lpData) // if rdp, data has been read
	{
		pTcpPackage = (Transport::TCP_PACKAGE *)lpData;
	}
	else
	{
		pIn->Read (pTcpPackage, sizeof (Transport::TCP_PACKAGE));
	}

	if (ReadyToReadData (pTcpPackage, iSize))
	{
		// get buffer
		if (IsPackageCompressed (pTcpPackage))
		{
			Buffer.SetSize (pTcpPackage->SizeBuffer);
			pBuff = (LPBYTE)Buffer.GetData ();
		}
		else
		{
			pBuff = m_poolWrite.GetCurNodeWrite();
		}

		if (pBuff == NULL)
		{
			throw Transport::CException (_T("m_poolWrite error"));
		}

		if (lpData) // if rdp, data has already been read
		{
			if (iSize != (sizeof (Transport::TCP_PACKAGE) + pTcpPackage->SizeBuffer))// verify size
			{
				throw Transport::CException (_T("RDP read data is error, size is wrong"));
			}
			RtlCopyMemory (pBuff, LPBYTE (lpData) + sizeof (Transport::TCP_PACKAGE), pTcpPackage->SizeBuffer);
		}
		else
		{
			pIn->Read (pBuff, pTcpPackage->SizeBuffer);
		}

		m_iSizeInZip += pTcpPackage->SizeBuffer;
		if (IsPackageCompressed (pTcpPackage))
		{
			LPBYTE	lpOut = m_poolWrite.GetCurNodeWrite();
			int iSizeOut = m_poolWrite.GetBlockSize();

			if (!Uncompress (pBuff, pTcpPackage->SizeBuffer, lpOut, &iSizeOut))
			{
				LogCompressionError (pIn);

				m_poolWrite.FreeNodeWrite();
				throw Transport::CException (_T("Error uncompress data"));
			}
			pTcpPackage->SizeBuffer = iSizeOut;
		}
		m_iSizeIn += pTcpPackage->SizeBuffer;
#ifdef _SEANXS_FEATURE_ENABLE_LOG
		CString str;
		str.Format(_T("Package readfrom : %d "), pTcpPackage->SizeBuffer + sizeof(Transport::TCP_PACKAGE));
		CManager::AddLogFile(str);
#endif
		// send data to driver
		m_poolWrite.PushCurNodeWrite();
	}
}

void CBaseUsb::SendDataToTransport ()
{
#ifdef NO_DRIVER
	while (GetStatus() == Connected)
	{
		Sleep (500);
	}
#endif

	LPBYTE							pBuff = NULL;
	Transport::TCP_PACKAGE			*pTcpPackage = NULL;
	PIRP_SAVE						pIrpSave = 0;
	Transport::TPtrTransport		pOut;
	CBuffer							BufferZip;

	pOut = GetTrasport();
	if (!pOut)
	{
		OnError(WSANO_DATA);
		return;
	}

	// init buffer for compress
	if (IsCompressionEnabled () && pOut->HasCompressSupport ())
	{
		BufferZip.SetSize(512*1024);
	}

	while (GetStatus() == Connected)
	{
		pOut = GetTrasport ();
		pBuff = m_poolRead.GetCurNodeRead();

		if (!pBuff)
		{
			OnError(WSANO_DATA);
			return;
		}

		pIrpSave = (PIRP_SAVE)pBuff;
		pTcpPackage = (Transport::TCP_PACKAGE*)m_poolRead.GetFullBuffer(pBuff);
		pTcpPackage->Type = GetTypeDataSend ();//Transport::IrpAnswerTcp;
		pTcpPackage->SizeBuffer = pIrpSave->NeedSize;

		if (!pOut || m_hDriver == INVALID_HANDLE_VALUE)
		{
			m_poolRead.PopCurNodeRead();
			OnError(WSANO_DATA);
			return;
		}

		if (pIrpSave->NeedSize == 0				// error - device is disconnected
			|| pIrpSave->NeedSize == -1)		// need more data
		{
			m_poolRead.PopCurNodeRead();
			OnError(WSANO_DATA);
			return;
		}

		if (GetStatus() != Connected)
		{
			m_poolRead.PopCurNodeRead();
			OnError(WSANO_DATA);
			return;
		}
#ifdef _SEANXS_FEATURE_ENABLE_LOG
		CString str;
		str.Format(_T("Package sendto : %d "), pTcpPackage->SizeBuffer + sizeof(Transport::TCP_PACKAGE));
		CManager::AddLogFile(str);
#endif
		if (IsCompressionEnabled () && pOut->HasCompressSupport ())
		{
			if (!Compress (pTcpPackage, BufferZip))
			{
				LogCompressionError (pOut);

				m_poolRead.FreeNodeRead();
				throw Transport::CException (_T("Error compress data"));
			}
#ifdef _SEANXS_FEATURE_ENABLE_LOG
			else
			{
				str.Empty();
				str.Format(_T("Package sendto(compressed) : %d "), pTcpPackage->SizeBuffer + sizeof(Transport::TCP_PACKAGE));
				CManager::AddLogFile(str);

			}
#endif
		}
		try
		{
			if (pTcpPackage->SizeBuffer > 0)
			{
				pOut->Write((PCHAR)pTcpPackage, pTcpPackage->SizeBuffer + sizeof (Transport::TCP_PACKAGE));
			}
			else
			{
				throw Transport::CException (L"Error package size");
			}
		}	
		catch (Transport::CException  &e)
		{
			(void)e;
			m_poolRead.PopCurNodeRead();
			OnError(WSANO_DATA);
			return;
		}

		m_poolRead.PopCurNodeRead();
	}
}

void CBaseUsb::ClearStatistics ()
{
	m_iSizeInZip = 0;
	m_iSizeOutZip = 0;
	m_iSizeIn = 0;
	m_iSizeOut = 0;
}

void CBaseUsb::Release()
{
	SetTrasport(NULL);
}

const char* CBaseUsb::GetStatusStr () const
{
	switch (m_State)
	{
	case Created:		return "Created";
	case Connecting:	return "Connecting";
	case Handshake:		return "Handshake";
	case Connected:		return "Connected";
	case Empty:			return "Empty";
	case Deleting:		return "Deleting";
	case Disconnecting: return "Disconnecting";
	case Reconnecting:	return "Reconnecting";
	default:			return "<???>";
	}
}

Transport::TPtrTransport CBaseUsb::GetTrasport () const
{
	CAutoLock lock (&m_csTr);
	Transport::TPtrTransport t = m_pTrasport;
	return t;
}

bool CBaseUsb::SetTrasport (Transport::TPtrTransport tr)  
{
	Transport::TPtrTransport old = GetTrasport();
	CAutoLock lock (&m_csTr);

	m_pTrasport = NULL;
	if (old)
	{
		old->ClearEvent ();
	}
	m_pTrasport = tr;

	return true;
}

DWORD WINAPI CBaseUsb::stReadDataFromDevice (LPVOID lpParam)
{
	TPtrUsbBase pBase ((CBaseUsb*)lpParam);
	pBase->ReadDataFromDevice ();
	return 0;
}

bool CBaseUsb::WaitStoppedReadDataFromDevice ()
{
	if (m_hReadDataFromDriver != INVALID_HANDLE_VALUE)
	{
		if (GetCurrentThreadId() != m_dwReadDataFromDriver)
		{
			WaitForSingleObject(m_hReadDataFromDriver, GetWaitThreadTimeMs ());
		}
		CloseHandle(m_hReadDataFromDriver);
		m_hReadDataFromDriver = INVALID_HANDLE_VALUE;
		m_dwReadDataFromDriver = 0;

		return true;
	}
	return false;
}

void CBaseUsb::ReadDataFromDevice ()
{
	try
	{
		SendDataToTransport ();
	}
	catch (Transport::CException &e)
	{
		OnError(e.GetErrorInt ());
	}
	catch (...)
	{
		OnError(0);
	}
}

bool CBaseUsb::StartReadDataFromDevice ()
{
	m_hReadDataFromDriver = CreateThread(NULL, 0, stReadDataFromDevice, this, 0, &m_dwReadDataFromDriver);
	return (m_hReadDataFromDriver != INVALID_HANDLE_VALUE);
}

int CBaseUsb::GetCompressionType () const
{
	if (m_aptrCompressor.get())
	{
		return m_aptrCompressor->GetCompressionType();
	}
	return 0;
}

CSecurity* CBaseUsb::GetSecuritySettings ()
{
	if (m_State != Handshake)
		return &m_security;
	else
		return NULL;
}

LONG CBaseUsb::GetType (const Transport::TCP_PACKAGE *pPackage)
{
	return pPackage->Type & (~Transport::TYPE_CompressedFlag);
}

bool CBaseUsb::Auth (Transport::TPtrTransport pTr)
{
	if (IsAuthEnable ())
	{
		if (!m_security.authBeginConnection)
		{
			return false;
		}
		if (m_security.authBeginConnection ((SOCKET)pTr->GetHandle(), CStringW(m_security.m_strPassw).GetBuffer ()))
		{
			return false;
		}

		return true;
	}

	return false;

}

bool CBaseUsb::Crypt (Transport::TPtrTransport pTr)
{
	if (IsCryptEnable ())
	{
		if (!m_security.cryptBeginConnection || m_security.m_pContext)
		{
			return false;
		}
		if (m_security.cryptBeginConnection ((SOCKET)pTr->GetHandle(), L"", &m_security.m_pContext))
		{
			return false;
		}

		return true;
	}

	return false;
}

CString CBaseUsb::GetVaribleName () const
{
	CString strRet;

	if (GetHostName().GetLength())
	{
		strRet = GetHostName() + L":";
	}
	strRet += m_settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);

	return strRet;
}

// see dllmain.h constants U2EC_STATE_xxx
int CBaseUsb::GetStatusForDll () const
{
	switch (m_State)
	{
	case Connecting:
	case Handshake:
		return U2EC_STATE_WAITING;

	case Connected:
		return U2EC_STATE_CONNECTED;

	case Created:
	case Deleting:
	case Empty:
	default:
		return U2EC_STATE_DISCONNECT;
	}
}

// return true if status is either wait/establish new connection, or already connected
bool CBaseUsb::IsConnectionActive () const
{
	switch (GetStatus ())
	{
	case CBaseUsb::Connecting:
	case CBaseUsb::Handshake:
	case CBaseUsb::Connected:
		return true;

	default:
		return false;
	}
}

CString CBaseUsb::GetNetSettings() const
{
	CString strNetSettings;
	strNetSettings  =	m_settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_REMOTE_HOST]);
	strNetSettings +=	":";
	strNetSettings +=	m_settings.GetParamStr (Consts::szUngNetSettings [Consts::UNS_TCP_PORT]);
	return strNetSettings;
}
