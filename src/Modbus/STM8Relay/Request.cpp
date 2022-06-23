/**
 * Modbus/STM8Relay/Request.cpp
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

#include <IO/Modbus/STM8Relay/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
namespace STM8Relay
{
/*
 * We specify channels as a bitmask or an array. Arrays are clearer for
 * JSON/scripting but less efficient. Bitmask is more concise. We'll
 * use arrays externally and bitmasks internally. Thus:
 *  [1, 2, 3, 7, 8] == 0b11000111 == 0xC7
 * We'll use 32 bits for this.
 */
Function Request::fillRequestData(PDU::Data& data)
{
	if(getCommand() == Command::query) {
		switch(commandData.delay) {
		case 0: {
			// Query relay status
			auto& req = data.readCoils.request;
			req.startAddress = 0;
			req.quantityOfCoils = RELAY_MAX_CHANNELS;
			return Function::ReadCoils;
		}

		case 1: {
			// Read digital inputs
			auto& req = data.readDiscreteInputs.request;
			req.startAddress = 0;
			req.quantityOfInputs = 0;
			return Function::ReadDiscreteInputs;
		}

		case 2:
		case 3:
		case 4:
		case 5: {
			// Read software version, etc.
			auto& req = data.readHoldingRegisters.request;
			req.startAddress = 1 << commandData.delay;
			req.quantityOfRegisters = 1;
			return Function::ReadHoldingRegisters;
		}

		default:
			// assert(false);
			return Function::None;
		}
	}

	if(getCommand() == Command::update) {
		// Change slave address
		auto& req = data.writeSingleRegister.request;
		req.address = 0x4000;
		req.value = commandData.delay;
		return Function::WriteSingleRegister;
	}

	// others
	for(auto ch = device.nodeIdMin(); ch <= device.nodeIdMax(); ++ch) {
		if(commandData.channelMask[ch]) {
			PDU::Data::WriteSingleCoil::Request req;

			auto cmd = getCommand();
			if(cmd == Command::toggle) {
				cmd = getDevice().getStates().channelStates[ch] ? Command::off : Command::on;
			}

			req.outputAddress = ch - 1;
			req.outputValue = (cmd == Command::on) ? decltype(req)::State::state_on : decltype(req)::State::state_off;

			//      debug_i("fillRequestData() - channel %u, cmd %u", ch, m_cmd);

			data.writeSingleCoil.request = req;
			return Function::WriteSingleCoil;
		}
	}

	// Send an invalid instruction
	debug_e("fillRequestData() - UNEXPECTED");
	return Function::None;
}

ErrorCode Request::callback(PDU& pdu)
{
	switch(pdu.function()) {
	case Function::ReadCoils: {
		auto& rsp = pdu.data.readCoils.response;
		auto valueCount = rsp.getCount();
		for(unsigned i = 0; i < valueCount; ++i) {
			auto val = rsp.getCoil(i);
			auto ch = device.nodeIdMin() + i;
			response.channelMask[ch] = true;
			response.channelStates[ch] = val;
		}

		break;
	}

	case Function::ReadDiscreteInputs:
	case Function::ReadHoldingRegisters:
	case Function::WriteSingleRegister:
		break;

	case Function::WriteSingleCoil: {
		// Other commands
		auto& rsp = pdu.data.writeSingleCoil.response;
		uint8_t ch = rsp.outputAddress + 1;
		// We've handled this channel, clear command mask bit
		commandData.channelMask[ch] = false;
		// Report back which bits were affected
		response.channelMask[ch] = true;
		if(getCommand() == Command::toggle) {
			// Use current states held by IODevice to determine effect of toggle command
			response.channelStates[ch] = !getDevice().getStates().channelStates[ch];
		} else if(getCommand() == Command::on) {
			response.channelStates[ch] = true;
		}

		// Re-submit request for next channel, if any, otherwise we're done
		if(commandData.channelMask.any()) {
			submit();
			return Error::pending;
		}

		break;
	}

	default:
		return Error::bad_command;
	}

	if(getCommand() == Command::query && commandData.delay < 5) {
		++commandData.delay;
		submit();
		return Error::pending;
	}

	return Error::success;
}

bool Request::setNode(DevNode node)
{
	if(node == DevNode_ALL) {
		for(auto ch = device.nodeIdMin(); ch <= device.nodeIdMax(); ++ch) {
			commandData.channelMask[ch] = true;
		}
		return true;
	}

	if(!getDevice().isValid(node)) {
		return false;
	}

	commandData.channelMask[node.id] = true;
	return true;
}

DevNode::States Request::getNodeStates(DevNode node)
{
	DevNode::States states;
	if(node == DevNode_ALL) {
		auto mask = isPending() ? commandData.channelMask : response.channelMask;
		for(auto ch = device.nodeIdMin(); ch <= device.nodeIdMax(); ++ch) {
			if(mask[ch]) {
				states += getDevice().getNodeStates(DevNode{ch});
			}
		}
	} else {
		states = getDevice().getNodeStates(node);
	}

	return states;
}

ErrorCode Request::parseJson(JsonObjectConst json)
{
	auto err = Modbus::Request::parseJson(json);
	if(!err) {
		unsigned delay = json[FS_delay];
		if(delay <= 255) {
			commandData.delay = delay;
		} else {
			err = Error::bad_param;
		}
	}

	return err;
}

void Request::getJson(JsonObject json) const
{
	Modbus::Request::getJson(json);

	auto mask = isPending() ? commandData.channelMask : response.channelMask;

	JsonArray nodes = json.createNestedArray(FS_nodes);
	for(auto ch = device.nodeIdMin(); ch <= device.nodeIdMax(); ++ch) {
		if(mask[ch]) {
			nodes.add(ch);
		}
	}

	if(response.channelMask.any()) {
		JsonArray states = json.createNestedArray(FS_states);

		debug_d("Channel mask = 0x%08x, states = 0x%08x", response.channelMask, response.channelStates);
		for(auto ch = device.nodeIdMin(); ch <= device.nodeIdMax(); ++ch) {
			if(response.channelMask[ch]) {
				states.add(response.channelStates[ch]);
			}
		}
	}
}

} // namespace STM8Relay
} // namespace Modbus
} // namespace IO
