/**
 * Modbus/Function.h
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

// Modbus function codes
enum class Function {
#define XX(tag, value) tag = value,
	MODBUS_FUNCTION_MAP(XX)
#undef XX
};

} // namespace Modbus
} // namespace IO

String toString(IO::Modbus::Function function);
