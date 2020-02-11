#pragma once

#include <WString.h>

namespace IO
{
namespace Modbus
{
enum class Direction {
	Incoming,
	Outgoing,
};

/**
 * @brief Modbus exception codes returned in response packets
 */
enum class Exception {
	/**
	 * @brief No exception, transaction completed normally
	 */
	Success = 0x00,

	/**
	 * @brief Function not allowed/supported/implemented, or device in wrong state to process request
	 *
	 * For example, an unconfigured device may be unable to return register values.
	 */
	IllegalFunction = 0x01,

	/**
	 * @brief Data address not allowed
	 *
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
	 * @brief Data value not allowed
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
};

inline bool operator!(Exception exception)
{
	return exception == Exception::Success;
}

String toString(Exception exception);

// Modbus function codes
#define MODBUS_FUNCTION_MAP(XX)                                                                                        \
	XX(None, 0x00)                                                                                                     \
	XX(ReadCoils, 0x01)                                                                                                \
	XX(ReadDiscreteInputs, 0x02)                                                                                       \
	XX(ReadHoldingRegisters, 0x03)                                                                                     \
	XX(ReadInputRegisters, 0x04)                                                                                       \
	XX(WriteSingleCoil, 0x05)                                                                                          \
	XX(WriteSingleRegister, 0x06)                                                                                      \
	XX(ReadExceptionStatus, 0x07)                                                                                      \
	XX(GetComEventCounter, 0x0b)                                                                                       \
	XX(GetComEventLog, 0x0c)                                                                                           \
	XX(WriteMultipleCoils, 0x0f)                                                                                       \
	XX(WriteMultipleRegisters, 0x10)                                                                                   \
	XX(ReportSlaveID, 0x11)                                                                                            \
	XX(MaskWriteRegister, 0x16)                                                                                        \
	XX(ReadWriteMultipleRegisters, 0x17)

// Modbus function codes
enum class Function {
#define XX(tag, value) tag = value,
	MODBUS_FUNCTION_MAP(XX)
#undef XX
};

String toString(Function function);

#define ATTR_PACKED __attribute__((aligned(1), packed))

/**
 * @brief Protocol Data Unit
 *
 * Content is independent of the communication layer.
 * Structure does NOT represent over-the-wire format as packing issues make handling PDU::Data awkward.
 * Therefore, the other fields are unaligned and adjusted as required when sent over the wire.
 *
 * `byteCount` field
 *
 * Some functions with a `byteCount` field are marked as `calculated`. This means it doesn't need
 * to be set when constructing requests or responses, and will be filled in later based on the
 * values of other fields.
 *
 * In other cases, a `setCount` method is provided to help ensure the byteCount field is set correctly.
 *
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
				uint8_t byteCount;		 ///< Use setCount()
				uint8_t coilStatus[250]; ///< Use setCoil()

				void setCoil(uint16_t coil, bool value)
				{
					if(coil < MaxCoils) {
						setBit(coilStatus, coil, value);
					}
				}

				void setCount(uint16_t count)
				{
					byteCount = (count + 7) / 8;
				}

				// This figure may be larger than the actual coil count
				uint16_t getCount() const
				{
					return byteCount * 8;
				}
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
				uint8_t byteCount; ///< Calculated
				uint8_t inputStatus[250];

				void setInput(uint16_t input, bool value)
				{
					if(input < MaxInputs) {
						setBit(inputStatus, input, value);
					}
				}

				void setCount(uint16_t count)
				{
					byteCount = (count + 7) / 8;
				}

				// This figure may be larger than the actual input count
				uint16_t getCount() const
				{
					return byteCount * 8;
				}
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
				uint8_t byteCount; ///< Calculated
				uint16_t values[MaxRegisters];

				void setCount(uint16_t count)
				{
					byteCount = count * 2;
				}

				uint16_t getCount() const
				{
					return byteCount / 2;
				}
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
				uint8_t byteCount; ///< Calculated
				uint16_t values[MaxRegisters];

				void setCount(uint16_t count)
				{
					byteCount = std::min(count, MaxRegisters) * 2;
				}

				uint16_t getCount() const
				{
					return byteCount / 2;
				}
			};

			Request request;
			Response response;
		};
		ReadInputRegisters readInputRegisters;

		// WriteSingleCoil = 0x05,
		union WriteSingleCoil {
			// These are the only two specified values: anything else is illegal
			static constexpr uint16_t state_on = 0xFF00;
			static constexpr uint16_t state_off = 0x0000;

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
			static constexpr uint16_t MaxEvents{64};
			struct ATTR_PACKED Response {
				uint8_t byteCount; ///< Calculated
				uint16_t status;
				uint16_t eventCount;
				uint16_t messageCount;
				uint8_t events[MaxEvents];

				void setEventCount(uint16_t count)
				{
					eventCount = std::min(count, MaxEvents);
					byteCount = 7 + eventCount;
				}
			};

			Response response;
		};
		GetComEventLog getComEventLog;

		// WriteMultipleCoils = 0x0f,
		union WriteMultipleCoils {
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfOutputs;
				uint8_t byteCount; ///< Calculated
				uint8_t values[246];

				void setCoil(uint16_t coil, bool state)
				{
					if(coil < MaxCoils) {
						setBit(values, coil, state);
					}
				}

				void setCount(uint16_t count)
				{
					quantityOfOutputs = std::min(count, MaxCoils);
					byteCount = (quantityOfOutputs + 7) / 8;
				}
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
				uint8_t byteCount; ///< Calculated
				uint16_t values[MaxRegisters];

				void setCount(uint16_t count)
				{
					quantityOfRegisters = std::min(count, MaxRegisters);
					byteCount = quantityOfRegisters * 2;
				}
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
				uint8_t writeByteCount; ///< Calculated
				uint16_t writeValues[MaxWriteRegisters];

				void setWriteCount(uint16_t count)
				{
					quantityToWrite = std::min(count, MaxWriteRegisters);
					writeByteCount = quantityToWrite * 2;
				}
			};
			struct ATTR_PACKED Response {
				uint8_t byteCount;
				uint16_t values[MaxReadRegisters];

				void setCount(uint16_t count)
				{
					byteCount = count * 2;
				}

				uint16_t getCount() const
				{
					return byteCount / 2;
				}
			};

			Request request;
			Response response;
		};
		ReadWriteMultipleRegisters readWriteMultipleRegisters;
	};

	Function function() const
	{
		return Function(functionCode & 0x7f);
	}

	void setFunction(Function function)
	{
		functionCode = uint8_t(function);
	}

	void setException(Exception exception)
	{
		functionCode |= 0x80;
		data.exceptionCode = uint8_t(exception);
	}

	bool exceptionFlag() const
	{
		return functionCode & 0x80;
	}

	Exception exception() const
	{
		return exceptionFlag() ? Exception(data.exceptionCode) : Exception::Success;
	}

	/**
	 * @name Swap byte order of any 16-bit fields
	 * @{
	 */
	void swapRequestByteOrder();
	void swapResponseByteOrder();
	/** @} */

	/**
	 * @name Get PDU size based on content
	 * Calculation uses byte count so doesn't access any 16-bit fields
	 * @{
	 */
	size_t getRequestSize() const
	{
		return 1 + getRequestDataSize(); // function + data
	}

	size_t getResponseSize() const
	{
		return 1 + getResponseDataSize(); // function + data
	}
	/** @} */

	/* Structure content */

	uint8_t functionCode;
	Data data;

private:
	size_t getRequestDataSize() const;
	size_t getResponseDataSize() const;

	static void setBit(uint8_t* values, uint16_t number, bool state)
	{
		uint8_t mask = 1 << (number % 8);
		if(state) {
			values[number / 8] |= mask;
		} else {
			values[number / 8] &= ~mask;
		}
	}
};

static_assert(offsetof(PDU, data) == 1, "PDU alignment error");

} // namespace Modbus
} // namespace IO
