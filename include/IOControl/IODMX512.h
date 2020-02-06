/*
 * IODMX512.h
 *
 *  Created on: 5 November 2018
 *      Author: mikee47
 *
 */

#pragma once

#include <IOControl.h>
#include <SimpleTimer.h>

// Device configuration
DECLARE_FSTR(DMX512_CONTROLLER_CLASSNAME)

class DMX512Device;
class DMX512Controller;

class DMX512Request: public IORequest
{
	friend DMX512Controller;

public:
	DMX512Request(DMX512Device& device) :
		IORequest(reinterpret_cast<IODevice&>(device))
	{
	}

	DMX512Device& device()
	{
		return reinterpret_cast<DMX512Device&>(m_device);
	}

	ioerror_t parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	bool setNode(devnode_id_t nodeId) override
	{
		m_node = nodeId;
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
enum DmxNodeState {
	DMXNF_disabled,
	DMXNF_enabling,
	DMXNF_enabled,
	DMXNF_disabling
};

struct DmxNodeData {
	uint8_t target = 0;
	uint8_t value = 0;
	DmxNodeState state = DMXNF_disabled;

	bool changed() const
	{
		return (state == DMXNF_enabling) || (state == DMXNF_disabling) || (target != value);
	}

	void enable()
	{
		if (state != DMXNF_enabled) {
			state = DMXNF_enabling;
		}
	}

	void disable()
	{
		if (state != DMXNF_disabled) {
			state = DMXNF_disabling;
		}
	}

	void setTarget(int newTarget)
	{
		if (newTarget < 0)
			newTarget = 0;
		else if (newTarget > 0xFF)
			newTarget = 0xFF;
		target = uint8_t(newTarget);
	}

	void setValue(uint8_t newValue)
	{
		value = newValue;
		target = value;
		state = newValue ? DMXNF_enabling : DMXNF_disabling;
	}

	bool adjust()
	{
		if (state == DMXNF_disabled) {
			return false;
		}

		uint8_t adjustTarget = (state == DMXNF_disabling) ? 0 : target;

		if (value == adjustTarget) {
			if (state == DMXNF_disabling) {
				state = DMXNF_disabled;
			}
			else if (state == DMXNF_enabling) {
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
class DMX512Device: public IODevice
{
//	friend DMX512Request;
	friend DMX512Controller;

public:
	DMX512Device(DMX512Controller& controller) :
		IODevice(reinterpret_cast<IOController&>(controller))
	{
	}

	~DMX512Device() override
	{
		delete m_nodeData;
	}

	static const device_class_info_t deviceClass();

	IORequest* createRequest() override
	{
		return new DMX512Request(*this);
	}

	uint16_t address() const override
	{
		return m_address;
	}

	devnode_id_t nodeIdMax() const override
	{
		return m_nodeCount - 1;
	}

	uint16_t maxNodes() const override
	{
		return m_nodeCount;
	}

	const DmxNodeData& getNodeData(devnode_id_t nodeId) const
	{
		assert(nodeId < m_nodeCount);
		return m_nodeData[nodeId];
	}

protected:
	ioerror_t init(JsonObjectConst config);
	/** @brief controller calls this before performing an update,
	 *  typically for effects processing.
	 *  Return true if value changed.
	 */
	bool update();
	ioerror_t execute(DMX512Request& request);

private:
	uint16_t m_address = 0x01; ///< Start address for this device, may occupy more than one slot
	uint8_t m_nodeCount = 1; ///< Number of DMX slots managed by this device
	DmxNodeData* m_nodeData = nullptr; ///< Data for each slot, starting at address
};

class HardwareSerial;

class DMX512Controller: public IOController
{
public:
	DMX512Controller(uint8_t instance) :
		IOController(instance)
	{
	}

	String classname() override
	{
		return DMX512_CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

	bool busy() const override
	{
		return m_updating;
	}

private:
	void execute(IORequest& request) override;
	void updateSlaves();
	void transmitComplete(HardwareSerial& serial);

	bool m_updating = false; ///< Currently sending update
	bool m_changed = false; ///< Data has changed
	SimpleTimer m_timer; ///< For slave update cycle timing
};
