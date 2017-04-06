/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    private.h

Abstract:

	Description of ICTRL and structures for interaction with third ring

Environment:

    Kernel/user mode

--*/
class CUsbPortToTcp;
class CPdoStubExtension;

#ifndef PRIVATE_H
#define PRIVATE_H

typedef struct _WorkingIrp 
{
	CUsbPortToTcp	*pBase;
	CPdoStubExtension *pPdo;
	LONG_PTR		pIrpIndef;
	UCHAR			MajorFunction;
	UCHAR			MinorFunction;
	ULONG			IoControlCode;
	PIRP			pIrp;
	//	BYTE			*pBuffer2;
	BYTE			*pAllocBuffer;
	LONG			lSizeBuffer;
	IO_STATUS_BLOCK ioStatus;
	USHORT			lOther;
	VOID			*pSecondBuffer;
	//PMDL			pSecondBufferMdl;
	LONG			lSecSizeBuffer;
	LONG			lLock;
	LONG            lLockIsoch;
	PVOID           pAddition;
	KEVENT	        *Event;
	BYTE			Is64:1;
	BYTE            Isoch:1;
	BYTE            Cancel:1;
	BYTE            Pending:1;
	//BYTE			FindBuffer:1;
	BYTE			NoAnswer:1;
	BYTE			Single:1;

}WORKING_IRP, *PWORKING_IRP;

typedef struct _FuncInterface
{
	LONG	lNumFunc;	
	LONG	Param1;
	LONG	Param2;
	LONG	Param3;
	LONG	Param4;
	LONG	Param5;
	LONG64	Param6;
	LONG64	Param7;
	LONG64	Param8;
}FUNC_INTERFACE, *PFUNC_INTERFACE;

#endif