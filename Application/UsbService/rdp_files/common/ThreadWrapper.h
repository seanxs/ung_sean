/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ThreadWrapper.h

Environment:

    User mode

--*/
#include "..\Consts\log.h"

class CThreadWrapper 
{
	HANDLE m_hWrapThread;
	DWORD  m_dwWrapThreadId;
	static DWORD WINAPI WrapThread(LPVOID pThreadParameter);
	virtual void WrapRoutine(LPVOID pCallerParam);

	CLog logger;

protected:
	CThreadWrapper();
	virtual ~CThreadWrapper();

public:
	void CreateWrapThread(LPVOID pCallerParam);

};

typedef struct _THREAD_WRAPPER_PARAMS 
{
	LPVOID pCallerParam;
	CThreadWrapper *pClassInstance;
} THREAD_WRAPPER_PARAMS, *PTHREAD_WRAPPER_PARAMS;
