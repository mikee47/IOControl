/*
 * IOModbus.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include <IO/Modbus/Modbus.h>

namespace Modbus
{
// Device configuration
DEFINE_FSTR(CONTROLLER_CLASSNAME, "modbus")

// Json tags
DEFINE_FSTR(ATTR_STATES, "states")

DEFINE_FSTR_LOCAL(ATTR_ADDRESS, "address")
DEFINE_FSTR_LOCAL(ATTR_BAUDRATE, "baudrate")

// Pin to switch  MAX485 between receive (low) and transmit (high)
#define MBPIN_TX_EN 12

/* Controller */

/*
 * Inherited classes call this after their own start() code.
 */
void Controller::start()
{
	m_driver.begin(MBPIN_TX_EN, ModbusDelegate(&Controller::modbusCallback, this));
	memset(&m_mbt, 0, sizeof(m_mbt));
	IO::Controller::start();
}

/*
 * Inherited classes call this before their own stop() code.
 */
void Controller::stop()
{
	IO::Controller::stop();
}

void Controller::execute(IO::Request& request)
{
	auto& req = reinterpret_cast<Request&>(request);
	m_mbt.param = uint32_t(&req);
	m_mbt.slaveId = req.device().address();
	m_mbt.baudrate = req.device().baudrate();
	req.fillRequestData(m_mbt);
	if(!m_driver.execute(m_mbt)) {
		debug_e("driver unexpectedly busy");
	}
}

void Controller::modbusCallback(ModbusTransaction& mbt)
{
	auto req = reinterpret_cast<Request*>(mbt.param);
	assert(req != nullptr);
	req->callback(m_mbt);
}

/* Device */

IO::Error Device::init(const Config& config)
{
	auto err = IO::Device::init(config);
	if(!!err) {
		return err;
	}

	if(config.slave.address == 0) {
		return IO::Error::no_address;
	}

	if(config.slave.baudrate == 0) {
		return IO::Error::no_baudrate;
	}

	m_config = config.slave;
	return IO::Error::success;
}

IO::Error Device::init(JsonObjectConst config)
{
	Config cfg{};
	parseJson(config, cfg);
	return init(cfg);
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	IO::Device::parseJson(json, cfg);
	cfg.slave.address = json[ATTR_ADDRESS];
	cfg.slave.baudrate = json[ATTR_BAUDRATE];
}

/* Request */

void Request::callback(const ModbusTransaction& mbt)
{
	complete(checkStatus(mbt) ? IO::Status::success : IO::Status::error);
}

bool Request::checkStatus(const ModbusTransaction& mbt)
{
	if(mbt.status == MBE_Success) {
		return true;
	}

	m_exception = mbt.status;

	String statusStr = modbusExceptionString(mbt.status);
	debug_e("modbus error %02X (%s)", mbt.status, statusStr.c_str());

	return false;
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);

	if(m_status == IO::Status::error) {
		IO::setError(json, m_exception, modbusExceptionString(m_exception));
	}
}

} // namespace Modbus
