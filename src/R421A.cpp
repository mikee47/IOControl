/*
 * R421A.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include "R421A.h"

#define MB_RELAY_ON     0x0100
#define MB_RELAY_OFF    0x0200

enum __attribute__((packed)) r421a_command_t {
	r421_query = 0x00,
	r421_close = 0x01,
	r421_open = 0x02,
	r421_toggle = 0x03,
	r421_latch = 0x04,
	r421_momentary = 0x05,
	r421_delay = 0x06,
	//
	r421_on = r421_close,
	r421_off = r421_open,
	r421_undefined = 0xFF,
	r421_send = r421_undefined
};

enum __attribute__((packed)) a421_relay_state {
	relay_open = 0x0000,
	relay_closed = 0x0001
};

#define ATTR_CHANNELS _F("channels")
DEFINE_FSTR_LOCAL(DEVICE_NAME, "r421a")

/* CModbusRequestR421A */

static r421a_command_t map(io_command_t cmd)
{
	switch (cmd) {
	case ioc_query:
		return r421_query;
	case ioc_on:
		return r421_close;
	case ioc_off:
		return r421_open;
	case ioc_toggle:
		return r421_toggle;
	case ioc_latch:
		return r421_latch;
	case ioc_momentary:
		return r421_momentary;
	case ioc_delay:
		return r421_delay;
	default:
		return r421_undefined;
	};
}

/*
 * We specify channels as a bitmask or an array. Arrays are clearer for
 * JSON/scripting but less efficient. Bitmask is more concise. We'll
 * use arrays externally and bitmasks internally. Thus:
 *  [1, 2, 3, 7, 8] == 0b11000111 == 0xC7
 * We'll use 32 bits for this.
 */
void ModbusRequestR421A::fillRequestData(ModbusTransaction& mbt)
{
	if (m_command == ioc_query) {
		// Query all channels
		mbt.function = MB_ReadHoldingRegisters;
		mbt.data[0] = 0x00;
		mbt.data[1] = device().nodeIdMin();
		mbt.data[2] = 0x00;
		mbt.data[3] = device().maxNodes();
		mbt.dataSize = 4;
		return;
	}

	// others
	for (unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
		if (bitRead(m_data.channelMask, ch)) {
			mbt.function = MB_WriteSingleRegister;
			mbt.data[0] = 0x00;
			mbt.data[1] = ch;
			mbt.data[2] = map(command());
			mbt.data[3] = (m_command == ioc_delay) ? m_data.delay : 0x00;
			mbt.dataSize = 4;

//      debug_i("fillRequestData() - channel %u, cmd %u", ch, m_cmd);

			return;
		}

	// Send an invalid instruction
	debug_e("fillRequestData() - UNEXPECTED");
	mbt.function = MB_None;
	mbt.dataSize = 0;
}

void ModbusRequestR421A::callback(const ModbusTransaction& mbt)
{
	if (!checkStatus(mbt)) {
		complete(status_error);
		return;
	}

	if (m_command == ioc_query) {
//    assert (mbt.function == MB_ReadHoldingRegisters);
		// data[0] is response size, in bytes: should correspond with mbt.dataSize + 1
		for (unsigned i = 1; i < mbt.dataSize; i += sizeof(uint16_t)) {
			uint8_t ch = device().nodeIdMin() + (i / sizeof(uint16_t));
			uint16_t val = modbus_makeword(&mbt.data[i]);
			if (val != relay_open && val != relay_closed)
				continue;  // Erroneous response - ignore

			uint32_t bitmask = _BV(ch);
			m_response.channelMask |= bitmask;
			if (val == relay_closed)
				m_response.channelStates |= bitmask;
		}
		complete(status_success);
		return;
	}

	// Other commands
	uint8_t channel = mbt.data[1];
	// We've handled this channel, clear command mask bit
	bitClear(m_data.channelMask, channel);
//  debug_i("Channel = %u, mask = %08X", channel, m_data.channelMask);
	// Report back which bits were affected
	bitSet(m_response.channelMask, channel);
	if (m_command == ioc_toggle) {
		// Use current states held by IODevice to determine effect of toggle command
		bool cur = bitRead(device().states().channelStates, channel);
		bitWrite(m_response.channelStates, channel, !cur);
	}
	else if (m_command == ioc_on)
		bitSet(m_response.channelStates, channel);

	// Finish if we've dealt with all the channels, otherwise execute the next one
	if (m_data.channelMask == 0)
		complete(status_success);
	else
		submit();
}

bool ModbusRequestR421A::setNode(devnode_id_t nodeId)
{
	if (nodeId == NODES_ALL) {
		for (unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
			m_data.channelMask |= _BV(ch);
		return true;
	}

	if (!device().nodeValid(nodeId))
		return false;

	m_data.channelMask |= _BV(nodeId);
	return true;
}

devnode_state_t ModbusRequestR421A::getNodeState(devnode_id_t nodeId)
{
	devnode_state_t ret = state_none;
	if (nodeId == NODES_ALL) {
		uint32_t mask = (status() == status_pending) ? m_data.channelMask : m_response.channelMask;
		for (unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
			if (bitRead(mask, ch))
				ret |= device().getNodeState(ch);
		return ret;
	}
	else
		ret = device().getNodeState(nodeId);

	return ret;

	/*
	 if (bitRead(m_response.channelMask, nodeId))
	 return bitRead(m_response.channelStates, nodeId) ? state_on : state_off;
	 else
	 return state_unknown;
	 */
}

ioerror_t ModbusRequestR421A::parseJson(JsonObjectConst json)
{
	ioerror_t err = ModbusRequest::parseJson(json);
	if (!err) {
		unsigned delay = json[ATTR_DELAY];
		if (delay <= 255)
			m_data.delay = delay;
		else
			err = ioe_bad_param;
	}

	return err;
}

void ModbusRequestR421A::getJson(JsonObject json) const
									{
	ModbusRequest::getJson(json);

	uint32_t mask = (m_status == status_pending) ? m_data.channelMask : m_response.channelMask;

	JsonArray nodes = json.createNestedArray(ATTR_NODES);
	for (unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
		if (bitRead(mask, ch))
			nodes.add(ch);

	if (m_response.channelMask) {
		JsonArray states = json.createNestedArray(ATTR_STATES);

		debug_d("Channel mask = %08X, states = %08X", m_response.channelMask, m_response.channelStates);
		for (unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch) {
			if (bitRead(m_response.channelMask, ch)) {
				states.add(bitRead(m_response.channelStates, ch));
			}
		}
	}
}

/* CModbusR421ADevice */

ioerror_t ModbusR421ADevice::init(JsonObjectConst config)
{
	ioerror_t err = ModbusDevice::init(config);
	if (err) {
		return err;
	}

	m_channelCount = std::min(config[ATTR_CHANNELS].as<int>(), R421A_MAX_CHANNELS);

	debug_d("Device %s has %u channels", m_id.c_str(), m_channelCount);

	return ioe_success;
}

static ioerror_t createDevice(IOController& controller, IODevice*& device)
{
	if (!controller.verifyClass(MODBUS_CONTROLLER_CLASSNAME))
		return ioe_bad_controller_class;

	device = new ModbusR421ADevice(reinterpret_cast<ModbusController&>(controller));
	return device ? ioe_success : ioe_nomem;
}

const device_class_info_t ModbusR421ADevice::deviceClass()
{
	return {DEVICE_NAME, createDevice};
}

void ModbusR421ADevice::requestComplete(IORequest& request)
{
	/*
	 * Keep track of channel states
	 */
	if (request.status() == status_success) {
		auto& req = reinterpret_cast<ModbusRequestR421A&>(request);
		r421_response_t& rsp = req.response();
		m_states.channelMask |= rsp.channelMask;
		m_states.channelStates &= ~rsp.channelMask;
		m_states.channelStates |= (rsp.channelStates & rsp.channelMask);
	}

	ModbusDevice::requestComplete(request);
}

devnode_state_t ModbusR421ADevice::getNodeState(devnode_id_t nodeId) const
													{
	if (nodeId == NODES_ALL) {
		devnode_state_t ret = state_none;
		for (unsigned ch = nodeIdMin(); ch <= nodeIdMax(); ++ch)
			if (!bitRead(m_states.channelMask, ch))
				ret |= state_unknown;
			else if (bitRead(m_states.channelStates, ch))
				ret |= state_on;
			else
				ret |= state_off;

		return ret;
	}

	if (!nodeValid(nodeId))
		return state_unknown;
	if (!bitRead(m_states.channelMask, nodeId))
		return state_unknown;
	return bitRead(m_states.channelStates, nodeId) ? state_on : state_off;
}

