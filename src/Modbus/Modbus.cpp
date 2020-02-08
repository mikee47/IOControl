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
	case Exception::InvalidSlaveID:
		return F("InvalidSlaveID");
	case Exception::InvalidFunction:
		return F("InvalidFunction");
	case Exception::ResponseTimedOut:
		return F("ResponseTimedOut");
	case Exception::InvalidCRC:
		return F("InvalidCRC");
	default:
		return F("Unknown") + String(unsigned(exception));
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

	memset(&m_trans, 0, sizeof(m_trans));
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
				controller->processResponse();
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
	m_trans = Transaction{};
	req.fillRequestData(m_trans);

	struct Header {
		uint8_t slaveId;
		uint8_t function;
	};
	Header hdr{uint8_t(req.device().address()), uint8_t(m_trans.function)};

#ifdef MODBUS_DEBUG
	m_printf(_F("> %02X %02X"), hdr.slaveId, hdr.function);
	for(uint8_t i = 0; i < m_trans.dataSize; ++i) {
		m_printf(_F(" %02X"), m_trans.data[i]);
	}
#endif

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

	struct Footer {
		// Sent in LSB, MSB order
		uint8_t crc[2];
		// Frame-end - tx interrupt occurs when last byte is removed from buffer so cannot be real data
		uint8_t padding;
	};

	// Write value and update CRC
	uint16_t crc{0xFFFF};
	auto write = [&](const void* data, size_t len) {
		uart_write(uart, data, len);
		crc = crc16_update(crc, data, len);
	};

	write(&hdr, sizeof(hdr));
	write(m_trans.data, m_trans.dataSize);
	Footer footer{lowByte(crc), highByte(crc), 0};
	uart_write(uart, &footer, sizeof(footer));

	// Now prepare for response
	m_trans.exception = Exception::ResponseTimedOut;
	m_trans.dataSize = 0;
	Serial.setUartCallback(uartCallback, this);

#ifdef MODBUS_DEBUG
	m_printf(_F(" %02X %02X\n"), lowByte(crc), highByte(crc));
#endif

	// Put a timeout on the overall transaction
	timer.initializeMs<MODBUS_TRANSACTION_TIMEOUT_MS>(
		[](void* param) {
			Serial.setUartCallback(nullptr);
			auto controller = static_cast<Controller*>(param);
			controller->completeTransaction();
		},
		this);
	timer.startOnce();
}

void Controller::processResponse()
{
	auto uart = Serial.getUart();
	auto avail = uart_rx_available(uart);

	// Smallest packet for initial read
	struct {
		uint8_t slaveId;
		uint8_t function;
		uint8_t tmp[3]; // CRC + 1 data byte
	} buf;
	static_assert(sizeof(buf) == 5, "alignment issue");

	if(avail < sizeof(buf)) {
		/*
		 * todo: Fail, but could retry
		 */
		m_trans.exception = Exception::ResponseTimedOut;
		return;
	}

	// Read data from serial buffer update CRC
	uint16_t crc = 0xFFFF;
	auto read = [&](void* buf, size_t len) {
		uart_read(uart, buf, len);
		crc = crc16_update(crc, buf, len);
	};

	read(&buf, sizeof(buf));

	size_t dataSize = 0;
	switch(Function(buf.function)) {
	/*
	 * 1 (read coils), 2 (read discrete inputs):
	 * 	uint8_t valueCount;
	 * 	uint8_t values[valueCount];
	*/
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs: {
		auto bitCount = makeWord(buf.tmp);
		dataSize = (bitCount + 7) / 8;
		break;
	}

	/*
	 * 4 (read input registers), 3 (read holding registers)
	 *	uint8_t valueCount;
	 *	uint16_t values[valueCount];
	 *
	*/
	case Function::ReadInputRegisters:
	case Function::ReadHoldingRegisters:
	case Function::ReadWriteMultipleRegisters:
		dataSize = 1 + (buf.tmp[0] * 2);
		break;

	case Function::ReadExceptionStatus:
		dataSize = 1;
		break;

	/*
	 * Exceptions (function | 0x80)
	 * 	uint8_t exceptionCode;
	 *
	 */
	case Function::ReportSlaveID:
		dataSize = buf.tmp[0];
		break;

	case Function::MaskWriteRegister:
		dataSize = 6;
		break;

	/*
	 * 5 (force/write single coil)
	 * 6 (preset/write single holding register)
	 * 	uint16_t address;
	 * 	uint16_t value;
	 *
	 * 15 (force/write multiple coils)
	 * 16 (preset/write multiple holding registers)
	 * 	uint16_t address;
	 * 	uint16_t valueCount;
	 *
	 */
	default:
		dataSize = (buf.function & 0x80) ? 1 : 4;
	}

	// Fetch any remaining data and put CRC in tmp[1..2]
	switch(dataSize) {
	case 1:
		m_trans.data[0] = buf.tmp[0];
		break;
	case 2:
		m_trans.data[0] = buf.tmp[0];
		m_trans.data[1] = buf.tmp[1];
		buf.tmp[1] = buf.tmp[2];
		read(&buf.tmp[2], 1);
		break;
	default:
		m_trans.data[0] = buf.tmp[0];
		m_trans.data[1] = buf.tmp[1];
		m_trans.data[2] = buf.tmp[2];
		if(dataSize > 3) {
			if(dataSize > MODBUS_DATA_SIZE) {
#ifdef MODBUS_DEBUG
				debug_e("MODBUS: Packet contains %u bytes - too big for buffer", dataSize);
#endif
				dataSize = MODBUS_DATA_SIZE;
			}
			read(&m_trans.data[3], dataSize - 3);
		}
		read(&buf.tmp[1], 2);
	}

	// deduct header and CRC to obtain data size
	if(avail > 4 + dataSize) {
#ifdef MODBUS_DEBUG
		debug_w("Too much data - %u in FIFO", avail);
#endif
		uart_flush(uart, UART_RX_ONLY);
	}

	m_trans.dataSize = dataSize;

#ifdef MODBUS_DEBUG
	m_printf(_F("< %02X %02X"), buf.slaveId, buf.function);
	for(unsigned i = 0; i < dataSize; ++i) {
		m_printf(" %02X", m_trans.data[i]);
	}
	m_printf(_F(" %02X %02X\n"), buf.tmp[1], buf.tmp[2]);
#endif

	// verify response is for correct Modbus slave
	if(buf.slaveId != request->device().address()) {
		m_trans.exception = Exception::InvalidSlaveID;
	}
	// verify response is for correct Modbus function code (mask exception bit 7)
	else if(Function(buf.function & 0x7F) != m_trans.function) {
		m_trans.exception = Exception::InvalidFunction;
	}
	// check whether Modbus exception occurred; return Modbus Exception Code
	else if(buf.function & 0x80) {
		m_trans.exception = Exception(m_trans.data[0]);
	} else if(crc != 0) {
		m_trans.exception = Exception::InvalidCRC;
	} else {
		m_trans.exception = Exception::Success;
	}
}

/*
 * Called by serial ISR when transaction has completed, or by expiry timer.
 */
void Controller::completeTransaction()
{
	debug_d("Modbus: completeTransaction(): %s", toString(m_trans.exception).c_str());

	auto req = request;
	assert(req != nullptr);
	request = nullptr;
	req->callback(m_trans);
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

void Request::callback(Transaction& mbt)
{
	complete(checkStatus(mbt) ? Status::success : Status::error);
}

bool Request::checkStatus(const Transaction& mbt)
{
	if(mbt.exception == Exception::Success) {
		return true;
	}

	m_exception = mbt.exception;

	String statusStr = toString(mbt.exception);
	debug_e("modbus error %02X (%s)", mbt.exception, statusStr.c_str());

	return false;
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
