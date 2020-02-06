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

struct modbus_slave_config_t {
	uint8_t address;
	unsigned baudrate;
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
	IO::Error init(JsonObjectConst config);

private:
	modbus_slave_config_t m_config;
};

class Controller : public IO::Controller
{
public:
	Controller(uint8_t instance) : IO::Controller(instance)
	{
	}

	String classname()
	{
		return CONTROLLER_CLASSNAME;
	}

	void start();
	void stop();

	bool busy() const
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
