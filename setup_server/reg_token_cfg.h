/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    reg_token_cfg.h

Abstract:

	Where to store tokens in windows registry for virtual USB projects.
	See reg_token_array.h

Environment:

   user mode

--*/

#pragma once

// registry location to save token
#define ROOT_KEY    HKEY_LOCAL_MACHINE
#define SUB_KEY     _T("SOFTWARE\\ELTIMA Software")
#define VALUE_NAME  _T("usb_driver_clients")
