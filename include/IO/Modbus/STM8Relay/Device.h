/**
 * Modbus/STM8Relay/Device.h
 *
 *  Created on: 24 June 2022
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
 * STM8S103 modbus relay board
 * ===========================
 *
 * 4-channel relay boards with optocouplers and digital inputs.
 * Advertised as using STM8S103 microcontroller, but no model number.
 * Relays controlled using Modbus 'coil' commands.
 * Digital inputs read as 'discrete inputs'. Connected directly to 3.3v microcontroller. Grounding pin sets bit.
 * 
 ****/

#pragma once

#include "../Device.h"

namespace IO
{
namespace Modbus
{
namespace STM8Relay
{
/**
 * @brief Tracks state of multiple relays
 */
struct StateMask {
	BitSet32 channelMask; ///< Identifies valid channels
	BitSet32 channelStates;
};

// Channels start with 1 for consistency with other devices
constexpr uint8_t RELAY_CHANNEL_MIN = 1;
constexpr uint8_t RELAY_MAX_CHANNELS = 16;

class Device : public Modbus::Device
{
public:
	class Factory : public FactoryTemplate<Device>
	{
	public:
		const FlashString& deviceClass() const override
		{
			return FS("stm8relay");
		}
	};

	static const Factory factory;

	/**
	 * @brief R421A device configuration
	 */
	struct Config {
		Modbus::Device::Config modbus; ///< Basic modbus configuration
		uint8_t channels;			   ///< Number of channels (typically 4 or 8)
	};

	using Modbus::Device::Device;

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	IO::Request* createRequest() override;

	const StateMask& getStates() const
	{
		return states;
	}

	DevNode::ID nodeIdMin() const override
	{
		return RELAY_CHANNEL_MIN;
	}

	uint16_t maxNodes() const override
	{
		return channelCount;
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
	StateMask states{};
	// Depends on device variant (e.g. 8, 4)
	uint8_t channelCount{0};
};

} // namespace STM8Relay
} // namespace Modbus
} // namespace IO
