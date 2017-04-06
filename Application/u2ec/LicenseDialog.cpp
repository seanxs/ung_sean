/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   LicenseDialog.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "LicenseDialog.h"
#include "resource.h"

//------------------------------------------------------------------
// class CLicenseDialogNonMfc

// static class variables
CString CLicenseDialogNonMfc::strUserName;
CString CLicenseDialogNonMfc::strUserCode;

CLicenseDialogNonMfc::CLicenseDialogNonMfc()
{
	// reset variables
	strUserName = _T("");
	strUserCode = _T("");
}

INT_PTR CLicenseDialogNonMfc::DoModal (HINSTANCE dllHinstance, HWND parentHwnd)
{
	const INT_PTR result = ::DialogBox (
		dllHinstance, 
		MAKEINTRESOURCE (IDD_ARMADILLO_LICENSE),
		parentHwnd,
		CLicenseDialogNonMfc::DialogProc);
	return result;
}

/*static*/
INT_PTR CALLBACK CLicenseDialogNonMfc::DialogProc (HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		// set focus to edit name control
		::SetFocus (GetDlgItem (hwndDlg, IDC_EDIT_NAME)); 
		return FALSE;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			// extract values
			CLicenseDialogNonMfc::strUserName = CLicenseDialogNonMfc::GetEditText (hwndDlg, IDC_EDIT_NAME);
			CLicenseDialogNonMfc::strUserCode = CLicenseDialogNonMfc::GetEditText (hwndDlg, IDC_EDIT_CODE);
			if (CLicenseDialogNonMfc::strUserName.IsEmpty ()
				|| CLicenseDialogNonMfc::strUserCode.IsEmpty ())
			{
				::MessageBox (
					hwndDlg, 
					_T("Name or Code field cannot be empty"), 
					_T("Error"), 
					MB_ICONERROR | MB_OK);
				// set focus back to edit name control
				::SetFocus (GetDlgItem (hwndDlg, IDC_EDIT_NAME)); 
			}
			else
				EndDialog (hwndDlg, IDOK);
			return TRUE;

		case IDCANCEL:
			EndDialog (hwndDlg, IDCANCEL);
			return TRUE;

		default:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				switch (LOWORD(wParam))
				{
				case IDC_EDIT_NAME:
					CLicenseDialogNonMfc::OnChangeEditName (hwndDlg);
					break;

				case IDC_EDIT_CODE:
					CLicenseDialogNonMfc::OnChangeEditCode (hwndDlg);
					break;
				}
			}
			break;
		}
		break;
	}

	return FALSE;
}

/*static*/
CString CLicenseDialogNonMfc::GetEditText (HWND hwndDlg, int idEdit)
{
	enum {BUF_SIZE = 1024};
	CString result;
	::SendDlgItemMessage (
		hwndDlg,
		idEdit, 
		WM_GETTEXT, 
		(WPARAM)BUF_SIZE, 
		(LPARAM)result.GetBufferSetLength (BUF_SIZE));
	result.ReleaseBuffer ();
	return result;
}

/*static*/
void CLicenseDialogNonMfc::OnChangeEditName (HWND hwndDlg)
{
	// possibly split name string into 2 fields by CRLF (LF) separator
	CString strName = CLicenseDialogNonMfc::GetEditText (hwndDlg, IDC_EDIT_NAME);
	CString strCode = CLicenseDialogNonMfc::GetEditText (hwndDlg, IDC_EDIT_CODE);

	if (!strCode.IsEmpty ())
	{
		// code filed is not empty, keep it that way
		return;
	}

	const int crLfIndex = strName.Find (_T("\r\n"));
	const int lfOnlyIndex = strName.Find (_T("\n"));

	if (crLfIndex != -1 || lfOnlyIndex != -1)
	{
		strCode = strName.Mid (lfOnlyIndex + 1); // everything after \n
		strName = strName.Left ((crLfIndex != -1) ? crLfIndex : lfOnlyIndex);
		if (strName.IsEmpty ())
		{
			// something wrong
			return;
		}
		::SendDlgItemMessage (hwndDlg, IDC_EDIT_NAME, WM_SETTEXT, (WPARAM)0, (LPARAM)((LPCTSTR)strName));
		::SendDlgItemMessage (hwndDlg, IDC_EDIT_CODE, WM_SETTEXT, (WPARAM)0, (LPARAM)((LPCTSTR)strCode));
	}
}

/*static*/
void CLicenseDialogNonMfc::OnChangeEditCode (HWND hwndDlg)
{
	// possibly remove CRLF at the end
	CString strCode = CLicenseDialogNonMfc::GetEditText (hwndDlg, IDC_EDIT_CODE);
	
	const int crLfIndex = strCode.Find (_T("\r\n"));
	const int lfOnlyIndex = strCode.Find (_T("\n"));
	const int size = strCode.GetLength ();

	if (crLfIndex != -1 && crLfIndex == (size-2))
	{
		strCode = strCode.Left (crLfIndex);
		::SendDlgItemMessage (hwndDlg, IDC_EDIT_CODE, WM_SETTEXT, (WPARAM)0, (LPARAM)((LPCTSTR)strCode));
	}
	else if (lfOnlyIndex != -1 && lfOnlyIndex == (size-1))
	{
		strCode = strCode.Left (lfOnlyIndex);
		::SendDlgItemMessage (hwndDlg, IDC_EDIT_CODE, WM_SETTEXT, (WPARAM)0, (LPARAM)((LPCTSTR)strCode));
	}
}
