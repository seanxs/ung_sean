/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ServiceManager.h

Environment:

    User mode

--*/

#pragma once

class CServiceManager
{
	CLog m_Log;
	HANDLE m_hRemoteControlMutex;
	HANDLE CreateRemoteControlMutex(LPCWSTR wsMutexName);

protected:
	SOCKET		m_sock;
	CString		m_strHost;
	USHORT		m_uPort;
	TCHAR		m_chEnd;
	BOOL		m_bAlreadyMes;

public:
	CServiceManager(void);
	~CServiceManager(void);

	void SetCharEnd (TCHAR chEnd);
	bool OpenConnection (LPCTSTR szHost, USHORT uPort);
	void CloseConnection ();
	virtual bool SendCommand (LPCSTR szCommand, int iSize, CStringA *strAnswer = NULL);
	virtual bool ParseAnsver (LPCSTR szAnswer);
};
