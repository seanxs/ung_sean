/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   EventWindow.h

Abstract:

   Window class. It is used for subscribing for WM_DEVICECHANGE event and monitoring
   changes of USB devices list

Environment:

    Kernel mode

--*/

#pragma once

#include "resource.h"       // main symbols
#include <atlhost.h>

// CEventWindow
class CEventWindow : 
	public CAxDialogImpl<CEventWindow>
{
public:
	CEventWindow()
	{}

	enum { IDD = IDD_EVENTWINDOW };


BEGIN_MSG_MAP(CEventWindow)
	MESSAGE_HANDLER(WM_DEVICECHANGE, OnDeviceChange)
END_MSG_MAP()

	LRESULT OnDeviceChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};
