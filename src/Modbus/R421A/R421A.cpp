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
DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "r421a")
DEFINE_FSTR_LOCAL(ATTR_CHANNELS, "channels")

enum r421a_command_t {
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

enum a421_relay_state_t {
	relay_open = 0x0000,
	relay_closed = 0x0001,
};

/* Request */

static r421a_command_t map(Command cmd)
{
	switch(cmd) {
	case Command::query:
		return r421_query;
	case Command::on:
		return r421_close;
	case Command::off:
		return r421_open;
	case Command::toggle:
		return r421_toggle;
	case Command::latch:
		return r421_latch;
	case Command::momentary:
		return r421_momentary;
	case Command::delay:
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
	if(m_command == Command::query) {
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
	for(auto ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch)
		if(m_data.channelMask[ch]) {
			mbt.function = Function::WriteSingleRegister;
			mbt.data[0] = 0x00;
			mbt.data[1] = ch;
			mbt.data[2] = map(command());
			mbt.data[3] = (m_command == Command::delay) ? m_data.delay : 0x00;
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
		complete(Status::error);
		return;
	}

	if(m_command == Command::query) {
		//    assert (mbt.function == MB_ReadHoldingRegisters);
		// data[0] is response size, in bytes: should correspond with mbt.dataSize + 1
		for(unsigned i = 1; i < mbt.dataSize; i += sizeof(uint16_t)) {
			uint8_t ch = device().nodeIdMin() + (i / sizeof(uint16_t));
			uint16_t val = makeWord(&mbt.data[i]);
			if(val != relay_open && val != relay_closed) {
				continue; // Erroneous response - ignore
			}

			m_response.channelMask + ch;
			m_response.channelStates[ch] = (val == relay_closed);
		}
		complete(Status::success);
		return;
	}

	// Other commands
	uint8_t ch = mbt.data[1];
	// We've handled this channel, clear command mask bit
	m_data.channelMask[ch] = false;
	//  debug_i("Channel = %u, mask = %08x", channel, m_data.channelMask);
	// Report back which bits were affected
	m_response.channelMask += ch;
	if(m_command == Command::toggle) {
		// Use current states held by IODevice to determine effect of toggle command
		m_response.channelStates[ch] = !device().states().channelStates[ch];
	} else if(m_command == Command::on) {
		m_response.channelStates += ch;
	}

	// Submit request for next channel, if any, otherwise we're done
	if(m_data.channelMask.any()) {
		submit();
	} else {
		complete(Status::success);
	}
}

bool Request::setNode(DevNode node)
{
	if(node == DevNode_ALL) {
		for(auto ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch) {
			m_data.channelMask += ch;
		}
		return true;
	}

	if(!device().isValid(node)) {
		return false;
	}

	m_data.channelMask += node.id;
	return true;
}

DevNode::States Request::getNodeStates(DevNode node)
{
	DevNode::States states;
	if(node == DevNode_ALL) {
		auto mask = (status() == Status::pending) ? m_data.channelMask : m_response.channelMask;
		for(auto ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch) {
			if(mask[ch]) {
				states += device().getNodeStates(DevNode{ch});
			}
		}
	} else {
		states = device().getNodeStates(node);
	}

	return states;
}

Error Request::parseJson(JsonObjectConst json)
{
	Error err = Modbus::Request::parseJson(json);
	if(!err) {
		unsigned delay = json[ATTR_DELAY];
		if(delay <= 255) {
			m_data.delay = delay;
		} else {
			err = Error::bad_param;
		}
	}

	return err;
}

void Request::getJson(JsonObject json) const
{
	Modbus::Request::getJson(json);

	auto mask = (m_status == Status::pending) ? m_data.channelMask : m_response.channelMask;

	JsonArray nodes = json.createNestedArray(ATTR_NODES);
	for(auto ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch) {
		if(mask[ch]) {
			nodes.add(ch);
		}
	}

	if(m_response.channelMask.any()) {
		JsonArray states = json.createNestedArray(Modbus::ATTR_STATES);

		debug_d("Channel mask = 0x%08x, states = 0x%08x", m_response.channelMask, m_response.channelStates);
		for(auto ch = device().nodeIdMin(); ch <= device().nodeIdMax(); ++ch) {
			if(m_response.channelMask[ch]) {
				states.add(m_response.channelStates[ch]);
			}
		}
	}
}

/* CModbusR421ADevice */

Error Device::init(const Config& config)
{
	Error err = Modbus::Device::init(config);
	if(!!err) {
		return err;
	}

	m_channelCount = std::min(config.channels, R421A_MAX_CHANNELS);

	debug_d("Device %s has %u channels", id().c_str(), m_channelCount);

	return Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	Modbus::Device::parseJson(json, cfg);
	cfg.channels = json[ATTR_CHANNELS].as<unsigned>();
}

Error Device::init(JsonObjectConst json)
{
	Config cfg{};
	parseJson(json, cfg);
	return init(cfg);
}

static Error createDevice(IO::Controller& controller, IO::Device*& device)
{
	if(!controller.verifyClass(Modbus::CONTROLLER_CLASSNAME)) {
		return Error::bad_controller_class;
	}

	device = new Device(reinterpret_cast<Modbus::Controller&>(controller));
	return device ? Error::success : Error::nomem;
}

const DeviceClassInfo deviceClass()
{
	return {DEVICE_CLASSNAME, createDevice};
}

void Device::requestComplete(IO::Request& request)
{
	/*
	 * Keep track of channel states
	 */
	if(request.status() == Status::success) {
		auto& req = reinterpret_cast<Request&>(request);
		auto& rsp = req.response();
		m_states.channelMask += rsp.channelMask;
		m_states.channelStates -= rsp.channelMask;
		m_states.channelStates += rsp.channelStates;
	}

	Modbus::Device::requestComplete(request);
}

DevNode::States Device::getNodeStates(DevNode node) const
{
	if(node == DevNode_ALL) {
		DevNode::States states;
		for(unsigned ch = nodeIdMin(); ch <= nodeIdMax(); ++ch) {
			if(!m_states.channelMask[ch]) {
				states += DevNode::State::unknown;
			} else if(m_states.channelStates[ch]) {
				states += DevNode::State::on;
			} else {
				states += DevNode::State::off;
			}
		}
		return states;
	}

	if(!isValid(node)) {
		return DevNode::State::unknown;
	}

	if(!m_states.channelMask[node.id]) {
		return DevNode::State::unknown;
	}

	return m_states.channelStates[node.id] ? DevNode::State::on : DevNode::State::off;
}

} // namespace R421A
} // namespace Modbus
} // namespace IO
