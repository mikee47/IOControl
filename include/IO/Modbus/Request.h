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
public:
	Request(Device& device) : IO::Request(device)
	{
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(IO::Request::device());
	}

	virtual Function fillRequestData(PDU::Data& data) = 0;

	/**
	 * @brief Process a received PDU
	 * @param pdu
	 * @retval ErrorCode If request is re-submitted, return Error::pending,
	 * otherwise request will be completed with given error.
	 */
	virtual ErrorCode callback(PDU& pdu) = 0;
};

} // namespace Modbus
} // namespace IO
