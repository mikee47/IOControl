/**
 * Modbus/R421A/Request.cpp
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

#include <IO/Modbus/R421A/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
namespace R421A
{
enum r421a_command_t {
	r421_query = 0x00,
	r421_close = 0x01,
	r421_open = 0x02,
	r421_toggle = 0x03,
	r421_latch = 0x04,
	r421_momentary = 0x05,
	r421_delay = 0x06,
	//
	r421_on = r421_close,
	r421_off = r421_open,
	r421_undefined = 0xFF,
	r421_send = r421_undefined
};

enum a421_relay_state_t {
	relay_open = 0x0000,
	relay_closed = 0x0001,
};

static r421a_command_t map(Command cmd)
{
	switch(cmd) {
	case Command::query:
		return r421_query;
	case Command::on:
		return r421_close;
	case Command::off:
		return r421_open;
	case Command::toggle:
		return r421_toggle;
	case Command::latch:
		return r421_latch;
	case Command::momentary:
		return r421_momentary;
	case Command::delay:
		return r421_delay;
	default:
		return r421_undefined;
	};
}

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
		// Query all channels
		auto& req = data.readHoldingRegisters.request;
		req.startAddress = device.nodeIdMin();
		req.quantityOfRegisters = device.maxNodes();
		return Function::ReadHoldingRegisters;
	}

	// others
	for(auto ch = device.nodeIdMin(); ch <= device.nodeIdMax(); ++ch) {
		if(commandData.channelMask[ch]) {
			PDU::Data::WriteSingleRegister::Request req;
			req.address = ch;
			req.value = map(getCommand()) << 8;
			if(getCommand() == Command::delay) {
				req.value |= commandData.delay;
			}

			//      debug_i("fillRequestData() - channel %u, cmd %u", ch, m_cmd);

			data.writeSingleRegister.request = req;
			return Function::WriteSingleRegister;
		}
	}

	// Send an invalid instruction
	debug_e("fillRequestData() - UNEXPECTED");
	return Function::None;
}

ErrorCode Request::callback(PDU& pdu)
{
	//	if(m_command == Command::query) {
	switch(pdu.function()) {
	case Function::ReadHoldingRegisters: {
		auto& rsp = pdu.data.readHoldingRegisters.response;
		auto valueCount = rsp.getCount();

		//    assert (mbt.function == MB_ReadHoldingRegisters);
		// data[0] is response size, in bytes: should correspond with mbt.dataSize + 1
		for(unsigned i = 0; i < valueCount; ++i) {
			auto val = rsp.values[i];
			if(val != relay_open && val != relay_closed) {
				continue; // Erroneous response - ignore
			}

			auto ch = device.nodeIdMin() + i;
			response.channelMask[ch] = true;
			response.channelStates[ch] = (val == relay_closed);
		}

		break;
	}

	case Function::WriteSingleRegister: {
		// Other commands
		auto& rsp = pdu.data.writeSingleRegister.response;
		uint8_t ch = rsp.address;
		// We've handled this channel, clear command mask bit
		commandData.channelMask[ch] = false;
		//  debug_i("Channel = %u, mask = %08x", channel, commandData.channelMask);
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
		assert(false);
		return Error::bad_command;
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

} // namespace R421A
} // namespace Modbus
} // namespace IO
