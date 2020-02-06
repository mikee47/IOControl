/*
 * IOControl.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include "IOControl.h"
#include "IOControls.h"

IODeviceClassMap IOController::m_deviceClasses;

// Manages configurable device instances
CIODeviceManager devmgr;

DEFINE_FSTR_LOCAL(METHOD_IOCONTROL, "iocontrol")

// Global json tags
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

#define XX(_tag, _comment) #_tag "\0"
DEFINE_FSTR_LOCAL(IOCOMMAND_STRINGS, IOCOMMAND_MAP(XX))
#undef XX

String IoCommandToStr(io_command_t cmd)
{
	return CStringArray(IOCOMMAND_STRINGS)[cmd];
}

static bool strToIoCommand(const char* str, io_command_t& cmd)
{
	CStringArray commandStrings(IOCOMMAND_STRINGS);
	auto i = commandStrings.indexOf(str);
	if(i < 0) {
		debug_w("Unknown IO command '%s'", str);
		return false;
	}

	cmd = io_command_t(i);
	return true;
}

/* CIORequest */

ioerror_t IORequest::parseJson(JsonObjectConst json)
{
	Json::getValue(json[ATTR_ID], m_id);

	// Command is optional - may have already been set
	const char* cmd;
	if(Json::getValue(json[ATTR_COMMAND], cmd) && !strToIoCommand(cmd, m_command)) {
		return ioe_bad_command;
	}

	devnode_id_t id;
	JsonArrayConst arr;
	if(Json::getValue(json[ATTR_NODE], id)) {
		unsigned count = json[ATTR_COUNT] | 1;
		while(count--) {
			if(!setNode(id++)) {
				return ioe_bad_node;
			}
		}
	} else if(Json::getValue(json[ATTR_NODES], arr)) {
		for(unsigned id : arr)
			if(!setNode(id))
				return ioe_bad_node;
	}
	// all nodes
	else if(!setNode(NODES_ALL)) {
		return ioe_bad_node;
	}

	return ioe_success;
}

void IORequest::getJson(JsonObject json) const
{
	if(m_id.length() != 0) {
		json[ATTR_ID] = m_id;
	}
	json[ATTR_METHOD] = String(METHOD_IOCONTROL);
	json[ATTR_COMMAND] = IoCommandToStr(m_command);
	json[ATTR_DEVICE] = m_device.id();
	setStatus(json, m_status);
}

ioerror_t IORequest::submit()
{
	return m_device.submit(*this);
}

// Called by controller when queued for execution
void IORequest::queued()
{
	if(m_control != nullptr) {
		m_control->requestQueued(*this);
	}
}

/*
 * Request has completed. Device will destroy Notify originator then destroy this request.
 */
void IORequest::complete(request_status_t status)
{
	debug_i("Request 0x%08X (%s) complete", this, m_id.c_str());
	m_status = status;
	/*
	 * Destroy this request object (via device/controller) before invoking any completion
	 * callback. This releases our allocated memory including space on the request queue.
	 */
	IOControl* control = m_control;
	String id = m_id;
	m_device.requestComplete(*this);
	if(control != nullptr) {
		control->requestComplete(id);
	}
}

String IORequest::caption()
{
	String s(uint32_t(this), HEX);
	s += " (";
	s += m_device.caption();
	s += '/';
	s += m_id;
	s += ')';
	return s;
}

/* CIODevice */

ioerror_t IODevice::init(JsonObjectConst config)
{
	m_name = config[ATTR_NAME].as<const char*>();
	return Json::getValue(config[ATTR_ID], m_id) ? ioe_success : ioe_no_device_id;
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
ioerror_t IODevice::start()
{
	if(m_state == devstate_normal || m_state == devstate_starting) {
		return ioe_success;
	}

	IORequest* req = createRequest();
	if(req == nullptr) {
		return ioe_nomem;
	}

	// This fails if device doesn't have any nodes
	if(!req->nodeQuery(NODES_ALL)) {
		delete req;
		m_state = devstate_normal;
		return ioe_success;
	}

	ioerror_t err = req->submit();
	if(err) {
		delete req;
		return err;
	}

	m_state = devstate_starting;
	return ioe_success;
}

/*
 * Inherited classes might override this method to place device in a
 * low-power state.
 */
ioerror_t IODevice::stop()
{
	return ioe_success;
}

void IODevice::requestComplete(IORequest& request)
{
	if(request.status() == status_error) {
		m_state = devstate_fault;
		m_controller.deviceError(*this);
	} else if(request.status() == status_success && m_state == devstate_starting) {
		m_state = devstate_normal;
	}
	m_controller.requestComplete(request);
}

String IODevice::caption()
{
	return m_controller.id() + '/' + m_id;
}

/* CController */

/*
 * Before constructing a device instance, verify the class names match
 */
bool IOController::verifyClass(String classname)
{
	if(this->classname() == classname) {
		return true;
	}

	debug_e("verifyClass(%s) - wrong controller class, require '%s'", this->classname().c_str(), classname.c_str());
	return false;
}

ioerror_t IOController::createDevice(JsonObjectConst config)
{
	String cls = config[ATTR_CLASS];
	device_class_t devclass = m_deviceClasses[cls];
	if(devclass == nullptr) {
		debug_e("Device class '%s' not registered", cls.c_str());
		return ioe_bad_device_class;
	}
	device_constructor_t create = devclass().constructor;
	assert(create);

	IODevice* device;
	ioerror_t err = create(*this, device);
	if(err) {
		debug_err(err, cls);
		return err;
	}
	err = device->init(config);
	if(err) {
		delete device;
		debug_err(err, cls);
		return err;
	}

	m_devices.addElement(device);
	debug_d("Device %s created, class %s", device->caption().c_str(), cls.c_str());

	return err;
}

void IOController::freeDevices()
{
	unsigned i = m_devices.count();
	while(i--) {
		delete m_devices[i];
		m_devices.remove(i);
	}
}

IODevice* IOController::findDevice(const String& id)
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
void IOController::startTimer()
{
	PRINT_HEAP();

	if(m_deviceCheckTimer == nullptr) {
		m_deviceCheckTimer = new SimpleTimer();
		if(m_deviceCheckTimer == nullptr) {
			return;
		}

		m_deviceCheckTimer->initializeMs<DEVICECHECK_INTERVAL>(
			[](void* arg) { static_cast<IOController*>(arg)->startDevices(); }, this);
	}

	m_deviceCheckTimer->startOnce();
}

void IOController::stopTimer()
{
	delete m_deviceCheckTimer;
	m_deviceCheckTimer = nullptr;
}

/*
 * Inherited controllers call this after their own begin() code.
 * We schedule device initialisation here.
 */
void IOController::startDevices()
{
	debug_i("%s.startDevices()", id().c_str());

	PRINT_HEAP();

	stopTimer();
	unsigned failCount = 0;
	for(unsigned i = 0; i < m_devices.count(); ++i) {
		IODevice* device = m_devices[i];
		ioerror_t err = device->start();

		debug_i("%s->start(): %s", device->caption().c_str(), ioerrorString(err).c_str());

		PRINT_HEAP();

		if(err) {
			++failCount;
		}
	}

	// Use the timer to retry later
	if(failCount != 0) {
		startTimer();
	}
}

void IOController::stopDevices()
{
	stopTimer();
	for(unsigned i = 0; i < m_devices.count(); ++i) {
		m_devices[i]->stop();
	}
}

/*
 * An error occurred on a device. Schedule a restart operation.
 */
void IOController::deviceError(IODevice& device)
{
	startTimer();
}

ioerror_t IOController::submit(IORequest& request)
{
	if(request.command() == ioc_undefined) {
		return ioe_no_command;
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
		return ioe_success;
	}

	debug_d("Queueing request %s", request.caption().c_str());
	if(!m_queue.enqueue(&request)) {
		return ioe_queue_full;
	}

	request.queued();

	executeNext();
	return ioe_success;
}

void IOController::requestComplete(IORequest& request)
{
	devmgr.callback(request);

	// Not yet added to queue - that's a problem
	if(m_queue.peek() != &request) {
		debug_e("** Request queue out of sync");
		assert(false);
	}

	delete m_queue.dequeue();

	executeNext();
}

/*
 *
 */
void IOController::executeNext()
{
	// If we're busy, we'll get called again when current transaction has completed
	if(!busy() && m_queue.count()) {
		IORequest* req = m_queue.peek();
		debug_i("Executing request 0x%08X, %s: %s", req, req->id().c_str(), IoCommandToStr(req->command()).c_str());
		devmgr.callback(*req);
		execute(*req);
	}
}

/* CIODeviceManager */

void CIODeviceManager::registerController(IOController& controller)
{
	m_controllers[controller.id()] = &controller;
	debug_i("Controller '%s' registered", controller.id().c_str());
}

/** @brief Load device config and create device tree.
 *
 * 	@fixme 15/8/18
 * 	There's a crash 28 (invalid location access) when devmgr.json is uploaded.
 * 	It implicates this method, pointing to the IODevice::getNodeState call.
 * 	Suspect it's trying to access a null object somwhere.
 *
 * 	During testing the call to devMgrInit() in main.cpp was commented out.
 * 	The crash then occurs every time devmgr.json is uploaded; need to track
 * 	through the logic as there's obviously a check missing, or some variable
 * 	hasn't been set to a default value.
 *
 */
ioerror_t CIODeviceManager::begin(const String& configFile)
{
	auto err = end();
	if(err) {
		return err;
	}

	IOJsonDocument config(2048);
	err = config.loadFromFile(configFile);
	if(err) {
		return err;
	}

	// Create devices
	JsonArrayConst devices = config[JS_DEVICES];
	for(auto dev : devices) {
		String ctrl = dev[ATTR_CONTROLLER];
		int i = m_controllers.indexOf(ctrl);
		if(i < 0) {
			// Not a fatal error - keep going as other devices might be OK
			err = ioe_bad_controller;
			debug_err(err, ctrl);
			continue;
		}

		m_controllers.valueAt(i)->createDevice(dev);
	}

	start();

	return err;
}

void CIODeviceManager::start()
{
	// Start all controllers
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		m_controllers.valueAt(i)->start();
	}
}

bool CIODeviceManager::canStop()
{
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		if(!m_controllers.valueAt(i)->canStop()) {
			return false;
		}
	}

	return true;
}

ioerror_t CIODeviceManager::stop()
{
	if(!canStop()) {
		return ioe_busy;
	}

	// Stop all controllers
	for(unsigned i = 0; i < m_controllers.count(); ++i) {
		IOController* controller = m_controllers.valueAt(i);
		controller->stop();
	}

	return ioe_success;
}

ioerror_t CIODeviceManager::end()
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

	return ioe_success;
}

IODevice* CIODeviceManager::findDevice(const String& id)
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

ioerror_t CIODeviceManager::createRequest(const String& devid, IORequest*& request)
{
	if(!devid) {
		return ioe_no_device_id;
	}

	auto dev = findDevice(devid);
	if(dev == nullptr) {
		return ioe_bad_device;
	}

	request = dev->createRequest();
	return request ? ioe_success : ioe_nomem;
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
void CIODeviceManager::handleMessage(WSCommandConnection* connection, JsonObject json, IOControl* control)
{
	IORequest* req;
	ioerror_t err = ioe_success;

	// Command group ?
	bool isDevnode = json.containsKey(ATTR_DEVNODES);
	if(isDevnode || json.containsKey(ATTR_DEVICES)) {
		io_command_t cmd = ioc_undefined;
		const char* s;
		if(Json::getValue(json[ATTR_COMMAND], s) && !strToIoCommand(s, cmd)) {
			err = ioe_bad_command;
			setError(json, err, s);
			return;
		}

		JsonArray arr = json[isDevnode ? ATTR_DEVNODES : ATTR_DEVICES];
		if(arr.size() > MAX_REQUESTS) {
			setError(json, ioe_queue_full);
			return;
		}

		String requestId = json[ATTR_ID];

		// Build requests then submit together
		FIFO<IORequest*, MAX_REQUESTS> queue;

		ioerror_t err = ioe_success;
		for(auto obj : arr) {
			if(isDevnode) {
				err = createRequest(obj[ATTR_DEVICE], req);
				if(err) {
					setError(obj, err);
					continue;
				}

				req->setID(requestId);
				req->setConnection(connection);
				req->setControl(control);
				if(cmd != ioc_undefined)
					req->setCommand(cmd);
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
				req->setConnection(connection);
				req->setControl(control);
				req->setCommand(cmd);
				req->setNode(NODES_ALL);
			}

			queue.enqueue(req);
		}

		if(!err)
			if(cmd == ioc_toggle) {
				// Change request to an explicit on or off command
				// For toggle command, first node gives state
				devnode_state_t state = state_none;
				for(unsigned i = 0; i < queue.count(); ++i)
					state |= queue[i]->getNodeState(NODES_ALL);
				for(unsigned i = 0; i < queue.count(); ++i)
					queue[i]->setCommand((state & state_on) ? ioc_off : ioc_on);
			}

		// Submit requests, or delete them all if there was an error
		while(queue.count()) {
			req = queue.dequeue();
			if(!err)
				err = req->submit();
			if(err)
				delete req;
		}
	} else {
		// Single request
		err = createRequest(json[ATTR_DEVICE], req);
		if(err) {
			setError(json, err, json[ATTR_DEVICE]);
			return;
		}

		req->setConnection(connection);
		req->setControl(control);
		err = req->parseJson(json);
		if(!err)
			err = req->submit();

		if(err) {
			delete req;
			setError(json, err);
		}
	}

	if(err)
		return;

	// Response will be sent when command starts executing
	json[DONT_RESPOND] = true;
}

String CIODeviceManager::getMethod() const
{
	return METHOD_IOCONTROL;
}
