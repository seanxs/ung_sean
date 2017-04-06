/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   RefCounter.h

Environment:

    Kernel mode

--*/

#pragma once 

extern "C" 
{
#pragma pack(push, 1)
#if _WIN32_WINNT==0x0500
#define _WIN2K_COMPAT_SLIST_USAGE
#endif
#include <ntddk.h>
#include <WINDEF.H>
#pragma pack(pop)
#include <minmax.h>
};


#define NEW_ALLOC_DEFINITION(TAG) void *operator new( size_t stAllocateBlock, POOL_TYPE PoolType = NonPagedPool) \
	{ \
	return ExAllocatePoolWithTag (PoolType, stAllocateBlock, TAG); \
	} \
	void *operator new []( size_t stAllocateBlock, POOL_TYPE PoolType = NonPagedPool) \
{ \
	return ExAllocatePoolWithTag (PoolType, stAllocateBlock, TAG); \
}


namespace Common
{

class CRefCounter
{
	friend void intrusive_ptr_add_ref(CRefCounter*);
	friend void intrusive_ptr_release(CRefCounter*);
	
protected:
	LONG				 m_refs;  
public: 
	CRefCounter():m_refs(0) {}
	virtual ~CRefCounter()	{}

	NEW_ALLOC_DEFINITION ('GNUE')
	void operator delete (void *pMem)
	{
		ExFreePool (pMem);
	}
	void operator delete [] (void *pMem)
	{
		ExFreePool (pMem);
	}
}; 
	
inline void intrusive_ptr_add_ref(CRefCounter* h)
{
	InterlockedIncrement (&h->m_refs);
}

inline void intrusive_ptr_release(CRefCounter* h)
{
	if (InterlockedDecrement (&h ->m_refs) == 0)
	{
		delete h;
		h = NULL;
	}
}

};
 