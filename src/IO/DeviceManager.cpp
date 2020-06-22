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
	JsonArrayConst devices = config[FS_devices];
	for(auto dev : devices) {
		String ctrl = dev[FS_controller];
		auto controller = findController(ctrl);
		if(controller == nullptr) {
			// Not a fatal error - keep going as other devices might be OK
			err = Error::bad_controller;
			debug_err(err, ctrl);
			continue;
		}

		controller->createDevice(dev);
	}

	start();

	return err;
}

void DeviceManager::start()
{
	// Start all controllers
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		m_controllers.valueAt(i)->start();
	}
}

bool DeviceManager::canStop()
{
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		if(!m_controllers.valueAt(i)->canStop()) {
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
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		auto controller = m_controllers.valueAt(i);
		controller->freeDevices();
	}

	return Error::success;
}

Device* DeviceManager::findDevice(const String& id)
{
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		auto controller = m_controllers.valueAt(i);
		auto device = controller->findDevice(id);
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
		if(arr.size() > IOCONTROL_MAX_REQUESTS) {
			return setError(json, Error::queue_full);
		}

		String requestId = json[FS_id];

		// Build requests then submit together
		Controller::RequestQueue queue;

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

			queue.enqueue(req);
		}

		if(!err) {
			if(cmd == Command::toggle) {
				// Change request to an explicit on or off command
				// For toggle command, first node gives state
				DevNode::States states{};
				for(unsigned i = 0; i < queue.count(); ++i) {
					states += queue[i]->getNodeStates(DevNode_ALL);
				}
				for(unsigned i = 0; i < queue.count(); ++i) {
					queue[i]->setCommand(states[DevNode::State::on] ? Command::off : Command::on);
				}
			}
		}

		// Submit requests, or delete them all if there was an error
		while(queue.count()) {
			req = queue.dequeue();
			if(!err) {
				err = req->submit();
			}
			if(err) {
				delete req;
			}
		}
	} else {
		// Single request
		err = createRequest(json[FS_device], req);
		if(err) {
			return setError(json, err, json[FS_device]);
		}

		req->onComplete(callback);
		err = req->parseJson(json);
		if(!err) {
			err = req->submit();
		}

		if(err) {
			delete req;
			return setError(json, err);
		}
	}

	return err;
}

} // namespace IO
