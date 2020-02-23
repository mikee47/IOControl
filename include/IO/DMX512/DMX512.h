/*
 * IODMX512.h
 *
 *  Created on: 5 November 2018
 *      Author: mikee47
 *
 */

#pragma once

#include <IO/RS485/Controller.h>

namespace IO
{
namespace DMX512
{
// Device configuration
DECLARE_FSTR(CONTROLLER_CLASSNAME)

class Device;

class Request : public IO::Request
{
	friend Controller;

public:
	Request(Device& device) : IO::Request(reinterpret_cast<IO::Device&>(device))
	{
	}

	Device& device()
	{
		return reinterpret_cast<Device&>(m_device);
	}

	Error parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	bool setNode(DevNode node) override;

	DevNode node() const
	{
		return m_node;
	}

	bool nodeAdjust(DevNode node, int value) override
	{
		setCommand(Command::adjust);
		setValue(value);
		return setNode(node);
	}

	void setValue(int code)
	{
		m_value = code;
	}

	int value() const
	{
		return m_value;
	}

	Error submit() override;

private:
	int m_value{};
	DevNode m_node{};
};

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

const DeviceClassInfo deviceClass();

class Device : public IO::RS485::Device
{
	friend Controller;

public:
	static constexpr size_t MaxPacketSize{520};

	struct Config : public IO::Device::Config {
		uint16_t address;
		uint8_t nodeCount;
	};

	using IO::RS485::Device::Device;

	~Device()
	{
		delete m_nodeData;
	}

	const IO::DeviceClassInfo classInfo() const override
	{
		return deviceClass();
	}

	const DeviceType type() const override
	{
		return DeviceType::DMX512;
	}

	Error init(const Config& config);
	Error init(JsonObjectConst config) override;

	IO::Request* createRequest() override
	{
		return new Request(*this);
	}

	uint16_t address() const override
	{
		return m_address;
	}

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

	Error start() override
	{
		return Error::success;
	}

	/** @brief controller calls this before performing an update,
	 *  typically for effects processing.
	 *  Return true if value changed.
	 */
	bool update();

	void updateSlaves();

private:
	friend Request;
	Error execute(Request& request);

	uint16_t m_address = 0x01;		///< Start address for this device, may occupy more than one slot
	uint8_t m_nodeCount = 1;		///< Number of DMX slots managed by this device
	NodeData* m_nodeData = nullptr; ///< Data for each slot, starting at `address`
	static SimpleTimer m_timer;		///< For slave update cycle timing
	static bool m_changed;			///< Data has changed
	static bool m_updating;			///< Currently sending update
};

} // namespace DMX512
} // namespace IO
