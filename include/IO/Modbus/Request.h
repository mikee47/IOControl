#pragma once

#include <IO/Request.h>
#include "Device.h"
#include "PDU.h"

namespace IO
{
namespace Modbus
{
class Request : public IO::Request
{
	friend Device;

public:
	Request(Device& device) : IO::Request(device)
	{
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(m_device);
	}

	void getJson(JsonObject json) const;

	// Transaction result
	Error error{};

protected:
	virtual Function fillRequestData(PDU::Data& data) = 0;
	virtual void callback(PDU& pdu) = 0;

private:
	Exception m_exception = Exception::Success;
};

} // namespace Modbus
} // namespace IO
