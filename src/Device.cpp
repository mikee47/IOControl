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
	if(!id) {
		return Error::no_device_id;
	}

	name = config.name;
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
	if(state == State::normal || state == State::starting) {
		return Error::success;
	}

	auto req = createRequest();
	if(req == nullptr) {
		return Error::no_mem;
	}

	// This fails if device doesn't have any nodes
	if(!req->nodeQuery(DevNode_ALL)) {
		delete req;
		state = State::normal;
		return Error::success;
	}

	req->setID(F("query"));
	req->submit();
	state = State::starting;

	return Error::success;
}

/*
 * Inherited classes might override this method to place device in a low-power state.
 */
ErrorCode Device::stop()
{
	state = State::stopped;
	return Error::success;
}

void Device::submit(Request* request)
{
	controller.submit(request);
}

void Device::handleEvent(Request* request, Event event)
{
	if(event == Event::RequestComplete) {
		if(request->error()) {
			state = State::fault;
			controller.deviceError(*this);
		} else if(state == State::starting) {
			state = State::normal;
		} else if(state == State::fault) {
			state = State::normal;
		}
	}

	controller.handleEvent(request, event);
}

String Device::caption() const
{
	String s;
	s += controller.getId().c_str();
	s += '/';
	s += id.c_str();
	return s;
}

} // namespace IO
