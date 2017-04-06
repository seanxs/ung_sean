/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   EventWindow.cpp

Abstract:

   Window class. It is used for subscribing for WM_DEVICECHANGE event and monitoring
   changes of USB devices list

Environment:

    Kernel mode

--*/

#include "stdafx.h"
#include "EventWindow.h"
#include <Dbt.h>
#include "dllmain.h"

LRESULT CEventWindow::OnDeviceChange(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = false;

	{
		if (pOnChangeDevList)
		{
			pOnChangeDevList ();
		}
		bHandled = true;
	}

	return 0;
}
