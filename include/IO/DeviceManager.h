/**
 * DeviceManager.h
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

#pragma once

#include <WString.h>
#include "Controller.h"
#include "Request.h"
#include <ArduinoJson.h>

namespace IO
{
class Request;

class DeviceManager
{
public:
	/**
	 * @brief Controllers register themselves so they can be located
	 * @note we don't own the controller; these are typically static objects
	 */
	void registerController(Controller& controller);

	/**
	 * @brief Device classes call this to register themselves
	 */
	void registerDeviceClass(const Device::Factory& devclass);

	Controller* findController(const String& id)
	{
		return std::find(controllers.begin(), controllers.end(), id);
	}

	/**
	 * @brief Load device config and create device tree.
	 */
	ErrorCode begin(JsonObjectConst config);

	ErrorCode end();

	void start();

	ErrorCode stop();

	bool canStop() const;

	Device* findDevice(const String& id);

	ErrorCode createRequest(const String& devid, Request*& request);

	/*
	 * Generally, requests should fit into the general model so that IO::Request can be used.
	 * If more custom behaviour is required then this method can be used to up-cast to the appropriate
	 * Request object.
	 * NOTE: At present there is no type checking on this, so use care.
	 */
	template <class RequestClass> ErrorCode createRequest(const String& devid, RequestClass*& request)
	{
		Request* req;
		auto err = createRequest(devid, req);
		if(!err) {
			request = static_cast<RequestClass*>(req);
		} else {
			debug_w("createRequest('%s'): %s", devid.c_str(), Error::toString(err).c_str());
		}
		return err;
	}

	/**
	 * @brief set the callback handler function for all I/O requests
	 * @note Callback invoked twice; once when executed, then again when completed.
	 */
	void setCallback(Request::Callback callback)
	{
		requestCallback = callback;
	}

	/**
	 * @brief invoke the callback, if one is registered
	 * @note called by Controller
	 */
	void invokeCallback(Request& request)
	{
		if(requestCallback) {
			requestCallback(request);
		}
	}

	ErrorCode handleMessage(JsonObject json, Request::Callback callback);

private:
	Controller::List controllers; ///< We don't own the controllers
	Request::Callback requestCallback;
};

extern DeviceManager devmgr;

} // namespace IO
