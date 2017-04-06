/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

        USB.H

Abstract:

   structures and APIs for USB drivers.

Environment:

    Kernel & user mode

Revision History:

    09-29-95 : created
    02-10-04 : Updated to include header versioning

--*/

#ifndef   __USB_FULL_H__
#define   __USB_FULL_H__

#pragma warning( disable : 4200 )

extern "C" {
#pragma pack(push, 8)
#include <ntddk.h>

#pragma pack(pop)


// USB 2.0 calls start at 0x0030

#if (_WIN32_WINNT < 0x0501)

#define URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR       0x002A
#define URB_FUNCTION_SYNC_RESET_PIPE                 0x0030
#define URB_FUNCTION_SYNC_CLEAR_STALL                0x0031

#endif

#if (_WIN32_WINNT < 0x0600)

#define URB_FUNCTION_CONTROL_TRANSFER_EX             0x0032
#define URB_FUNCTION_SET_PIPE_IO_POLICY              0x0033
#define URB_FUNCTION_GET_PIPE_IO_POLICY              0x0034

#endif


struct _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL {
    struct _URB_HEADER Hdr;  // function code indicates get or set.
    PVOID Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
    UCHAR   Recipient:5;                // Recipient {Device,Interface,Endpoint}
    UCHAR   Reserved1:3;
    UCHAR   Reserved2;
    UCHAR   InterfaceNumber;            // wValue - high byte
    UCHAR   MS_PageIndex;               // wValue - low byte
    USHORT  MS_FeatureDescriptorIndex;  // wIndex field
    USHORT  Reserved3;
};


struct _URB_CONTROL_TRANSFER_EX_FULL {
    struct _URB_HEADER Hdr;
    USBD_PIPE_HANDLE PipeHandle;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    ULONG Timeout;                      // *optional* timeout in milliseconds
                                        // for this request, 0 = no timeout
                                        // if this is a chain of commands
#ifdef WIN64
    ULONG Pad;
#endif
    struct _URB_HCD_AREA hca;           // fields for HCD use
    UCHAR SetupPacket[8];
};

typedef enum _USB_IO_PRIORITY_FULL  {

    UsbIoPriorty_Normal_FULL = 0,
    UsbIoPriority_High_FULL = 8,
    UsbIoPriority_VeryHigh_FULL = 16

} USB_IO_PRIORITY_FULL  ;

typedef struct _USB_IO_PARAMETERS_FULL {

    /* iomap and schedule priority for pipe */
    _USB_IO_PRIORITY_FULL UsbIoPriority;    
   
    /* max-irp, number of irps that can complete per frame */
    ULONG UsbMaxPendingIrps;

    /* These fields are read-only */
    /* This is the same value returned in the pipe_info structure and is the largest request the 
       controller driver can handle */
    ULONG UsbMaxControllerTransfer;

} USB_IO_PARAMETERS_FULL, *PUSB_IO_PARAMETERS_FULL;


struct _URB_PIPE_IO_POLICY_FULL 
{
    
    struct _URB_HEADER Hdr;
    /* NULL indicates default pipe */
    USBD_PIPE_HANDLE PipeHandle; 
    _USB_IO_PARAMETERS_FULL UsbIoParamters;
  
};

typedef struct _URB_FULL {
    union {
        struct _URB_HEADER
            UrbHeader;
        struct _URB_SELECT_INTERFACE
            UrbSelectInterface;
        struct _URB_SELECT_CONFIGURATION
            UrbSelectConfiguration;
        struct _URB_PIPE_REQUEST
            UrbPipeRequest;
        struct _URB_FRAME_LENGTH_CONTROL
            UrbFrameLengthControl;
        struct _URB_GET_FRAME_LENGTH
            UrbGetFrameLength;
        struct _URB_SET_FRAME_LENGTH
            UrbSetFrameLength;
        struct _URB_GET_CURRENT_FRAME_NUMBER
            UrbGetCurrentFrameNumber;
        struct _URB_CONTROL_TRANSFER
            UrbControlTransfer;
        struct _URB_BULK_OR_INTERRUPT_TRANSFER
            UrbBulkOrInterruptTransfer;
        struct _URB_ISOCH_TRANSFER
            UrbIsochronousTransfer;

        // for standard control transfers on the default pipe
        struct _URB_CONTROL_DESCRIPTOR_REQUEST
            UrbControlDescriptorRequest;
        struct _URB_CONTROL_GET_STATUS_REQUEST
            UrbControlGetStatusRequest;
        struct _URB_CONTROL_FEATURE_REQUEST
            UrbControlFeatureRequest;
        struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST
            UrbControlVendorClassRequest;
        struct _URB_CONTROL_GET_INTERFACE_REQUEST
            UrbControlGetInterfaceRequest;
        struct _URB_CONTROL_GET_CONFIGURATION_REQUEST
            UrbControlGetConfigurationRequest;
            
        struct _URB_CONTROL_TRANSFER_EX_FULL
            UrbControlTransferEx;

        struct _URB_PIPE_IO_POLICY_FULL
            UrbPipeIoPolicy;

        struct _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL
            UrbOSFeatureDescriptorRequest;
    };
} URB_FULL, *PURB_FULL;

typedef struct _USB_BUS_INTERFACE_USBDI_V0_FULL {

    USHORT Size;
    USHORT Version;
    
    PVOID BusContext;
    PINTERFACE_REFERENCE InterfaceReference;
    PINTERFACE_REFERENCE InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    PVOID GetUSBDIVersion;
    PVOID QueryBusTime;
    PVOID SubmitIsoOutUrb;
    PVOID QueryBusInformation;

} USB_BUS_INTERFACE_USBDI_V0_FULL, *PUSB_BUS_INTERFACE_USBDI_V0_FULL;

/*
    New extensions for Windows XP
*/
typedef struct _USB_BUS_INTERFACE_USBDI_V1_FULL {

    USHORT Size;
    USHORT Version;
    
    PVOID BusContext;
    PVOID InterfaceReference;
    PVOID InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    PVOID GetUSBDIVersion;
    PVOID QueryBusTime;
    PVOID SubmitIsoOutUrb;
    PVOID QueryBusInformation;
    PVOID IsDeviceHighSpeed;
    
} USB_BUS_INTERFACE_USBDI_V1_FULL, *PUSB_BUS_INTERFACE_USBDI_V1_FULL;


/*
    New extensions for Windows XP
*/
typedef struct _USB_BUS_INTERFACE_USBDI_V2_FULL {

    USHORT Size;
    USHORT Version;
    
    PVOID BusContext;
    PINTERFACE_REFERENCE InterfaceReference;
    PINTERFACE_REFERENCE InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    PVOID GetUSBDIVersion;
    PVOID QueryBusTime;
    PVOID SubmitIsoOutUrb;
    PVOID QueryBusInformation;
    PVOID IsDeviceHighSpeed;

    PVOID EnumLogEntry;
    
} USB_BUS_INTERFACE_USBDI_V2_FULL, *PUSB_BUS_INTERFACE_USBDI_V2_FULL;

#ifndef USB_BUSIFFN
#define USB_BUSIFFN __stdcall
#endif

typedef VOID
    (USB_BUSIFFN *PUSB_BUSIFFN_GETUSBDI_VERSION_FULL) (
        IN PVOID,
        IN OUT PUSBD_VERSION_INFORMATION,
        IN OUT PULONG 
    );

typedef NTSTATUS
    (USB_BUSIFFN *PUSB_BUSIFFN_QUERY_BUS_TIME_FULL) (
        IN PVOID,
        IN PULONG
    );    

typedef NTSTATUS
    (USB_BUSIFFN *PUSB_BUSIFFN_SUBMIT_ISO_OUT_URB_FULL) (
        IN PVOID,
        IN PURB
    );

typedef NTSTATUS
    (USB_BUSIFFN *PUSB_BUSIFFN_QUERY_BUS_INFORMATION_FULL) (
        IN PVOID,
        IN ULONG,
        IN OUT PVOID,
        IN OUT PULONG,
        OUT PULONG
    );    

typedef BOOLEAN
    (USB_BUSIFFN *PUSB_BUSIFFN_IS_DEVICE_HIGH_SPEED_FULL) (
        IN PVOID
    );   

typedef NTSTATUS
    (USB_BUSIFFN *PUSB_BUSIFFN_ENUM_LOG_ENTRY_FULL) (
        IN PVOID,
        IN ULONG,
        IN ULONG,
        IN ULONG,
        IN ULONG
    );   

typedef NTSTATUS
    (USB_BUSIFFN *PUSB_BUSIFFN_QUERY_BUS_TIME_EX_FULL) (
        IN PVOID,
        OUT PULONG
    );   

typedef NTSTATUS
    (USB_BUSIFFN *PUSB_BUSIFFN_QUERY_CONTROLLER_TYPE_FULL) (
        IN PVOID,
        OUT PULONG,
        OUT PUSHORT,
        OUT PUSHORT,
        OUT PUCHAR,
        OUT PUCHAR,
        OUT PUCHAR,
        OUT PUCHAR
    ); 

typedef struct _USB_BUS_INTERFACE_USBDI_V3_FULL {

    USHORT Size;
    USHORT Version;
    
    PVOID BusContext;
    PINTERFACE_REFERENCE InterfaceReference;
    PINTERFACE_REFERENCE InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    PUSB_BUSIFFN_GETUSBDI_VERSION_FULL GetUSBDIVersion;
    PUSB_BUSIFFN_QUERY_BUS_TIME_FULL QueryBusTime;
    PUSB_BUSIFFN_SUBMIT_ISO_OUT_URB_FULL SubmitIsoOutUrb;
    PUSB_BUSIFFN_QUERY_BUS_INFORMATION_FULL QueryBusInformation;
    PUSB_BUSIFFN_IS_DEVICE_HIGH_SPEED_FULL IsDeviceHighSpeed;
    PUSB_BUSIFFN_ENUM_LOG_ENTRY_FULL EnumLogEntry;

    PUSB_BUSIFFN_QUERY_BUS_TIME_EX_FULL QueryBusTimeEx;
    PUSB_BUSIFFN_QUERY_CONTROLLER_TYPE_FULL QueryControllerType;
           
} USB_BUS_INTERFACE_USBDI_V3_FULL, *PUSB_BUS_INTERFACE_USBDI_V3_FULL;

#if (_WIN32_WINNT < 0x0501)

    #define IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX  \
                                    CTL_CODE(FILE_DEVICE_USB,  \
                                    USB_GET_NODE_CONNECTION_INFORMATION_EX,  \
                                    METHOD_BUFFERED,  \
                                    FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct _USB_NODE_CONNECTION_INFORMATION_EX {
    ULONG ConnectionIndex;  /* INPUT */
    /* usb device descriptor returned by this device
       during enumeration */
    USB_DEVICE_DESCRIPTOR DeviceDescriptor;/* OUTPUT */
    UCHAR CurrentConfigurationValue;/* OUTPUT */
    /* values for the speed field are defined in USB200.h */
    UCHAR Speed;/* OUTPUT */
    BOOLEAN DeviceIsHub;/* OUTPUT */
    USHORT DeviceAddress;/* OUTPUT */
    ULONG NumberOfOpenPipes;/* OUTPUT */
    USB_CONNECTION_STATUS ConnectionStatus;/* OUTPUT */
    USB_PIPE_INFO PipeList[0];/* OUTPUT */
} USB_NODE_CONNECTION_INFORMATION_EX, *PUSB_NODE_CONNECTION_INFORMATION_EX;
#pragma pack(pop)

#endif

#define USB_BUSIF_USBDI_VERSION_0_FULL         0x0000
#define USB_BUSIF_USBDI_VERSION_1_FULL         0x0001
#define USB_BUSIF_USBDI_VERSION_2_FULL         0x0002
#define USB_BUSIF_USBDI_VERSION_3_FULL         0x0003

typedef struct _USB_TOPOLOGY_ADDRESS_FULL {
    ULONG PciBusNumber;
    ULONG PciDeviceNumber;
    ULONG PciFunctionNumber;
    ULONG Reserved;
    USHORT RootHubPortNumber;
    USHORT HubPortNumber[5];
    USHORT Reserved2;
} USB_TOPOLOGY_ADDRESS_FULL, *PUSB_TOPOLOGY_ADDRESS_FULL;

typedef
VOID
(*USB_IDLE_CALLBACK_FULL)(
    PVOID Context
    );

typedef struct _USB_IDLE_CALLBACK_INFO_FULL {
    USB_IDLE_CALLBACK_FULL IdleCallback;
    PVOID IdleContext;
} USB_IDLE_CALLBACK_INFO_FULL, *PUSB_IDLE_CALLBACK_INFO_FULL;

}


#endif /*  __USB_H__ */

