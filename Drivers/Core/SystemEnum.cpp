/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   SystemEnum.cpp

Environment:

    Kernel mode

--*/

#include "SystemEnum.h"
#include "Buffer.h"

#pragma warning(disable : 4995)

CSystemEnum::CSystemEnum(void)
{
}

CSystemEnum::~CSystemEnum(void)
{
}

bool CSystemEnum::EnumDir (PUNICODE_STRING strDir, FEnunCallback funcEnum, PVOID pContext)
{
	OBJECT_ATTRIBUTES		DirAttr;
	NTSTATUS				NtStatus;
	CBuffer					pBuff;
	CBuffer					pPath;
	BOOLEAN					bFirst = TRUE;
	ULONG					ObjectIndex = 0;
	ULONG					LengthReturned = 0;
	ULONG					Index = 0;
	POBJECT_NAMETYPE_INFO	pObjName = NULL;
	PWCHAR					szPath;
	HANDLE					hDir;
	ULONG					i;
	UNICODE_STRING			strPath;
	

	// init
	pBuff.SetSize (1024);
	pObjName = (POBJECT_NAMETYPE_INFO) pBuff.GetData ();
	pPath.SetSize (2048);
	szPath = (PWCHAR)pPath.GetData ();
	bFirst = TRUE;
	ObjectIndex = 0;
	LengthReturned = 0;
	Index = 0;
	InitializeObjectAttributes(&DirAttr, strDir, OBJ_CASE_INSENSITIVE, NULL, NULL);

	// open directory
	NtStatus = ZwOpenDirectoryObject(&hDir, DIRECTORY_QUERY, &DirAttr);
	if (NtStatus == STATUS_SUCCESS)
	{
		// list path
		while (ZwQueryDirectoryObject(hDir, pBuff.GetData (), pBuff.GetBufferSize (), FALSE, bFirst, &ObjectIndex, &LengthReturned) >= 0)
		{
			bFirst = FALSE;
			for (i=0; Index < ObjectIndex; ++Index, ++i)
			{
				RtlStringCchCopyW(szPath, pPath.GetBufferSize () / sizeof (WCHAR) ,strDir->Buffer);
				if ((pPath.GetBufferSize () - wcslen(szPath)*2 - sizeof(WCHAR)) > pObjName[i].ObjectName.Length)
				{
					RtlStringCchCatW(szPath, pPath.GetBufferSize () / sizeof (WCHAR), L"\\");
					RtlStringCchCatW(szPath, pPath.GetBufferSize () / sizeof (WCHAR), pObjName[i].ObjectName.Buffer);

					RtlInitUnicodeString (&strPath, szPath);

					if (funcEnum (pContext, &strPath, &pObjName[i].ObjectType))
					{
						ZwClose (hDir);
						return true;
					}
				}
			}
		}
		ZwClose (hDir);
	}

	return false;
}

PVOID CSystemEnum::GetDeviceByPath (PWSTR pwszObjectName)
{
	int						i;
	UNICODE_STRING			DirString;
	OBJECT_ATTRIBUTES		DirAttr;
	NTSTATUS				NtStatus;
	HANDLE					hDir;
	CBuffer					Buf;
	CBuffer					DriverPath;
	WCHAR					*szDriverPath;
	ULONG					ObjectIndex = 0;
	ULONG					LengthReturned = 0;
	BOOLEAN					bFirst = TRUE;
	ULONG					Index = 0;
	POBJECT_NAMETYPE_INFO	pObjName;
	PDRIVER_OBJECT			pDriverObject = NULL;
	PDEVICE_OBJECT			pDeviceObject = NULL;
	PDEVICE_OBJECT			pResult = NULL;

	ObjectIndex = 0;
	LengthReturned = 0;
	Index = 0;
	bFirst = TRUE;

	Buf.SetSize (1024);
	DriverPath.SetSize (1024);

	szDriverPath = (WCHAR*)DriverPath.GetData ();
	pObjName = (POBJECT_NAMETYPE_INFO) Buf.GetData ();

	RtlInitUnicodeString(&DirString, L"\\Driver");
	InitializeObjectAttributes(&DirAttr, &DirString, OBJ_CASE_INSENSITIVE, NULL, NULL);
	NtStatus = ZwOpenDirectoryObject(&hDir, DIRECTORY_QUERY, &DirAttr);
	if (NtStatus == STATUS_SUCCESS)
	{
		while (ZwQueryDirectoryObject(hDir, Buf.GetData (), Buf.GetBufferSize (), FALSE, bFirst, &ObjectIndex, &LengthReturned) >= 0)
		{
			bFirst = FALSE;
			for (i=0; Index < ObjectIndex; ++Index, ++i)
			{
				static int iSize = wcslen(L"\\Driver\\") + sizeof(WCHAR);

				RtlStringCchCopyW (szDriverPath, DriverPath.GetBufferSize () / sizeof (WCHAR), L"\\Driver\\");
				if ((DriverPath.GetBufferSize () - iSize) > pObjName[i].ObjectName.Length)
				{
					RtlStringCchCatW(szDriverPath, DriverPath.GetBufferSize () / sizeof (WCHAR), pObjName[i].ObjectName.Buffer);

					pDriverObject = (PDRIVER_OBJECT)GetDriverByPath (szDriverPath);
					if (pDriverObject != NULL)
					{
						pDeviceObject = pDriverObject->DeviceObject;
						while (pDeviceObject)
						{
							NtStatus = IoGetDeviceProperty (pDeviceObject, 
								DevicePropertyPhysicalDeviceObjectName,
								DriverPath.GetBufferSize (),
								szDriverPath,
								&LengthReturned);         

							if (NT_SUCCESS (NtStatus))
							{
								if (wcscmp (szDriverPath, pwszObjectName) == 0)
								{
									pResult = pDeviceObject;
									break;
								}
							}
							pDeviceObject = pDeviceObject->NextDevice;
						}
					}
				}
			}
		}
		ZwClose(hDir);
	}

	return pResult;
}

PVOID CSystemEnum::GetDriverByPath (PWSTR pwszObjectName)
{
	PVOID              pObject = NULL;
	UNICODE_STRING     DeviceName;
	NTSTATUS           NtStatus = 0;
	POBJECT_TYPE	   ObjType;
	static ULONG	   uVersion = 0;

	if (uVersion == 0)
	{
		ULONG	   uMinor;
		PsGetVersion (&uVersion, &uMinor, NULL, NULL);
		uVersion *= 100;
		uVersion += uMinor;
	}

	if (!pwszObjectName || KeGetCurrentIrql() > PASSIVE_LEVEL)
		return pObject;


	RtlInitUnicodeString(&DeviceName, pwszObjectName);

	__try
	{
	    if (uVersion < 601)
		{
			ObjType = (POBJECT_TYPE)IoDriverObjectType;
		}
		else
		{
			ObjType = *((POBJECT_TYPE*)IoDriverObjectType); 
		}

		NtStatus = ObReferenceObjectByName(&DeviceName, OBJ_CASE_INSENSITIVE, NULL, 0, ObjType, KernelMode, NULL, (PVOID*)&pObject);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		NtStatus = STATUS_ACCESS_VIOLATION;
	}
	if (NT_SUCCESS(NtStatus))
	{
		ObDereferenceObject((PVOID)pObject);
	}

	return pObject;
}

bool CSystemEnum::SymbolicLinkToDriverName (PUNICODE_STRING strSymboleLink, PUNICODE_STRING strDevObj, ULONG *uSize)
{
	HANDLE                hSymLink;
	OBJECT_ATTRIBUTES     SymAttr;
	NTSTATUS			  status = STATUS_UNSUCCESSFUL;		

	InitializeObjectAttributes (&SymAttr, strSymboleLink, OBJ_CASE_INSENSITIVE, NULL, NULL);
	if (STATUS_SUCCESS == ZwOpenSymbolicLinkObject (&hSymLink, SYMBOLIC_LINK_QUERY, &SymAttr))
	{
		if (STATUS_SUCCESS == ZwQuerySymbolicLinkObject (hSymLink, strDevObj, uSize))
		{
			ZwClose (hSymLink);
			return true;
		}
		ZwClose (hSymLink);
	}

	return false;
}

bool CSystemEnum::GetPathByObject (PVOID pObj, PUNICODE_STRING strPath)
{
	CBuffer						pBuff;
	POBJECT_NAME_INFORMATION	Info;
	ULONG						lRes;

	pBuff.SetSize (1024);

	RtlFillMemory (pBuff.GetData (), pBuff.GetBufferSize (), 0);
	Info = (POBJECT_NAME_INFORMATION)pBuff.GetData ();

	if (ObQueryNameString (pObj, Info, pBuff.GetBufferSize (), &lRes) == STATUS_SUCCESS)
	{
		if (Info->Name.Length)
		{
			RtlUnicodeStringCopy (strPath, &Info->Name);
			return true;
		}
	}

	return false;
}

bool FindSlCallback2 (PVOID pContext, PUNICODE_STRING strItem, PUNICODE_STRING ObjectType)
{
	TCONTEXT				*pConts = (TCONTEXT*)pContext;
	CBuffer					pBuff;
	UNICODE_STRING			strPath;
	ULONG					uSize = 0;

	CSystemEnum::FEnunSymbolicLink pFuncCallBack = (CSystemEnum::FEnunSymbolicLink)pConts->pCont1;
	PUNICODE_STRING	strPathDev = (PUNICODE_STRING)pConts->pCont2;

	// init unicode string
	pBuff.SetSize (NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR));
	RtlZeroMemory (pBuff.GetData (), pBuff.GetBufferSize ());
	RtlInitUnicodeString (&strPath, (PWCHAR)pBuff.GetData ());
	strPath.MaximumLength = (USHORT)pBuff.GetBufferSize ();

	if (CSystemEnum::SymbolicLinkToDriverName (strItem, &strPath, &uSize))
	{
		PDEVICE_OBJECT pObj;
		pObj = (PDEVICE_OBJECT)CSystemEnum::GetDeviceByPath (strPath.Buffer);
		if (pObj)
		{
			CBuffer				buff (NonPagedPool, 1024);
			ULONG				uRes;
			UNICODE_STRING		strDev;

			if (NT_SUCCESS (IoGetDeviceProperty (pObj, 
				DevicePropertyPhysicalDeviceObjectName,
				buff.GetBufferSize (),
				buff.GetData (),
				&uRes)))
			{
				RtlInitUnicodeString (&strDev, (PWCHAR)buff.GetData ());

				if (RtlCompareUnicodeString (strPathDev, &strPath, FALSE) == 0)
				{
					pFuncCallBack (pConts->pCont4, pObj/*(PDEVICE_OBJECT)pConts->pCont3*/, strPathDev, strItem);
				}
			}
		}

		if (RtlCompareUnicodeString (strPathDev, &strPath, FALSE) == 0)
		{
			pFuncCallBack (pConts->pCont4, (PDEVICE_OBJECT)pConts->pCont3, strPathDev, strItem);
		}
	}
	return false;
}

bool FindSlCallback1 (PVOID pContext, PUNICODE_STRING strItem, PUNICODE_STRING ObjectType)
{
	TCONTEXT				*pConts = (TCONTEXT*)pContext;
	CBuffer					pBuff;
	UNICODE_STRING			strPath;
	ULONG					uSize = 0;

	// init unicode string
	pBuff.SetSize (NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR));
	RtlZeroMemory (pBuff.GetData (), pBuff.GetBufferSize ());
	RtlInitUnicodeString (&strPath, (PWCHAR)pBuff.GetData ());
	strPath.MaximumLength = (USHORT)pBuff.GetBufferSize ();

	if (CSystemEnum::SymbolicLinkToDriverName (strItem, &strPath, &uSize))
	{
		static	WCHAR			szDosDev [] = L"\\GLOBAL??";
		UNICODE_STRING			strDosDev;

		{
			TCONTEXT				Context;

			RtlInitUnicodeString (&strDosDev, szDosDev);

			Context.pCont1 = pConts->pCont1;
			Context.pCont2 = &strPath;
			Context.pCont3 = pConts->pCont3;
			Context.pCont4 = pConts->pCont4;
			CSystemEnum::EnumDir (&strDosDev, FindSlCallback2, &Context);
		}
	}
	return false;
}

bool CSystemEnum::FindAllChildGlobaleSymbolicLinks (PDEVICE_OBJECT pDev, FEnunSymbolicLink funcEnum, PVOID pContext)
{
	CBuffer					pBuff;
	UNICODE_STRING			strPath;
	PDEVICE_RELATIONS		pDevRelations = GetDeviceRelations (pDev);
	TCONTEXT				Context;
	bool					bRet = false;

	pBuff.SetSize (NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR));

	if (pDevRelations)
	{
		for (ULONG a = 0; a < pDevRelations->Count; a++)
		{
			ULONG	uDisk;
			// get path device
			if (GetDiskNumber (pDevRelations->Objects [a], uDisk))
			{
				RtlZeroMemory (pBuff.GetData (), pBuff.GetBufferSize ());

				swprintf ((WCHAR*)pBuff.GetData (), L"\\Device\\Harddisk%d", uDisk);
				RtlInitUnicodeString (&strPath, (PWCHAR)pBuff.GetData ());
				strPath.MaximumLength = (USHORT)pBuff.GetBufferSize ();

				Context.pCont1 = funcEnum;
				Context.pCont2 = &strPath;
				Context.pCont3 = pDevRelations->Objects [a];
				Context.pCont4 = pContext;
				EnumDir (&strPath, FindSlCallback1, &Context);
				bRet = true;
			}
			else
			{
				bRet |= FindAllChildGlobaleSymbolicLinks (pDevRelations->Objects [a], funcEnum, pContext);
			}
			ObDereferenceObject (pDevRelations->Objects [a]);
		}

		ExFreePool (pDevRelations);
	}
	return bRet;
}

PDEVICE_RELATIONS CSystemEnum::GetDeviceRelations (PDEVICE_OBJECT pDev, DEVICE_RELATION_TYPE Type)
{
    KEVENT              pnpEvent;
    NTSTATUS            status;
    PDEVICE_OBJECT      targetObject = NULL;
    PIO_STACK_LOCATION  irpStack;
    PIRP                pnpIrp;
	IO_STATUS_BLOCK		ioBlock;

    PAGED_CODE();

	ioBlock.Information = 0;
    //
    // Initialize the event
    //
    KeInitializeEvent( &pnpEvent, NotificationEvent, FALSE );

    targetObject = IoGetAttachedDeviceReference( pDev );

    //
    // Build an Irp
    //
    pnpIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP,
        targetObject,
        NULL,
        0,
        NULL,
        &pnpEvent,
        &ioBlock
        );

    if ( pnpIrp ) 
	{
		//
		// Pnp Irps all begin life as STATUS_NOT_SUPPORTED;
		//
		pnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

		//
		// Get the top of stack
		//
		irpStack = IoGetNextIrpStackLocation( pnpIrp );

		//
		// Set the top of stack
		//
		RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );
		irpStack->MajorFunction = IRP_MJ_PNP;
		irpStack->MinorFunction = IRP_MN_QUERY_DEVICE_RELATIONS;
		irpStack->Parameters.QueryDeviceRelations.Type = Type;

		//
		// Call the driver
		//
		status = IoCallDriver( targetObject, pnpIrp );
		if (status == STATUS_PENDING) 
		{

		    LARGE_INTEGER Timeout;
		    Timeout.QuadPart = Int32x32To64(50, -10000);

			//
			// Block until the irp comes back.
			// Important thing to note here is when you allocate 
			// the memory for an event in the stack you must do a  
			// KernelMode wait instead of UserMode to prevent 
			// the stack from getting paged out.
			//

			status = KeWaitForSingleObject(&pnpEvent, Executive, KernelMode, FALSE, &Timeout);
            if (status == STATUS_TIMEOUT)
            {
				IoCancelIrp (pnpIrp);
				status = ioBlock.Status;
			}
			else
				status = ioBlock.Status;
		}
	}

    //
    // Done with reference
    //
	if (targetObject)
	{
		ObDereferenceObject( targetObject );
	}

    //
    // Done
    //
    return (PDEVICE_RELATIONS)ioBlock.Information;
}

bool CSystemEnum::GetDiskNumber (PDEVICE_OBJECT pDev, ULONG &uNumber)
{
    NTSTATUS				status;
    PDEVICE_OBJECT			targetObject = NULL;
    PIO_STACK_LOCATION		irpStack;
    PIRP					pIrp;
	IO_STATUS_BLOCK			ioBlock;
	STORAGE_DEVICE_NUMBER	NumDisk;
	KEVENT					Event;

    PAGED_CODE();

	ioBlock.Information = 0;
    //
    // Initialize the event
    //
    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    targetObject = IoGetAttachedDeviceReference( pDev );
    //
    // Build an Irp
    //
    pIrp = IoBuildDeviceIoControlRequest(
        IOCTL_STORAGE_GET_DEVICE_NUMBER,
        targetObject,
        &NumDisk,
        sizeof (NumDisk),
        &NumDisk,
        sizeof (NumDisk),
		FALSE,
		&Event,
        &ioBlock
        );

    if ( pIrp ) 
	{
		//
		// Call the driver
		//
		status = IoCallDriver( targetObject, pIrp );
		if (status == STATUS_PENDING) 
		{

		    LARGE_INTEGER Timeout;
		    Timeout.QuadPart = Int32x32To64(50, -10000);

			//
			// Block until the irp comes back.
			// Important thing to note here is when you allocate 
			// the memory for an event in the stack you must do a  
			// KernelMode wait instead of UserMode to prevent 
			// the stack from getting paged out.
			//

			status = KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, &Timeout);
            if (status == STATUS_TIMEOUT)
            {
				IoCancelIrp (pIrp);
				status = ioBlock.Status;
			}
			else
				status = ioBlock.Status;
		}
	}

    //
    // Done with reference
    //
	if (targetObject)
	{
		ObDereferenceObject( targetObject );
	}

	if (NT_SUCCESS (status))
	{
		uNumber = NumDisk.DeviceNumber;
	}

    //
    // Done
    //
    return NT_SUCCESS (status);

}