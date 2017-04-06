#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows NT 4 or later.
#define _WIN32_WINNT 0x0500		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif						

#include <windows.h>
#include <atlstr.h>
#include <atlcomtime.h>
#include <sddl.h>

#pragma warning( disable : 4200 )
#pragma comment( lib, "setupapi.lib" )
