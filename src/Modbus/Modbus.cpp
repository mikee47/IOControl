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
#include "HardwareSerial.h"
#include "Platform/System.h"
#include <driver/uart.h>
#include <espinc/uart_register.h>

#if DEBUG_VERBOSE_LEVEL == DBG
#define MODBUS_DEBUG
#endif

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

namespace
{
uint16_t crc16_update(uint16_t crc, uint8_t a)
{
	crc ^= a;

	for(unsigned i = 0; i < 8; ++i) {
		if(crc & 1)
			crc = (crc >> 1) ^ 0xA001;
		else
			crc = (crc >> 1);
	}

	return crc;
}

uint16_t crc16_update(uint16_t crc, const void* data, size_t count)
{
	auto p = static_cast<const uint8_t*>(data);
	while(count--) {
		crc = crc16_update(crc, *p++);
	}

	return crc;
}

// Perform unaligned byteswap on array of uint16_t
auto bswap = [](void* values, unsigned count) {
	auto p = static_cast<uint8_t*>(values);
	while(count--) {
		std::swap(p[0], p[1]);
		p += 2;
	}
};

} // namespace

String toString(Exception exception)
{
	switch(exception) {
	case Exception::Success:
		return F("Success");
	case Exception::IllegalFunction:
		return F("IllegalFunction");
	case Exception::IllegalDataAddress:
		return F("IllegalDataAddress");
	case Exception::IllegalDataValue:
		return F("IllegalDataValue");
	case Exception::SlaveDeviceFailure:
		return F("SlaveDeviceFailure");
	default:
		return F("Unknown") + String(unsigned(exception));
	}
}

/**
 * @brief Get size (in bytes) of PDU Data for request packet
 */
size_t PDU::getRequestDataSize() const
{
	switch(function()) {
	case Function::ReadCoils:
		return 2 + (data.readCoils.request.quantityOfCoils + 7) / 8;
	case Function::ReadDiscreteInputs:
		return 2 + (data.readDiscreteInputs.request.quantityOfInputs + 7) / 8;
	case Function::ReadHoldingRegisters:
		return sizeof(data.readHoldingRegisters.request);
	case Function::ReadInputRegisters:
		return sizeof(data.readInputRegisters.request);
	case Function::WriteSingleCoil:
		return sizeof(data.writeSingleCoil.request);
	case Function::WriteSingleRegister:
		return sizeof(data.writeSingleRegister.request);
	case Function::ReadExceptionStatus:
		return 0;
	case Function::GetComEventCounter:
		return 0;
	case Function::GetComEventLog:
		return 0;
	case Function::WriteMultipleCoils:
		return 5 + data.writeMultipleCoils.request.byteCount;
	case Function::WriteMultipleRegisters:
		return 5 + data.writeMultipleRegisters.request.byteCount;
	case Function::ReportSlaveID:
		return 0;
	case Function::MaskWriteRegister:
		return sizeof(data.maskWriteRegister.request);
	case Function::ReadWriteMultipleRegisters:
		return 9 + data.readWriteMultipleRegisters.request.writeByteCount;
	default:
		return 0;
	}
}

/**
 * @brief Get size (in bytes) of PDU Data in received response packet
 */
size_t PDU::getResponseDataSize() const
{
	if(exceptionFlag()) {
		return 1;
	}

	switch(function()) {
	case Function::ReadCoils:
		return 1 + data.readCoils.response.byteCount;
	case Function::ReadDiscreteInputs:
		return 1 + data.readDiscreteInputs.response.byteCount;
	case Function::ReadHoldingRegisters:
		return 1 + data.readHoldingRegisters.response.byteCount;
	case Function::ReadInputRegisters:
		return 1 + data.readInputRegisters.response.byteCount;
	case Function::WriteSingleCoil:
		return sizeof(data.writeSingleCoil.response);
	case Function::WriteSingleRegister:
		return sizeof(data.writeSingleRegister.response);
	case Function::ReadExceptionStatus:
		return sizeof(data.readExceptionStatus.response);
	case Function::GetComEventCounter:
		return sizeof(data.getComEventCounter.response);
	case Function::GetComEventLog:
		return sizeof(data.getComEventLog.response.byteCount);
	case Function::WriteMultipleCoils:
		return sizeof(data.writeMultipleCoils.response);
	case Function::WriteMultipleRegisters:
		return sizeof(data.writeMultipleRegisters.response);
	case Function::ReportSlaveID:
		return 1 + data.reportSlaveID.response.byteCount;
	case Function::MaskWriteRegister:
		return sizeof(data.maskWriteRegister.response);
	case Function::ReadWriteMultipleRegisters:
		return 1 + data.readWriteMultipleRegisters.response.byteCount;
	default:
		return 0;
	}
}

void PDU::swapRequestByteOrder(bool incoming)
{
	if(exceptionFlag()) {
		return;
	}

	switch(function()) {
	case Function::None:
	case Function::GetComEventLog:
	case Function::ReadExceptionStatus:
	case Function::ReportSlaveID:
		break;

	case Function::ReadCoils:
	case Function::ReadDiscreteInputs:
	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters:
	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
	case Function::GetComEventCounter:
	case Function::WriteMultipleCoils:
		bswap(&data.readCoils.request.startAddress, 2);
		break;

	case Function::WriteMultipleRegisters: {
		auto& req{data.writeMultipleRegisters.request};
		auto count = req.quantityOfRegisters;
		bswap(&req.startAddress, 2);
		bswap(req.values, incoming ? req.quantityOfRegisters : count);
		break;
	}

	case Function::MaskWriteRegister: {
		bswap(&data.maskWriteRegister.request.address, 3);
		break;
	}

	case Function::ReadWriteMultipleRegisters: {
		auto& req = data.readWriteMultipleRegisters.request;
		auto count = req.writeByteCount;
		bswap(&req.readAddress, 3);
		bswap(&req.writeValues, (incoming ? req.writeByteCount : count) / 2);
		break;
	}
	}
}

void PDU::swapResponseByteOrder(bool incoming)
{
	if(exceptionFlag()) {
		return;
	}

	switch(function()) {
	case Function::None:
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs:
	case Function::ReadExceptionStatus:
	case Function::ReportSlaveID:
		break;

	case Function::ReadHoldingRegisters: {
	case Function::ReadInputRegisters:
	case Function::ReadWriteMultipleRegisters:
		auto& req = data.readHoldingRegisters.response;
		bswap(req.values, req.byteCount / 2);
		break;
	}

	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
	case Function::GetComEventCounter:
	case Function::WriteMultipleCoils:
	case Function::WriteMultipleRegisters:
		bswap(&data.writeSingleCoil.response.outputAddress, 2);
		break;

	case Function::GetComEventLog:
		bswap(&data.getComEventLog.response.status, 3);
		break;

	case Function::MaskWriteRegister:
		bswap(&data.maskWriteRegister.response.address, 3);
		break;
	}
}

/* Controller */

/*
 * Inherited classes call this after their own start() code.
 */
void Controller::start()
{
	this->txEnablePin = MBPIN_TX_EN;

	Serial.end();
	Serial.setUartCallback(nullptr);
	Serial.setTxBufferSize(0);
	Serial.setRxBufferSize(0);
	Serial.begin(MODBUS_DEFAULT_BAUDRATE);

	// Using alternate serial pins
	Serial.swap();

	// Put default serial pins in safe state
	pinMode(1, INPUT_PULLUP);
	pinMode(3, INPUT_PULLUP);

	digitalWrite(txEnablePin, 0);
	pinMode(txEnablePin, OUTPUT);

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
		Serial.setUartCallback(nullptr);
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

	auto uart = Serial.getUart();

	auto& req = reinterpret_cast<Request&>(request);
	this->request = &req;

	ADU adu;
	adu.slaveId = req.device().address();
	requestFunction = req.fillRequestData(adu.pdu.data);
	adu.pdu.functionCode = uint8_t(requestFunction);
	auto aduSize = 2 + adu.pdu.getRequestDataSize(); // SlaveID + function + data
	adu.pdu.swapRequestByteOrder(false);
	auto crc = crc16_update(0xFFFF, &adu, aduSize);
	adu.buffer[aduSize++] = uint8_t(crc);
	adu.buffer[aduSize++] = uint8_t(crc >> 8);

	debug_hex(DBG, ">", adu.buffer, aduSize);

	// Prepare UART for comms
	auto baudrate = req.device().baudrate();
	if(baudrate == 0) {
		debug_w("Using default baudrate for request");
		baudrate = MODBUS_DEFAULT_BAUDRATE;
	}
	Serial.setBaudRate(baudrate);
	Serial.clear();

	// Transmit request data whilst keeping TX_EN assserted
	digitalWrite(txEnablePin, 1);
	uart_write(uart, adu.buffer, aduSize + 1);

	// Prepare for response
	Serial.setUartCallback(uartCallback, this);

	// Put a timeout on the overall transaction
	timer.initializeMs<MODBUS_TRANSACTION_TIMEOUT_MS>(
		[](void* param) {
			Serial.setUartCallback(nullptr);
			auto controller = static_cast<Controller*>(param);
			controller->transactionTimeout();
		},
		this);
	timer.startOnce();
}

Error Controller::readResponse(ADU& adu)
{
	auto uart = Serial.getUart();
	auto avail = uart_rx_available(uart);

	if(avail < 5) {
		/*
		 * todo: Fail, but could retry
		 */
		return Error::timeout;
	}

	// Read packet
	auto received = uart_read(uart, adu.buffer, sizeof(adu.buffer));

	// Compute the expected response size
	auto dataSize = adu.pdu.getResponseDataSize();
	auto aduSize = 2 + dataSize + 2; // slaveId + function + data + CRC

	if(received < aduSize) {
		debug_w("MB: Only %u bytes read, %u expected", received, aduSize);
		return Error::timeout;
	}

	if(avail > aduSize) {
		debug_w("MB: Too much data, %u bytes available but only %u required", avail, aduSize);
		if(avail > received) {
			uart_flush(uart, UART_RX_ONLY);
		}
	}

	debug_hex(DBG, "<", adu.buffer, aduSize);

	auto crc = crc16_update(0xFFFF, adu.buffer, aduSize);
	if(crc != 0) {
		return Error::bad_checksum;
	}

	if(adu.slaveId != request->device().address()) {
		// Mismatch with command slave ID
		return Error::bad_param;
	}

	if(adu.pdu.function() != requestFunction) {
		// Mismatch with command function
		return Error::bad_command;
	}

	adu.pdu.swapResponseByteOrder(true);
	return Error::success;
}

void Controller::completeTransaction()
{
	auto req = request;
	assert(req != nullptr);
	request = nullptr;

	ADU adu;
	auto err = readResponse(adu);

	if(!!err) {
		debug_e("Modbus: %s", toString(err).c_str());
		req->complete(Status::error);
	} else {
		debug_d("Modbus: completeTransaction(): %s", toString(adu.pdu.exception()).c_str());
		req->callback(adu.pdu);
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
