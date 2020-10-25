#pragma once

#include "Exception.h"
#include "Function.h"

namespace IO
{
namespace Modbus
{
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
				static constexpr uint16_t MaxCoils{sizeof(coilStatus) * 8};

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
				static constexpr uint16_t MaxInputs{sizeof(inputStatus) * 8};

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

			Request request;
			Response response;
		};
		ReadDiscreteInputs readDiscreteInputs;

		// 16-bit access
		// ReadHoldingRegisters = 0x03,
		union ReadHoldingRegisters {
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfRegisters;
			};

			struct ATTR_PACKED Response {
				static constexpr uint16_t MaxRegisters{250 / 2};
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
			struct ATTR_PACKED Request {
				uint16_t startAddress;
				uint16_t quantityOfRegisters;
			};

			struct ATTR_PACKED Response {
				static constexpr uint16_t MaxRegisters{250 / 2};
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
			struct ATTR_PACKED Request {
				enum State : uint16_t {
					state_off = 0x0000,
					state_on = 0xFF00,
				};

				uint16_t outputAddress;
				State outputValue;
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
			struct ATTR_PACKED Response {
				static constexpr uint16_t MaxEvents{64};
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
				static constexpr uint16_t MaxCoils{sizeof(values) * 8};

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
			struct ATTR_PACKED Request {
				static constexpr uint16_t MaxRegisters{123};
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

		// ReportServerId = 0x11,
		union ReportServerId {
			struct ATTR_PACKED Response {
				enum RunStatus : uint8_t {
					runstatus_off = 0x00,
					runstatus_on = 0xFF,
				};

				uint8_t byteCount;
				uint8_t serverId;
				RunStatus runStatus;
				uint8_t data[248]; ///< Content is specific to slave device

				void setCount(uint8_t dataSize)
				{
					byteCount = 2 + dataSize;
				}

				uint8_t getCount() const
				{
					return byteCount - 2;
				}
			};

			Response response;
		};
		ReportServerId reportServerId;

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
			struct ATTR_PACKED Request {
				static constexpr uint16_t MaxWriteRegisters{121};
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
				static constexpr uint16_t MaxReadRegisters{125};
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
