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

	IO::Error parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	bool setNode(IO::DevNode node) override
	{
		m_node = node;
		return true;
	}

	uint8_t node() const
	{
		return m_node;
	}

	int code() const
	{
		return m_code;
	}

protected:
	void execute();

private:
	uint8_t m_node = 0;
	int m_code = 0;
};

// Bit values
enum DmxNodeState { DMXNF_disabled, DMXNF_enabling, DMXNF_enabled, DMXNF_disabling };

struct DmxNodeData {
	uint8_t target;
	uint8_t value;
	DmxNodeState state;

	bool changed() const
	{
		return (state == DMXNF_enabling) || (state == DMXNF_disabling) || (target != value);
	}

	void enable()
	{
		if(state != DMXNF_enabled) {
			state = DMXNF_enabling;
		}
	}

	void disable()
	{
		if(state != DMXNF_disabled) {
			state = DMXNF_disabling;
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
		state = newValue ? DMXNF_enabling : DMXNF_disabling;
	}

	bool adjust()
	{
		if(state == DMXNF_disabled) {
			return false;
		}

		uint8_t adjustTarget = (state == DMXNF_disabling) ? 0 : target;

		if(value == adjustTarget) {
			if(state == DMXNF_disabling) {
				state = DMXNF_disabled;
			} else if(state == DMXNF_enabling) {
				state = DMXNF_enabled;
			}
			return false;
		}

		int step = (state == DMXNF_enabled) ? 1 : 4;
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
	//	friend DMX512Request;
	friend Controller;

public:
	struct Config: public IO::Device::Config {
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

	static const IO::DeviceClassInfo deviceClass();

	IO::Request* createRequest() override
	{
		return new Request(*this);
	}

	uint16_t address() const override
	{
		return m_address;
	}

	IO::DevNode nodeIdMax() const override
	{
		return m_nodeCount - 1;
	}

	uint16_t maxNodes() const override
	{
		return m_nodeCount;
	}

	const DmxNodeData& getNodeData(IO::DevNode node) const
	{
		assert(node < m_nodeCount);
		return m_nodeData[node];
	}

protected:
	IO::Error init(const Config& config);
	IO::Error init(JsonObjectConst config) override;
	void parseJson(JsonObjectConst json, Config& cfg);

	/** @brief controller calls this before performing an update,
	 *  typically for effects processing.
	 *  Return true if value changed.
	 */
	bool update();

	IO::Error execute(Request& request);

private:
	uint16_t m_address = 0x01;		   ///< Start address for this device, may occupy more than one slot
	uint8_t m_nodeCount = 1;		   ///< Number of DMX slots managed by this device
	DmxNodeData* m_nodeData = nullptr; ///< Data for each slot, starting at address
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

private:
	void execute(IO::Request& request) override;
	void updateSlaves();
	void transmitComplete(HardwareSerial& serial);

	bool m_updating = false; ///< Currently sending update
	bool m_changed = false;  ///< Data has changed
	SimpleTimer m_timer;	 ///< For slave update cycle timing
};

} // namespace DMX512
