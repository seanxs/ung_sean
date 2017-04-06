/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   ThreadWrapper.cpp

Environment:

    User mode

--*/

#include "stdafx.h"
#include "log.h"
#include "ThreadWrapper.h"

CThreadWrapper::CThreadWrapper() {

	logger.SetLogDest(Nirvana);

	ULONG FuncLogArea = LOG_CONSTR;

	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() ENTER");

	m_hWrapThread = NULL;
	m_dwWrapThreadId = 0;

	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() EXIT");
}

CThreadWrapper::~CThreadWrapper() {
	ULONG FuncLogArea = LOG_CONSTR;

	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() ENTER");

	TerminateThread(m_hWrapThread, 0);
	CloseHandle(m_hWrapThread);

	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() EXIT");
}

void CThreadWrapper::CreateWrapThread(LPVOID pCallerParam) {
	ULONG FuncLogArea = LOG_INIT;

	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() ENTER");

	PTHREAD_WRAPPER_PARAMS pParams = new THREAD_WRAPPER_PARAMS;

	pParams->pCallerParam = pCallerParam;
	pParams->pClassInstance = this;

	m_hWrapThread = CreateThread(NULL, 0, WrapThread, pParams, 0, &m_dwWrapThreadId);

	if(m_hWrapThread) {
		logger.LogString(FuncLogArea, LogInfo, __FUNCTION__":() WrapThread created successful. TID = %d", m_dwWrapThreadId);
	}
	else {
		logger.LogString(FuncLogArea, LogError, __FUNCTION__":() CreateThread() failed! GLE = %d", GetLastError());
	}

	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() EXIT");
}

DWORD WINAPI CThreadWrapper::WrapThread(LPVOID pThreadParameter) {
	ULONG FuncLogArea = LOG_THREAD;
	PTHREAD_WRAPPER_PARAMS pParam = (PTHREAD_WRAPPER_PARAMS) pThreadParameter;
	CLog *pLog = &pParam->pClassInstance->logger;

	pLog->LogString(FuncLogArea, LogTrace, __FUNCTION__":() ENTER");

	Sleep(100);

	pParam->pClassInstance->WrapRoutine(pParam->pCallerParam);
	delete pParam;

	pLog->LogString(FuncLogArea, LogTrace, __FUNCTION__":() EXIT");
	return 0;
}

void CThreadWrapper::WrapRoutine(LPVOID pCallerParam) {
	ULONG FuncLogArea = LOG_GENERAL;
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() ENTER");
	logger.LogString(FuncLogArea, LogTrace, __FUNCTION__":() EXIT");
}
