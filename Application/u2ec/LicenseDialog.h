/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   LicenseDialog.h

Environment:

    User mode

--*/

#pragma once

// NOTE: it supposed to be used in a single instance at a time
class CLicenseDialogNonMfc
{
public:
	CLicenseDialogNonMfc();

	INT_PTR DoModal (HINSTANCE dllHinstance, HWND parentHwnd);

protected:
	static INT_PTR CALLBACK DialogProc (HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	static CString GetEditText (HWND hwndDlg, int idEdit);
	static void    OnChangeEditName (HWND hwndDlg);
	static void    OnChangeEditCode (HWND hwndDlg);

public:
	static CString strUserName;
	static CString strUserCode;
};
