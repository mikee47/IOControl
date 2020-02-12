/*
 * IOModbus.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 * Modbus uses UART0 via MAX485 chip with extra GPIO to control direction.
 * We use custom serial port code because it's actually simpler and much more efficient
 * than using the framework.
 *
 *  - Transactions will easily fit into hardware FIFO so no need for additional memory overhead;
 *  - ISR set to trigger on completion of transmit/receive packets rather than individual bytes;
 *
 */

#include <IO/Modbus/Modbus.h>
#include "Digital.h"
#include "Platform/System.h"
#include <driver/uart.h>

namespace IO
{
namespace Modbus
{
// Device configuration
DEFINE_FSTR(CONTROLLER_CLASSNAME, "modbus")

// Json tags
DEFINE_FSTR(ATTR_STATES, "states")

DEFINE_FSTR_LOCAL(ATTR_ADDRESS, "address")
DEFINE_FSTR_LOCAL(ATTR_BAUDRATE, "baudrate")

// Default values
static constexpr unsigned TRANSACTION_TIMEOUT_MS = 300;
static constexpr unsigned DEFAULT_BAUDRATE = 9600;

/* Controller */

/*
 * Inherited classes call this after their own start() code.
 */
void Controller::start()
{
	if(!serial.initState(state)) {
		debug_e("MB: Serial init failed");
		return;
	}

	Serial::Config cfg{
		.mode{Serial::Mode::Shared},
		.config{UART_8N1},
		.baudrate{DEFAULT_BAUDRATE},
	};
	if(!serial.acquire(state, cfg)) {
		debug_e("MB: Serial acquire failed");
		return;
	}

	request = nullptr;
	requestFunction = Function::None;
	IO::Controller::start();
}

/*
 * Inherited classes call this before their own stop() code.
 */
void Controller::stop()
{
	IO::Controller::stop();
}

void Controller::transmitComplete(Serial::State& state)
{
	auto controller = reinterpret_cast<Controller*>(state.controller);
	controller->serial.setTransmit(false);
}

void Controller::receive(Serial::State& state)
{
	// Prevent further callbacks
	state.onReceive = nullptr;

	// Wait a bit before processing the response. This enforces a minimum inter-message delay
	auto controller = reinterpret_cast<Controller*>(state.controller);
	controller->timer
		.initializeMs<50>(
			[](void* param) {
				auto controller = static_cast<Controller*>(param);
				controller->completeTransaction();
			},
			controller)
		.startOnce();
}

/*
 * Submit a modbus command.
 *
 * The ADU must be valid and contain two reserved bytes at the end for the checksum,
 * which this method calculates.
 *
 * We write data into the hardware FIFO: no need for any software buffering.
 *
 */
void Controller::execute(IO::Request& request)
{
	// Fail if transaction already in progress
	if(busy()) {
		debug_e("Modbus unexpectedly busy");
		return;
	}

	auto& req = reinterpret_cast<Request&>(request);

	// Fill out the ADU packet
	ADU adu;
	requestFunction = req.fillRequestData(adu.pdu.data);
	adu.pdu.setFunction(requestFunction);
	adu.slaveAddress = req.device().address();
	auto aduSize = adu.prepareRequest();
	if(aduSize == 0) {
		request.complete(Status::error);
		return;
	}

	debug_i("MB: 1");

	// Prepare UART for comms
	Serial::Config cfg{
		.mode{Serial::Mode::Exclusive},
		.config{UART_8N1},
		.baudrate{req.device().baudrate()},
	};
	debug_i("MB: 2");
	if(cfg.baudrate == 0) {
		debug_w("Using default baudrate for request");
		cfg.baudrate = DEFAULT_BAUDRATE;
	}
	debug_i("MB: 3");
	if(!serial.acquire(state, cfg)) {
		debug_e("Failed to acquire serial port");
		request.complete(Status::error);
		return;
	}
	debug_i("MB: 4");
		serial.flush();
	debug_i("MB: 5");

	// OK, issue the request
	this->request = &req;
	send(adu, aduSize);

	// Put a timeout on the overall transaction
	timer.initializeMs<TRANSACTION_TIMEOUT_MS>(
		[](void* param) {
			auto controller = static_cast<Controller*>(param);
			controller->transactionTimeout();
		},
		this);
	timer.startOnce();
}

Error Controller::readResponse(ADU& adu)
{
	// Read packet
	auto receivedSize = serial.read(adu.buffer, ADU::MaxSize);

	// Parse the received packet
	Error err;
	if(request == nullptr) {
		err = adu.parseRequest(receivedSize);
	} else {
		err = adu.parseResponse(receivedSize);
	}
	if(!!err) {
		return err;
	}

	// If packet is unsolicited (slave mode) then we're done
	if(request == nullptr) {
		return Error::success;
	}

	// In master mode, check for consistency with current request
	if(adu.slaveAddress != request->device().address()) {
		// Mismatch with command slave ID
		return Error::bad_param;
	}

	if(adu.pdu.function() != requestFunction) {
		// Mismatch with command function
		return Error::bad_command;
	}

	return Error::success;
}

void Controller::completeTransaction()
{
	ADU adu;
	auto err = readResponse(adu);

	// Flush any surplus data and release the serial port
	serial.flush(UART_RX_ONLY);
	serial.release(state);

	// Re-enable callbacks
	state.onReceive = receive;

	auto req = request;
	request = nullptr;

	if(!!err) {
		debug_e("MB: %s", toString(err).c_str());
		if(req != nullptr) {
			req->complete(Status::error);
		}
	} else {
		debug_d("MB: received '%s': %s", toString(adu.pdu.function()).c_str(), toString(adu.pdu.exception()).c_str());
		if(req == nullptr) {
			// Request
			handleIncomingRequest(adu);
		} else {
			// Normal response
			req->callback(adu.pdu);
		}
	}
}

void Controller::send(ADU& adu, size_t size)
{
	if(size != 0) {
		serial.setTransmit(true);
		serial.write(adu.buffer, size);
		// NUL pad so we don'final byte doesn't get cut off
		uint8_t nul{0};
		serial.write(&nul, 1);

		debug_i("MB: Sent %u bytes...", size);
	}
}

void Controller::transactionTimeout()
{
	//
	ADU adu;
	auto receivedSize = serial.read(adu.buffer, ADU::MaxSize);
	debug_hex(INFO, "TIMEOUT", adu.buffer, receivedSize);
	//

	serial.release(state);

	auto req = request;
	assert(req != nullptr);
	request = nullptr;

	debug_w("MB: Timeout");
	req->complete(Status::error);
}

void Controller::handleIncomingRequest(ADU& adu)
{
	if(requestCallback) {
		if(requestCallback(adu)) {
			sendResponse(adu);
		}
		return;
	}

	if(adu.slaveAddress == ADU::BROADCAST_ADDRESS) {
		// Forward ADU to all devices
		for(unsigned i = 0; i < m_devices.count(); ++i) {
			auto dev = reinterpret_cast<Device*>(m_devices[i]);
			dev->onBroadcast(adu);
		}
	} else {
		// Forward ADU to specific device
		for(unsigned i = 0; i < m_devices.count(); ++i) {
			auto dev = reinterpret_cast<Device*>(m_devices[i]);
			if(adu.slaveAddress == dev->address()) {
				dev->onRequest(adu);
				break;
			}
		}
	}
}

/* Device */

Error Device::init(const Config& config)
{
	auto err = IO::Device::init(config);
	if(!!err) {
		return err;
	}

	if(config.slave.address == 0) {
		return Error::no_address;
	}

	if(config.slave.baudrate == 0) {
		return Error::no_baudrate;
	}

	m_config = config.slave;
	return Error::success;
}

Error Device::init(JsonObjectConst config)
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

void Request::callback(PDU& pdu)
{
	complete(!m_exception ? Status::success : Status::error);
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);

	if(m_status == Status::error) {
		setError(json, unsigned(m_exception), toString(m_exception));
	}
}

} // namespace Modbus
} // namespace IO
