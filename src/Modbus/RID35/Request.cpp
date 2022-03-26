/**
 * Modbus/RID35/Request.cpp
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

#include <IO/Modbus/RID35/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
namespace RID35
{
namespace
{
uint16_t getRegisterAddress(Register reg)
{
	unsigned regNum = unsigned(reg) * 2;
	if(regNum < StdRegCount) {
		return StdRegBase + regNum;
	}

	return OvfRegBase + unsigned(reg) - unsigned(Register::TotalKwh);
}

} // namespace

Function Request::fillRequestData(PDU::Data& data)
{
	if(getCommand() == Command::query) {
		// Query all channels
		auto& req = data.readInputRegisters.request;
		if(regCount == 0) {
			req.startAddress = StdRegBase;
			req.quantityOfRegisters = StdRegCount;
		} else {
			req.startAddress = OvfRegBase;
			req.quantityOfRegisters = OvfRegCount;
		}
		return Function::ReadInputRegisters;
	}

	// Send an invalid instruction
	debug_e("fillRequestData() - UNEXPECTED");
	return Function::None;
}

ErrorCode Request::callback(PDU& pdu)
{
	switch(pdu.function()) {
	case Function::ReadInputRegisters: {
		auto& rsp = pdu.data.readInputRegisters.response;
		auto count = rsp.getCount();
		if(regCount + count > RegisterCount) {
			return Error::bad_size;
		}
		auto prevCount = regCount;
		memcpy(&regValues[regCount], rsp.values, count * sizeof(uint16_t));
		regCount += count;

		if(getCommand() == Command::query && prevCount == 0) {
			// Submit request for rollover counter addresses
			submit();
			return Error::pending;
		}

		break;
	}

	case Function::ReadHoldingRegisters: {
		auto& rsp = pdu.data.readHoldingRegisters.response;
		regCount = rsp.getCount();

		for(unsigned i = 0; i < regCount; ++i) {
			regValues[i] = rsp.values[i];
		}

		break;
	}

	default:
		return Error::bad_command;
	}

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	Modbus::Request::getJson(json);

	if(error()) {
		return;
	}

	JsonArray values = json.createNestedArray(FS_value);

	for(unsigned i = 0; i < StdRegCount; i += 2) {
		union {
			uint32_t val;
			float f;
		} u;
		u.val = (regValues[i] << 16) | regValues[i + 1];
		values.add(u.f);
	}
	for(unsigned i = 0; i < OvfRegCount; ++i) {
		values.add(regValues[StdRegCount + i]);
	}
}

} // namespace RID35
} // namespace Modbus
} // namespace IO
