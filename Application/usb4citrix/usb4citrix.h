/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   usb4citrix.h

Environment:

    User mode

--*/

#include "stdafx.h"
#include "citrix.h"

extern "C" 
{
#include "ica.h"
#include "ica-c2h.h"
#include "wdapi.h"
#include "vdapi.h"
#include "vd.h"
}

extern "C" 
{
int _stdcall DriverOpen( PVD, PVDOPEN, PUINT16 );
int _stdcall DriverClose( PVD, PDLLCLOSE, PUINT16 );
int _stdcall DriverInfo( PVD, PDLLINFO, PUINT16 );
int _stdcall DriverPoll( PVD, PDLLPOLL, PUINT16 );
int _stdcall DriverQueryInformation( PVD, PVDQUERYINFORMATION, PUINT16 );
int _stdcall DriverSetInformation( PVD, PVDSETINFORMATION, PUINT16 );
int _stdcall DriverGetLastError( PVD, PVDLASTERROR );
void WFCAPI ICADataArrival( PVOID, USHORT, LPBYTE, USHORT );
};

