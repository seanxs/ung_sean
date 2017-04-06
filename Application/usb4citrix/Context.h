/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Context.h

Environment:

    User mode

--*/

#pragma once
#include "..\Consts\Lock.h"

#include "Cchannel.h"


#include <boost\shared_ptr.hpp>
#include <list>
#include <vector>

extern "C" 
{
#include "ica.h"
#include "ica-c2h.h"
#include "wdapi.h"
#include "vdapi.h"
#include "vd.h"
}


class TEventBuffer
{
public:
	LPVOID	lpBuffer;
	long	Offset;
	long	Size;
	LPVOID	lpData;
};

typedef TEventBuffer* TPtrEventBuffer;
typedef std::list <TEventBuffer*> TListEventBuffer;

class TCitrix
{
public:
	TCitrix () ;
	~TCitrix ();

	std::string					strName;
	PCHANNEL_INIT_EVENT_FN		pfEvent;
	PCHANNEL_OPEN_EVENT_FN		pfEventOpen;
	PVD							pVd;
	USHORT						uVirtualChannelNum;
	PVOID						pWd;                     // returned when we register our hook
	PQUEUEVIRTUALWRITEPROC		pQueueVirtualWrite;		// returned when we register our hook
	BOOL						fIsHpc;					// T: The engine is HPC
	PSENDDATAPROC				pSendData;				// pointer to the HPC engine SendData function.  Returned when we register our hook.
	ULONG						ulUserData;
	CRITICAL_SECTION			m_cs;
	USHORT						MaxSize;

	TPtrEventBuffer GetBuffer ();
	void FreeBuffer (TPtrEventBuffer pBuffer);
	void DeleteBuffer ();

	bool IsEmpty () {return (arrUsed.size () == 0);}

	TListEventBuffer			arrFree;
	TListEventBuffer			arrUsed;
};

typedef boost::shared_ptr <TCitrix> TPtrCitrix;
typedef std::vector <TPtrCitrix> TArrCitrix;

extern TPtrCitrix	pCitrix;
extern CLock		lockCon;
