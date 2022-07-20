/**
 * Modbus/GenericRequest.cpp
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

#include <IO/Modbus/GenericRequest.h>
#include <IO/Strings.h>
#include <debug_progmem.h>
#include <Data/HexString.h>

namespace IO
{
namespace Modbus
{
namespace
{
DEFINE_FSTR(FS_modbus, "modbus")

constexpr uint16_t maxValueCount{20};

} // namespace

Function GenericRequest::fillRequestData(PDU::Data& data)
{
	switch(function) {
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs: {
		auto& req = data.readDiscreteInputs.request;
		req.startAddress = address;
		req.quantityOfInputs = count;
		return function;
	}

	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters: {
		auto& req = data.readInputRegisters.request;
		req.startAddress = address + offset;
		req.quantityOfRegisters = std::min(maxValueCount, uint16_t(count - offset));
		return function;
	}

	case Function::WriteSingleRegister: {
		if(!values) {
			return Function::None;
		}
		auto& req = data.writeSingleRegister.request;
		req.address = address;
		req.value = values[0];
		return function;
	}

	case Function::WriteMultipleRegisters: {
		if(!values) {
			return Function::None;
		}
		auto& req = data.writeMultipleRegisters.request;
		req.startAddress = address;
		req.setCount(count);
		memcpy(req.values, &values[0], req.byteCount);
		return function;
	}

	default:
		return Function::None;
	}
}

ErrorCode GenericRequest::callback(PDU& pdu)
{
	this->pdu.reset(new PDU{pdu});

	if(pdu.function() != function) {
		return Error::bad_function;
	}

	switch(function) {
	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters: {
		if(!values) {
			values.reset(new uint16_t[count]);
			if(!values) {
				return Error::no_mem;
			}
		}
		auto& rsp = pdu.data.readHoldingRegisters.response;
		auto n = rsp.getCount();
		for(unsigned i = 0; i < n; ++i) {
			values[offset + i] = rsp.values[i];
		}
		offset += n;
		if(offset < count) {
			submit();
			return Error::pending;
		}

		break;
	}

	default:;
	}

	return Error::success;
}

ErrorCode GenericRequest::parseJson(JsonObjectConst json)
{
	auto err = Request::parseJson(json);

	if(err) {
		return err;
	}

	auto readValues = [](const char* ptr, uint16_t* out) -> size_t {
		size_t n{0};
		char* endptr;
		while(isspace(*ptr)) {
			++ptr;
		}
		while(*ptr != '\0') {
			uint16_t val = strtoul(ptr, &endptr, 0);
			if(out) {
				out[n] = val;
			}
			++n;
			ptr = endptr;
		}
		return n;
	};

	function = Function(json[FS_function].as<uint8_t>());
	if(function == Function::None) {
		return Error::bad_function;
	}
	count = json[FS_count];
	address = json[FS_address];
	offset = 0;

	auto valptr = json[FS_value].as<const char*>();
	if(valptr != nullptr) {
		auto numValues = readValues(valptr, nullptr);
		if(numValues != 0) {
			values.reset(new uint16_t[numValues]);
			readValues(valptr, values.get());
			debug_hex(INFO, "VALUE", values.get(), numValues);
			count = numValues;
		}
	}

	return err;
}

void GenericRequest::getJson(JsonObject json) const
{
	Request::getJson(json);

	json[FS_function] = unsigned(function);
	json[FS_address] = address;

	if(error() || !pdu) {
		return;
	}
	auto jvalues = json.createNestedArray(FS_value);

	switch(pdu->function()) {
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs: {
		auto& rsp = pdu->data.readDiscreteInputs.response;
		auto count = rsp.getCount();
		for(unsigned i = 0; i < count; ++i) {
			jvalues.add(rsp.getInput(i));
		}
		break;
	}

	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters: {
		for(unsigned i = 0; i < count; ++i) {
			jvalues.add(values[i]);
		}
		return;
	}

	default:;
		pdu->swapResponseByteOrder();
		json["hex"] = makeHexString(reinterpret_cast<const uint8_t*>(pdu.get()), pdu->getResponseSize(), ' ');
	}
}

} // namespace Modbus
} // namespace IO