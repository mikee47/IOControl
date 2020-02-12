/*
 * IOControl.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 * A simple IO device stack for embedded devices such as the ESP8266.
 *
 */

#pragma once

#include <WString.h>
#include <SimpleTimer.h>
#include <FIFO.h>
#include <WHashMap.h>
#include <ArduinoJson.h>
#include <Data/CStringArray.h>
#include "BitSet.h"
#include "Status.h"

#ifdef ENABLE_HEAP_PRINTING
#ifdef ENABLE_MALLOC_COUNT
#include <malloc_count.h>

#define PRINT_HEAP()                                                                                                   \
	debug_i("%s %s #%u : Free Heap = %u, Used = %u, Peak = %u", __FILE__, __FUNCTION__, __LINE__,                      \
			system_get_free_heap_size(), MallocCount::getCurrent(), MallocCount::getPeak())
#else
#define PRINT_HEAP()                                                                                                   \
	debug_i("%s %s #%u : Free Heap = %u", __FILE__, __FUNCTION__, __LINE__, system_get_free_heap_size())
#endif
#else
#define PRINT_HEAP()
#endif

// Maximum queued requests per controller
#ifndef IOCONTROL_MAX_REQUESTS
#define IOCONTROL_MAX_REQUESTS 16
#endif

// Controller attempts device restart on error at this interval
#define DEVICECHECK_INTERVAL 10000

namespace IO
{
// Global tags
DECLARE_FSTR(ATTR_COMMAND)
DECLARE_FSTR(ATTR_NAME)
DECLARE_FSTR(ATTR_DEVICE)
DECLARE_FSTR(ATTR_DEVICES)
DECLARE_FSTR(ATTR_ID)

// Devices may have nodes
DECLARE_FSTR(ATTR_NODE)
DECLARE_FSTR(ATTR_NODES)
DECLARE_FSTR(ATTR_DEVNODES)
DECLARE_FSTR(ATTR_COUNT)

// Global tags
DECLARE_FSTR(ATTR_DELAY)

/*
 * IO Commands
 */
#define IOCOMMAND_MAP(XX)                                                                                              \
	XX(undefined, "Undefined or invalid")                                                                              \
	XX(query, "Query node states")                                                                                     \
	XX(off, "Turn node off or set to minimum")                                                                         \
	XX(on, "Turn node on or set to maximum")                                                                           \
	XX(toggle, "Toggle node(s) between on and off")                                                                    \
	XX(latch, "Relay nodes")                                                                                           \
	XX(momentary, "Relay nodes")                                                                                       \
	XX(delay, "Relay nodes")                                                                                           \
	XX(send, "Action with no acknowledgement or response")                                                             \
	XX(adjust, "Adjust value")

enum class Command {
#define XX(tag, comment) tag,
	IOCOMMAND_MAP(XX)
#undef XX
};

String toString(Command cmd);

class Controller;
class Device;
class Request;
class DeviceManager;

/*
 * A request goes through the following states:
 *  submitted - request.submit()
 *  queued    - Controller places request on internal queue
 *  executed  - Controller retrieves request from queue
 *  completed - Request fully handled, status indicates success/failure
 *
 * An Controller invokes this callback twice, when a request is about to
 * be executed and again when it has completed.
 * request.status() will be 'Status::pending' at execution, any other value
 * at completion.
 */
using IoCallback = Delegate<void(Request& request)>;

using DeviceConstructor = Error (&)(Controller& controller, Device*& device);

struct DeviceClassInfo {
	const FlashString& name;
	DeviceConstructor constructor;

	bool operator==(const DeviceClassInfo& other) const
	{
		return &name == &other.name;
	}
};

using GetDeviceClass = const DeviceClassInfo (*)();

using DeviceClassMap = HashMap<String, GetDeviceClass>;
using IODeviceList = Vector<Device*>;

/*
 * General information about shared port requirements
 */
struct PortInfo {
	enum Mode {
		Read,
		Write,
		ReadWrite,
	};
	Mode io;
	Mode interrupt;
};

/*
 * Serialises hardware requests on a physical bus.
 */
class Controller
{
	friend Device;

public:
	using RequestQueue = FIFO<Request*, IOCONTROL_MAX_REQUESTS>;

	Controller(uint8_t instance) : m_devices(4, 4), m_instance(instance)
	{
	}

	virtual ~Controller()
	{
		freeDevices();
		stopTimer();
	}

	static void registerDeviceClass(const GetDeviceClass devclass)
	{
		String classname = devclass().name;
		m_deviceClasses[classname] = devclass;
		debug_i("Device class '%s' registered", classname.c_str());
	}

	uint8_t instance() const
	{
		return m_instance;
	}

	IODeviceList& devices()
	{
		return m_devices;
	}

	bool verifyClass(const String& classname);

	void freeDevices();
	Error createDevice(JsonObjectConst config);

	template <class DeviceClass> Error createDevice(DeviceClass* device, const typename DeviceClass::Config& config);

	Device* findDevice(const String& id);

	virtual String classname() = 0;

	virtual void start()
	{
		startDevices();
	}

	// MUST call canStop() first to ensure it's safe to stop
	virtual void stop()
	{
		stopDevices();
	}

	virtual bool canStop()
	{
		return m_queue.count() == 0;
	}

	String id()
	{
		return classname() + String(m_instance);
	}

	virtual bool busy() const = 0;

	/**
	 * @brief If we have use of a shared resource (e.g. Serial port) then it calls this
	 * method to ask whether to allow another controller to make use of it.
	 * @param other The controller making the request
	 * @param info Some information about the port
	 * @retval bool false by default, deny access
	 */
	virtual bool allowPortAccess(Controller* other, const PortInfo& info)
	{
		return false;
	}

protected:
	/*
	 * Returns false if request could not be queued. Caller may then either
	 * delete the request or try again later.
	 */
	Error submit(Request* request);

	virtual void execute(Request& request) = 0;

	void requestComplete(Request* request);

	void startTimer();
	void stopTimer();

protected:
	IODeviceList m_devices;
	RequestQueue m_queue;

private:
	void executeNext();

	void deviceError(Device& device);

	void startDevices();
	void stopDevices();

private:
	static DeviceClassMap m_deviceClasses;
	SimpleTimer* m_deviceCheckTimer = nullptr;
	uint8_t m_instance = 0;
};

/**
 * @brief Identifies a device node
 */
struct DevNode {
	using ID = uint16_t;
	ID id;

	// For a normal on/off output node
	enum class State {
		off,
		on,
		someon,
		unknown,
	};

	using States = BitSet<uint8_t, State>;

	bool operator==(const DevNode& other) const
	{
		return id == other.id;
	}
};

// Special value to indicate all nodes
static constexpr DevNode DevNode_ALL{0xFFFF};

/*
 * Device state is used to automate initialisation and fault recovery.
 *
 *  uninitialised -> initialising -> normal
 *
 * First initialisation fails, retried
 *
 *  uninitialised -> initialising -> fault -> initialising -> normal
 *
 * Failed during operation, re-initialised
 *
 *  normal -> fault -> initialising -> normal
 */
enum __attribute__((packed)) device_state_t {
	// Awaiting initialisation by controller
	devstate_stopped,
	// Initialisation in progress
	devstate_starting,
	// Initialisation or other request failed
	devstate_fault,
	// Normal operation
	devstate_normal
};

/*
 * Handles requests for a specific device; the requests are executed by the relevant
 * controller.
 */
class Device
{
	friend Request;
	friend Controller;

public:
	struct Config {
		String id;
		String name;
	};

	/*
	 * Inherited classes should implement this method.
	 * User code then registers required device classes
	 */
	Device(Controller& controller) : m_controller(controller)
	{
	}

	virtual ~Device()
	{
	}

	virtual const IO::DeviceClassInfo getClass() const = 0;

	Error init(const Config& config);
	virtual Error init(JsonObjectConst config) = 0;

	virtual Request* createRequest() = 0;

	const String& id() const
	{
		return m_id;
	}

	const String& name() const
	{
		return m_name ?: m_id;
	}

	virtual uint16_t address() const
	{
		return 0;
	}

	String caption();

	Controller& controller() const
	{
		return m_controller;
	}

	device_state_t state()
	{
		return m_state;
	}

	// Typically devices have a contiguous valid range of node IDs
	virtual DevNode::ID nodeIdMin() const
	{
		return 0;
	}

	virtual DevNode::ID nodeIdMax() const
	{
		return 0;
	}

	// 0 if device doesn't support nodes
	virtual uint16_t maxNodes() const
	{
		return 0;
	}

	virtual DevNode::States getNodeStates(DevNode node) const
	{
		return DevNode::States{};
	}

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	virtual Error start();
	virtual Error stop();

	Error submit(Request* request)
	{
		return m_controller.submit(request);
	}

	virtual void requestComplete(Request* request);

private:
	String m_id;
	String m_name;
	Controller& m_controller;
	device_state_t m_state = devstate_stopped;
};

template <class DeviceClass>
Error Controller::createDevice(DeviceClass* device, const typename DeviceClass::Config& config)
{
	DeviceClassInfo cls = DeviceClass::deviceClass();
	assert(cls.constructor);

	Device* dev = nullptr;
	dev = nullptr;
	Error err = cls.constructor(*this, dev);
	if(!!err) {
		debug_err(err, String(cls.name));
		return err;
	}
	assert(dev != nullptr);
	err = reinterpret_cast<DeviceClass*>(dev)->init(config);
	if(!!err) {
		delete dev;
		debug_err(err, String(cls.name));
		return err;
	}

	m_devices.addElement(dev);
	debug_d("Device %s created, class %s", dev->caption().c_str(), String(cls.name).c_str());

	device = reinterpret_cast<DeviceClass*>(dev);

	return err;
}

/** @brief Request represents a single user request/response over a bus.
 *
 * Inherited classes provide additional methods to encapsulate
 * specific commands or functions.
 * Create the appropriate object using new(), or using the device manager,
 * then configure the request. Finally, call submit(). If submit() is not
 * called then the request object must be delete()ed.
 * When the request has completed the callback is invoked, after
 * which the request object is destroyed.
 *
 * Each call to submit() _always_ has a corresponding call to complete(),
 * even if the request is never attempted. This ensures error
 * is reported to the callback and the object released.
 *
 * In brief, ownership of a request belongs with the user up until submit()
 * is called, which passes ownership to the IO mechanism.
 */
class Request
{
	friend Controller;

public:
	Request(Device& device) : m_device(device)
	{
		debug_d("Request %p created", this);
	}

	// Prevent copying; if required, add `virtual clone()`
	Request(const Request&) = delete;

	virtual ~Request()
	{
		debug_d("Request %p (%s) destroyed", this, m_id.c_str());
	}

	Device& device() const
	{
		return m_device;
	}

	Status status() const
	{
		return m_status;
	}

	String caption();

	virtual Error parseJson(JsonObjectConst json);

	/**
	 * @brief Submit a request
	 *
	 * If the request cannot be submitted (invalid, queue full) then it is destroyed
	 * immediately and an error returned; the completion routines is not called.
	 *
	 * The request may be executed immediately or queued as appropriate. In either case
	 * the result of the request is posted to the callback routine.
	 *
	 * @retval Error Indicates whether the request was accepted.
	 */
	virtual Error submit()
	{
		return m_device.submit(this);
	}

	/*
	 * Usually called by device or controller, but can also be used to
	 * pass a request to its callback first, for example on a configuration
	 * error.
	 */
	void complete(Status status);

	/*
	 * Get response as JSON message, in textual format
	 */
	virtual void getJson(JsonObject json) const;

	void setID(const String& value)
	{
		m_id = value;
	}

	void setCommand(Command cmd)
	{
		debug_d("setCommand(0x%08x: %s)", cmd, toString(cmd).c_str());
		m_command = cmd;
	}

	void setParam(void* param)
	{
		m_param = param;
	}

	bool nodeQuery(DevNode node)
	{
		m_command = Command::query;
		return setNode(node);
	}

	bool nodeOff(DevNode node)
	{
		setCommand(Command::off);
		return setNode(node);
	}

	bool nodeOn(DevNode node)
	{
		setCommand(Command::on);
		return setNode(node);
	}

	bool nodeToggle(DevNode node)
	{
		setCommand(Command::toggle);
		return setNode(node);
	}

	virtual bool nodeAdjust(DevNode node, int value)
	{
		return false;
	}

	virtual bool setNode(DevNode node)
	{
		return false;
	}

	virtual DevNode::States getNodeStates(DevNode node)
	{
		return DevNode::State::unknown;
	}

	/*
	 * Generic set state command. 0 is off, otherwise on.
	 * Dimmable nodes use percentage level 0 - 100.
	 */
	virtual bool setNodeState(DevNode node, DevNode::State state)
	{
		if(state == DevNode::State::on) {
			setCommand(Command::on);
		} else if(state == DevNode::State::off) {
			setCommand(Command::off);
		} else {
			return false;
		}
		return setNode(node);
	}

	const String& id() const
	{
		return m_id;
	}

	Command command() const
	{
		return m_command;
	}

	/**
	 * @brief User-assigned parameter
	 */
	void* param() const
	{
		return m_param;
	}

protected:
	Device& m_device;
	void* m_param;							///< User-assigned parameter
	Command m_command = Command::undefined; ///< Active command
	Status m_status = Status::pending;
	String m_id; ///< User assigned request ID
};

using ControllerMap = HashMap<String, Controller*>;

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
	void registerDeviceClass(GetDeviceClass devclass);

	Controller* findController(const String& name) const
	{
		return m_controllers[name];
	}

	/**
	 * @brief Load device config and create device tree.
	 */
	Error begin(JsonObjectConst config);

	Error end();

	void start();

	Error stop();

	bool canStop();

	Device* findDevice(const String& id);

	Error createRequest(const String& devid, Request*& request);

	/*
	 * Generally, requests should fit into the general model so that IO::Request can be used.
	 * If more custom behviour is required then this method can be used to up-cast to the appropriate
	 * Request object.
	 * NOTE: At present there is no type checking on this, so use care.
	 */
	template <class RequestClass> Error createRequest(const String& devid, RequestClass*& request)
	{
		Request* req;
		auto err = createRequest(devid, req);
		if(!err) {
			request = reinterpret_cast<RequestClass*>(req);
		} else {
			debug_w("createRequest('%s'): %s", devid.c_str(), IO::toString(err).c_str());
		}
		return err;
	}

	/**
	 * @brief set the callback handler function for all I/O requests
	 * @note Callback invoked twice; once when executed, then again when completed.
	 */
	void setCallback(IoCallback callback)
	{
		m_callback = callback;
	}

	/**
	 * @brief invoke the callback, if one is registered
	 * @note called by Controller
	 */
	void callback(Request& request)
	{
		if(m_callback)
			m_callback(request);
	}

	Error handleMessage(JsonObject json, void* param);

private:
	ControllerMap m_controllers; ///< We don't own the controllers
	IoCallback m_callback = nullptr;
};

extern DeviceManager devmgr;

} // namespace IO
