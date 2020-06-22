/*
 * R421A.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#pragma once

#include <IO/Modbus/Device.h>

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

/**
 * @brief Tracks state of multiple relays
 */
struct StateMask {
	BitSet32 channelMask; ///< Identifies valid channels
	BitSet32 channelStates;
};

// Channels start with 1
constexpr uint8_t R421_CHANNEL_MIN = 1;

// R421 devices don't respond to channel numbers greater than 16
constexpr uint8_t R421A_MAX_CHANNELS = 16;

namespace IO
{
namespace Modbus
{
namespace R421A
{
const DeviceClassInfo deviceClass();

class Device : public Modbus::Device
{
public:
	struct Config {
		Modbus::Device::Config modbus;
		uint8_t channels;
	};

	using Modbus::Device::Device;

	const DeviceClassInfo classInfo() const override
	{
		return deviceClass();
	}

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	IO::Request* createRequest() override;

	const StateMask& states() const
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

	void handleEvent(IO::Request* request, Event event) override;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

private:
	// Tracks current output states as far as possible
	StateMask m_states{};
	// Depends on device variant (e.g. 8, 4)
	uint8_t m_channelCount{0};
};

} // namespace R421A
} // namespace Modbus
} // namespace IO
