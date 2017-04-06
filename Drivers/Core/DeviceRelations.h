/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   DeviceRelations.h

Abstract:

   Class that monitors new devices apperance and old devices deletion on a bus. 
   Is used to connect filters drivers for new devices on a bus.

Environment:

    Kernel mode

--*/
#ifndef DEVICE_RELATIONS_H
#define DEVICE_RELATIONS_H

#include "BaseClass.h"
//#include "List.h"
#include "lock.h"
#include "ArrayT.h"

class CDeviceRelations;

class CPdoBase : public virtual CBaseClass
{
private:
	PVOID				m_DeviceIndef;
	CDeviceRelations	*m_pRelation;
	bool				m_bMySelf;

public:
	CPdoBase (PVOID	DeviceIndef, CDeviceRelations *pRelation) : CBaseClass (), m_DeviceIndef (DeviceIndef), m_pRelation (pRelation), m_bMySelf (false) {}
	virtual ~CPdoBase  ();

	NEW_ALLOC_DEFINITION ('LRVD')

	PVOID GetDevIndef () {return m_DeviceIndef;}
	void SetDevIndef (PVOID pIndef) {m_DeviceIndef = pIndef;}

	void ClearParent(){ m_pRelation = NULL; }

	void IncMyself ();
	void DecMyself ();
};

typedef boost::intrusive_ptr <CPdoBase> TPtrPdoBase;
typedef CArrayT <TPtrPdoBase>			TArrPdo;

class CDeviceRelations : public virtual CBaseClass
{
public:
	CDeviceRelations(void);
	~CDeviceRelations(void);

public:
	NEW_ALLOC_DEFINITION ('SRDC')

	TPtrPdoBase GetChild (int iNum);
	
	virtual void QueryRelations (PDEVICE_RELATIONS RelationsNew);
	virtual TPtrPdoBase IsExistPdo (PVOID DeviceIndef);
	virtual TPtrPdoBase AddPdo (PDEVICE_OBJECT PhysicalDeviceObject) = 0;
	virtual NTSTATUS RemovePdoNum (int uNum, bool bLock = true);
	virtual NTSTATUS RemovePdo (PVOID pIndef);

	TArrPdo& GetList () {return m_arrPdo;}
	CSpinLock& SpinDr () {return m_spinDr;}


protected:
	CSpinLock	m_spinDr;
	TArrPdo		m_arrPdo;
	bool		m_bRelationSelf;

};

#endif