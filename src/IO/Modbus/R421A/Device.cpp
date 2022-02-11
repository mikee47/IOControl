/**
 * Modbus/R421A/Device.cpp
 *
 *  Created on: 1 May 2018
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
 *
 * This file is part of the IOControl Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#include <IO/Modbus/R421A/Device.h>
#include <IO/Modbus/R421A/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
namespace R421A
{
namespace
{
DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "r421a")

ErrorCode createDevice(IO::Controller& controller, const char* id, IO::Device*& device)
{
	if(!controller.verifyClass(RS485::CONTROLLER_CLASSNAME)) {
		return Error::bad_controller_class;
	}

	device = new Device(reinterpret_cast<RS485::Controller&>(controller), id);
	return device ? Error::success : Error::no_mem;
}

} // namespace

const DeviceClassInfo deviceClass()
{
	return {DEVICE_CLASSNAME, createDevice};
}

ErrorCode Device::init(const Config& config)
{
	auto err = Modbus::Device::init(config.modbus);
	if(err) {
		return err;
	}

	m_channelCount = std::min(config.channels, R421A_MAX_CHANNELS);

	debug_d("Device %s has %u channels", id().c_str(), m_channelCount);

	return Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	Modbus::Device::parseJson(json, cfg.modbus);
	cfg.channels = json[FS_channels].as<unsigned>();
}

ErrorCode Device::init(JsonObjectConst json)
{
	Config cfg{};
	parseJson(json, cfg);
	return init(cfg);
}

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

void Device::handleEvent(IO::Request* request, Event event)
{
	if(event == Event::RequestComplete && !request->error()) {
		// Keep track of channel states
		auto& rsp = reinterpret_cast<Request*>(request)->response();
		m_states.channelMask += rsp.channelMask;
		m_states.channelStates -= rsp.channelMask;
		m_states.channelStates += rsp.channelStates;
	}

	IO::Modbus::Device::handleEvent(request, event);
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
