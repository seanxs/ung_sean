/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Context.cpp

Environment:

    User mode

--*/

#include "StdAfx.h"
#include "Context.h"

CLock		lockCon;
TPtrCitrix	pCitrix;

TCitrix::TCitrix () 
	: pWd (NULL)
	, pQueueVirtualWrite (NULL)
	, pSendData (NULL)
	, ulUserData (0xCAACCAAC)
	, pfEventOpen (NULL)  
{
	InitializeCriticalSection (&m_cs);
}
TCitrix::~TCitrix ()
{
	DeleteBuffer ();
	DeleteCriticalSection (&m_cs);
}

TPtrEventBuffer TCitrix::GetBuffer ()
{
	TPtrEventBuffer Buffer;
	EnterCriticalSection (&m_cs);
	if (arrFree.size ())
	{
		Buffer = arrFree.front ();
		arrFree.erase (arrFree.begin ());
	}
	else
	{
		Buffer = TPtrEventBuffer (new TEventBuffer);
	}

	// clear
	Buffer->Offset = 0;
	Buffer->Size = 0;
	Buffer->lpBuffer = NULL;

	arrUsed.push_back (Buffer);
	LeaveCriticalSection (&m_cs);

	return Buffer;
}

void TCitrix::FreeBuffer (TPtrEventBuffer pBuffer)
{
	TListEventBuffer::iterator item;
	EnterCriticalSection (&m_cs);
	for (item = arrUsed.begin (); item != arrUsed.end (); item++)
	{
		if (*item == pBuffer)
		{
			pBuffer->lpData = NULL;
			arrFree.push_back (pBuffer);
			arrUsed.erase (item);
			break;
		}
	}
	LeaveCriticalSection (&m_cs);
}

void TCitrix::DeleteBuffer ()
{
	EnterCriticalSection (&m_cs);
	while (arrUsed.size ())
	{
		FreeBuffer (arrUsed.front ());
	}
	LeaveCriticalSection (&m_cs);
	arrFree.clear ();
}