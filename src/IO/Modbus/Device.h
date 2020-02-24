#pragma once

#include <IO/RS485/Device.h>
#include "ADU.h"

namespace IO
{
namespace Modbus
{
constexpr unsigned DEFAULT_BAUDRATE = 9600;

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
	struct SlaveConfig {
		uint8_t address;
		unsigned baudrate;
		unsigned timeout; ///< Max time between command/response
	};

	struct Config : public RS485::Device::Config {
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
	void readResponse(Request* request);

	SlaveConfig m_config;
	Function requestFunction{};
};

} // namespace Modbus
} // namespace IO
