/**
 * Modbus/Exception.h
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
 *
 * This file is part of the IOControl Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#pragma once

#include <WString.h>

namespace IO
{
namespace Modbus
{
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
	XX(ReportServerId, 0x11)                                                                                           \
	XX(MaskWriteRegister, 0x16)                                                                                        \
	XX(ReadWriteMultipleRegisters, 0x17)

} // namespace Modbus
} // namespace IO
