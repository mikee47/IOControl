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
Function Request::fillRequestData(PDU::Data& data)
{
	if(getCommand() == Command::query) {
		// Query all channels
		auto& req = data.readInputRegisters.request;
		req.startAddress = 0x01;
		req.quantityOfRegisters = RegisterCount;
		return Function::ReadInputRegisters;
	}

	// Send an invalid instruction
	debug_e("fillRequestData() - UNEXPECTED");
	return Function::None;
}

ErrorCode Request::callback(PDU& pdu)
{
	memset(regValues, 0xff, sizeof(regValues));
	switch(pdu.function()) {
	case Function::ReadInputRegisters: {
		auto& rsp = pdu.data.readInputRegisters.response;
		regCount = rsp.getCount();

		for(unsigned i = 0; i < regCount; ++i) {
			regValues[i] = rsp.values[i];
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

	for(unsigned i = 0; i < regCount; i += 2) {
		union {
			uint32_t val;
			float f;
		} u;
		u.val = (regValues[i] << 16) | regValues[i + 1];
		values.add(u.f);
	}
}

} // namespace RID35
} // namespace Modbus
} // namespace IO
