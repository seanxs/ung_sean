/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   SecurityUtils.cpp

Environment:

    Kernel mode

--*/

#include "SecurityUtils.h"
#include "Buffer.h"
#include "SystemEnum.h"

PACL CSecurityUtils::BuildSecurityDescriptor (PSID pSid, SECURITY_DESCRIPTOR &sd)
{
	ULONG aclSize = 0;
	PACL pAcl = NULL;
	NTSTATUS status;

	if (!pSid)
	{
		return NULL;
	}

	//
	// Calculate how big our ACL needs to be to support one ACE (in
	// this case we'll use a predefined Se export for local system
	// as the SID)
	//
	aclSize = sizeof(ACL);
	aclSize += RtlLengthSid(SeExports->SeLocalSystemSid);
	aclSize += RtlLengthSid(SeExports->SeLocalServiceSid);
	aclSize += RtlLengthSid(SeExports->SeNetworkServiceSid);
	aclSize += RtlLengthSid(pSid);

	//
	// Room for 3 ACEs (one for each SID above); note we don't
	// include the end of the structure as we've accounted for that
	// length in the SID lengths already.
	//
	aclSize += 4 * FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart);

	pAcl = (PACL) ExAllocatePoolWithTag(PagedPool, aclSize, 'ds');

	do 
	{
		status = RtlCreateAcl(pAcl, aclSize, ACL_REVISION);
		if (!NT_SUCCESS(status)) 
		{
			break;
		}

		status = RtlAddAccessAllowedAce(pAcl,
			ACL_REVISION,
			GENERIC_READ | GENERIC_WRITE | DELETE,
			SeExports->SeLocalSystemSid);
		if (!NT_SUCCESS(status)) 
		{
			break;
		}

		status = RtlAddAccessAllowedAce(pAcl,
			ACL_REVISION,
			GENERIC_READ | GENERIC_WRITE | DELETE,
			SeExports->SeLocalServiceSid);
		if (!NT_SUCCESS(status)) 
		{
			break;
		}

		status = RtlAddAccessAllowedAce(pAcl,
			ACL_REVISION,
			GENERIC_READ | GENERIC_WRITE | DELETE,
			SeExports->SeNetworkServiceSid);
		if (!NT_SUCCESS(status)) 
		{
			break;
		}

		status = RtlAddAccessAllowedAce(pAcl,
			ACL_REVISION,
			GENERIC_READ | GENERIC_WRITE | DELETE,
			pSid );
		if (!NT_SUCCESS(status)) 
		{
			break;
		}

		//
		// Create a security descriptor
		//
		status = RtlCreateSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
		if (!NT_SUCCESS(status)) 
		{
			break;
		}

		//
		// Associate the above pAcl with the security descriptor
		//
		status = RtlSetDaclSecurityDescriptor(&sd, TRUE, pAcl, FALSE);
		if (!NT_SUCCESS(status)) 
		{
			break;
		}

		return pAcl;
	}while (false);

	if (pAcl != NULL) 
	{
		ExFreePool(pAcl);
		pAcl = NULL;
	}

	return pAcl;
}

bool CSecurityUtils::GetSecurityDescriptor(PDEVICE_OBJECT pDev, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG &bLength)
{
	HANDLE		hDev;
	NTSTATUS	status;
	ULONG		lNeed;

	status = ObOpenObjectByPointer(pDev,
		OBJ_KERNEL_HANDLE,
		NULL,
		READ_CONTROL,
		0,
		KernelMode,
		&hDev);

	if (!NT_SUCCESS (status))
	{
		return false;
	}

	status = ZwQuerySecurityObject (hDev, DACL_SECURITY_INFORMATION, SecurityDescriptor, bLength, &lNeed);

	if (STATUS_BUFFER_TOO_SMALL == status)
	{
		bLength = lNeed;
	}
	if (!NT_SUCCESS (status))
	{
		ZwClose (hDev);
		return false;
	}

	ZwClose (hDev);

	return true;
}

bool CSecurityUtils::SetSecurityDescriptor(PDEVICE_OBJECT pDev, PSECURITY_DESCRIPTOR SecurityDescriptor)
{
	HANDLE		hDev;
	NTSTATUS	status;
	ULONG		lNeed;

#if DBG
	CBuffer				Buffer (NonPagedPool, 1024);
	UNICODE_STRING		String;
	CBaseClass			Class;

	RtlZeroMemory (Buffer.GetData(), Buffer.GetBufferSize());
	String.Buffer = (PWCHAR)Buffer.GetData();
	String.Length = 0;
	String.MaximumLength = (USHORT)Buffer.GetBufferSize();
	Class.SetDebugLevel(CBaseClass::DebugSpecial);

	if (CSystemEnum::GetPathByObject(pDev, &String))
	{
		String.Buffer = String.Buffer;
	}
#endif

	status = ObOpenObjectByPointer(pDev,
		OBJ_KERNEL_HANDLE,
		NULL,
		WRITE_DAC,
		0,
		KernelMode,
		&hDev);

	if (!NT_SUCCESS (status))
	{
		return false;
	}

	status = ZwSetSecurityObject (hDev, DACL_SECURITY_INFORMATION, SecurityDescriptor);

	if (!NT_SUCCESS (status))
	{
#if DBG
		Class.DebugPrint (CBaseClass::DebugSpecial, "SetSecurityDescriptor false %S", String.Buffer);
#endif
		ZwClose (hDev);
		return false;
	}
#if DBG
	Class.DebugPrint (CBaseClass::DebugSpecial, "SetSecurityDescriptor true %S", String.Buffer);
#endif


	ZwClose (hDev);

	return true;
}