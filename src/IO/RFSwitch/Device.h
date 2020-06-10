#pragma once

#include <IO/Device.h>
#include "Controller.h"

namespace IO
{
namespace RFSwitch
{
/*
 * A specific type of RF device protocol.
 * Actual RF I/O is performed by Controller.
 */
class Device : public IO::Device
{
public:
	Device(Controller& controller) : IO::Device(controller)
	{
	}

	const DeviceClassInfo classInfo() const override;

	IO::Request* createRequest() override
	{
		return new Request(*this);
	}

	const Protocol& protocol() const
	{
		return m_protocol;
	}

	Error start() override
	{
		m_state = devstate_normal;
		return Error::success;
	}

protected:
	Error init(JsonObjectConst config) override;

protected:
	Protocol m_protocol;
};

} // namespace RFSwitch
} // namespace IO
