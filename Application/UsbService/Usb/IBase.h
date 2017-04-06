/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    IBase.h

Abstract:

	Interface class for USB device server/client.

Environment:

    User mode

--*/

#ifndef IBASE_H
#define IBASE_H

class IBase
{
public:
	IBase () {}
	virtual ~IBase () {}

public:
	virtual void Start () = 0;
	virtual void Stop () = 0;
	virtual int GetStatus () = 0;

};

#endif
