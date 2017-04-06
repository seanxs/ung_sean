/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    FilterStub.cpp

Environment:

    Kernel mode

--*/

#include "FilterStub.h"
#include "Core\Buffer.h"
#include "PdoExtension.h"
#include "UsbDevToTcp.h"
#include <wdmsec.h>
#include <initguid.h>
#include "Common\common.h"
#include "Core\SystemEnum.h"
#include "Core\SecurityUtils.h"

CFilterStub::CFilterStub(CPdoExtension *pBase, PDEVICE_OBJECT PhysicalDeviceObject, CDeviceRelations *pRelation)
	: m_pBase (pBase), m_PhysicalDeviceObject (PhysicalDeviceObject), CPdoBase (PhysicalDeviceObject, pRelation)
	, m_bSetSecurity (false)
{
	SetDebugName ("CFilterStub");
}

CFilterStub::~CFilterStub(void)
{
}

TPtrPdoBase CFilterStub::AddPdo (PDEVICE_OBJECT PhysicalDeviceObject)
{
	CFilterStub *pStub = new CFilterStub (m_pBase, PhysicalDeviceObject, this);
	TPtrPdoBase	pBase (pStub);

	DebugPrint (DebugTrace, "AddPdo");

	pStub->AttachToDevice (m_DriverObject, PhysicalDeviceObject);

	SpinDr().Lock();
	pBase->IncMyself();
	GetList().Add (pBase);
	SpinDr().UnLock();

	return pStub;
}

NTSTATUS CFilterStub::DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status = STATUS_SUCCESS;
	PDEVICE_RELATIONS       Relations = NULL;

	DebugPrint (DebugTrace, "%s", MajorFunctionString (Irp));
	DebugPrint (DebugSpecial, "%x %s", DeviceObject, PnPMinorFunctionString (Irp));
    
    irpStack = IoGetCurrentIrpStackLocation (Irp);
    //
    // If the device has been removed, the driver should 
    // not pass the IRP down to the next lower driver.
    //

	++m_irpCounter;
    
    switch (irpStack->MinorFunction) 
	{
    case IRP_MN_QUERY_DEVICE_RELATIONS:
		if (irpStack->Parameters.QueryDeviceRelations.Type == BusRelations)
		{
			Relations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
			if (Relations)
			{
				QueryRelations (Relations);

				if (m_pBase && m_pBase->m_pTcp && wcslen (m_pBase->m_pTcp->m_szUserSid))
				{
					PDEVICE_OBJECT			pDev;
					CBuffer					buff (PagedPool, 1024);

					if (!m_bSetSecurity)
					{
						pDev = m_Self->AttachedDevice;
						
						CSecurityUtils::SetSecurityDescriptor(m_pBase->GetDeviceObject (), &m_pBase->m_sd);
						while (pDev)
						{
							CSecurityUtils::SetSecurityDescriptor(pDev, &m_pBase->m_sd);
							pDev = pDev->AttachedDevice;
						}
						m_bSetSecurity = true;
					}
				}
			}
		}
		break;

    case IRP_MN_REMOVE_DEVICE:
		return PnpRemove (DeviceObject, Irp);
	};

	--m_irpCounter;
	return CFilterExtension::DispatchPnp (DeviceObject, Irp);
}

NTSTATUS CFilterStub::PnpRemove (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	CBuffer					buff (NonPagedPool, 1024);
	ULONG					LengthReturned;
	UNICODE_STRING			strPath;
	NTSTATUS				status;

	if (NT_SUCCESS (IoGetDeviceProperty (	m_PhysicalDeviceObject, 
											DevicePropertyPhysicalDeviceObjectName,
											buff.GetBufferSize (),
											buff.GetData (),
											&LengthReturned)))
	{
		RtlInitUnicodeString (&strPath, (PWCHAR)buff.GetData ());

		m_pBase->DelLink (&strPath);
	}

	//
	// Wait for all outstanding requests to complete
	//
	DebugPrint(DebugInfo,"Waiting for outstanding requests");
	--m_irpCounter;
	m_irpCounter.WaitRemove ();

	IoSkipCurrentIrpStackLocation(Irp);

	status = IoCallDriver(m_NextLowerDriver, Irp);

	SetNewPnpState (DriverBase::Deleted);

	*((CFilterExtension**)m_Self->DeviceExtension) = NULL;

	IoDetachDevice(m_NextLowerDriver);
	IoDeleteDevice(DeviceObject);
	DecMyself();

	return status;
}

NTSTATUS CFilterStub::AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
	NTSTATUS                status = STATUS_SUCCESS;
	PDEVICE_OBJECT          deviceObject = NULL;
	ULONG                   deviceType = FILE_DEVICE_UNKNOWN | FILE_AUTOGENERATED_DEVICE_NAME;


	DebugPrint (DebugTrace, "AttachToDevice");
	//
	// IoIsWdmVersionAvailable(1, 0x20) returns TRUE on os after Windows 2000.
	//
	if (!IoIsWdmVersionAvailable(1, 0x20)) 
	{
		//
		// Win2K system bugchecks if the filter attached to a storage device
		// doesn't specify the same DeviceType as the device it's attaching
		// to. This bugcheck happens in the filesystem when you disable
		// the devicestack whose top level deviceobject doesn't have a VPB.
		// To workaround we will get the toplevel object's DeviceType and
		// specify that in IoCreateDevice.
		//
		deviceObject = IoGetAttachedDeviceReference(PhysicalDeviceObject);
		deviceType = deviceObject->DeviceType;
		ObDereferenceObject(deviceObject);
	}

	//
	// Create a filter device object.
	//

	{
		UNICODE_STRING			Name;
		WCHAR					szBuffer [50] = L"\\Device\\efltr-%lu";
		bool					bSecurity = false;

		if (m_pBase->m_pTcp)
		{
			bSecurity = wcslen (m_pBase->m_pTcp->m_szUserSid)?true:false;
			if (bSecurity && !m_pBase->m_pAcl)
			{
				m_pBase->m_pAcl = CSecurityUtils::BuildSecurityDescriptor(m_pBase->m_pTcp->m_sid.GetData (), m_pBase->m_sd);
			}
		}

		for (int a = 0; a < 100; a ++)
		{
			RtlStringCchPrintfW ( szBuffer, 50, L"\\Device\\TESTvu-%lu", a);
			RtlInitUnicodeString( &Name, szBuffer);


				status = IoCreateDevice (DriverObject,
				                         sizeof (CFilterExtension*),
				                         &Name,  // No Name
				                         deviceType,
				                         FILE_DEVICE_SECURE_OPEN,
				                         FALSE,
				                         &deviceObject);

			if (NT_SUCCESS (status)) 
			{
				if (bSecurity)
				{
					CSecurityUtils::SetSecurityDescriptor(deviceObject, &m_pBase->m_sd);
				}
				break;

			}
		}
	}

	if (!NT_SUCCESS (status) || !deviceObject) 
	{
		return false;
	}
	DebugPrint (DebugTrace, "AddDevice PDO (0x%x) FDO (0x%x)",
		PhysicalDeviceObject, deviceObject);


	*((CFilterExtension**)deviceObject->DeviceExtension) = this;

	m_NextLowerDriver = IoAttachDeviceToDeviceStack (
		deviceObject,
		PhysicalDeviceObject);
	//
	// Failure for attachment is an indication of a broken plug & play system.
	//

	if(NULL == m_NextLowerDriver) 
	{
		DebugPrint (DebugError, "Did not Attach to PDO: 0x%x", PhysicalDeviceObject);
		IoDeleteDevice(deviceObject);
		return STATUS_UNSUCCESSFUL;
	}

	deviceObject->Flags |= m_NextLowerDriver->Flags &
		(DO_BUFFERED_IO | DO_DIRECT_IO |
		DO_POWER_PAGABLE );


	deviceObject->DeviceType = m_NextLowerDriver->DeviceType;

	deviceObject->Characteristics = m_NextLowerDriver->Characteristics;

	m_Self = deviceObject;

	//
	// Set the initial state of the Filter DO
	//
	m_DevicePnPState =  DriverBase::NotStarted;
	m_PreviousPnPState = DriverBase::NotStarted;

	DebugPrint(DebugTrace, "AddDevice: %x to %x->%x", deviceObject,
		m_NextLowerDriver,
		PhysicalDeviceObject);

	deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}
