/*
 * IOModbus.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 *
 * Implements IOControl stack on top of physical interface (modbus.cpp).
 *
 */

#pragma once

#include <IO/Control.h>
#include <IO/RS485/Controller.h>
#include "ADU.h"

namespace IO
{
namespace Modbus
{
constexpr unsigned DEFAULT_BAUDRATE = 9600;

class Device;

class Request : public IO::Request
{
	friend Device;

public:
	Request(Device& device) : IO::Request(reinterpret_cast<IO::RS485::Device&>(device))
	{
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(m_device);
	}

	void getJson(JsonObject json) const;

	// Transaction result
	Error error;

protected:
	virtual Function fillRequestData(PDU::Data& data) = 0;
	virtual void callback(PDU& pdu) = 0;

private:
	Exception m_exception = Exception::Success;
};

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
	struct SlaveConfig {
		uint8_t address;
		unsigned baudrate;
		unsigned timeout; ///< Max time between command/response
	};

	struct Config : public IO::Device::Config {
		SlaveConfig slave;
	};

	using RS485::Device::Device;

	const DeviceType type() const override
	{
		return DeviceType::Modbus;
	}

	Error init(const Config& config);
	Error init(JsonObjectConst config) override;

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

	uint16_t address() const override
	{
		return m_config.address;
	}

	unsigned baudrate() const
	{
		return m_config.baudrate ?: DEFAULT_BAUDRATE;
	}

	void handleEvent(IO::Request* request, Event event) override;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

private:
	void execute(Request* request);
	void readRequest();
	void readResponse(Request* request);

	SlaveConfig m_config;
	Function requestFunction{};
};

Error readRequest(RS485::Controller& controller, ADU& adu);

static inline void sendResponse(RS485::Controller& controller, ADU& adu)
{
	auto aduSize = adu.prepareResponse();
	controller.send(adu.buffer, aduSize);
}

} // namespace Modbus
} // namespace IO
