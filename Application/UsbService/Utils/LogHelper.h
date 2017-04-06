/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   LogHelper.h

Abstract:

	Helper functions to send debug log messages to user log.
	Using only for development.

--*/

#pragma once

namespace ung { namespace log {

void error(const char *msg);
void debug(const char *msg);
void verbose(const char *msg);

void debug2(const char *msg1, const char *msg2);
void verbose2(const char *msg1, const char *msg2);
void verbose2(const char *msg1, const CString &str);

void debugint(const char *msg, const int value);
void verboseint(const char *msg, const int value);
void verbosehex(const char *msg, const int value);

void verboseint2(const char *msg, const int value1, const int value2);

}} // namespace ung { namespace log {
