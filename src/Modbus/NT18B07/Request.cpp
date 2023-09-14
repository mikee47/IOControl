/**
 * Modbus/NT18B07/Request.cpp
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

#include <IO/Modbus/NT18B07/Request.h>
#include <IO/Strings.h>

namespace IO::Modbus::NT18B07
{
Function Request::fillRequestData(PDU::Data& data)
{
	if(getCommand() == Command::query) {
		auto& req = data.readHoldingRegisters.request;
		req.startAddress = 0;
		req.quantityOfRegisters = channelCount;
		return Function::ReadHoldingRegisters;
	}

	return Function::None;
}

ErrorCode Request::callback(PDU& pdu)
{
	switch(pdu.function()) {
	case Function::ReadHoldingRegisters: {
		auto& rsp = pdu.data.readHoldingRegisters.response;
		getDevice().updateValues(rsp.values, rsp.getCount());
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

	getDevice().getValues(json.createNestedArray(FS_value));
}

} // namespace IO::Modbus::NT18B07
