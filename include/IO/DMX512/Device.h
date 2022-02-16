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
		if(newTarget < 0) {
			target = 0;
		} else if(newTarget > 0xFF) {
			target = 0xFF;
		} else {
			target = uint8_t(newTarget);
		}
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
		value = (newValue <= 0) ? 0 : (newValue >= 0xFF) ? 0xFF : newValue;
		return true;
	}
};

class Device : public RS485::Device
{
	friend Request;

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
			DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "dmx")
			return DEVICE_CLASSNAME;
		}
	};

	static const Factory factory;

	static constexpr size_t MaxPacketSize{520};

	struct Config {
		IO::RS485::Device::Config rs485;
		uint8_t nodeCount;
	};

	using IO::RS485::Device::Device;

	~Device()
	{
		delete m_nodeData;
	}

	const DeviceType type() const override
	{
		return DeviceType::DMX512;
	}

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	IO::Request* createRequest() override;

	DevNode::ID nodeIdMax() const override
	{
		return m_nodeCount - 1;
	}

	uint16_t maxNodes() const override
	{
		return m_nodeCount;
	}

	const NodeData& getNodeData(uint8_t nodeId) const
	{
		assert(nodeId < m_nodeCount);
		return m_nodeData[nodeId];
	}

	bool isValid(DevNode node) const
	{
		return node == DevNode_ALL || node.id < m_nodeCount;
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
	uint8_t m_nodeCount{1};		   ///< Number of DMX slots managed by this device
	NodeData* m_nodeData{nullptr}; ///< Data for each slot, starting at `address`
	static SimpleTimer m_timer;	///< For slave update cycle timing
	static bool m_changed;		   ///< Data has changed
	static bool m_updating;		   ///< Currently sending update
};

} // namespace DMX512
} // namespace IO
