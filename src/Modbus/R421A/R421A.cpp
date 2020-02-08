/*
 * R421A.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include <IO/Modbus/R421A/R421A.h>

#define MB_RELAY_ON 0x0100
#define MB_RELAY_OFF 0x0200

namespace IO
{
namespace Modbus
{
namespace R421A
{
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
	relay_closed = 0x0001,
};

#define ATTR_CHANNELS _F("channels")
DEFINE_FSTR_LOCAL(DEVICE_NAME, "r421a")

/* CModbusRequestR421A */

static r421a_command_t map(IO::Command cmd)
{
	switch(cmd) {
	case IO::Command::query:
		return r421_query;
	case IO::Command::on:
		return r421_close;
	case IO::Command::off:
		return r421_open;
	case IO::Command::toggle:
		return r421_toggle;
	case IO::Command::latch:
		return r421_latch;
	case IO::Command::momentary:
		return r421_momentary;
	case IO::Command::delay:
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
void Request::fillRequestData(Transaction& mbt)
{
	if(m_command == IO::Command::query) {
		// Query all channels
		mbt.function = Function::ReadHoldingRegisters;
		mbt.data[0] = 0x00;
		mbt.data[1] = device().nodeIdMin();
		mbt.data[2] = 0x00;
		mbt.data[3] = device().maxNodes();
		mbt.dataSize = 4;
		return;
	}

	// others
	for(unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
		if(bitRead(m_data.channelMask, ch)) {
			mbt.function = Function::WriteSingleRegister;
			mbt.data[0] = 0x00;
			mbt.data[1] = ch;
			mbt.data[2] = map(command());
			mbt.data[3] = (m_command == IO::Command::delay) ? m_data.delay : 0x00;
			mbt.dataSize = 4;

			//      debug_i("fillRequestData() - channel %u, cmd %u", ch, m_cmd);

			return;
		}

	// Send an invalid instruction
	debug_e("fillRequestData() - UNEXPECTED");
	mbt.function = Function::None;
	mbt.dataSize = 0;
}

void Request::callback(Transaction& mbt)
{
	if(!checkStatus(mbt)) {
		complete(IO::Status::error);
		return;
	}

	if(m_command == IO::Command::query) {
		//    assert (mbt.function == MB_ReadHoldingRegisters);
		// data[0] is response size, in bytes: should correspond with mbt.dataSize + 1
		for(unsigned i = 1; i < mbt.dataSize; i += sizeof(uint16_t)) {
			uint8_t ch = device().nodeIdMin() + (i / sizeof(uint16_t));
			uint16_t val = makeWord(&mbt.data[i]);
			if(val != relay_open && val != relay_closed)
				continue; // Erroneous response - ignore

			uint32_t bitmask = _BV(ch);
			m_response.channelMask |= bitmask;
			if(val == relay_closed)
				m_response.channelStates |= bitmask;
		}
		complete(IO::Status::success);
		return;
	}

	// Other commands
	uint8_t channel = mbt.data[1];
	// We've handled this channel, clear command mask bit
	bitClear(m_data.channelMask, channel);
	//  debug_i("Channel = %u, mask = %08x", channel, m_data.channelMask);
	// Report back which bits were affected
	bitSet(m_response.channelMask, channel);
	if(m_command == IO::Command::toggle) {
		// Use current states held by IODevice to determine effect of toggle command
		bool cur = bitRead(device().states().channelStates, channel);
		bitWrite(m_response.channelStates, channel, !cur);
	} else if(m_command == IO::Command::on)
		bitSet(m_response.channelStates, channel);

	// Finish if we've dealt with all the channels, otherwise execute the next one
	if(m_data.channelMask == 0)
		complete(IO::Status::success);
	else
		submit();
}

bool Request::setNode(IO::DevNode node)
{
	if(node == IO::NODES_ALL) {
		for(unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
			m_data.channelMask |= _BV(ch);
		return true;
	}

	if(!device().isValid(node))
		return false;

	m_data.channelMask |= _BV(node);
	return true;
}

IO::devnode_state_t Request::getNodeState(IO::DevNode node)
{
	IO::devnode_state_t ret = IO::state_none;
	if(node == IO::NODES_ALL) {
		uint32_t mask = (status() == IO::Status::pending) ? m_data.channelMask : m_response.channelMask;
		for(unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
			if(bitRead(mask, ch))
				ret |= device().getNodeState(ch);
		return ret;
	} else
		ret = device().getNodeState(node);

	return ret;

	/*
	 if (bitRead(m_response.channelMask, node))
	 return bitRead(m_response.channelStates, node) ? state_on : state_off;
	 else
	 return state_unknown;
	 */
}

IO::Error Request::parseJson(JsonObjectConst json)
{
	IO::Error err = Modbus::Request::parseJson(json);
	if(!err) {
		unsigned delay = json[IO::ATTR_DELAY];
		if(delay <= 255)
			m_data.delay = delay;
		else
			err = IO::Error::bad_param;
	}

	return err;
}

void Request::getJson(JsonObject json) const
{
	Modbus::Request::getJson(json);

	uint32_t mask = (m_status == IO::Status::pending) ? m_data.channelMask : m_response.channelMask;

	JsonArray nodes = json.createNestedArray(IO::ATTR_NODES);
	for(unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
		if(bitRead(mask, ch))
			nodes.add(ch);

	if(m_response.channelMask) {
		JsonArray states = json.createNestedArray(Modbus::ATTR_STATES);

		debug_d("Channel mask = 0x%08x, states = 0x%08x", m_response.channelMask, m_response.channelStates);
		for(unsigned ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch) {
			if(bitRead(m_response.channelMask, ch)) {
				states.add(bitRead(m_response.channelStates, ch));
			}
		}
	}
}

/* CModbusR421ADevice */

IO::Error Device::init(const Config& config)
{
	IO::Error err = Modbus::Device::init(config);
	if(!!err) {
		return err;
	}

	m_channelCount = std::min(config.channels, R421A_MAX_CHANNELS);

	debug_d("Device %s has %u channels", id().c_str(), m_channelCount);

	return IO::Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	Modbus::Device::parseJson(json, cfg);
	cfg.channels = json[ATTR_CHANNELS].as<unsigned>();
}

IO::Error Device::init(JsonObjectConst json)
{
	Config cfg{};
	parseJson(json, cfg);
	return init(cfg);
}

static IO::Error createDevice(IO::Controller& controller, IO::Device*& device)
{
	if(!controller.verifyClass(Modbus::CONTROLLER_CLASSNAME)) {
		return IO::Error::bad_controller_class;
	}

	device = new Device(reinterpret_cast<Modbus::Controller&>(controller));
	return device ? IO::Error::success : IO::Error::nomem;
}

const IO::DeviceClassInfo Device::deviceClass()
{
	return {DEVICE_NAME, createDevice};
}

void Device::requestComplete(IO::Request& request)
{
	/*
	 * Keep track of channel states
	 */
	if(request.status() == IO::Status::success) {
		auto& req = reinterpret_cast<Request&>(request);
		auto& rsp = req.response();
		m_states.channelMask |= rsp.channelMask;
		m_states.channelStates &= ~rsp.channelMask;
		m_states.channelStates |= (rsp.channelStates & rsp.channelMask);
	}

	Modbus::Device::requestComplete(request);
}

IO::devnode_state_t Device::getNodeState(IO::DevNode node) const
{
	if(node == IO::NODES_ALL) {
		IO::devnode_state_t ret = IO::state_none;
		for(unsigned ch = nodeIdMin(); ch <= nodeIdMax(); ++ch)
			if(!bitRead(m_states.channelMask, ch)) {
				ret |= IO::state_unknown;
			} else if(bitRead(m_states.channelStates, ch)) {
				ret |= IO::state_on;
			} else {
				ret |= IO::state_off;
			}
		return ret;
	}

	if(!isValid(node)) {
		return IO::state_unknown;
	}
	if(!bitRead(m_states.channelMask, node)) {
		return IO::state_unknown;
	}
	return bitRead(m_states.channelStates, node) ? IO::state_on : IO::state_off;
}

} // namespace R421A
} // namespace Modbus
} // namespace IO
