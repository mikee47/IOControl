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
	struct Config {
		IO::Device::Config base;
		struct Slave {
			uint8_t address;
			unsigned baudrate;
			unsigned timeout; ///< Max time between command/response
		};
		Slave slave;
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
	bool execute(Request* request);
	bool readResponse(Request* request);

	Config::Slave m_config;
	Function requestFunction{};
};

} // namespace Modbus
} // namespace IO
