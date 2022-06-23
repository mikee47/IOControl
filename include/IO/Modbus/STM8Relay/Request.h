/**
 * Modbus/STM8Relay/Request.h
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

#include "../Request.h"
#include "Device.h"

namespace IO
{
namespace Modbus
{
namespace STM8Relay
{
class Request : public Modbus::Request
{
public:
	Request(Device& device) : Modbus::Request(device)
	{
	}

	ErrorCode parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	const Device& getDevice() const
	{
		return static_cast<const Device&>(device);
	}

	bool setNode(DevNode node) override;

	bool nodeLatch(DevNode node)
	{
		setCommand(Command::latch);
		return setNode(node);
	}

	bool nodeMomentary(DevNode node)
	{
		setCommand(Command::momentary);
		return setNode(node);
	}

	bool nodeDelay(DevNode node, uint8_t secs)
	{
		setCommand(Command::delay);
		commandData.delay = secs;
		return setNode(node);
	}

	DevNode::States getNodeStates(DevNode node) override;

	bool setNodeState(DevNode node, DevNode::State state) override
	{
		if(state == DevNode::State::on) {
			nodeOn(node);
		} else if(state == DevNode::State::off) {
			nodeOff(node);
		} else {
			return false;
		}
		return true;
	}

	StateMask& getResponse()
	{
		return response;
	}

	const StateMask& getResponse() const
	{
		return response;
	}

	Function fillRequestData(PDU::Data& data) override;
	ErrorCode callback(PDU& pdu) override;

private:
	// Associated command data
	struct CommandData {
		BitSet32 channelMask;
		uint8_t delay;
	};

	CommandData commandData{};
	StateMask response{};
};

} // namespace STM8Relay
} // namespace Modbus
} // namespace IO
