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
#include <espinc/uart_register.h>

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

// Pin to switch  MAX485 between receive (low) and transmit (high)
static constexpr uint8_t MBPIN_TX_EN = 12;

// Default values
static constexpr unsigned MODBUS_TRANSACTION_TIMEOUT_MS = 300;
static constexpr unsigned MODBUS_DEFAULT_BAUDRATE = 9600;

/* Controller */

/*
 * Inherited classes call this after their own start() code.
 */
void Controller::start()
{
	this->txEnablePin = MBPIN_TX_EN;

	uart_config cfg{
		.uart_nr = UART0,
		.tx_pin = 1,
		.mode = UART_FULL,
		.options = 0,
		.baudrate = MODBUS_DEFAULT_BAUDRATE,
		.config = UART_8N1,
		.rx_size = ADU::MaxSize,
		.tx_size = ADU::MaxSize,
	};
	uart = uart_init_ex(cfg);
	if(uart == nullptr) {
		debug_e("UART init failed");
		return;
	}

	uart->rx_headroom = 0;
	uart_set_callback(uart, uartCallback, this);

	// Using alternate serial pins
	//	uart_swap(uart, 1);

	// Put default serial pins in safe state
	//	pinMode(1, INPUT_PULLUP);
	//	pinMode(3, INPUT_PULLUP);

	digitalWrite(txEnablePin, 0);
	pinMode(txEnablePin, OUTPUT);

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

void Controller::uartCallback(uart_t* uart, uint32_t status)
{
	auto controller = static_cast<Controller*>(uart_get_callback_param(uart));
	// Guard against spurious interrupts
	if(controller == nullptr) {
		return;
	}

	// Tx FIFO empty
	if(status & UART_TXFIFO_EMPTY_INT_ST) {
		// Set MAX485 back into read mode
		digitalWrite(controller->txEnablePin, 0);
	}

	// Rx FIFO full or timeout
	if(status & (UART_RXFIFO_FULL_INT_ST | UART_RXFIFO_TOUT_INT_ST)) {
		uart->callback = nullptr;
		controller->timer.stop();

		// Process the received data in task context
		System.queueCallback(
			[](void* param) {
				auto controller = static_cast<Controller*>(param);
				controller->completeTransaction();
			},
			controller);
	}
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

	if(uart == nullptr) {
		debug_e("Not initialised");
		request.complete(Status::error);
		return;
	}

	auto& req = reinterpret_cast<Request&>(request);
	this->request = &req;

	ADU adu;
	requestFunction = req.fillRequestData(adu.pdu.data);
	adu.pdu.setFunction(requestFunction);
	adu.slaveId = req.device().address();
	auto aduSize = adu.prepareRequest();
	if(aduSize == 0) {
		request.complete(Status::error);
		return;
	}

	// Prepare UART for comms
	auto baudrate = req.device().baudrate();
	if(baudrate == 0) {
		debug_w("Using default baudrate for request");
		baudrate = MODBUS_DEFAULT_BAUDRATE;
	}
	uart_set_baudrate(uart, baudrate);
	uart_flush(uart, UART_FULL);

	// Transmit request data whilst keeping TX_EN assserted
	digitalWrite(txEnablePin, 1);
	uart_write(uart, adu.buffer, aduSize + 1);

	// Put a timeout on the overall transaction
	timer.initializeMs<MODBUS_TRANSACTION_TIMEOUT_MS>(
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
	auto receivedSize = uart_read(uart, adu.buffer, ADU::MaxSize);

	// Flush any surplus data
	uart_flush(uart, UART_RX_ONLY);

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
	if(adu.slaveId != request->device().address()) {
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
	// Re-enable callbacks
	uart->callback = uartCallback;

	ADU adu;
	auto err = readResponse(adu);

	auto req = request;
	request = nullptr;

	if(!!err) {
		debug_e("Modbus: %s", toString(err).c_str());
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

void Controller::sendResponse(ADU& adu)
{
	auto aduSize = adu.prepareResponse();
	if(aduSize != 0) {
		digitalWrite(txEnablePin, 1);
		uart_write(uart, adu.buffer, aduSize + 1);
	}
}

void Controller::transactionTimeout()
{
	auto req = request;
	assert(req != nullptr);
	request = nullptr;

	debug_w("MB: Timeout");
	req->complete(Status::error);
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
