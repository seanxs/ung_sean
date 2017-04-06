/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   Timeouts.h

Abstract:

	Windows timer to run specified callback after some period.

Environment:

    User mode

--*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

namespace eltima {
namespace utils {

class ITimeout;
typedef boost::shared_ptr<ITimeout> ShptrTimeout;

class ITimeout
{
public:
	/// factory method
	static ShptrTimeout newTimeout();

	typedef boost::function<void()> TimeoutCallback;

	/// Start timeout countdown for specified amount of time. 
	/// After that time period is elapsed the callback will be launched.
	virtual void Start(const unsigned timeout_ms, ITimeout::TimeoutCallback callback) = 0;
	
	/// Start periodic function call after specified timeout.
	virtual void StartPeriodic(const unsigned timeout_ms, ITimeout::TimeoutCallback callback) = 0;

	/// Cancel timeout countdown
	virtual void Cancel() = 0;

	virtual ~ITimeout() {}
};

} // namespace utils
} // namespace eltima
