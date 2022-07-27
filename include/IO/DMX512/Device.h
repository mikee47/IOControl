/**
 * DMX512/Device.h
 *
 *  Created on: 5 November 2018
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

#include "../RS485/Device.h"
#include <Data/Range.h>

namespace IO
{
namespace DMX512
{
class Request;

struct NodeData {
	enum class State {
		disabled,
		enabling,
		enabled,
		disabling,
	};

	uint8_t target;
	uint8_t value;
	State state;

	bool changed() const
	{
		return (state == State::enabling) || (state == State::disabling) || (target != value);
	}

	void enable()
	{
		if(state != State::enabled) {
			state = State::enabling;
		}
	}

	void disable()
	{
		if(state != State::disabled) {
			state = State::disabling;
		}
	}

	void setTarget(int newTarget)
	{
		target = TRange(0, 0xff).clip(newTarget);
	}

	void setValue(uint8_t newValue)
	{
		value = newValue;
		target = value;
		state = newValue ? State::enabling : State::disabling;
	}

	bool adjust()
	{
		if(state == State::disabled) {
			return false;
		}

		uint8_t adjustTarget = (state == State::disabling) ? 0 : target;

		if(value == adjustTarget) {
			if(state == State::disabling) {
				state = State::disabled;
			} else if(state == State::enabling) {
				state = State::enabled;
			}
			return false;
		}

		int step = (state == State::enabled) ? 1 : 4;
		int newValue = value + ((value < adjustTarget) ? step : -step);
		value = TRange(0, 0xff).clip(newValue);
		return true;
	}
};

class Device : public RS485::Device
{
	friend Request;

public:
	class Factory : public FactoryTemplate<Device>
	{
	public:
		const FlashString& deviceClass() const override
		{
			return FS("dmx");
		}
	};

	static const Factory factory;

	static constexpr size_t MaxPacketSize{520};

	/**
	 * @brief DMX512 Device Configuration
	 */
	struct Config {
		IO::RS485::Device::Config rs485; ///< RS485 config
		/**
		 * @brief Number of nodes controlled by this device
		 */
		uint8_t nodeCount;
	};

	using IO::RS485::Device::Device;

	const DeviceType type() const override
	{
		return DeviceType::DMX512;
	}

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	IO::Request* createRequest() override;

	uint16_t maxNodes() const override
	{
		return nodeCount;
	}

	const NodeData& getNodeData(uint8_t nodeId) const
	{
		assert(nodeId < nodeCount);
		return nodeData[nodeId];
	}

	bool isValid(DevNode node) const
	{
		return node == DevNode_ALL || node.id < nodeCount;
	}

	void handleEvent(IO::Request* request, Event event) override;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	ErrorCode start() override
	{
		return Error::success;
	}

	/** @brief controller calls this before performing an update,
	 *  typically for effects processing.
	 *  Return true if value changed.
	 */
	bool update();

	void updateSlaves();

	ErrorCode execute(Request& request);

private:
	uint8_t nodeCount{1};				  ///< Number of DMX slots managed by this device
	std::unique_ptr<NodeData[]> nodeData; ///< Data for each slot, starting at `address`
	static SimpleTimer timer;			  ///< For slave update cycle timing
	static bool dataChanged;			  ///< Data has changed
	static bool updating;				  ///< Currently sending update
};

} // namespace DMX512
} // namespace IO
