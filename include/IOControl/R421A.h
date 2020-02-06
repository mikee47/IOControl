/*
 * R421A.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#pragma once

#include "IOModbus.h"

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

/*
 * R421 devices don't respond to channel numbers greater than 16.
 */
#define R421A_MAX_CHANNELS 16

// Channels start with 1
#define R421_CHANNEL_MIN 1

struct r421_response_t {
	// Response data
	uint32_t channelMask;
	uint32_t channelStates;
};

class ModbusR421ADevice;

class ModbusRequestR421A : public ModbusRequest
{
private:
	// Associated command data
	struct {
		uint32_t channelMask;
		uint8_t delay;
	} m_data = {0, 0};

	r421_response_t m_response = {0, 0};

protected:
	void fillRequestData(ModbusTransaction& mbt);
	void callback(const ModbusTransaction& mbt);

public:
	ModbusRequestR421A(ModbusDevice& device) : ModbusRequest(device)
	{
		m_data = {};
		m_response = {};
	}

	ioerror_t parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	const ModbusR421ADevice& device() const
	{
		return reinterpret_cast<ModbusR421ADevice&>(m_device);
	}

	bool setNode(devnode_id_t nodeId) override;

	bool nodeLatch(devnode_id_t nodeId)
	{
		setCommand(ioc_latch);
		return setNode(nodeId);
	}

	bool nodeMomentary(devnode_id_t nodeId)
	{
		setCommand(ioc_momentary);
		return setNode(nodeId);
	}

	bool nodeDelay(devnode_id_t nodeId, uint8_t secs)
	{
		setCommand(ioc_delay);
		m_data.delay = secs;
		return setNode(nodeId);
	}

	devnode_state_t getNodeState(devnode_id_t nodeId) override;

	bool setNodeState(devnode_id_t nodeId, devnode_state_t state) override
	{
		if(state == state_on) {
			nodeOn(nodeId);
		} else if(state == state_off) {
			nodeOff(nodeId);
		} else {
			return false;
		}
		return true;
	}

	r421_response_t& response()
	{
		return m_response;
	}
};

class ModbusR421ADevice : public ModbusDevice
{
private:
	// Tracks current output states as far as possible
	r421_response_t m_states = {0, 0};
	// Depends on device variant (e.g. 8, 4)
	uint8_t m_channelCount;

protected:
	ioerror_t init(JsonObjectConst config);

	// We use this to track states
	void requestComplete(IORequest& request);

public:
	ModbusR421ADevice(ModbusController& controller) : ModbusDevice(controller)
	{
	}

	IORequest* createRequest() override
	{
		return new ModbusRequestR421A(*this);
	}

	const r421_response_t& states() const
	{
		return m_states;
	}

	devnode_id_t nodeIdMin() const override
	{
		return R421_CHANNEL_MIN;
	}

	devnode_id_t nodeIdMax() const override
	{
		return R421_CHANNEL_MIN + m_channelCount - 1;
	}

	uint16_t maxNodes() const override
	{
		return m_channelCount;
	}

	bool nodeValid(devnode_id_t nodeId) const
	{
		return nodeId >= nodeIdMin() && nodeId <= nodeIdMax();
	}

	devnode_state_t getNodeState(devnode_id_t nodeId) const override;

	static const device_class_info_t deviceClass();
};
