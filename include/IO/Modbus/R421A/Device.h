/**
 * Modbus/R421A/Device.h
 *
 *  Created on: 1 May 2018
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
 *
 * R421A08 modbus 8-channel relay board
 * ====================================
 *
 * Channels are numbered 1 - 8. This is the 16-bit address field in a request.
 *
 * To simplify operation we use a bitmask to specify relay states.
 * We use the channel number directly so values can range between 0x0001 and 0x01FE
 *
 * The query command returns a range of states, but other commands work with only a single channel.
 * We therefore implement a mechanism to iterate through all requested channels using the same request.
 *
 * There is a similar 4-channel board but with no markings and (as yet) no documentation.
 * However all commands appear to work so a designation of R421A04 seems appropriate.
 * Some typos in the 8-channel datasheet indicate that it was adapted from that of a 4-channel version.
 *
 * R421A04 - 32 addresses set with DIP1-5, DIP6 ON for RTU mode
 * R421A08 - 64 addresses set with DIP1-6, RTU mode only
 *
 ****/

#pragma once

#include <IO/Modbus/Device.h>

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
class Device : public Modbus::Device
{
public:
	class Factory : public IO::Device::Factory
	{
	public:
		IO::Device* createDevice(IO::Controller& controller, const char* id) const override
		{
			return new Device(reinterpret_cast<RS485::Controller&>(controller), id);
		}

		const FlashString& controllerClass() const override
		{
			return RS485::CONTROLLER_CLASSNAME;
		}

		const FlashString& deviceClass() const override
		{
			DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "r421a")
			return DEVICE_CLASSNAME;
		}
	};

	static const Factory factory;

	struct Config {
		Modbus::Device::Config modbus;
		uint8_t channels;
	};

	using Modbus::Device::Device;

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
