/*
 * R421A.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#pragma once

#include <IO/Modbus/Request.h>
#include "Device.h"

/**
 *  R421A08 modbus 8-channel relay board
 *
 *  Channels are numbered 1 - 8. This is the 16-bit address field
 *  in a request.
 *
 *  To simplify operation we use a bitmask to specify relay states.
 *  We use the channel number directly so values can
 *  range between 0x0001 and 0x01FE
 *
 * The query command returns a range of states, but other commands
 * work with only a single channel. We therefore implement a
 * mechanism to iterate through all requested channels using
 * the same request.
 *
 *  There is a similar 4-channel board but with no markings and
 *  (as yet) no documentation. However all commands appear to work
 *  so a designation of R421A04 seems appropriate. Some typos
 *  in the 8-channel datasheet indicate that it was adapted from
 *  that of a 4-channel version.
 *
 *  R421A04 - 32 addresses set with DIP1-5, DIP6 ON for RTU mode
 *  R421A08 - 64 addresses set with DIP1-6, RTU mode only
 *
 */

namespace IO
{
namespace Modbus
{
namespace R421A
{
class Request : public Modbus::Request
{
public:
	Request(Modbus::R421A::Device& device) : Modbus::Request(device)
	{
	}

	ErrorCode parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(Modbus::Request::device());
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
		m_data.delay = secs;
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

	StateMask& response()
	{
		return m_response;
	}

	const StateMask& response() const
	{
		return m_response;
	}

	Function fillRequestData(PDU::Data& data) override;
	ErrorCode callback(PDU& pdu) override;

private:
	// Associated command data
	struct Data {
		BitSet32 channelMask;
		uint8_t delay;
	};

	Data m_data{};
	StateMask m_response{};
};

} // namespace R421A
} // namespace Modbus
} // namespace IO
