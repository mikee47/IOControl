/*
 * IODMX512.h
 *
 *  Created on: 5 November 2018
 *      Author: mikee47
 *
 */

#pragma once

#include <IO/Control.h>
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

	bool setNode(DevNode node) override
	{
		m_nodeId = node.id;
		return m_nodeId == node.id;
	}

	void setCode(int code)
	{
		m_code = code;
	}

	uint8_t nodeId() const
	{
		return m_nodeId;
	}

	int code() const
	{
		return m_code;
	}

protected:
	void execute();

private:
	int m_code = 0;
	uint8_t m_nodeId = 0;
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

protected:
	Error init(const Config& config);
	Error init(JsonObjectConst config) override;
	void parseJson(JsonObjectConst json, Config& cfg);

	/** @brief controller calls this before performing an update,
	 *  typically for effects processing.
	 *  Return true if value changed.
	 */
	bool update();

	Error execute(Request& request);

private:
	uint16_t m_address = 0x01;		///< Start address for this device, may occupy more than one slot
	uint8_t m_nodeCount = 1;		///< Number of DMX slots managed by this device
	NodeData* m_nodeData = nullptr; ///< Data for each slot, starting at `address`
};

class Controller : public IO::Controller
{
public:
	Controller(uint8_t instance) : IO::Controller(instance)
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

protected:
	void execute(IO::Request& request) override;

private:
	void updateSlaves();
	void transmitComplete(HardwareSerial& serial);

	bool m_updating = false; ///< Currently sending update
	bool m_changed = false;  ///< Data has changed
	SimpleTimer m_timer;	 ///< For slave update cycle timing
};

const DeviceClassInfo deviceClass();

} // namespace DMX512
} // namespace IO
