/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   RefCounter.h

Environment:

    User mode

--*/

#pragma once 

#include "boost/noncopyable.hpp"
#include "Lock.h"

namespace Common
{

class CRefCounter : public boost::noncopyable
{
	friend void intrusive_ptr_add_ref(CRefCounter*);
	friend void intrusive_ptr_release(CRefCounter*);
	
protected:
	CLock					m_Lock;
	DWORD					m_refs;  
public: 
	CRefCounter():m_refs(0) {}
	virtual ~CRefCounter()	{}
	DWORD GetRefs() const { return m_refs; } 
}; 
	
inline void intrusive_ptr_add_ref(CRefCounter* h)
{
	CAutoLock lock (&h->m_Lock);
	h ->m_refs++;
}

inline void intrusive_ptr_release(CRefCounter* h)
{
	CAutoLock lock (&h->m_Lock);
	h ->m_refs--;
	if (h ->m_refs < 2)
	{
		h ->m_refs = h ->m_refs;
	}
	if (h ->m_refs == 0)
	{
		delete h;
		h = NULL;
	}
}

} // namespace Common
