/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   SystemEnum.h

Environment:

    Kernel mode

--*/
#pragma once

#include "baseclass.h"

struct TCONTEXT
{
	PVOID pCont1;
	PVOID pCont2;
	PVOID pCont3;
	PVOID pCont4;
};

class CSystemEnum : public CBaseClass
{
public:
	typedef bool (*FEnunCallback) (PVOID pContext, PUNICODE_STRING strItem, PUNICODE_STRING ObjectType);
	typedef bool (*FEnunSymbolicLink) (PVOID pContext, PDEVICE_OBJECT pDev, PUNICODE_STRING strDev, PUNICODE_STRING strSymboleLink);

	NEW_ALLOC_DEFINITION ('ESC')

public:
	CSystemEnum(void);
	~CSystemEnum(void);

	static bool EnumDir (PUNICODE_STRING strDir, FEnunCallback funcEnum, PVOID pContext);
	static PVOID GetDriverByPath (PWSTR pwszObjectName);
	static PVOID GetDeviceByPath (PWSTR pwszObjectName);
	static bool GetPathByObject (PVOID pObj, PUNICODE_STRING strPath);
	static bool SymbolicLinkToDriverName (PUNICODE_STRING strSymbolicLink, PUNICODE_STRING strDevObj, ULONG *uSize);

	static bool FindAllChildGlobaleSymbolicLinks (PDEVICE_OBJECT pDev, FEnunSymbolicLink funcEnum, PVOID pContext);
	static PDEVICE_RELATIONS GetDeviceRelations (PDEVICE_OBJECT pDev, DEVICE_RELATION_TYPE Type = BusRelations);


	static bool GetDiskNumber (PDEVICE_OBJECT pDev, ULONG &uNumber);
};

bool FindSlCallback1(PVOID pContext, PUNICODE_STRING strItem, PUNICODE_STRING ObjectType);

extern "C" 
{

typedef struct _OBJECT_NAMETYPE_INFO 
{               
  UNICODE_STRING ObjectName;
  UNICODE_STRING ObjectType;
} OBJECT_NAMETYPE_INFO, *POBJECT_NAMETYPE_INFO;   

NTSYSAPI
NTSTATUS
NTAPI
ZwOpenDirectoryObject (
    OUT PHANDLE             DirectoryHandle,
    IN ACCESS_MASK          DesiredAccess,
    IN POBJECT_ATTRIBUTES   ObjectAttributes
);

ZwQueryDirectoryObject (
    IN HANDLE       DirectoryHandle,
    OUT PVOID       Buffer,
    IN ULONG        Length,
    IN BOOLEAN      ReturnSingleEntry,
    IN BOOLEAN      RestartScan,
    IN OUT PULONG   Context,
    OUT PULONG      ReturnLength OPTIONAL
);

extern POBJECT_TYPE IoDriverObjectType;
extern POBJECT_TYPE IoDeviceObjectType;

NTSTATUS ObReferenceObjectByName ( 
     IN PUNICODE_STRING ObjectName, 
     IN ULONG Attributes, 
     IN PACCESS_STATE AccessState OPTIONAL, 
     IN ACCESS_MASK DesiredAccess OPTIONAL, 
     IN POBJECT_TYPE ObjectType, 
     IN KPROCESSOR_MODE AccessMode, 
     IN OUT PVOID ParseContext OPTIONAL, 
     OUT PVOID *Object 
); 

}