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

// Maximum number of bytes in transaction data
#ifndef MODBUS_DATA_SIZE
#define MODBUS_DATA_SIZE 64
#endif

namespace IO
{
namespace Modbus
{
// Device configuration
DECLARE_FSTR(CONTROLLER_CLASSNAME)

// json tags
DECLARE_FSTR(ATTR_STATES)

class Device;
class Controller;

/**
 * @brief Modbus exception codes
 */
enum class Exception {
	Success = 0x00,

	/**
	 * @brief Protocol illegal function exception
	 *
	 * The function code received in the query is not an allowable action for the server (or slave).
	 * This may be because the function code is only applicable to newer devices, and was not implemented
	 * in the unit selected.
	 * It could also indicate that the server (or slave) is in the wrong state to process a request of this
	 * type, for example because it is unconfigured and is being asked to return register values.
	 */
	IllegalFunction = 0x01,

	/**
	 * @brief Protocol illegal data address exception
	 *
	 * The data address received in the query is not an allowable address for the server (or slave).
	 * More specifically, the combination of reference number and transfer length is invalid.
	 * For a controller with 100 registers, the ADU addresses the first register as 0, and the last one as 99.
	 *
	 * If a request is submitted with a starting register address of 96 and a quantity of registers of 4, then
	 * this request will successfully operate (address-wise at least) on registers 96, 97, 98, 99.
	 *
	 * If a request is submitted with a starting register address of 96 and a quantity of registers of 5,
	 * then this request will fail with Exception Code 0x02 "Illegal Data Address" since it attempts to operate
	 * on registers 96, 97, 98, 99 and 100, and there is no register with address 100.
	 */
	IllegalDataAddress = 0x02,

	/**
	 * @brief Protocol illegal data value exception
	 *
	 * A value contained in the query data field is not an allowable value for server (or slave).
	 *
	 * This indicates a fault in the structure of the remainder of a complex request, such as that
	 * the implied length is incorrect. It specifically does NOT mean that a data item submitted for
	 * storage in a register has a value outside the expectation of the application program, since the
	 * MODBUS protocol is unaware of the significance of any particular value of any particular register.
	 */
	IllegalDataValue = 0x03,

	/**
	 * @brief Protocol slave device failure exception
	 *
	 * An unrecoverable error occurred while the server (or slave) was attempting to perform the requested action.
	 */
	SlaveDeviceFailure = 0x04,

	//	InvalidSlaveID = 0xE0,
	//	InvalidFunction = 0xE1,
	//	ResponseTimedOut = 0xE2,
	//	InvalidCRC = 0xE3,
};

inline bool operator!(Exception exception)
{
	return exception == Exception::Success;
}

String toString(Exception exception);

// Modbus function codes
enum class Function {
	None = 0x00,
	ReadCoils = 0x01,
	ReadDiscreteInputs = 0x02,
	ReadHoldingRegisters = 0x03,
	ReadInputRegisters = 0x04,
	WriteSingleCoil = 0x05,
	WriteSingleRegister = 0x06,
	ReadExceptionStatus = 0x07,
	GetComEventCounter = 0x0b,
	GetComEventLog = 0x0c,
	WriteMultipleCoils = 0x0f,
	WriteMultipleRegisters = 0x10,
	ReportSlaveID = 0x11, // Also known as 'report server ID'
	MaskWriteRegister = 0x16,
	ReadWriteMultipleRegisters = 0x17,
};

// 16-bit words are stored big-endian; use this macro to pull them from response buffer
static inline uint16_t makeWord(const uint8_t* buffer)
{
	return (buffer[0] << 8) | buffer[1];
}

#define ATTR_PACKED __attribute__((aligned(1), packed))

/**
 * @brief Protocol Data Unit
 *
 * Content is independent of the communication layer.
 * Structure does NOT represent over-the-wire format as packing issues make handling PDU::Data awkward.
 * Therefore, the other fields are unaligned and adjusted as required when sent over the wire.
 */
struct PDU {
	/*
	 * Data is packed as some fields are misaligned.
	 *
	 * MODBUS sends 16-bit values MSB first, so appropriate byte-swapping is handled by the driver.
	 * Other than that, the data is un-modified.
	 */
	union Data {
		// For exception response
		uint8_t exceptionCode;

		// ReadCoils = 0x01
		union ReadCoils {
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfCoils;
			};
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint8_t coilStatus[250];
			};
			static constexpr uint16_t MaxCoils{sizeof(Response::coilStatus) * 8};

			Request request;
			Response response;
		};
		ReadCoils readCoils;

		// ReadDiscreteInputs = 0x02
		union ReadDiscreteInputs {
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfInputs;
			};
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint8_t inputStatus[250];
			};
			static constexpr uint16_t MaxInputs{sizeof(Response::inputStatus) * 8};

			Request request;
			Response response;
		};
		ReadDiscreteInputs readDiscreteInputs;

		// 16-bit access
		// ReadHoldingRegisters = 0x03,
		union ReadHoldingRegisters {
			static constexpr uint16_t MaxRegisters{250 / 2};
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfRegisters;
			};
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint16_t values[MaxRegisters];
			};

			Request request;
			Response response;
		};
		ReadHoldingRegisters readHoldingRegisters;

		// ReadInputRegisters = 0x04,
		union ReadInputRegisters {
			static constexpr uint16_t MaxRegisters{250 / 2};
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfRegisters;
			};
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint16_t values[MaxRegisters];
			};

			Request request;
			Response response;
		};
		ReadInputRegisters readInputRegisters;

		// WriteSingleCoil = 0x05,
		union WriteSingleCoil {
			struct ATTR_PACKED Request {
				uint16_t outputAddress;
				uint16_t outputValue;
			};
			using Response = Request;

			Request request;
			Response response;
		};
		WriteSingleCoil writeSingleCoil;

		// WriteSingleRegister = 0x06,
		union WriteSingleRegister {
			struct ATTR_PACKED Request {
				uint16_t address;
				uint16_t value;
			};
			using Response = Request;

			Request request;
			Response response;
		};
		WriteSingleRegister writeSingleRegister;

		// ReadExceptionStatus = 0x07,
		union ReadExceptionStatus {
			struct ATTR_PACKED Response {
				uint8_t outputData;
			};

			Response response;
		};
		ReadExceptionStatus readExceptionStatus;

		// GetComEventCounter = 0x0b,
		union GetComEventCounter {
			struct ATTR_PACKED Response {
				uint16_t status;
				uint16_t eventCount;
			};

			Response response;
		};
		GetComEventCounter getComEventCounter;

		// GetComEventLog = 0x0c,
		union GetComEventLog {
			static constexpr uint8_t MaxEvents{64};
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint16_t status;
				uint16_t eventCount;
				uint16_t messageCount;
				uint8_t events[MaxEvents];
			};

			Response response;
		};
		GetComEventLog getComEventLog;

		// WriteMultipleCoils = 0x0f,
		union WriteMultipleCoils {
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfOutputs;
				uint8_t byteCount;
				uint8_t values[246];
			};
			static constexpr uint16_t MaxCoils{sizeof(Request::values) * 8};
			struct ATTR_PACKED Response {
				uint16_t startAddress;
				uint16_t quantityOfOutputs;
			};

			Request request;
			Response response;
		};
		WriteMultipleCoils writeMultipleCoils;

		// WriteMultipleRegisters = 0x10,
		union WriteMultipleRegisters {
			static constexpr uint16_t MaxRegisters{123};
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfRegisters;
				uint8_t byteCount;
				uint16_t values[MaxRegisters];
			};
			struct ATTR_PACKED Response {
				uint16_t startAddress;
				uint16_t quantityOfRegisters;
			};

			Request request;
			Response response;
		};
		WriteMultipleRegisters writeMultipleRegisters;

		// ReportSlaveID = 0x11,
		union ReportSlaveID {
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint8_t data[250]; ///< Content is specific to slave device
			};

			Response response;
		};
		ReportSlaveID reportSlaveID;

		// MaskWriteRegister = 0x16,
		union MaskWriteRegister {
			struct ATTR_PACKED Request {
				uint16_t address;
				uint16_t andMask;
				uint16_t orMask;
			};
			using Response = Request;

			Request request;
			Response response;
		};
		MaskWriteRegister maskWriteRegister;

		// ReadWriteMultipleRegisters = 0x17,
		union ReadWriteMultipleRegisters {
			static constexpr uint16_t MaxReadRegisters{125};
			static constexpr uint16_t MaxWriteRegisters{121};
			struct ATTR_PACKED Request {
				uint16_t readAddress;
				uint16_t quantityToRead;
				uint16_t writeAddress;
				uint16_t quantityToWrite;
				uint8_t writeByteCount;
				uint16_t writeValues[MaxWriteRegisters];
			};
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint16_t values[MaxReadRegisters];
			};

			Request request;
			Response response;
		};
		ReadWriteMultipleRegisters readWriteMultipleRegisters;
	};

	uint8_t functionCode;
	Data data;

	Function function() const
	{
		return Function(functionCode & 0x7f);
	}

	bool exceptionFlag() const
	{
		return functionCode & 0x80;
	}

	Exception exception() const
	{
		return exceptionFlag() ? Exception(data.exceptionCode) : Exception::Success;
	}

	size_t getRequestDataSize() const;
	size_t getResponseDataSize() const;
	void swapRequestByteOrder(bool incoming);
	void swapResponseByteOrder(bool incoming);
};

static_assert(offsetof(PDU, data) == 1, "PDU alignment error");

// Buffer to construct requests and process responses
union ADU {
	struct {
		uint8_t slaveId;
		PDU pdu;
	};
	uint8_t buffer[256];
};

static_assert(offsetof(ADU, pdu) == 1, "ADU alignment error");

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

	// Transaction result
	Error error;

protected:
	virtual Function fillRequestData(PDU::Data& data) = 0;
	virtual void callback(PDU& pdu) = 0;

private:
	Exception m_exception = Exception::Success;
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
		unsigned timeout; ///< Max time between command/response
	};

	struct Config : public IO::Device::Config {
		SlaveConfig slave;
	};

	Device(Controller& controller) : IO::Device(reinterpret_cast<IO::Controller&>(controller))
	{
	}

	Error init(const Config& config);
	Error init(JsonObjectConst config) override;

	uint16_t address() const override
	{
		return m_config.address;
	}

	unsigned baudrate() const
	{
		return m_config.baudrate;
	}

protected:
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
		return request != nullptr;
	}

private:
	void execute(IO::Request& request) override;
	static void IRAM_ATTR uartCallback(uart_t* uart, uint32_t status);
	Error readResponse(ADU& adu);
	void completeTransaction();
	void transactionTimeout();

private:
	Request* request{nullptr};
	SimpleTimer timer; ///< Use to schedule callback and timeout
	Function requestFunction{};
	uint8_t txEnablePin{0};
};

} // namespace Modbus
} // namespace IO
