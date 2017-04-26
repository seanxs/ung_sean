/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   DebugPrintUrb.cpp

Abstract:

   URB additional info output class

Environment:

    Kernel mode

--*/
#include "DebugPrintUrb.h"
#include "urb.h"

void  CDebugPrintUrb::DebugUrb (CBaseClass *pBase, PURB urb)
{
	pBase->DebugPrint2 (DebugError, "Function - %s", UrbFunctionString (urb->UrbHeader.Function));
	pBase->DebugPrint2 (DebugError, "Length - %d", urb->UrbHeader.Length);
	pBase->DebugPrint2 (DebugError, "Status - %x", urb->UrbHeader.Status);
	pBase->DebugPrint2 (DebugError, "UsbdDeviceHandle - %x", urb->UrbHeader.UsbdDeviceHandle);
	pBase->DebugPrint2 (DebugError, "UsbdFlags - %x", urb->UrbHeader.UsbdFlags);

	switch  (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER :
		pBase->DebugPrint2 (DebugWarning, "Reserved - 0x%x", urb->UrbControlVendorClassRequest.Reserved);
		pBase->DebugPrint2 (DebugWarning, "TransferFlags - 0x%x", urb->UrbControlVendorClassRequest.TransferFlags);
		pBase->DebugPrint2 (DebugWarning, "TransferBufferLength - %d", urb->UrbControlVendorClassRequest.TransferBufferLength);
		pBase->DebugPrint2 (DebugWarning, "TransferBuffer - 0x%x", urb->UrbControlVendorClassRequest.TransferBuffer);
		DebugBuffer (pBase, (char*)urb->UrbControlVendorClassRequest.TransferBuffer, urb->UrbControlVendorClassRequest.TransferBufferLength);
		pBase->DebugPrint2 (DebugWarning, "TransferBufferMDL - 0x%x", urb->UrbControlVendorClassRequest.TransferBufferMDL);
		pBase->DebugPrint2 (DebugWarning, "UrbLink - 0x%x", urb->UrbControlVendorClassRequest.UrbLink);
		Debug_URB_HCD_AREA (pBase, &urb->UrbControlVendorClassRequest.hca);
		pBase->DebugPrint2 (DebugWarning, "RequestTypeReservedBits - %d", urb->UrbControlVendorClassRequest.RequestTypeReservedBits);
		pBase->DebugPrint2 (DebugWarning, "Request - %d", urb->UrbControlVendorClassRequest.Request);
		pBase->DebugPrint2 (DebugWarning, "Value - %d", urb->UrbControlVendorClassRequest.Value);
		pBase->DebugPrint2 (DebugWarning, "Index - %d", urb->UrbControlVendorClassRequest.Index);
		pBase->DebugPrint2 (DebugWarning, "Reserved1 - %d", urb->UrbControlVendorClassRequest.Reserved1);
		break;
	case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
	case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_SET_FEATURE_TO_OTHER:
	case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
	case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
	case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
		pBase->DebugPrint2 (DebugWarning, "UrbLink - 0x%x", urb->UrbControlFeatureRequest.UrbLink);
		pBase->DebugPrint2 (DebugWarning, "Reserved0 - %d", urb->UrbControlFeatureRequest.Reserved0);
		pBase->DebugPrint2 (DebugWarning, "FeatureSelector - %d", urb->UrbControlFeatureRequest.FeatureSelector);
		pBase->DebugPrint2 (DebugWarning, "Index - %d", urb->UrbControlFeatureRequest.Index);
		pBase->DebugPrint2 (DebugWarning, "Reserved1 - %d", urb->UrbControlFeatureRequest.Reserved1);
		Debug_URB_HCD_AREA (pBase, &urb->UrbControlFeatureRequest.hca);
		break;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
		pBase->DebugPrint2 (DebugWarning, "Reserved - 0x%x", urb->UrbControlDescriptorRequest.Reserved);
		pBase->DebugPrint2 (DebugWarning, "Reserved0 - 0x%x", urb->UrbControlDescriptorRequest.Reserved0);
		pBase->DebugPrint2 (DebugWarning, "TransferBufferLength - %d", urb->UrbControlDescriptorRequest.TransferBufferLength);
		pBase->DebugPrint2 (DebugWarning, "TransferBuffer - 0x%x", urb->UrbControlDescriptorRequest.TransferBuffer);
		DebugBuffer (pBase, (char*)urb->UrbControlDescriptorRequest.TransferBuffer, urb->UrbControlDescriptorRequest.TransferBufferLength);
		pBase->DebugPrint2 (DebugWarning, "TransferBufferMDL - 0x%x", urb->UrbControlDescriptorRequest.TransferBufferMDL);
		pBase->DebugPrint2 (DebugWarning, "UrbLink - 0x%x", urb->UrbControlDescriptorRequest.UrbLink);
		Debug_URB_HCD_AREA (pBase, &urb->UrbControlDescriptorRequest.hca);
		pBase->DebugPrint2 (DebugWarning, "Reserved1 - 0x%x", urb->UrbControlDescriptorRequest.Reserved1);
		pBase->DebugPrint2 (DebugWarning, "Index - %d", urb->UrbControlDescriptorRequest.Index);
		pBase->DebugPrint2 (DebugWarning, "DescriptorType - %d", urb->UrbControlDescriptorRequest.DescriptorType);
		pBase->DebugPrint2 (DebugWarning, "LanguageId - %d", urb->UrbControlDescriptorRequest.LanguageId);
		pBase->DebugPrint2 (DebugWarning, "Reserved2 - %d", urb->UrbControlDescriptorRequest.Reserved2);
		break;
	case URB_FUNCTION_SELECT_CONFIGURATION:
		pBase->DebugPrint2 (DebugWarning, "ConfigurationDescriptor - 0x%x", urb->UrbSelectConfiguration.ConfigurationDescriptor);
		pBase->DebugPrint2 (DebugWarning, "ConfigurationHandle - 0x%x", urb->UrbSelectConfiguration.ConfigurationHandle);
		pBase->DebugPrint2 (DebugWarning, "Interface - 0x%x", urb->UrbSelectConfiguration.Interface);
        pBase->DebugPrint2 (DebugWarning, "Interface size - 0x%x", urb->UrbSelectConfiguration.Interface.Length);
		break;
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		pBase->DebugPrint2 (DebugWarning, "TransferFlags - 0x%x", urb->UrbBulkOrInterruptTransfer.TransferFlags);
		pBase->DebugPrint2 (DebugWarning, "TransferBufferLength - %d", urb->UrbBulkOrInterruptTransfer.TransferBufferLength);
		pBase->DebugPrint2 (DebugWarning, "TransferBuffer - 0x%x", urb->UrbBulkOrInterruptTransfer.TransferBuffer);
		pBase->DebugPrint2 (DebugWarning, "TransferBufferMDL - 0x%x", urb->UrbBulkOrInterruptTransfer.TransferBufferMDL);
		pBase->DebugPrint2 (DebugWarning, "UrbLink - 0x%x", urb->UrbBulkOrInterruptTransfer.UrbLink);
		DebugBuffer (pBase, (char*)urb->UrbBulkOrInterruptTransfer.TransferBuffer, urb->UrbBulkOrInterruptTransfer.TransferBufferLength);
		Debug_URB_HCD_AREA (pBase, &urb->UrbBulkOrInterruptTransfer.hca);
		break;

	}
}

void CDebugPrintUrb::DebugUrb (CBaseClass *pBase, PIRP Irp)
{
	PIO_STACK_LOCATION  irpStack;
	PURB				urb;
	
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	if(irpStack->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL)
	{
		return;
	}
	urb = (PURB)irpStack->Parameters.Others.Argument1;

	pBase->DebugPrint2 (DebugError, "UserBuffer 0x%x", Irp->UserBuffer);
    DebugUrb (pBase, urb);
}

void CDebugPrintUrb::DebugBuffer (CBaseClass *pBase, char* lpBuffer, LONG lSize)
{
	return ;
}

void CDebugPrintUrb::Debug_URB_HCD_AREA (CBaseClass *pBase, _URB_HCD_AREA *hcdArea)
{
}

PCHAR CDebugPrintUrb::UrbFunctionString(USHORT Func)
{
#ifdef _FEATURE_ENABLE_DEBUGPRINT
	switch (Func)
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		return "URB_FUNCTION_SELECT_CONFIGURATION";
	case URB_FUNCTION_SELECT_INTERFACE:
        return "URB_FUNCTION_SELECT_INTERFACE";
	case URB_FUNCTION_ABORT_PIPE:
		return "URB_FUNCTION_ABORT_PIPE";
	case URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL:
		return "URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL";
	case URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL:
		return "URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL";
	case URB_FUNCTION_GET_FRAME_LENGTH:
		return "URB_FUNCTION_GET_FRAME_LENGTH";
	case URB_FUNCTION_SET_FRAME_LENGTH:
		return "URB_FUNCTION_SET_FRAME_LENGTH";
	case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
		return "URB_FUNCTION_GET_CURRENT_FRAME_NUMBER";
	case  URB_FUNCTION_CONTROL_TRANSFER:
		return "URB_FUNCTION_CONTROL_TRANSFER";
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		return "URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER";
	case URB_FUNCTION_ISOCH_TRANSFER:
		return "URB_FUNCTION_ISOCH_TRANSFER";
	case URB_FUNCTION_RESET_PIPE:
		return "URB_FUNCTION_RESET_PIPE";
	//case URB_FUNCTION_SYNC_RESET_PIPE:
	//	return "URB_FUNCTION_SYNC_RESET_PIPE";
	//case URB_FUNCTION_SYNC_CLEAR_STALL:
	//	return "URB_FUNCTION_SYNC_CLEAR_STALL";
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		return "URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE";
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:
		return "URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT";
	case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE:
		return "URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE";
	case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT:
		return "URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT";
	case URB_FUNCTION_SET_FEATURE_TO_DEVICE:
		return "URB_FUNCTION_SET_FEATURE_TO_DEVICE";
	case URB_FUNCTION_SET_FEATURE_TO_INTERFACE:
		return "URB_FUNCTION_SET_FEATURE_TO_INTERFACE";
	case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT:
		return "URB_FUNCTION_SET_FEATURE_TO_ENDPOINT";
	case URB_FUNCTION_SET_FEATURE_TO_OTHER:
		return "URB_FUNCTION_SET_FEATURE_TO_OTHER";
	case URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE";
	case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE";
	case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT";
	case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER:
		return "URB_FUNCTION_CLEAR_FEATURE_TO_OTHER";
	case URB_FUNCTION_GET_STATUS_FROM_DEVICE:
		return "URB_FUNCTION_GET_STATUS_FROM_DEVICE";
	case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:
		return "URB_FUNCTION_GET_STATUS_FROM_INTERFACE";
	case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
		return "URB_FUNCTION_GET_STATUS_FROM_ENDPOINT";
	case URB_FUNCTION_GET_STATUS_FROM_OTHER:
		return "URB_FUNCTION_GET_STATUS_FROM_OTHER";
	case URB_FUNCTION_VENDOR_DEVICE:
		return "URB_FUNCTION_VENDOR_DEVICE";
	case URB_FUNCTION_VENDOR_INTERFACE:
		return "URB_FUNCTION_VENDOR_INTERFACE";
	case URB_FUNCTION_VENDOR_ENDPOINT:
		return "URB_FUNCTION_VENDOR_ENDPOINT";
	case URB_FUNCTION_VENDOR_OTHER:
		return "URB_FUNCTION_VENDOR_OTHER";
	case URB_FUNCTION_CLASS_DEVICE:
		return "URB_FUNCTION_CLASS_DEVICE";
	case URB_FUNCTION_CLASS_INTERFACE:
		return "URB_FUNCTION_CLASS_INTERFACE";
	case URB_FUNCTION_CLASS_ENDPOINT:
		return "URB_FUNCTION_CLASS_ENDPOINT";
	case URB_FUNCTION_CLASS_OTHER:
		return "URB_FUNCTION_CLASS_OTHER";
	case URB_FUNCTION_GET_CONFIGURATION:
		return "URB_FUNCTION_GET_CONFIGURATION";
	case URB_FUNCTION_GET_INTERFACE:
		return "URB_FUNCTION_GET_INTERFACE";
    case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR:
        return "URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR";
    case URB_FUNCTION_SYNC_RESET_PIPE:
        return "URB_FUNCTION_SYNC_RESET_PIPE";
    case URB_FUNCTION_SYNC_CLEAR_STALL:
        return "URB_FUNCTION_SYNC_CLEAR_STALL";
    case URB_FUNCTION_CONTROL_TRANSFER_EX:
        return "URB_FUNCTION_CONTROL_TRANSFER_EX";
    //case URB_FUNCTION_SET_PIPE_IO_POLICY:
    //    return "URB_FUNCTION_SET_PIPE_IO_POLICY";
    //case URB_FUNCTION_GET_PIPE_IO_POLICY:
    //    return "URB_FUNCTION_GET_PIPE_IO_POLICY";
	}
 
	char Buf [100];
	sprintf (Buf, "Unknown 0x%x", Func);
	return 0;
#else
	return 0;
#endif
}


#if _FEATURE_ENABLE_DEBUGPRINT
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
#endif

PCHAR CDebugPrintUrb::UsbIoControl (LONG lFunc)
{
#if _FEATURE_ENABLE_DEBUGPRINT

	switch (lFunc)
	{
	case IOCTL_INTERNAL_USB_SUBMIT_URB:
		return "IOCTL_INTERNAL_USB_SUBMIT_URB";
	case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
		return "IOCTL_INTERNAL_USB_GET_PORT_STATUS";
	case IOCTL_INTERNAL_USB_RESET_PORT:
		return "IOCTL_INTERNAL_USB_RESET_PORT";
	case IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO:
		return "IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO";
	case IOCTL_INTERNAL_USB_ENABLE_PORT:
		return "IOCTL_INTERNAL_USB_ENABLE_PORT";
	case IOCTL_INTERNAL_USB_GET_HUB_COUNT:
		return "IOCTL_INTERNAL_USB_GET_HUB_COUNT";
	case IOCTL_INTERNAL_USB_CYCLE_PORT:
		return "IOCTL_INTERNAL_USB_CYCLE_PORT";
	case IOCTL_INTERNAL_USB_GET_HUB_NAME:
		return "IOCTL_INTERNAL_USB_GET_HUB_NAME";
	case IOCTL_INTERNAL_USB_GET_BUS_INFO:
		return "IOCTL_INTERNAL_USB_GET_BUS_INFO";
	case IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME:
		return "IOCTL_INTERNAL_USB_GET_CONTROLLER_NAME";
	case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE";
	case IOCTL_INTERNAL_USB_GET_BUSGUID_INFO:
		return "IOCTL_INTERNAL_USB_GET_BUSGUID_INFO";
	case IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO:
		return "IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO";
	case IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY:
		return "IOCTL_INTERNAL_USB_NOTIFY_IDLE_READY";
	case IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND:
		return "IOCTL_INTERNAL_USB_REQ_GLOBAL_SUSPEND";
	case IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME:
		return "IOCTL_INTERNAL_USB_REQ_GLOBAL_RESUME";
	case IOCTL_INTERNAL_USB_RECORD_FAILURE:
		return "IOCTL_INTERNAL_USB_RECORD_FAILURE";
	case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE_EX";
	case IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE:
		return "IOCTL_INTERNAL_USB_GET_TT_DEVICE_HANDLE";
	case IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS:
		return "IOCTL_INTERNAL_USB_GET_TOPOLOGY_ADDRESS";
	case IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO:
		return "IOCTL_INTERNAL_USB_GET_DEVICE_CONFIG_INFO";
	case IOCTL_USB_HCD_GET_STATS_1:
		return "IOCTL_USB_HCD_GET_STATS_1";
	//case IOCTL_USB_HCD_GET_STATS_2:
	//	return "IOCTL_USB_HCD_GET_STATS_2";
	case IOCTL_USB_HCD_DISABLE_PORT:
		return "IOCTL_USB_HCD_DISABLE_PORT";
	case IOCTL_USB_HCD_ENABLE_PORT:
		return "IOCTL_USB_HCD_ENABLE_PORT";
	case IOCTL_USB_DIAGNOSTIC_MODE_ON:
		return "IOCTL_USB_DIAGNOSTIC_MODE_ON";
	case IOCTL_USB_DIAGNOSTIC_MODE_OFF:
		return "IOCTL_USB_DIAGNOSTIC_MODE_OFF";
	case IOCTL_USB_GET_ROOT_HUB_NAME:
		return "IOCTL_USB_GET_ROOT_HUB_NAME";
	//case IOCTL_GET_HCD_DRIVERKEY_NAME:
	//	return "IOCTL_GET_HCD_DRIVERKEY_NAME";
	//case IOCTL_USB_GET_NODE_INFORMATION:
	//	return "IOCTL_USB_GET_NODE_INFORMATION";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION";
	case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
		return "IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION";
	case IOCTL_USB_GET_NODE_CONNECTION_NAME:
		return "IOCTL_USB_GET_NODE_CONNECTION_NAME";
	case IOCTL_USB_DIAG_IGNORE_HUBS_ON:
		return "IOCTL_USB_DIAG_IGNORE_HUBS_ON";
	case IOCTL_USB_DIAG_IGNORE_HUBS_OFF:
		return "IOCTL_USB_DIAG_IGNORE_HUBS_OFF";
	//case IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME:
	//	return "IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME";
	case IOCTL_USB_GET_HUB_CAPABILITIES:
		return "IOCTL_USB_GET_HUB_CAPABILITIES";
	case IOCTL_USB_HUB_CYCLE_PORT:
		return "IOCTL_USB_HUB_CYCLE_PORT";
	case IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES:
		return "IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES";
	case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
		return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX";
	//case IOCTL_USB_RESET_HUB:
	//	return "IOCTL_USB_RESET_HUB";


	case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
		return "IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION";
	//case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
	//	return "IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX";
	default:
		{
			char Buf [100];
			sprintf (Buf, "Unknown 0x%x", lFunc);
			return 0;
		}

		//return "unknown";
	}
	return 0;
#else
	return 0;
#endif
}

void CDebugPrintUrb::DebugUrbControl (CBaseClass *pBase, PURB urb)
{
	switch (urb->UrbHeader.Function)
	{
	case URB_FUNCTION_CONTROL_TRANSFER:
		{
			pBase->DebugPrint2 (DebugError, "Function - %s", UrbFunctionString (urb->UrbHeader.Function));
			pBase->DebugPrint2 (DebugError, "Length - %d", urb->UrbHeader.Length);
			pBase->DebugPrint2 (DebugError, "Status - %x", urb->UrbHeader.Status);
			pBase->DebugPrint2 (DebugError, "UsbdDeviceHandle - %x", urb->UrbHeader.UsbdDeviceHandle);
			pBase->DebugPrint2 (DebugError, "UsbdFlags - %x", urb->UrbHeader.UsbdFlags);

			PUSB_COMMON_DESCRIPTOR pDesc = (PUSB_COMMON_DESCRIPTOR)urb->UrbControlTransfer.TransferBuffer;
			if (!pDesc)
			{
				return;
			}

			switch (pDesc->bDescriptorType)
			{
			case USB_DEVICE_DESCRIPTOR_TYPE:
				{
					PUSB_DEVICE_DESCRIPTOR pDevDesc = (PUSB_DEVICE_DESCRIPTOR)pDesc;
					pBase->DebugPrint2 (DebugError, "  bLength - %d", pDevDesc->bLength);
					pBase->DebugPrint2 (DebugError, "  bcdUSB - %d", pDevDesc->bcdUSB);
					pBase->DebugPrint2 (DebugError, "  bDeviceClass - %d", pDevDesc->bDeviceClass);
					pBase->DebugPrint2 (DebugError, "  bDeviceSubClass - %d", pDevDesc->bDeviceSubClass);
					pBase->DebugPrint2 (DebugError, "  bDeviceProtocol - %d", pDevDesc->bDeviceProtocol);
					pBase->DebugPrint2 (DebugError, "  bMaxPacketSize0 - %d", pDevDesc->bMaxPacketSize0);
					pBase->DebugPrint2 (DebugError, "  idVendor - %d", pDevDesc->idVendor);
					pBase->DebugPrint2 (DebugError, "  idProduct - %d", pDevDesc->idProduct);
					pBase->DebugPrint2 (DebugError, "  bcdDevice - %d", pDevDesc->bcdDevice);
					pBase->DebugPrint2 (DebugError, "  iManufacturer - %d", pDevDesc->iManufacturer);
					pBase->DebugPrint2 (DebugError, "  iProduct - %d", pDevDesc->iProduct);
					pBase->DebugPrint2 (DebugError, "  iSerialNumber - %d", pDevDesc->iSerialNumber);
					pBase->DebugPrint2 (DebugError, "  bNumConfigurations - %d\r\n", pDevDesc->bNumConfigurations);
				}
				break;
			case USB_CONFIGURATION_DESCRIPTOR_TYPE:
				{
					PUSB_CONFIGURATION_DESCRIPTOR	pConfDesc = (PUSB_CONFIGURATION_DESCRIPTOR)pDesc;
					PUSB_INTERFACE_DESCRIPTOR		pInterfaceDesc;
					char							*pTemp;
					int								iOffset;

					pBase->DebugPrint2 (DebugError, "  bLength - %d", pConfDesc->bLength);
					pBase->DebugPrint2 (DebugError, "  wTotalLength - %d", pConfDesc->wTotalLength);
					pBase->DebugPrint2 (DebugError, "  bNumInterfaces - %d", pConfDesc->bNumInterfaces);
					pBase->DebugPrint2 (DebugError, "  bConfigurationValue - %d", pConfDesc->bConfigurationValue);
					pBase->DebugPrint2 (DebugError, "  iConfiguration - %d", pConfDesc->iConfiguration);
					pBase->DebugPrint2 (DebugError, "  bmAttributes - %d", pConfDesc->bmAttributes);
					pBase->DebugPrint2 (DebugError, "  MaxPower - %d\r\n", pConfDesc->MaxPower);
					if (pConfDesc->wTotalLength == urb->UrbControlTransfer.TransferBufferLength)
					{
						pTemp = (char*)pConfDesc;
						iOffset = pConfDesc->bLength;
						for (int a = 0; iOffset < pConfDesc->wTotalLength; a++)
						{
							pDesc = (PUSB_COMMON_DESCRIPTOR)(pTemp + iOffset);

							switch (pDesc->bDescriptorType)
							{
							case USB_INTERFACE_DESCRIPTOR_TYPE:
								DebugUrbInterfaceDesc ( pBase, (PUSB_INTERFACE_DESCRIPTOR)pDesc);
								break;
							case USB_ENDPOINT_DESCRIPTOR_TYPE:
								DebugUrbEndpointDesc ( pBase, (PUSB_ENDPOINT_DESCRIPTOR)pDesc);
								break;
							}
							iOffset += pDesc->bLength;
						}
					}
				}
				break;
			}
		}
		break;
	case URB_FUNCTION_SELECT_CONFIGURATION:
		{
			PUSB_CONFIGURATION_DESCRIPTOR	pConfDesc = (PUSB_CONFIGURATION_DESCRIPTOR)urb->UrbSelectConfiguration.ConfigurationDescriptor; 
			PUSBD_INTERFACE_INFORMATION		pInterface;
			int								iSize;
			char							*pTemp;

			pBase->DebugPrint2 (DebugError, "Function - %s", UrbFunctionString (urb->UrbHeader.Function));
			pBase->DebugPrint2 (DebugError, "Length - %d", urb->UrbHeader.Length);
			pBase->DebugPrint2 (DebugError, "Status - %x", urb->UrbHeader.Status);
			pBase->DebugPrint2 (DebugError, "UsbdDeviceHandle - %x", urb->UrbHeader.UsbdDeviceHandle);
			pBase->DebugPrint2 (DebugError, "UsbdFlags - %x", urb->UrbHeader.UsbdFlags);

			pInterface = &urb->UrbSelectConfiguration.Interface;
			for (int a = 0; a < pConfDesc->bNumInterfaces; a++)
			{
				DebugUrbInterfaceDesc (pBase, pInterface);

				pTemp = (char*)pInterface;
				pInterface = (PUSBD_INTERFACE_INFORMATION)(pTemp + pInterface->Length);
			}
		}
		break;
	case URB_FUNCTION_SELECT_INTERFACE:
		{
			pBase->DebugPrint2 (DebugError, "Function - %s", UrbFunctionString (urb->UrbHeader.Function));
			pBase->DebugPrint2 (DebugError, "Length - %d", urb->UrbHeader.Length);
			pBase->DebugPrint2 (DebugError, "Status - %x", urb->UrbHeader.Status);
			pBase->DebugPrint2 (DebugError, "UsbdDeviceHandle - %x", urb->UrbHeader.UsbdDeviceHandle);
			pBase->DebugPrint2 (DebugError, "UsbdFlags - %x", urb->UrbHeader.UsbdFlags);

			DebugUrbInterfaceDesc (pBase, &urb->UrbSelectInterface.Interface);
		}
		break;
	}
}

int CDebugPrintUrb::DebugUrbInterfaceDesc (CBaseClass *pBase, PUSB_INTERFACE_DESCRIPTOR pDesc)
{
	char							*pTemp;
	int								iOffset = 0;
	PUSB_ENDPOINT_DESCRIPTOR		pDescEndpoint;

	pBase->DebugPrint2 (DebugError, "    bLength - %d", pDesc->bLength);
	pBase->DebugPrint2 (DebugError, "    bDescriptorType - %d", pDesc->bDescriptorType);
	pBase->DebugPrint2 (DebugError, "    bInterfaceNumber - %d", pDesc->bInterfaceNumber);
	pBase->DebugPrint2 (DebugError, "    bAlternateSetting - %d", pDesc->bAlternateSetting);
	pBase->DebugPrint2 (DebugError, "    bNumEndpoints - %d", pDesc->bNumEndpoints);
	pBase->DebugPrint2 (DebugError, "    bInterfaceClass - %d", pDesc->bInterfaceClass);
	pBase->DebugPrint2 (DebugError, "    bInterfaceSubClass - %d", pDesc->bInterfaceSubClass);
	pBase->DebugPrint2 (DebugError, "    bInterfaceProtocol - %d", pDesc->bInterfaceProtocol);
	pBase->DebugPrint2 (DebugError, "    iInterface - %d\r\n", pDesc->iInterface);

	return iOffset;
}

int CDebugPrintUrb::DebugUrbEndpointDesc (CBaseClass *pBase, PUSB_ENDPOINT_DESCRIPTOR pDesc)
{
	pBase->DebugPrint2 (DebugError, "      bLength - %d", pDesc->bLength);
	pBase->DebugPrint2 (DebugError, "      bDescriptorType - %d", pDesc->bDescriptorType);
	pBase->DebugPrint2 (DebugError, "      bEndpointAddress - %d (%s)", pDesc->bEndpointAddress & 0x7F, (pDesc->bEndpointAddress & 0x80)?"in":"out");
	pBase->DebugPrint2 (DebugError, "      bmAttributes - %s (%d)", PipeType (pDesc->bmAttributes), pDesc->bmAttributes);
	pBase->DebugPrint2 (DebugError, "      wMaxPacketSize - %d", pDesc->wMaxPacketSize);
	pBase->DebugPrint2 (DebugError, "      bInterval - %d\r\n", pDesc->bInterval);
	return pDesc->bLength;
}

PCHAR CDebugPrintUrb::PipeType (UCHAR lFunc)
{
	switch (lFunc & 0x03)
	{
	case UsbdPipeTypeControl:
		return "Control";
	case UsbdPipeTypeIsochronous:
		return "Isochronous";
	case UsbdPipeTypeBulk:
		return "Bulk";
	case UsbdPipeTypeInterrupt:
		return "Interrupt";
	}

	static char Buffer [100];
	RtlStringCbPrintfA(Buffer, sizeof(Buffer), "%d", lFunc);
	return Buffer;
}

void CDebugPrintUrb::DebugUrbEndpointDesc (CBaseClass *pBase, PUSBD_PIPE_INFORMATION pDesc)
{
	pBase->DebugPrint2 (DebugError, "    MaximumPacketSize - %d", pDesc->MaximumPacketSize);
	pBase->DebugPrint2 (DebugError, "    EndpointAddress - %d (%s)", pDesc->EndpointAddress & 0x7F, (pDesc->EndpointAddress & 0x80)?"in":"out");
	pBase->DebugPrint2 (DebugError, "    Interval - %d", pDesc->Interval );
	pBase->DebugPrint2 (DebugError, "    PipeType - %s (%d)", PipeType (pDesc->PipeType), pDesc->PipeType );
	pBase->DebugPrint2 (DebugError, "    PipeHandle - 0x%x", pDesc->PipeHandle );
	pBase->DebugPrint2 (DebugError, "    MaximumTransferSize - %d", pDesc->MaximumTransferSize );
	pBase->DebugPrint2 (DebugError, "    PipeFlags - %d\n", pDesc->PipeFlags );
}

void CDebugPrintUrb::DebugUrbInterfaceDesc (CBaseClass *pBase, PUSBD_INTERFACE_INFORMATION pInterface)
{
	pBase->DebugPrint2 (DebugError, "  Length - %d", pInterface->Length);
	pBase->DebugPrint2 (DebugError, "  InterfaceNumber - %d", pInterface->InterfaceNumber );
	pBase->DebugPrint2 (DebugError, "  AlternateSetting - %d", pInterface->AlternateSetting  );
	pBase->DebugPrint2 (DebugError, "  Class - %d", pInterface->Class );
	pBase->DebugPrint2 (DebugError, "  SubClass - %d", pInterface->SubClass );
	pBase->DebugPrint2 (DebugError, "  Protocol - %d", pInterface->Protocol );
	pBase->DebugPrint2 (DebugError, "  InterfaceHandle - 0x%x", pInterface->InterfaceHandle );
	pBase->DebugPrint2 (DebugError, "  NumberOfPipes - %d\n", pInterface->NumberOfPipes );

	for (ULONG b = 0; b < pInterface->NumberOfPipes; b++)
	{
		DebugUrbEndpointDesc (pBase, &pInterface->Pipes [b]);
	}
}