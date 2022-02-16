/**
 * Device.cpp
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

#include <IO/Device.h>
#include <IO/Request.h>
#include <IO/Controller.h>
#include <IO/Strings.h>

namespace IO
{
ErrorCode Device::init(const Config& config)
{
	if(!m_id) {
		return Error::no_device_id;
	}

	m_name = config.name;
	return Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	cfg.name = json[FS_name].as<const char*>();
}

/*
 * Perform device-specific startup operations.
 * By default we fetch device state.
 *
 * This method will be called periodically by controller if
 * uninitialised or faultly. Note that stop() is NOT called
 * first. That is only called if, for example, controller is being restarted
 * or configuration reloaded.
 */
ErrorCode Device::start()
{
	if(m_state == devstate_normal || m_state == devstate_starting) {
		return Error::success;
	}

	auto req = createRequest();
	if(req == nullptr) {
		return Error::no_mem;
	}

	// This fails if device doesn't have any nodes
	if(!req->nodeQuery(DevNode_ALL)) {
		delete req;
		m_state = devstate_normal;
		return Error::success;
	}

	req->setID(F("query"));
	req->submit();
	m_state = devstate_starting;

	return Error::success;
}

/*
 * Inherited classes might override this method to place device in a low-power state.
 */
ErrorCode Device::stop()
{
	return Error::success;
}

void Device::submit(Request* request)
{
	m_controller.submit(request);
}

void Device::handleEvent(Request* request, Event event)
{
	if(event == Event::RequestComplete) {
		if(request->error()) {
			m_state = devstate_fault;
			m_controller.deviceError(*this);
		} else if(m_state == devstate_starting) {
			m_state = devstate_normal;
		}
	}

	m_controller.handleEvent(request, event);
}

String Device::caption()
{
	String s;
	s += m_controller.id();
	s += '/';
	s += m_id.c_str();
	return s;
}

} // namespace IO
