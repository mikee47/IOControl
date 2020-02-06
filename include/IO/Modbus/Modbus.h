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
#include "driver.h"

namespace Modbus
{
// Device configuration
DECLARE_FSTR(CONTROLLER_CLASSNAME)

// json tags
DECLARE_FSTR(ATTR_STATES)

class Device;
class Controller;

class Request : public IO::Request
{
	friend Controller;

public:
	Request(Device& device) : IO::Request(reinterpret_cast<IO::Device&>(device))
	{
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(m_device);
	}

	void getJson(JsonObject json) const;

protected:
	virtual void fillRequestData(ModbusTransaction& mbt) = 0;
	virtual void callback(const ModbusTransaction& mbt) = 0;

	// Return true if handled
	bool checkStatus(const ModbusTransaction& mbt);

private:
	ModbusExceptionCode m_exception = MBE_Success;
};

/*
 * A virtual device, represents a modbus slave device.
 * Actual devices must implement
 *  createRequest()
 *  callback()
 *  fillRequestData()
 */
class Device : public IO::Device
{
public:
	struct SlaveConfig {
		uint8_t address;
		unsigned baudrate;
	};

	struct Config : public IO::Device::Config {
		SlaveConfig slave;
	};

	Device(Controller& controller) : IO::Device(reinterpret_cast<IO::Controller&>(controller))
	{
	}

	uint16_t address() const override
	{
		return m_config.address;
	}

	unsigned baudrate() const
	{
		return m_config.baudrate;
	}

protected:
	IO::Error init(const Config& config);
	IO::Error init(JsonObjectConst config) override;
	void parseJson(JsonObjectConst json, Config& cfg);

private:
	SlaveConfig m_config;
};

class Controller : public IO::Controller
{
public:
	Controller(uint8_t instance) : IO::Controller(instance)
	{
	}

	String classname() override
	{
		return CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

	bool busy() const override
	{
		return m_driver.busy();
	}

private:
	void modbusCallback(ModbusTransaction& mbt);
	void execute(IO::Request& request);

private:
	ModbusDriver m_driver;
	ModbusTransaction m_mbt;
};

} // namespace Modbus
