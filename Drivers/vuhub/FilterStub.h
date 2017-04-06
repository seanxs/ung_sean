/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    FilterStub.h

Environment:

    Kernel mode

--*/

#pragma once

#include "Core\FilterExtension.h"
#include "Core\DeviceRelations.h"
#include "PdoExtension.h"

class CFilterStub : public CFilterExtension, public CPdoBase, public virtual CDeviceRelations
{
public:
	CFilterStub(CPdoExtension *pBase, PDEVICE_OBJECT PhysicalDeviceObject, CDeviceRelations *pRelation);
	~CFilterStub(void);

	NEW_ALLOC_DEFINITION ('bSFC')

	virtual TPtrPdoBase AddPdo (PDEVICE_OBJECT PhysicalDeviceObject);
	virtual NTSTATUS DispatchPnp (PDEVICE_OBJECT DeviceObject, PIRP Irp);
	virtual NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);

	NTSTATUS PnpRemove (PDEVICE_OBJECT DeviceObject, PIRP Irp);
private:
	CPdoExtension	*m_pBase;
	bool			m_bSetSecurity;
	PDEVICE_OBJECT	m_PhysicalDeviceObject;

};
