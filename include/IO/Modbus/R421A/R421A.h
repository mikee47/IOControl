/*
 * R421A.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#pragma once

#include "../Modbus.h"

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

namespace R421A
{
struct r421_response_t {
	// Response data
	uint32_t channelMask;
	uint32_t channelStates;
};

class Device;

class Request : public Modbus::Request
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
	Request(Modbus::Device& device) : Modbus::Request(device)
	{
		m_data = {};
		m_response = {};
	}

	IO::Error parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	const Device& device() const
	{
		return reinterpret_cast<Device&>(m_device);
	}

	bool setNode(IO::DevNode node) override;

	bool nodeLatch(IO::DevNode node)
	{
		setCommand(IO::Command::latch);
		return setNode(node);
	}

	bool nodeMomentary(IO::DevNode node)
	{
		setCommand(IO::Command::momentary);
		return setNode(node);
	}

	bool nodeDelay(IO::DevNode node, uint8_t secs)
	{
		setCommand(IO::Command::delay);
		m_data.delay = secs;
		return setNode(node);
	}

	IO::devnode_state_t getNodeState(IO::DevNode node) override;

	bool setNodeState(IO::DevNode node, IO::devnode_state_t state) override
	{
		if(state == IO::state_on) {
			nodeOn(node);
		} else if(state == IO::state_off) {
			nodeOff(node);
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

class Device : public Modbus::Device
{
private:
	// Tracks current output states as far as possible
	r421_response_t m_states = {0, 0};
	// Depends on device variant (e.g. 8, 4)
	uint8_t m_channelCount;

protected:
	IO::Error init(JsonObjectConst config);

	// We use this to track states
	void requestComplete(IO::Request& request);

public:
	Device(Modbus::Controller& controller) : Modbus::Device(controller)
	{
	}

	IO::Request* createRequest() override
	{
		return new Request(*this);
	}

	const r421_response_t& states() const
	{
		return m_states;
	}

	IO::DevNode nodeIdMin() const override
	{
		return R421_CHANNEL_MIN;
	}

	IO::DevNode nodeIdMax() const override
	{
		return R421_CHANNEL_MIN + m_channelCount - 1;
	}

	uint16_t maxNodes() const override
	{
		return m_channelCount;
	}

	bool isValid(IO::DevNode node) const
	{
		return node >= nodeIdMin() && node <= nodeIdMax();
	}

	IO::devnode_state_t getNodeState(IO::DevNode node) const override;

	static const IO::DeviceClassInfo deviceClass();
};

} // namespace R421A
