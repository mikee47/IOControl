/*
 * IODMX512.h
 *
 *  Created on: 5 November 2018
 *      Author: mikee47
 *
 */

#pragma once

#include <IO/Control.h>
#include <IO/Serial.h>
#include <SimpleTimer.h>

class HardwareSerial;

namespace IO
{
namespace DMX512
{
// Device configuration
DECLARE_FSTR(CONTROLLER_CLASSNAME)

class Device;
class Controller;

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

/*
 * A virtual device, represents a DMX512 slave device.
 * Actual devices must implement
 *  createRequest()
 *  callback()
 *  fillRequestData()
 */
class Device : public IO::Device
{
	friend Controller;

public:
	struct Config : public IO::Device::Config {
		uint16_t address;
		uint8_t nodeCount;
	};

	Device(Controller& controller) : IO::Device(reinterpret_cast<IO::Controller&>(controller))
	{
	}

	~Device() override
	{
		delete m_nodeData;
	}

	const IO::DeviceClassInfo getClass() const override
	{
		return deviceClass();
	}

	Controller& controller() const
	{
		return reinterpret_cast<Controller&>(IO::Device::controller());
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

private:
	friend Request;
	Error execute(Request& request);

	uint16_t m_address = 0x01;		///< Start address for this device, may occupy more than one slot
	uint8_t m_nodeCount = 1;		///< Number of DMX slots managed by this device
	NodeData* m_nodeData = nullptr; ///< Data for each slot, starting at `address`
};

class Controller : public IO::Controller
{
public:
	static constexpr size_t MaxPacketSize{520};

	Controller(Serial& serial, uint8_t instance)
		: IO::Controller(instance), serial{serial}, state{.controller{this},
														  .onTransmitComplete{transmitComplete},
														  .txBufferSize{MaxPacketSize}}
	{
	}

	String classname() override
	{
		return CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

	bool busy() const override
	{
		return m_updating;
	}

	void deviceChanged();

protected:
	void execute(IO::Request& request) override;

private:
	void updateSlaves();
	static void IRAM_ATTR transmitComplete(Serial::State& state);
	void transmitComplete();

	Serial& serial;
	Serial::State state;
	bool m_updating{false}; ///< Currently sending update
	bool m_changed{false};  ///< Data has changed
	SimpleTimer m_timer;	///< For slave update cycle timing
};

} // namespace DMX512
} // namespace IO
