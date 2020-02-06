/*
 * IOModbus.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include "IOModbus.h"

// Device configuration
DEFINE_FSTR(MODBUS_CONTROLLER_CLASSNAME, "modbus")

// Json tags
DEFINE_FSTR(ATTR_STATES, "states")

DEFINE_FSTR_LOCAL(ATTR_ADDRESS, "address")
DEFINE_FSTR_LOCAL(ATTR_BAUDRATE, "baudrate")

// Pin to switch  MAX485 between receive (low) and transmit (high)
#define MBPIN_TX_EN 12

/* CModbusController */

/*
 * Inherited classes call this after their own start() code.
 */
void ModbusController::start()
{
	m_modbus.begin(MBPIN_TX_EN, ModbusDelegate(&ModbusController::modbusCallback, this));
	memset(&m_mbt, 0, sizeof(m_mbt));
	IOController::start();
}

/*
 * Inherited classes call this before their own stop() code.
 */
void ModbusController::stop()
{
	IOController::stop();
}

void ModbusController::execute(IORequest& request)
{
	auto& req = reinterpret_cast<ModbusRequest&>(request);
	m_mbt.param = uint32_t(&req);
	m_mbt.slaveId = req.device().address();
	m_mbt.baudrate = req.device().baudrate();
	req.fillRequestData(m_mbt);
	if(!m_modbus.execute(m_mbt)) {
		debug_e("modbus unexpectedly busy");
	}
}

void ModbusController::modbusCallback(ModbusTransaction& mbt)
{
	auto req = reinterpret_cast<ModbusRequest*>(mbt.param);
	assert(req != nullptr);
	req->callback(m_mbt);
}

/* CModbusDevice */

ioerror_t ModbusDevice::init(JsonObjectConst config)
{
	ioerror_t err = IODevice::init(config);
	if(err)
		return err;

	m_config.address = config[ATTR_ADDRESS];
	m_config.baudrate = config[ATTR_BAUDRATE];
	if(m_config.address == 0) {
		return ioe_no_address;
	}
	if(m_config.baudrate == 0) {
		return ioe_no_baudrate;
	}

	return ioe_success;
}

/* CModbusRequest */

void ModbusRequest::callback(const ModbusTransaction& mbt)
{
	complete(checkStatus(mbt) ? status_success : status_error);
}

bool ModbusRequest::checkStatus(const ModbusTransaction& mbt)
{
	if(mbt.status == MBE_Success) {
		return true;
	}

	m_exception = mbt.status;

	String statusStr = modbusExceptionString(mbt.status);
	debug_e("modbus error %02X (%s)", mbt.status, statusStr.c_str());

	return false;
}

void ModbusRequest::getJson(JsonObject json) const
{
	IORequest::getJson(json);

	if(m_status == status_error) {
		setError(json, m_exception, modbusExceptionString(m_exception));
	}
}
