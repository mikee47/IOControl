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
enum __attribute__((packed)) ModbusExceptionCode {
	/**
	 * @brief Protocol illegal function exception
	 *
	 * The function code received in the query is not an allowable action for the server (or slave).
	 * This may be because the function code is only applicable to newer devices, and was not implemented
	 * in the unit selected.
	 * It could also indicate that the server (or slave) is in the wrong state to process a request of this
	 * type, for example because it is unconfigured and is being asked to return register values.
	 */
	MBE_IllegalFunction = 0x01,

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
	MBE_IllegalDataAddress = 0x02,

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
	MBE_IllegalDataValue = 0x03,

	/**
	 * @brief Protocol slave device failure exception
	 *
	 * An unrecoverable error occurred while the server (or slave) was attempting to perform the requested action.
	 */
	MBE_SlaveDeviceFailure = 0x04,

	/* Class-defined success/exception codes */

	/**
	 * @brief Success code
	 *
	 * Modbus transaction was successful, the following checks were valid:
	 *
	 * 	- slave ID
	 * 	- function code
	 * 	- response code
	 * 	- data
	 * 	- CRC
	 */
	MBE_Success = 0x00,

	/**
	 * @brief Invalid response slave ID
	 *
	 * The slave ID in the response does not match that of the request.
	 */
	MBE_InvalidSlaveID = 0xE0,

	/**
	 * @brief Invalid response function
	 *
	 * The function code in the response does not match that of the request.
	 */
	MBE_InvalidFunction = 0xE1,

	/**
	 * @brief Response timed-out
	 *
	 * The entire response was not received within the timeout period
	 */
	MBE_ResponseTimedOut = 0xE2,

	/**
	 * @brief Invalid response CRC
	 *
	 * The CRC in the response does not match the one calculated.
	 */
	MBE_InvalidCRC = 0xE3,
};

String modbusExceptionString(ModbusExceptionCode status);

// Modbus function codes
enum __attribute__((packed)) Function {
	MB_None = 0x00,

	// Bit-wise access
	MB_ReadCoils = 0x01,
	MB_ReadDiscreteInputs = 0x02,
	MB_WriteSingleCoil = 0x05,
	MB_WriteMultipleCoils = 0x0f,

	// 16-bit access
	MB_ReadHoldingRegisters = 0x03,
	MB_ReadInputRegisters = 0x04,
	MB_WriteSingleRegister = 0x06,
	MB_WriteMultipleRegisters = 0x10,
	MB_MaskWriteRegister = 0x16,
	MB_ReadWriteMultipleRegisters = 0x17,

	// File record access
	MB_ReadFileRecord = 0x14,
	MB_WriteFileRecord = 0x15,

	// Diagnostics
	MB_ReadExceptionStatus = 0x07,
	MB_Diagnostic = 0x08,
	MB_GetComEventCounter = 0x0b,
	MB_GetComEventLog = 0x0c,
	MB_ReportSlaveID = 0x11,
	MB_ReadDeviceIdentification = 0x2b,
};

// 16-bit words are stored big-endian; use this macro to pull them from response buffer
static inline uint16_t makeWord(const uint8_t* buffer)
{
	return (buffer[0] << 8) | buffer[1];
}

/** @brief Public details of a transaction
 *
 *
 */
struct Transaction {
	Function function;
	ModbusExceptionCode status;
	uint8_t data[MODBUS_DATA_SIZE]; ///< command/response data
	uint8_t dataSize;
};

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
	virtual void fillRequestData(Transaction& mbt) = 0;
	virtual void callback(Transaction& mbt) = 0;

	// Return true if handled
	bool checkStatus(const Transaction& mbt);

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
		unsigned timeout; ///< Max time between command/response
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
		return request != nullptr;
	}

private:
	void execute(IO::Request& request);

private:
	static void IRAM_ATTR uartCallback(uart_t* uart, uint32_t status);
	void IRAM_ATTR receiveComplete();
	ModbusExceptionCode processResponse();
	void completeTransaction(ModbusExceptionCode status);

private:
	Request* request{nullptr};
	Transaction m_trans{};
	uint8_t txEnablePin{0};
	SimpleTimer timer; ///< Use to schedule callback and timeout
};

} // namespace Modbus
} // namespace IO
