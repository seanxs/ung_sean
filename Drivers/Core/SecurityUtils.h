/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   SecurityUtils.h

Environment:

    Kernel mode

--*/

#pragma once
#include "BaseClass.h"
class CSecurityUtils
{
private:
	CSecurityUtils(void) {}
	~CSecurityUtils(void) {}

public:
	static PACL BuildSecurityDescriptor (IN PSID pSid, OUT SECURITY_DESCRIPTOR &sd);
	static bool GetSecurityDescriptor(PDEVICE_OBJECT pDev, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG &bLength);
	static bool SetSecurityDescriptor(PDEVICE_OBJECT pDev, PSECURITY_DESCRIPTOR SecurityDescriptor);

};

