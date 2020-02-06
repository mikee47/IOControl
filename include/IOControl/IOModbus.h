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

#include <IOControl.h>
#include "Modbus.h"

// Device configuration
DECLARE_FSTR(MODBUS_CONTROLLER_CLASSNAME)

// json tags
DECLARE_FSTR(ATTR_STATES)

class ModbusDevice;
class ModbusController;

class ModbusRequest : public IORequest
{
	friend ModbusController;

public:
	ModbusRequest(ModbusDevice& device) : IORequest(reinterpret_cast<IODevice&>(device))
	{
	}

	const ModbusDevice& device() const
	{
		return reinterpret_cast<const ModbusDevice&>(m_device);
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
class ModbusDevice : public IODevice
{
public:
	ModbusDevice(ModbusController& controller) : IODevice(reinterpret_cast<IOController&>(controller))
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
	ioerror_t init(JsonObjectConst config);

private:
	modbus_slave_config_t m_config;
};

class ModbusController : public IOController
{
public:
	ModbusController(uint8_t instance) : IOController(instance)
	{
	}

	String classname()
	{
		return MODBUS_CONTROLLER_CLASSNAME;
	}

	void start();
	void stop();

	bool busy() const
	{
		return m_modbus.busy();
	}

private:
	void modbusCallback(ModbusTransaction& mbt);
	void execute(IORequest& request);

private:
	Modbus m_modbus;
	ModbusTransaction m_mbt;
};
