/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   usb4citrix.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "usb4citrix.h"
#include "usb4rdpstub.h"
#include "clterr.h"
#include "context.h"


#include "miapi.h"

/*******************************************************************************
 *
 *  DriverOpen
 *
 *    Called once to set up things.
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to virtual driver data structure
 *    pVdOpen (input/output)
 *       pointer to the structure VDOPEN
 *    puiSize (Output)
 *       size of VDOPEN structure.
 *
 * EXIT:
 *    CLIENT_STATUS_SUCCESS - no error
 *    CLIENT_ERROR_NO_MEMORY - could not allocate data buffer
 *    On other errors, an error code is returned from lower level functions.
 *
 ******************************************************************************/

int _stdcall DriverOpen(PVD pVd, PVDOPEN pVdOpen, PUINT16 puiSize)
{
	if (!hUsb4rdp)
	{
		hUsb4rdp = LoadLibrary (_T("usb4rdp32.dll"));
		if (hUsb4rdp)
		{
			pfVirtualChannelEntry = (VirtualChannelEntry)GetProcAddress (hUsb4rdp, "VirtualChannelEntry");

			CHANNEL_ENTRY_POINTS ChannelEntryPoints;

			ChannelEntryPoints.cbSize = sizeof (CHANNEL_ENTRY_POINTS);
			ChannelEntryPoints.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
			ChannelEntryPoints.pVirtualChannelInit = VirtualChannelInit;
			ChannelEntryPoints.pVirtualChannelOpen = VirtualChannelOpen;
			ChannelEntryPoints.pVirtualChannelClose = VirtualChannelClose;
			ChannelEntryPoints.pVirtualChannelWrite = VirtualChannelWrite;

			if (pfVirtualChannelEntry)
			{
				if (pfVirtualChannelEntry (&ChannelEntryPoints))
				{
					if (pCitrix)
					{
						pCitrix->pfEvent (pCitrix.get (), CHANNEL_EVENT_INITIALIZED, NULL, 0);
					}
				}
			}
		}
	}

	if (!pCitrix)
	{
		return CLIENT_ERROR;
	}

	pCitrix->pVd = pVd;
	pCitrix->pfEvent (pCitrix.get (), CHANNEL_EVENT_CONNECTED, L"citrix", sizeof (TCHAR) * (wcslen (L"citrix") + 1));
 
    return CLIENT_STATUS_SUCCESS;
}

/*******************************************************************************
 *
 *  DriverClose
 *
 *  The user interface calls VdClose to close a Vd before unloading it.
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to procotol driver data structure
 *    pVdClose (input/output)
 *       pointer to the structure DLLCLOSE
 *    puiSize (input)
 *       size of DLLCLOSE structure.
 *
 * EXIT:
 *    CLIENT_STATUS_SUCCESS - no error
 *
 ******************************************************************************/

int _stdcall DriverClose(PVD pVd, PDLLCLOSE pVdClose, PUINT16 puiSize)
{
	if (!pCitrix)
	{
		return CLIENT_ERROR;
	}

	pCitrix->pVd = pVd;
	pCitrix->pfEvent (pCitrix.get (), CHANNEL_EVENT_DISCONNECTED, L"citrix", wcslen (L"citrix"));

	if (hUsb4rdp)
	{
		FreeLibrary (hUsb4rdp);
		hUsb4rdp = NULL;
		pfVirtualChannelEntry = NULL;
	}

    return CLIENT_STATUS_SUCCESS;
}

/*******************************************************************************
 *
 *  ICADataArrival
 *
 *   A data PDU arrived over our channel.
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to virtual driver data structure
 *
 *    uChan (input)
 *       ICA channel the data is for.
 *
 *    pBuf (input)
 *       Buffer with arriving data packet
 *
 *    Length (input)
 *       Length of the data packet
 *
 * EXIT:
 *       void
 *
 ******************************************************************************/

void WFCAPI ICADataArrival(PVOID pVd, USHORT uChan, LPBYTE pBuf, USHORT Length)
{
	if (pCitrix)
	{
		pCitrix->pfEventOpen (DWORD (pCitrix.get ()), CHANNEL_EVENT_DATA_RECEIVED, pBuf, Length, Length, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST);
	}
	else
	{
		//AtlTrace ("ICADataArrival error\n");
	}
}

/*******************************************************************************
 *
 *  DriverPoll
 *
 *  The Winstation driver calls DriverPoll
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to virtual driver data structure
 *    pVdPoll (input)
 *       pointer to the structure DLLPOLL or DLL_HPC_POLL
 *    puiSize (input)
 *       size of DLLPOOL structure.
 *
 * EXIT:
 *    CLIENT_STATUS_SUCCESS - OK so far.  We will be polled again.
 *    CLIENT_STATUS_NO_DATA - No more data to send.
 *    CLIENT_STATUS_ERROR_RETRY - Could not send the data to the WD's output queue - no space.
 *                                Hopefully, space will be available next time.
 *    Otherwise, a fatal error code is returned from lower level functions.
 *
 *    NOTE:  CLIENT_STATUS_NO_DATA signals the WD that it is OK to stop
 *           polling until another host to client packet comes through.
 *
 * REMARKS:
 *    If polling is enabled (pre-HPC client, or HPC client with polling enabled),
 *    this function is called regularly.  DriverPoll is always called at least once.
 *    Otherwise (HPC with polling disabled), DriverPoll is called only when requested
 *    via WDSET_REQUESTPOLL or SENDDATA_NOTIFY.
 *
 ******************************************************************************/

int _stdcall DriverPoll(PVD pVd, PDLLPOLL pVdPoll, PUINT16 puiSize)
{
    int rc = CLIENT_STATUS_NO_DATA;

	if(pCitrix)
	{
		EnterCriticalSection (&pCitrix->m_cs);
		if (!pCitrix->IsEmpty ())
		{
			if (pCitrix->fIsHpc)
			{
				PDLL_HPC_POLL pVdPollHpc = (PDLL_HPC_POLL)pVdPoll;
				TPtrEventBuffer pBuffer;

				if (pVdPollHpc->pUserData)
				{
					pBuffer = (TPtrEventBuffer)pVdPollHpc->pUserData;
					if (pBuffer != pCitrix->arrUsed.front ())
					{
						ATLTRACE ("Error sent data");
					}
				}
				else
				{
					pBuffer = pCitrix->arrUsed.front ();
				}
				SendDataHpc (pBuffer);
			}
			else
			{
				MEMORY_SECTION MemorySections[1];
				TPtrEventBuffer pBuffer = pCitrix->arrUsed.front ();
				int		iSize = ((pBuffer->Size - pBuffer->Offset) < pCitrix->MaxSize)?(pBuffer->Size - pBuffer->Offset):pCitrix->MaxSize;

				MemorySections [0].length = iSize;
				MemorySections [0].pSection = (LPBYTE)pBuffer->lpBuffer + pBuffer->Offset;

				rc = pCitrix->pQueueVirtualWrite(pCitrix->pWd, pCitrix->uVirtualChannelNum, MemorySections, 1, FLUSH_IMMEDIATELY);
				// Normal status returns are CLIENT_STATUS_SUCCESS (it worked) or CLIENT_ERROR_NO_OUTBUF (no room in output queue)

				if(CLIENT_STATUS_SUCCESS == rc)
				{
					pBuffer->Offset += iSize;
					if (pBuffer->Offset >= pBuffer->Size)
					{
						//AtlTrace ("DriverPoll completed %d %d %d\n", iSize, pBuffer->Offset, pBuffer->Size);
						pCitrix->pfEventOpen (DWORD (pCitrix.get ()), CHANNEL_EVENT_WRITE_COMPLETE, pBuffer->lpData, 0, 0, 0);
						pCitrix->FreeBuffer (pCitrix->arrUsed.front ());
					}
				}
				else if(CLIENT_ERROR_NO_OUTBUF == rc)
				{
					rc = CLIENT_STATUS_ERROR_RETRY;
				}
			}
		}
		LeaveCriticalSection (&pCitrix->m_cs);
		//AtlTrace (">>DriverPoll\n");
	}

	return rc;
}

/*******************************************************************************
 *
 *  DriverInfo
 *
 *    This routine is called to get module information
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to virtual driver data structure
 *    pVdInfo (output)
 *       pointer to the structure DLLINFO
 *    puiSize (output)
 *       size of DLLINFO structure
 *
 * EXIT:
 *    CLIENT_STATUS_SUCCESS - no error
 *
 ******************************************************************************/

int _stdcall DriverInfo(PVD pVd, PDLLINFO pVdInfo, PUINT16 puiSize)
{
    USHORT		ByteCount;
    PICO_C2H	pVdData;
    PMODULE_C2H pHeader;
    PVDFLOW pFlow;

    ByteCount = sizeof(ICO_C2H);

    *puiSize = sizeof(DLLINFO);

    // Check if buffer is big enough
    // If not, the caller is probably trying to determine the required
    // buffer size, so return it in ByteCount.

    if(pVdInfo->ByteCount < ByteCount)
	{
        pVdInfo->ByteCount = ByteCount;
        return(CLIENT_ERROR_BUFFER_TOO_SMALL);
    }

    // Initialize default data

    pVdInfo->ByteCount = ByteCount;
    pVdData = (PICO_C2H) pVdInfo->pBuffer;

    // Initialize module header

    pHeader = &pVdData->Header.Header;
	pHeader = (PMODULE_C2H) pVdInfo->pBuffer;
    pHeader->ByteCount = ByteCount;
    pHeader->ModuleClass = Module_VirtualDriver;

    pHeader->VersionL = 1;
    pHeader->VersionH = 1;

    //strcpy((char*)(pHeader->HostModuleName), "ICA"); // max 8 characters (unsafe)
    strcpy_s((char*)(pHeader->HostModuleName), sizeof(pHeader->HostModuleName), "ICA"); // max 8 characters

    // Initialize virtual driver header

    pFlow = &pVdData->Header.Flow;
    pFlow->BandwidthQuota = 0;
    pFlow->Flow = VirtualFlow_None;

    // add our own data

	pVdData->usMaxDataSize = pCitrix?pCitrix->MaxSize:0;

    pVdInfo->ByteCount = ByteCount;

    return(CLIENT_STATUS_SUCCESS);
}

/*******************************************************************************
 *
 *  DriverQueryInformation
 *
 *   Required vd function.
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to virtual driver data structure
 *    pVdQueryInformation (input/output)
 *       pointer to the structure VDQUERYINFORMATION
 *    puiSize (output)
 *       size of VDQUERYINFORMATION structure
 *
 * EXIT:
 *    CLIENT_STATUS_SUCCESS - no error
 *
 ******************************************************************************/

int _stdcall DriverQueryInformation(PVD pVd, PVDQUERYINFORMATION pVdQueryInformation, PUINT16 puiSize)
{
    //TRACE((TC_VD, TT_API1, "VDPING: DriverQueryInformation entered"));
    //TRACE((TC_VD, TT_API2, "pVdQueryInformation->VdInformationClass = %d",	pVdQueryInformation->VdInformationClass));

    *puiSize = sizeof(VDQUERYINFORMATION);

    return(CLIENT_STATUS_SUCCESS);
}

/*******************************************************************************
 *
 *  DriverSetInformation
 *
 *   Required vd function.
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to virtual driver data structure
 *    pVdSetInformation (input/output)
 *       pointer to the structure VDSETINFORMATION
 *    puiSize (input)
 *       size of VDSETINFORMATION structure
 *
 * EXIT:
 *    CLIENT_STATUS_SUCCESS - no error
 *
 ******************************************************************************/

int _stdcall DriverSetInformation(PVD pVd, PVDSETINFORMATION pVdSetInformation, PUINT16 puiSize)
{
    //TRACE((TC_VD, TT_API1, "VDPING: DriverSetInformation entered"));
    //TRACE((TC_VD, TT_API2, "pVdSetInformation->VdInformationClass = %d", pVdSetInformation->VdInformationClass));

   return(CLIENT_STATUS_SUCCESS);
}

/*******************************************************************************
 *
 *  DriverGetLastError
 *
 *   Queries error data.
 *   Required vd function.
 *
 * ENTRY:
 *    pVd (input)
 *       pointer to virtual driver data structure
 *    pLastError (output)
 *       pointer to the error structure to return (message is currently always
 *       NULL)
 *
 * EXIT:
 *    CLIENT_STATUS_SUCCESS - no error
 *
 ******************************************************************************/

int _stdcall DriverGetLastError(PVD pVd, PVDLASTERROR pLastError)
{
    //TRACE((TC_VD, TT_API1, "VDPING: DriverGetLastError entered"));

    // Interpret last error and pass back code and string data

    pLastError->Error = pVd->LastError;
    pLastError->Message[0] = '\0';
    return(CLIENT_STATUS_SUCCESS);
}
