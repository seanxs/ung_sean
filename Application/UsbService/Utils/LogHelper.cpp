/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    LogHelper.cpp

Abstract:

	Helper functions to send debug log messages to user log.
	Using only for development.

Environment:

    User mode

--*/

#include "stdafx.h"
#include <atlstr.h>
#include "LogHelper.h"
#include "Manager.h"

#define SHOW_THREAD_ID_IN_LOG

namespace ung { namespace log {

enum LogLevels {
	LOGLEVEL_DISABLED	= 0,	// constant for logger itself: level to disable all
	LOGLEVEL_ERROR		= 1,
	LOGLEVEL_WARNING	= 2,
	LOGLEVEL_INFO		= 3,
	LOGLEVEL_DEBUG		= 4,
	LOGLEVEL_VERBOSE	= 5,
	LOGLEVEL_ALL		= 6,	// constant for logger itself: level to enable all
};

// this constant defines which level of messages we actually accept to write into real log
const int EnabledLevel = LOGLEVEL_ALL;

LPCTSTR get_log_level_prefix(const int level)
{
	switch (level)
	{
	case LOGLEVEL_ERROR:	return _T("[E] ");
	case LOGLEVEL_DEBUG:	return _T("[d] ");
	case LOGLEVEL_VERBOSE:	return _T("[v] ");
	default:				return _T("    ");	
	}
}

void to_log(const CString &msg)
{
#ifdef SHOW_THREAD_ID_IN_LOG
	// with thread support
	CString str;
	//str.Format(_T("[process %u | thread %u] %s"), GetCurrentProcessId(), GetCurrentThreadId(), msg);
	str.Format(_T("[thread %u] %s"), GetCurrentThreadId(), msg);
	CManager::AddLogFile (str);
#else
	CManager::AddLogFile (msg);
#endif
}

void log_message(const int level, const char *msg)
{
	if (level > EnabledLevel)
		return;

	// convert to CString
	const CString str = CString(get_log_level_prefix(level)) + CString(CA2CT(msg, CP_UTF8));
	to_log(str);
}

void log_message2(const int level, const char *msg1, const char *msg2)
{
	if (level > EnabledLevel)
		return;

	// convert to CString
	const CString str = CString(get_log_level_prefix(level)) 
		+ CString(CA2CT(msg1, CP_UTF8)) + CString(CA2CT(msg2, CP_UTF8));
	to_log(str);
}

void log_message_int(const int level, const char *msg, const int value)
{
	if (level > EnabledLevel)
		return;

	// convert to CString
	CString str = CString(get_log_level_prefix(level)) + CString(CA2CT(msg, CP_UTF8));
	str.AppendFormat(_T("%d"), value);
	to_log(str);
}

void log_message_hex(const int level, const char *msg, const int value)
{
	if (level > EnabledLevel)
		return;

	// convert to CString
	CString str = CString(get_log_level_prefix(level)) + CString(CA2CT(msg, CP_UTF8));
	str.AppendFormat(_T("0x%X"), value);
	to_log(str);
}

void log_message_int2(const int level, const char *msg, const int value1, const int value2)
{
	if (level > EnabledLevel)
		return;

	// convert to CString
	CString str = CString(get_log_level_prefix(level)) + CString(CA2CT(msg, CP_UTF8));
	str.AppendFormat(_T("%d, %d"), value1, value2);
	to_log(str);
}

void error(const char *msg)
{
	log_message(LOGLEVEL_ERROR, msg);
}

void debug(const char *msg)
{
	log_message(LOGLEVEL_DEBUG, msg);
}

void verbose(const char *msg)
{
	log_message(LOGLEVEL_VERBOSE, msg);
}

void debug2(const char *msg1, const char *msg2)
{
	log_message2(LOGLEVEL_DEBUG, msg1, msg2);
}

void verbose2(const char *msg1, const char *msg2)
{
	log_message2(LOGLEVEL_VERBOSE, msg1, msg2);
}

void verbose2(const char *msg1, const CString &str)
{
	const CStringA utf8 = CT2CA(str, CP_UTF8);
	verbose2(msg1, (LPCSTR)utf8);
}

void debugint(const char *msg, const int value)
{
	log_message_int(LOGLEVEL_DEBUG, msg, value);
}

void verboseint(const char *msg, const int value)
{
	log_message_int(LOGLEVEL_VERBOSE, msg, value);
}

void verbosehex(const char *msg, const int value)
{
	log_message_hex(LOGLEVEL_VERBOSE, msg, value);
}

void verboseint2(const char *msg, const int value1, const int value2)
{
	log_message_int2(LOGLEVEL_VERBOSE, msg, value1, value2);
}

}} // namespace ung { namespace log {
