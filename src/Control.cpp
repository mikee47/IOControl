/*
 * IOControl.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include <IO/Control.h>

namespace IO
{
DeviceClassMap Controller::m_deviceClasses;

// Manages configurable device instances
DeviceManager devmgr;

// Global json tags
DEFINE_FSTR(ATTR_COMMAND, "command")
DEFINE_FSTR(ATTR_NAME, "name")
DEFINE_FSTR(ATTR_DEVICE, "device")
DEFINE_FSTR(ATTR_DEVICES, "devices")
DEFINE_FSTR(ATTR_ID, "id")
DEFINE_FSTR(ATTR_NODE, "node")
DEFINE_FSTR(ATTR_NODES, "nodes")
DEFINE_FSTR(ATTR_DEVNODES, "devnodes")
DEFINE_FSTR(ATTR_COUNT, "count")
DEFINE_FSTR(ATTR_DELAY, "delay")

// Config tags
DEFINE_FSTR_LOCAL(JS_DEVICES, "devices")
DEFINE_FSTR_LOCAL(ATTR_CONTROLLER, "controller")
DEFINE_FSTR_LOCAL(ATTR_CLASS, "class")

#define XX(tag, comment) #tag "\0"
DEFINE_FSTR_LOCAL(IOCOMMAND_STRINGS, IOCOMMAND_MAP(XX))
#undef XX

String toString(Command cmd)
{
	return CStringArray(IOCOMMAND_STRINGS)[unsigned(cmd)];
}

static bool strToIoCommand(const char* str, Command& cmd)
{
	CStringArray commandStrings(IOCOMMAND_STRINGS);
	auto i = commandStrings.indexOf(str);
	if(i < 0) {
		debug_w("Unknown IO command '%s'", str);
		return false;
	}

	cmd = Command(i);
	return true;
}

/* Request */

Error Request::parseJson(JsonObjectConst json)
{
	Json::getValue(json[ATTR_ID], m_id);

	// Command is optional - may have already been set
	const char* cmd;
	if(Json::getValue(json[ATTR_COMMAND], cmd) && !strToIoCommand(cmd, m_command)) {
		return Error::bad_command;
	}

	DevNode node;
	JsonArrayConst arr;
	if(Json::getValue(json[ATTR_NODE], node.id)) {
		unsigned count = json[ATTR_COUNT] | 1;
		while(count--) {
			if(!setNode(node)) {
				return Error::bad_node;
			}
			node.id++;
		}
	} else if(Json::getValue(json[ATTR_NODES], arr)) {
		for(DevNode::ID id : arr) {
			if(!setNode(DevNode{id})) {
				return Error::bad_node;
			}
		}
	}
	// all nodes
	else if(!setNode(DevNode_ALL)) {
		return Error::bad_node;
	}

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	if(m_id.length() != 0) {
		json[ATTR_ID] = m_id;
	}
	json[ATTR_COMMAND] = toString(m_command);
	json[ATTR_DEVICE] = m_device.id();
	setStatus(json, m_status);
}

Error Request::submit()
{
	return m_device.submit(*this);
}

/*
 * Request has completed. Device will destroy Notify originator then destroy this request.
 */
void Request::complete(Status status)
{
	debug_i("Request %p (%s) complete", this, m_id.c_str());
	m_status = status;
	m_device.requestComplete(*this);
}

String Request::caption()
{
	String s(uint32_t(this), HEX);
	s += " (";
	s += m_device.caption();
	s += '/';
	s += m_id;
	s += ')';
	return s;
}

/* Device */

Error Device::init(const Config& config)
{
	if(!config.id) {
		return Error::no_device_id;
	}

	m_id = config.id;
	m_name = config.name;
	return Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	cfg.id = json[ATTR_ID].as<const char*>();
	cfg.name = json[ATTR_NAME].as<const char*>();
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
Error Device::start()
{
	if(m_state == devstate_normal || m_state == devstate_starting) {
		return Error::success;
	}

	Request* req = createRequest();
	if(req == nullptr) {
		return Error::nomem;
	}

	// This fails if device doesn't have any nodes
	if(!req->nodeQuery(DevNode_ALL)) {
		delete req;
		m_state = devstate_normal;
		return Error::success;
	}

	Error err = req->submit();
	if(!!err) {
		delete req;
		return err;
	}

	m_state = devstate_starting;
	return Error::success;
}

/*
 * Inherited classes might override this method to place device in a low-power state.
 */
Error Device::stop()
{
	return Error::success;
}

void Device::requestComplete(Request& request)
{
	if(request.status() == Status::error) {
		m_state = devstate_fault;
		m_controller.deviceError(*this);
	} else if(request.status() == Status::success && m_state == devstate_starting) {
		m_state = devstate_normal;
	}
	m_controller.requestComplete(request);
}

String Device::caption()
{
	return m_controller.id() + '/' + m_id;
}

/* Controller */

/*
 * Before constructing a device instance, verify the controller class
 */
bool Controller::verifyClass(const String& classname)
{
	if(this->classname() == classname) {
		return true;
	}

	debug_e("verifyClass(%s) - wrong controller class, require '%s'", this->classname().c_str(), classname.c_str());
	return false;
}

Error Controller::createDevice(JsonObjectConst config)
{
	String cls = config[ATTR_CLASS];
	auto devclass = m_deviceClasses[cls];
	if(devclass == nullptr) {
		debug_e("Device class '%s' not registered", cls.c_str());
		return Error::bad_device_class;
	}
	auto create = devclass().constructor;
	assert(create);

	Device* device = nullptr;
	Error err = create(*this, device);
	if(!!err) {
		debug_err(err, cls);
		return err;
	}
	assert(device != nullptr);
	err = device->init(config);
	if(!!err) {
		delete device;
		debug_err(err, cls);
		return err;
	}

	m_devices.addElement(device);
	debug_d("Device %s created, class %s", device->caption().c_str(), cls.c_str());

	return err;
}

void Controller::freeDevices()
{
	unsigned i = m_devices.count();
	while(i--) {
		delete m_devices[i];
		m_devices.remove(i);
	}
}

Device* Controller::findDevice(const String& id)
{
	for(unsigned i = 0; i < m_devices.count(); ++i) {
		if(m_devices[i]->id() == id) {
			return m_devices[i];
		}
	}

	return nullptr;
}

/*
 * This timer is used for handling device re-starts so we don't need it to hog memory all
 * the time.
 */
void Controller::startTimer()
{
	PRINT_HEAP();

	if(m_deviceCheckTimer == nullptr) {
		m_deviceCheckTimer = new SimpleTimer();
		if(m_deviceCheckTimer == nullptr) {
			return;
		}

		m_deviceCheckTimer->initializeMs<DEVICECHECK_INTERVAL>(
			[](void* arg) { static_cast<Controller*>(arg)->startDevices(); }, this);
	}

	m_deviceCheckTimer->startOnce();
}

void Controller::stopTimer()
{
	delete m_deviceCheckTimer;
	m_deviceCheckTimer = nullptr;
}

/*
 * Inherited controllers call this after their own begin() code.
 * We schedule device initialisation here.
 */
void Controller::startDevices()
{
	debug_i("%s.startDevices()", id().c_str());

	PRINT_HEAP();

	stopTimer();
	unsigned failCount = 0;
	for(unsigned i = 0; i < m_devices.count(); ++i) {
		Device* device = m_devices[i];
		Error err = device->start();

		debug_i("%s->start(): %s", device->caption().c_str(), toString(err).c_str());

		PRINT_HEAP();

		if(!!err) {
			++failCount;
		}
	}

	// Use the timer to retry later
	if(failCount != 0) {
		startTimer();
	}
}

void Controller::stopDevices()
{
	stopTimer();
	for(unsigned i = 0; i < m_devices.count(); ++i) {
		m_devices[i]->stop();
	}
}

/*
 * An error occurred on a device. Schedule a restart operation.
 */
void Controller::deviceError(Device& device)
{
	startTimer();
}

Error Controller::submit(Request& request)
{
	if(request.command() == Command::undefined) {
		return Error::no_command;
	}

	/* Can re-submit a request instead of completing it to retry or progress
	 * a multi-IO call without having to create a new request object.
	 * So we only need to be in the queue once.
	 * Callback is invoked only at initial execution.
	 */
	// Same request might be submitted multiple times before completing
	if(m_queue.count() && m_queue.peek() == &request) {
		debug_d("Re-submitting request %s", request.caption().c_str());
		// Execute directly, don't invoke callback
		execute(request);
		return Error::success;
	}

	debug_d("Queueing request %s", request.caption().c_str());
	if(!m_queue.enqueue(&request)) {
		return Error::queue_full;
	}

	executeNext();
	return Error::success;
}

void Controller::requestComplete(Request& request)
{
	devmgr.callback(request);

	// Check queue is in sync
	assert(m_queue.peek() == &request);

	delete m_queue.dequeue();

	executeNext();
}

/*
 *
 */
void Controller::executeNext()
{
	// If we're busy, we'll get called again when current transaction has completed
	if(!busy() && m_queue.count()) {
		Request* req = m_queue.peek();
		debug_i("Executing request %p, %s: %s", req, req->id().c_str(), toString(req->command()).c_str());
		devmgr.callback(*req);
		execute(*req);
	}
}

/* DeviceManager */

void DeviceManager::registerController(Controller& controller)
{
	m_controllers[controller.id()] = &controller;
	debug_i("Controller '%s' registered", controller.id().c_str());
}

Error DeviceManager::begin(JsonObjectConst config)
{
	auto err = end();
	if(!!err) {
		return err;
	}

	// Create devices
	JsonArrayConst devices = config[JS_DEVICES];
	for(auto dev : devices) {
		String ctrl = dev[ATTR_CONTROLLER];
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

Error DeviceManager::stop()
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

Error DeviceManager::end()
{
	auto err = stop();
	if(!!err) {
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

Error DeviceManager::createRequest(const String& devid, Request*& request)
{
	if(!devid) {
		return Error::no_device_id;
	}

	auto dev = findDevice(devid);
	if(dev == nullptr) {
		return Error::bad_device;
	}

	request = dev->createRequest();
	return request ? Error::success : Error::nomem;
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
Error DeviceManager::handleMessage(JsonObject json, void* param)
{
	Request* req;
	Error err = Error::success;

	// Command group ?
	bool isDevnode = json.containsKey(ATTR_DEVNODES);
	if(isDevnode || json.containsKey(ATTR_DEVICES)) {
		Command cmd = Command::undefined;
		const char* s;
		if(Json::getValue(json[ATTR_COMMAND], s) && !strToIoCommand(s, cmd)) {
			err = Error::bad_command;
			return setError(json, err, s);
		}

		JsonArray arr = json[isDevnode ? ATTR_DEVNODES : ATTR_DEVICES];
		if(arr.size() > IOCONTROL_MAX_REQUESTS) {
			return setError(json, Error::queue_full);
		}

		String requestId = json[ATTR_ID];

		// Build requests then submit together
		Controller::RequestQueue queue;

		Error err = Error::success;
		for(auto obj : arr) {
			if(isDevnode) {
				err = createRequest(obj[ATTR_DEVICE], req);
				if(!!err) {
					setError(obj, err);
					break;
				}

				req->setID(requestId);
				req->setParam(param);
				if(cmd != Command::undefined) {
					req->setCommand(cmd);
				}
				err = req->parseJson(obj);
				if(!!err) {
					delete req;
					setError(obj, err);
					break;
				}
			} else {
				err = createRequest(obj, req);
				if(!!err) {
					setError(json, err);
					break;
				}

				req->setID(requestId);
				req->setParam(param);
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
			if(!!err) {
				delete req;
			}
		}
	} else {
		// Single request
		err = createRequest(json[ATTR_DEVICE], req);
		if(!!err) {
			return setError(json, err, json[ATTR_DEVICE]);
		}

		req->setParam(param);
		err = req->parseJson(json);
		if(!err) {
			err = req->submit();
		}

		if(!!err) {
			delete req;
			return setError(json, err);
		}
	}

	return err;
}

} // namespace IO
