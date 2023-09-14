/**
 * Modbus/STS/Fan/Request.cpp
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

#include <IO/Modbus/STS/Fan/Request.h>
#include <IO/Strings.h>

namespace IO::Modbus::STS::Fan
{
Function Request::fillRequestData(PDU::Data& data)
{
	switch(getCommand()) {
	case Command::query: {
		auto& req = data.readHoldingRegisters.request;
		req.startAddress = 0;
		req.quantityOfRegisters = channelCount;
		return (index == 0) ? Function::ReadHoldingRegisters : Function::ReadInputRegisters;
	}

	case Command::set: {
		if(node == DevNode_ALL) {
			auto& req = data.writeMultipleRegisters.request;
			req.startAddress = 0;
			req.setCount(channelCount);
			for(unsigned i = 0; i < channelCount; ++i) {
				req.values[i] = value;
			}
			return Function::WriteMultipleRegisters;
		}

		auto& req = data.writeSingleRegister.request;
		req.address = node.id;
		req.value = value;
		return Function::WriteSingleRegister;
	}

	default:
		return Function::None;
	}
}

ErrorCode Request::callback(PDU& pdu)
{
	if(getCommand() == Command::set) {
		return Error::success;
	}

	if(getCommand() != Command::query) {
		return Error::bad_command;
	}

	auto func = pdu.function();
	switch(func) {
	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters: {
		auto& rsp = pdu.data.readHoldingRegisters.response;
		auto n = std::min(uint16_t(channelCount), rsp.getCount());
		auto& data = getDevice().data;
		for(unsigned i = 0; i < n; ++i) {
			if(func == Function::ReadHoldingRegisters) {
				data.speed[i] = rsp.values[i];
			} else {
				data.rpm[i] = rsp.values[i];
			}
		}
		break;
	}

	default:
		return Error::bad_command;
	}

	if(index < 1) {
		++index;
		submit();
		return Error::pending;
	}

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	Modbus::Request::getJson(json);

	if(error()) {
		return;
	}

	getDevice().getValues(json);
}

} // namespace IO::Modbus::STS::Fan
