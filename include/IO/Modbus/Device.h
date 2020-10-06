#pragma once

#include <IO/RS485/Device.h>
#include "ADU.h"

namespace IO
{
namespace Modbus
{
class Request;

/*
 * A virtual device, represents a modbus slave device.
 * Actual devices must implement
 *  createRequest()
 *  callback()
 *  fillRequestData()
 */
class Device : public RS485::Device
{
public:
	using RS485::Device::Device;

	ErrorCode init(const RS485::Device::Config& config);

	const DeviceType type() const override
	{
		return DeviceType::Modbus;
	}

	/**
	 * @brief Handle a broadcast message
	 */
	virtual void onBroadcast(const ADU& adu)
	{
	}

	/**
	 * @brief Handle a message specifically for this device
	 */
	virtual void onRequest(ADU& adu)
	{
	}

	void handleEvent(IO::Request* request, Event event) override;

private:
	ErrorCode execute(Request* request);
	ErrorCode readResponse(Request* request);

	Function requestFunction{};
};

} // namespace Modbus
} // namespace IO
