/**
 * Modbus/PDU.cpp
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

#include <IO/Modbus/PDU.h>
#include <FlashString/Map.hpp>

namespace IO
{
namespace Modbus
{
namespace
{
// Perform unaligned byteswap on array of uint16_t
void bswap(void* values, unsigned count)
{
	auto p = static_cast<uint8_t*>(values);
	while(count--) {
		std::swap(p[0], p[1]);
		p += 2;
	}
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
	default:
		return F("Unknown") + String(unsigned(exception));
	}
}

String toString(Function function)
{
#define XX(tag, value) DEFINE_FSTR_LOCAL(str_##tag, #tag)
	MODBUS_FUNCTION_MAP(XX)
#undef XX

#define XX(tag, value) {Function::tag, &str_##tag},
	DEFINE_FSTR_MAP_LOCAL(map, Function, FSTR::String, MODBUS_FUNCTION_MAP(XX))
#undef XX

	auto v = map[function];
	return v ? String(v) : F("Unknown_") + String(unsigned(function));
}

/**
 * @brief Get size (in bytes) of PDU Data for request packet
 */
size_t PDU::getRequestDataSize() const
{
	switch(function()) {
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs:
	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters:
	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
		return 4;

	case Function::WriteMultipleCoils:
	case Function::WriteMultipleRegisters:
		return 5 + data.writeMultipleCoils.request.byteCount;

	case Function::MaskWriteRegister:
		return 6;

	case Function::ReadWriteMultipleRegisters:
		return 9 + data.readWriteMultipleRegisters.request.writeByteCount;

	case Function::ReadExceptionStatus:
	case Function::GetComEventCounter:
	case Function::GetComEventLog:
	case Function::ReportServerId:
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
	case Function::ReadDiscreteInputs:
	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters:
	case Function::ReportServerId:
	case Function::ReadWriteMultipleRegisters:
	case Function::GetComEventLog:
		return 1 + data.readCoils.response.byteCount;

	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
	case Function::GetComEventCounter:
	case Function::WriteMultipleCoils:
	case Function::WriteMultipleRegisters:
		return 4;

	case Function::ReadExceptionStatus:
		return 1;

	case Function::MaskWriteRegister:
		return 6;

	default:
		return 0;
	}
}

void PDU::swapRequestByteOrder()
{
	if(exceptionFlag()) {
		return;
	}

	switch(function()) {
	case Function::None:
	case Function::GetComEventLog:
	case Function::ReadExceptionStatus:
	case Function::ReportServerId:
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
		auto& req = data.writeMultipleRegisters.request;
		bswap(&req.startAddress, 2);
		bswap(req.values, req.byteCount / 2);
		break;
	}

	case Function::MaskWriteRegister: {
		bswap(&data.maskWriteRegister.request.address, 3);
		break;
	}

	case Function::ReadWriteMultipleRegisters: {
		auto& req = data.readWriteMultipleRegisters.request;
		bswap(&req.readAddress, 4);
		bswap(&req.writeValues, req.writeByteCount / 2);
		break;
	}
	}
}

void PDU::swapResponseByteOrder()
{
	if(exceptionFlag()) {
		return;
	}

	switch(function()) {
	case Function::None:
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs:
	case Function::ReadExceptionStatus:
	case Function::ReportServerId:
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

} // namespace Modbus
} // namespace IO
