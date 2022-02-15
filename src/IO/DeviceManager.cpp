/**
 * DeviceManager.cpp
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

#include <IO/DeviceManager.h>
#include <IO/Strings.h>

namespace IO
{
DeviceManager devmgr;

void DeviceManager::registerController(Controller& controller)
{
	m_controllers[controller.id()] = &controller;
	debug_i("Controller '%s' registered", controller.id().c_str());
}

ErrorCode DeviceManager::begin(JsonObjectConst config)
{
	auto err = end();
	if(err) {
		return err;
	}

	// Create devices
	JsonObjectConst devices = config[FS_devices];
	for(JsonPairConst dev : devices) {
		String ctrl = dev.value()[FS_controller];
		auto controller = findController(ctrl);
		if(controller == nullptr) {
			// Not a fatal error - keep going as other devices might be OK
			err = Error::bad_controller;
			debug_err(err, ctrl);
			continue;
		}

		Device* inst;
		controller->createDevice(dev.key().c_str(), dev.value(), inst);
	}

	start();

	return err;
}

void DeviceManager::start()
{
	// Start all controllers
	for(auto controller : m_controllers) {
		(*controller)->start();
	}
}

bool DeviceManager::canStop() const
{
	for(auto controller : m_controllers) {
		if(!(*controller)->canStop()) {
			return false;
		}
	}

	return true;
}

ErrorCode DeviceManager::stop()
{
	if(!canStop()) {
		return Error::busy;
	}

	// Stop all controllers
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		auto controller = m_controllers.valueAt(i);
		controller->stop();
	}

	return Error::success;
}

ErrorCode DeviceManager::end()
{
	auto err = stop();
	if(err) {
		return err;
	}

	// Destroy devices
	for(auto controller : m_controllers) {
		(*controller)->freeDevices();
	}

	return Error::success;
}

Device* DeviceManager::findDevice(const String& id) const
{
	for(auto controller : m_controllers) {
		auto device = (*controller)->findDevice(id);
		if(device != nullptr) {
			return device;
		}
	}

	debug_e("Device '%s' not registered", id.c_str());
	return nullptr;
}

ErrorCode DeviceManager::createRequest(const String& devid, Request*& request)
{
	if(!devid) {
		return Error::no_device_id;
	}

	auto dev = findDevice(devid);
	if(dev == nullptr) {
		return Error::bad_device;
	}

	request = dev->createRequest();
	return request ? Error::success : Error::no_mem;
}

/*
 * May contain multiple commands so in turn multiple requests may be queued.
 *
 * COMMAND
 * DEVICE
 * DEVICES[]
 * DEVNODES[]
 *
 */
ErrorCode DeviceManager::handleMessage(JsonObject json, Request::Callback callback)
{
	Request* req;
	ErrorCode err = Error::success;

	// Command group ?
	bool isDevnode = json.containsKey(FS_devnodes);
	if(isDevnode || json.containsKey(FS_devices)) {
		Command cmd = Command::undefined;
		const char* s;
		if(Json::getValue(json[FS_command], s) && !fromString(cmd, s)) {
			err = Error::bad_command;
			return setError(json, err, s);
		}

		JsonArray arr = json[isDevnode ? FS_devnodes : FS_devices];
		String requestId = json[FS_id];

		// Build requests then submit together
		Request::OwnedList queue;

		for(auto obj : arr) {
			if(isDevnode) {
				err = createRequest(obj[FS_device], req);
				if(err) {
					setError(obj, err);
					break;
				}

				req->setID(requestId);
				req->onComplete(callback);
				if(cmd != Command::undefined) {
					req->setCommand(cmd);
				}
				err = req->parseJson(obj);
				if(err) {
					delete req;
					setError(obj, err);
					break;
				}
			} else {
				err = createRequest(obj, req);
				if(err) {
					setError(json, err);
					break;
				}

				req->setID(requestId);
				req->onComplete(callback);
				req->setCommand(cmd);
				req->setNode(DevNode_ALL);
			}

			queue.add(req);
		}

		if(!err) {
			if(cmd == Command::toggle) {
				// Change request to an explicit on or off command
				// For toggle command, first node gives state
				DevNode::States states{};
				for(auto& req : queue) {
					states += req.getNodeStates(DevNode_ALL);
				}
				auto cmd = states[DevNode::State::on] ? Command::off : Command::on;
				for(auto& req : queue) {
					req.setCommand(cmd);
				}
			}
		}

		// Submit requests
		Request* req;
		while((req = queue.pop())) {
			req->submit();
		}
	} else {
		// Single request
		err = createRequest(json[FS_device], req);
		if(err) {
			return setError(json, err, nullptr, json[FS_device]);
		}

		req->onComplete(callback);
		err = req->parseJson(json);
		if(err) {
			delete req;
			return setError(json, err);
		}

		req->submit();
	}

	return err;
}

} // namespace IO
