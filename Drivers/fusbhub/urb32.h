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
#ifndef URB_32_HEADER
#define URB_32_HEADER
#pragma warning( disable : 4200 )

extern "C" {
#pragma pack(push, 8)
#include <ntddk.h>
#pragma pack(pop)

typedef ULONG USBD_PIPE_HANDLE_32;
typedef ULONG USBD_CONFIGURATION_HANDLE_32;
typedef ULONG USBD_INTERFACE_HANDLE_32;

struct _URB_HEADER_32 {
    //
    // Fields filled in by client driver
    //
    USHORT Length;
    USHORT Function;
    USBD_STATUS Status;
    //
    // Fields used only by USBD
    //
    ULONG UsbdDeviceHandle; // device handle assigned to this device
                            // by USBD
    ULONG UsbdFlags;        // flags field reserved for USBD use.
};

typedef enum _USBD_PIPE_TYPE_32 {
    UsbdPipeTypeControl_32,
    UsbdPipeTypeIsochronous_32,
    UsbdPipeTypeBulk_32,
    UsbdPipeTypeInterrupt_32
} USBD_PIPE_TYPE_32;

typedef struct _USBD_PIPE_INFORMATION_32 {
    //
    // OUTPUT
    // These fields are filled in by USBD
    //
    USHORT MaximumPacketSize;  // Maximum packet size for this pipe
    UCHAR EndpointAddress;     // 8 bit USB endpoint address (includes direction)
                               // taken from endpoint descriptor
    UCHAR Interval;            // Polling interval in ms if interrupt pipe

    USBD_PIPE_TYPE_32	PipeType;   // PipeType identifies type of transfer valid for this pipe
    USBD_PIPE_HANDLE_32	PipeHandle;

    //
    // INPUT
    // These fields are filled in by the client driver
    //
    ULONG MaximumTransferSize; // Maximum size for a single request
                               // in bytes.
    ULONG PipeFlags;
} USBD_PIPE_INFORMATION_32, *PUSBD_PIPE_INFORMATION_32;


typedef struct _USBD_INTERFACE_INFORMATION_32 {
    USHORT Length;       // Length of this structure, including
                         // all pipe information structures that
                         // follow.
    //
    // INPUT
    //
    // Interface number and Alternate setting this
    // structure is associated with
    //
    UCHAR InterfaceNumber;
    UCHAR AlternateSetting;

    //
    // OUTPUT
    // These fields are filled in by USBD
    //
    UCHAR Class;
    UCHAR SubClass;
    UCHAR Protocol;
    UCHAR Reserved;

    USBD_INTERFACE_HANDLE_32 InterfaceHandle;
    ULONG NumberOfPipes;

    //
    // INPUT/OUPUT
    // see PIPE_INFORMATION

    USBD_PIPE_INFORMATION_32 Pipes[1];
} USBD_INTERFACE_INFORMATION_32, *PUSBD_INTERFACE_INFORMATION_32;

struct _URB_SELECT_INTERFACE_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    USBD_CONFIGURATION_HANDLE_32 ConfigurationHandle;

    // client must input AlternateSetting & Interface Number
    // class driver returns interface and handle
    // for new alternate setting
    USBD_INTERFACE_INFORMATION_32 Interface;
};

typedef struct _USB_CONFIGURATION_DESCRIPTOR_32 {
    UCHAR bLength;
    UCHAR bDescriptorType;
    USHORT wTotalLength;
    UCHAR bNumInterfaces;
    UCHAR bConfigurationValue;
    UCHAR iConfiguration;
    UCHAR bmAttributes;
    UCHAR MaxPower;
} USB_CONFIGURATION_DESCRIPTOR_32, *PUSB_CONFIGURATION_DESCRIPTOR_32;

struct _URB_SELECT_CONFIGURATION_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    // NULL indicates to set the device
    // to the 'unconfigured' state
    // ie set to configuration 0
    ULONG32 ConfigurationDescriptor;
    USBD_CONFIGURATION_HANDLE_32 ConfigurationHandle;
    USBD_INTERFACE_INFORMATION_32 Interface;
};

struct _URB_PIPE_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    USBD_PIPE_HANDLE_32 PipeHandle;
    ULONG Reserved;
};

struct _URB_FRAME_LENGTH_CONTROL_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
};

struct _URB_GET_FRAME_LENGTH_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG FrameLength;
    ULONG FrameNumber;
};

struct _URB_SET_FRAME_LENGTH_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    LONG FrameLengthDelta;
};

struct _URB_GET_CURRENT_FRAME_NUMBER_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG FrameNumber;
};

struct _URB_HCD_AREA_32 {
    ULONG Reserved8[8];
};

struct _URB_CONTROL_TRANSFER_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    USBD_PIPE_HANDLE_32 PipeHandle;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *reserved MBZ*
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    UCHAR SetupPacket[8];
};

struct _URB_BULK_OR_INTERRUPT_TRANSFER_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    USBD_PIPE_HANDLE_32 PipeHandle;
    ULONG TransferFlags;                // note: the direction bit will be set by USBD
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
};

struct _URB_CONTROL_TRANSFER_EX_32 {
    struct _URB_HEADER_32 Hdr;
    USBD_PIPE_HANDLE_32 PipeHandle;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG Timeout;                      // *optional* timeout in milliseconds
                                        // for this request, 0 = no timeout
                                        // if this is a chain of commands
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    UCHAR SetupPacket[8];
};

typedef enum _USB_IO_PRIORITY_32  {

    UsbIoPriorty_Normal_32 = 0,
    UsbIoPriority_High_32 = 8,
    UsbIoPriority_VeryHigh_32 = 16

} USB_IO_PRIORITY_32  ;


typedef struct _USB_IO_PARAMETERS_32 {

    /* iomap and schedule priority for pipe */
    USB_IO_PRIORITY_32 UsbIoPriority;    
   
    /* max-irp, number of irps that can complete per frame */
    ULONG UsbMaxPendingIrps;

    /* These fields are read-only */
    /* This is the same value returned in the pipe_info structure and is the largest request the 
       controller driver can handle */
    ULONG UsbMaxControllerTransfer;

} USB_IO_PARAMETERS_32, *PUSB_IO_PARAMETERS_32;

struct _URB_PIPE_IO_POLICY_32 {
    
    struct _URB_HEADER_32 Hdr;
    /* NULL indicates default pipe */
    USBD_PIPE_HANDLE_32 PipeHandle; 
    USB_IO_PARAMETERS_32 UsbIoParamters;
  
};

struct _URB_CONTROL_DESCRIPTOR_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *reserved MBZ*
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    USHORT Reserved1;
    UCHAR Index;
    UCHAR DescriptorType;
    USHORT LanguageId;
    USHORT Reserved2;
};

struct _URB_CONTROL_GET_STATUS_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *reserved MBZ*
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    UCHAR Reserved1[4];
    USHORT Index;                       // zero, interface or endpoint
    USHORT Reserved2;
};

struct _URB_CONTROL_FEATURE_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG Reserved;
    ULONG Reserved2;
    ULONG Reserved3;
    ULONG Reserved4;
    ULONG  Reserved5;
    ULONG UrbLink;               // *reserved MBZ*
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    USHORT Reserved0;
    USHORT FeatureSelector;
    USHORT Index;                       // zero, interface or endpoint
    USHORT Reserved1;
};

struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG Reserved;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *reserved MBZ*
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    UCHAR RequestTypeReservedBits;
    UCHAR Request;
    USHORT Value;
    USHORT Index;
    USHORT Reserved1;
};

struct _URB_CONTROL_GET_INTERFACE_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *reserved MBZ*
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    UCHAR Reserved1[4];
    USHORT Interface;
    USHORT Reserved2;
};

struct _URB_CONTROL_GET_CONFIGURATION_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    ULONG Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *resrved MBZ*
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    UCHAR Reserved1[8];
};

struct _URB_OS_FEATURE_DESCRIPTOR_REQUEST_32 {
    struct _URB_HEADER_32 Hdr;  // function code indicates get or set.
    ULONG Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use
    UCHAR   Recipient:5;                // Recipient {Device,Interface,Endpoint}
    UCHAR   Reserved1:3;
    UCHAR   Reserved2;
    UCHAR   InterfaceNumber;            // wValue - high byte
    UCHAR   MS_PageIndex;               // wValue - low byte
    USHORT  MS_FeatureDescriptorIndex;  // wIndex field
    USHORT  Reserved3;
};

typedef struct _USBD_ISO_PACKET_DESCRIPTOR_32 {
    ULONG Offset;       // INPUT Offset of the packet from the begining of the
                        // buffer.

    ULONG Length;       // OUTPUT length of data received (for in).
                        // OUTPUT 0 for OUT.
    USBD_STATUS Status; // status code for this packet.
} USBD_ISO_PACKET_DESCRIPTOR_32, *PUSBD_ISO_PACKET_DESCRIPTOR_32;

struct _URB_ISOCH_TRANSFER_32 
{
    //
    // This block is the same as CommonTransfer
    //
    struct _URB_HEADER_32 Hdr;                 // function code indicates get or set.
    USBD_PIPE_HANDLE_32 PipeHandle;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    ULONG TransferBuffer;
    ULONG  TransferBufferMDL;             // *optional*
    ULONG UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA_32 hca;           // fields for HCD use

    //
    // this block contains transfer fields
    // specific to isochronous transfers
    //

    // 32 bit frame number to begin this transfer on, must be within 1000
    // frames of the current USB frame or an error is returned.

    // START_ISO_TRANSFER_ASAP flag in transferFlags:
    // If this flag is set and no transfers have been submitted
    // for the pipe then the transfer will begin on the next frame
    // and StartFrame will be updated with the frame number the transfer
    // was started on.
    // If this flag is set and the pipe has active transfers then
    // the transfer will be queued to begin on the frame after the
    // last transfer queued is completed.
    //
    ULONG StartFrame;
    // number of packets that make up this request
    ULONG NumberOfPackets;
    // number of packets that completed with errors
    ULONG ErrorCount;
    USBD_ISO_PACKET_DESCRIPTOR_32 IsoPacket[1];
};


typedef struct _URB_32 {
    union {
        struct _URB_HEADER_32
            UrbHeader;
        struct _URB_SELECT_INTERFACE_32
            UrbSelectInterface;
        struct _URB_SELECT_CONFIGURATION_32
            UrbSelectConfiguration;
        struct _URB_PIPE_REQUEST_32
            UrbPipeRequest;
        struct _URB_FRAME_LENGTH_CONTROL_32
            UrbFrameLengthControl;
        struct _URB_GET_FRAME_LENGTH_32
            UrbGetFrameLength;
        struct _URB_SET_FRAME_LENGTH_32
            UrbSetFrameLength;
        struct _URB_GET_CURRENT_FRAME_NUMBER_32
            UrbGetCurrentFrameNumber;
        struct _URB_CONTROL_TRANSFER_32
            UrbControlTransfer;
            
        struct _URB_CONTROL_TRANSFER_EX_32
            UrbControlTransferEx;

        struct _URB_PIPE_IO_POLICY_32
            UrbPipeIoPolicy;

        struct _URB_BULK_OR_INTERRUPT_TRANSFER_32
            UrbBulkOrInterruptTransfer;
        struct _URB_ISOCH_TRANSFER_32
            UrbIsochronousTransfer;

        // for standard control transfers on the default pipe
        struct _URB_CONTROL_DESCRIPTOR_REQUEST_32
            UrbControlDescriptorRequest;
        struct _URB_CONTROL_GET_STATUS_REQUEST_32
            UrbControlGetStatusRequest;
        struct _URB_CONTROL_FEATURE_REQUEST_32
            UrbControlFeatureRequest;
        struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST_32
            UrbControlVendorClassRequest;
        struct _URB_CONTROL_GET_INTERFACE_REQUEST_32
            UrbControlGetInterfaceRequest;
        struct _URB_CONTROL_GET_CONFIGURATION_REQUEST_32
            UrbControlGetConfigurationRequest;
        struct _URB_OS_FEATURE_DESCRIPTOR_REQUEST_32
            UrbOSFeatureDescriptorRequest;
    };
} URB_32, *PURB_32;

typedef struct _USB_BUS_INTERFACE_USBDI_V0_32 {

    USHORT Size;
    USHORT Version;
    
    ULONG BusContext;
    ULONG InterfaceReference;
    ULONG InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    ULONG GetUSBDIVersion;
    ULONG QueryBusTime;
    ULONG SubmitIsoOutUrb;
    ULONG QueryBusInformation;
    
} USB_BUS_INTERFACE_USBDI_V0_32, *PUSB_BUS_INTERFACE_USBDI_V0_32;

typedef struct _USB_BUS_INTERFACE_USBDI_V1_32 {

    USHORT Size;
    USHORT Version;
    
    ULONG BusContext;
    ULONG InterfaceReference;
    ULONG InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    ULONG GetUSBDIVersion;
    ULONG QueryBusTime;
    ULONG SubmitIsoOutUrb;
    ULONG QueryBusInformation;
    ULONG IsDeviceHighSpeed;
    
} USB_BUS_INTERFACE_USBDI_V1_32, *PUSB_BUS_INTERFACE_USBDI_V1_32;

typedef struct _USB_BUS_INTERFACE_USBDI_V2_32 {

    USHORT Size;
    USHORT Version;
    
    ULONG BusContext;
    ULONG InterfaceReference;
    ULONG InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    ULONG GetUSBDIVersion;
    ULONG QueryBusTime;
    ULONG SubmitIsoOutUrb;
    ULONG QueryBusInformation;
    ULONG IsDeviceHighSpeed;

    ULONG EnumLogEntry;
    
} USB_BUS_INTERFACE_USBDI_V2_32, *PUSB_BUS_INTERFACE_USBDI_V2_32;

// version 3 Vista and later

typedef struct _USB_BUS_INTERFACE_USBDI_V3_32 {

    USHORT Size;
    USHORT Version;
    
    ULONG BusContext;
    ULONG InterfaceReference;
    ULONG InterfaceDereference;
    
    // interface specific entries go here

    // the following functions must be callable at high IRQL,
    // (ie higher than DISPATCH_LEVEL)
    
    ULONG GetUSBDIVersion;
    ULONG QueryBusTime;
    ULONG SubmitIsoOutUrb;
    ULONG QueryBusInformation;
    ULONG IsDeviceHighSpeed;
    ULONG EnumLogEntry;

    ULONG QueryBusTimeEx;
    ULONG QueryControllerType;
           
} USB_BUS_INTERFACE_USBDI_V3_32, *PUSB_BUS_INTERFACE_USBDI_V3_32;

typedef struct _INTERFACE_32 {
    USHORT Size;
    USHORT Version;
    ULONG Context;
    ULONG InterfaceReference;
    ULONG InterfaceDereference;
    // interface specific entries go here
} INTERFACE_32, *PINTERFACE_32;

typedef struct {
    INTERFACE_32 Interface;
    ULONG   USBCAMD_WaitOnDeviceEvent;
    ULONG   USBCAMD_BulkReadWrite;
    ULONG   USBCAMD_SetVideoFormat;
    ULONG   USBCAMD_SetIsoPipeState;
    ULONG   USBCAMD_CancelBulkReadWrite;
} USBCAMD_INTERFACE_32, *PUSBCAMD_INTERFACE_32;

};
#endif