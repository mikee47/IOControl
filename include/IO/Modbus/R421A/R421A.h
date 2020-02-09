/*
 * R421A.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#pragma once

#include "../Modbus.h"

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

/*
 * R421 devices don't respond to channel numbers greater than 16.
 */
constexpr uint8_t R421A_MAX_CHANNELS = 16;

// Channels start with 1
constexpr uint8_t R421_CHANNEL_MIN = 1;

namespace IO
{
namespace Modbus
{
namespace R421A
{
struct Response {
	// Response data
	BitSet32 channelMask;
	BitSet32 channelStates;
};

class Device;

class Request : public Modbus::Request
{
public:
	Request(Modbus::Device& device) : Modbus::Request(device)
	{
	}

	Error parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	const Device& device() const
	{
		return reinterpret_cast<Device&>(m_device);
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

	Response& response()
	{
		return m_response;
	}

protected:
	Function fillRequestData(PDU::Data& data) override;
	void callback(PDU& pdu) override;

private:
	// Associated command data
	struct Data {
		BitSet32 channelMask;
		uint8_t delay;
	};

	Data m_data{};
	Response m_response{};
};

const IO::DeviceClassInfo deviceClass();

class Device : public Modbus::Device
{
public:
	struct Config : public Modbus::Device::Config {
		uint8_t channels;
	};

	Device(Modbus::Controller& controller) : Modbus::Device(controller)
	{
	}

	const IO::DeviceClassInfo getClass() const override
	{
		return deviceClass();
	}

	Error init(const Config& config);
	Error init(JsonObjectConst config) override;

	IO::Request* createRequest() override
	{
		return new Request(*this);
	}

	const Response& states() const
	{
		return m_states;
	}

	DevNode::ID nodeIdMin() const override
	{
		return R421_CHANNEL_MIN;
	}

	DevNode::ID nodeIdMax() const override
	{
		return R421_CHANNEL_MIN + m_channelCount - 1;
	}

	uint16_t maxNodes() const override
	{
		return m_channelCount;
	}

	bool isValid(DevNode node) const
	{
		return node.id >= nodeIdMin() && node.id <= nodeIdMax();
	}

	DevNode::States getNodeStates(DevNode node) const override;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	// We use this to track states
	void requestComplete(IO::Request* request) override;

private:
	// Tracks current output states as far as possible
	Response m_states{};
	// Depends on device variant (e.g. 8, 4)
	uint8_t m_channelCount{0};
};

} // namespace R421A
} // namespace Modbus
} // namespace IO
