/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

private.h

Abstract:

Description of ICTRL and structures for interaction

Environment:

Kernel/user mode

--*/
#ifndef PRIVATE_H
#define PRIVATE_H

#include "Common\public.h"

//class URB_FULL;
class CPdoExtension;
namespace evuh
{
	class CUsbDevToTcp;
};

enum eThread
{
	ThreadIn,
	ThreadOut,
	ThreadInt,
	ThreadMax
};

typedef struct _IRP_NUMBER
{
	PIRP			Irp;
	CPdoExtension*	pPdo;
	evuh::CUsbDevToTcp*	pTcp;
	LONG			Count;
	bool			Cancel;
	KEVENT			Event;
	bool			bWait;
	LONG			Lock;
	int				CountWait;
	bool			bManualPending;
	PVOID			Reserved;
	int				iRes1;
	ULONG_PTR		iRes2;

	PIRP_SAVE		SaveIrp;

	int				iCount;
	bool			bIsoch;
	bool			bNoAnswer;

	URB_FULL *	Urb;
}IRP_NUMBER, *PIRP_NUMBER;

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

#if DBG
#define IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE     CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_GET_DEVICE_HANDLE, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_USB_GET_HUB_CAPABILITIES  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_HUB_CAPABILITIES,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_HUB_CYCLE_PORT  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_HUB_CYCLE_PORT,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_CONNECTION_ATTRIBUTES,\
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_CONNECTION_INFORMATION_EX,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION   \
                                            CTL_CODE(FILE_DEVICE_USB,  \
                                            USB_IDLE_NOTIFICATION,  \
                                            METHOD_NEITHER,  \
                                            FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY     CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_IDLE_NOTIFICATION_EX, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND    CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_REQ_GLOBAL_SUSPEND, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME     CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_REQ_GLOBAL_RESUME, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_RECORD_FAILURE        CTL_CODE(FILE_DEVICE_USB,  \
                                                    USB_RECORD_FAILURE, \
                                                    METHOD_NEITHER,  \
                                                    FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_DEVICE_HANDLE_EX, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_TT_DEVICE_HANDLE, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_TOPOLOGY_ADDRESS, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO    CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_HUB_CONFIG_INFO, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)
#endif


#if (_WIN32_WINNT < 0x0600)
#define IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS   CTL_CODE(FILE_DEVICE_USB,  \
                                                      USB_GET_TOPOLOGY_ADDRESS, \
                                                      METHOD_NEITHER,  \
                                                      FILE_ANY_ACCESS)

#define IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION   \
                                            CTL_CODE(FILE_DEVICE_USB,  \
                                            USB_IDLE_NOTIFICATION,  \
                                            METHOD_NEITHER,  \
                                            FILE_ANY_ACCESS)
#endif

// bool IsochCompleted (PIRP_NUMBER stDevice);
//void CompleteIrpFull (PIRP_NUMBER	irpNum, bool bAsync = false);
#endif