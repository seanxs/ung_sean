/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   usb4rdpstub.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "usb4rdpstub.h"
#include "usb4citrix.h"
#include "clterr.h"
#include "Context.h"

HMODULE	hUsb4rdp = NULL;
VirtualChannelEntry pfVirtualChannelEntry = NULL;

UINT VCAPITYPE VirtualChannelInit(LPVOID FAR *ppInitHandle, PCHANNEL_DEF pChannel, INT channelCount, ULONG versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	pCitrix = TPtrCitrix (new TCitrix ());
	pCitrix->strName = pChannel->name; 
	pCitrix->pfEvent = pChannelInitEventProc;
	*ppInitHandle = pCitrix.get ();
	
	return CHANNEL_RC_OK;
}

UINT VCAPITYPE VirtualChannelOpen(LPVOID pInitHandle, LPDWORD pOpenHandle, PCHAR pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
{
	WDQUERYINFORMATION wdqi;
    VDWRITEHOOK        vdwh;
    WDSETINFORMATION   wdsi;
    UINT16             uiSize;
    OPENVIRTUALCHANNEL OpenVirtualChannel;
	VDWRITEHOOKEX      vdwhex;					// struct for getting more engine information, used by HPC
	int                rc;

	*pOpenHandle = (DWORD)pCitrix.get ();
	pCitrix->pfEventOpen = pChannelOpenEventProc;
    // Get a virtual channel
    wdqi.WdInformationClass = WdOpenVirtualChannel;
    wdqi.pWdInformation = (PVOID)&OpenVirtualChannel;
    wdqi.WdInformationLength = sizeof(OPENVIRTUALCHANNEL);
    OpenVirtualChannel.pVCName = (PVOID)pCitrix->strName.c_str ();

    uiSize = sizeof(WDQUERYINFORMATION);

    rc = VdCallWd(pCitrix->pVd, WDxQUERYINFORMATION, &wdqi, &uiSize);
    if(rc != CLIENT_STATUS_SUCCESS)
    {
        return CHANNEL_RC_BAD_CHANNEL;
    }

	pCitrix->uVirtualChannelNum = OpenVirtualChannel.Channel;
	pCitrix->pVd->pPrivate   = pCitrix.get (); /* pointer to private data, if needed */

    // Register write hooks for our virtual channel
    vdwh.Type  = pCitrix->uVirtualChannelNum;
	vdwh.pVdData = pCitrix->pVd;
    vdwh.pProc = (PVDWRITEPROCEDURE) ICADataArrival;
    wdsi.WdInformationClass  = WdVirtualWriteHook;
    wdsi.pWdInformation      = &vdwh;
    wdsi.WdInformationLength = sizeof(VDWRITEHOOK);
    uiSize                   = sizeof(WDSETINFORMATION);

    rc = VdCallWd(pCitrix->pVd, WDxSETINFORMATION, &wdsi, &uiSize);

    if(CLIENT_STATUS_SUCCESS != rc)
    {
        return CHANNEL_RC_BAD_CHANNEL;
    }

    pCitrix->pWd = vdwh.pWdData;										// get the pointer to the WD data
	pCitrix->MaxSize = vdwh.MaximumWriteSize - 1;
	//pCitrix->MaxSize = (pCitrix->MaxSize > 2000)?2000:pCitrix->MaxSize;
    pCitrix->pQueueVirtualWrite = vdwh.pQueueVirtualWriteProc;			// grab pointer to function to use to send data to the host

    // Do extra initialization to determine if we are talking to an HPC client

    wdsi.WdInformationClass = WdVirtualWriteHookEx;
    wdsi.pWdInformation = (PVOID)&vdwhex;
    wdsi.WdInformationLength = sizeof(VDWRITEHOOKEX);
    vdwhex.usVersion = HPC_VD_API_VERSION_LEGACY;				// Set version to 0; older clients will do nothing

    rc = VdCallWd(pCitrix->pVd, WDxQUERYINFORMATION, &wdsi, &uiSize);

    if(CLIENT_STATUS_SUCCESS == rc)
	{
		pCitrix->fIsHpc = (HPC_VD_API_VERSION_LEGACY != vdwhex.usVersion);	// if version returned, this is HPC or later
		pCitrix->pSendData = vdwhex.pSendDataProc;         // save HPC SendData API address
   
		// If it is an HPC client, tell it the highest version of the HPC API we support.

		if(pCitrix->fIsHpc)
		{
		   WDSET_HPC_PROPERITES hpcProperties;

		   hpcProperties.usVersion = HPC_VD_API_VERSION_V1;
		   hpcProperties.pWdData = pCitrix->pWd;
		   hpcProperties.ulVdOptions = HPC_VD_OPTIONS_NO_POLLING;
		   wdsi.WdInformationClass = WdHpcProperties;
		   wdsi.pWdInformation = &hpcProperties;
		   wdsi.WdInformationLength = sizeof(WDSET_HPC_PROPERITES);

		   rc = VdCallWd(pCitrix->pVd, WDxSETINFORMATION, &wdsi, &uiSize);
		   if(CLIENT_STATUS_SUCCESS != rc)
		   {
			   return CHANNEL_RC_BAD_CHANNEL;
		   }
		}
	}
	else
	{
		pCitrix->fIsHpc = FALSE;
		pCitrix->pSendData = NULL;
	}


	return CHANNEL_RC_OK;
}

UINT VCAPITYPE VirtualChannelClose(DWORD openHandle)
{
	// cloase
	return CHANNEL_RC_OK;
}

UINT VCAPITYPE VirtualChannelWrite(DWORD openHandle, LPVOID pData, ULONG dataLength, LPVOID pUserData)
{
	if (DWORD (pCitrix.get ()) == openHandle)
	{

		if (pCitrix->fIsHpc)
		{
			EnterCriticalSection (&pCitrix->m_cs);
			TPtrEventBuffer pBuff = pCitrix->GetBuffer ();
			pBuff->lpBuffer = pData;
			pBuff->Size = dataLength;
			pBuff->lpData = pUserData;

			if (pCitrix->arrUsed.size () == 1)
			{
				SendDataHpc (pBuff);
			}
			LeaveCriticalSection (&pCitrix->m_cs);
		}
		else
		{
			EnterCriticalSection (&pCitrix->m_cs);
			TPtrEventBuffer pBuff = pCitrix->GetBuffer ();

			pBuff->lpBuffer = pData;
			pBuff->Size = dataLength;
			pBuff->lpData = pUserData;
			LeaveCriticalSection (&pCitrix->m_cs);
		}

		return CHANNEL_RC_OK;
	}

	AtlTrace ("VirtualChannelWrite error\n");
	return CHANNEL_RC_NO_BUFFER;
}
	
int SendDataHpc (TPtrEventBuffer pBuffer)
{
	int iRet = CLIENT_STATUS_NO_DATA;
	while (pBuffer->Size > pBuffer->Offset)
	{
		USHORT uLen = USHORT((pBuffer->Size - pBuffer->Offset > pCitrix->MaxSize)?pCitrix->MaxSize:(pBuffer->Size - pBuffer->Offset));
		iRet = pCitrix->pSendData ((DWORD)pCitrix->pWd, pCitrix->uVirtualChannelNum, (LPBYTE)pBuffer->lpBuffer + pBuffer->Offset, uLen, pBuffer, SENDDATA_NOTIFY);
		switch (iRet)
		{
		case CLIENT_ERROR_NO_OUTBUF:
			pBuffer->Offset += uLen;
			break;
		case CLIENT_ERROR_BUFFER_STILL_BUSY:
			pBuffer->Offset += uLen;
		case CLIENT_STATUS_NO_DATA:
			iRet = CLIENT_STATUS_ERROR_RETRY;
			break;
		}
	}

	if (pBuffer->Offset >= pBuffer->Size)
	{
		pCitrix->pfEventOpen (DWORD (pCitrix.get ()), CHANNEL_EVENT_WRITE_COMPLETE, pBuffer->lpData, 0, 0, 0);
		pCitrix->FreeBuffer (pCitrix->arrUsed.front ());
	}

	return iRet;
}