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
#include <cmdio/CommandHandler.h>
#include <Data/CStringArray.h>
#include <ArduinoStreamUtils.h>

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
#define MAX_REQUESTS 16

// IOController attempts device restart on error at this interval
#define DEVICECHECK_INTERVAL 10000

// Global tags
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

enum __attribute__((packed)) io_command_t {
#define XX(_tag, _comment) ioc_##_tag,
	IOCOMMAND_MAP(XX)
#undef XX
		ioc_MAX
};

String IoCommandToStr(io_command_t cmd);

class IOController;
class IODevice;
class IORequest;
class CIODeviceManager;

/*
 * A request goes through the following states:
 *  submitted - request.submit()
 *  queued    - IOController places request on internal queue
 *  executed  - IOController retrieves request from queue
 *  completed - Request fully handled, status indicates success/failure
 *
 * An IOController invokes this callback twice, when a request is about to
 * be executed and again when it has completed.
 * request.status() will be 'status_pending' at execution, any other value
 * at completion.
 */
typedef Delegate<void(IORequest& request)> IoCallback;

typedef ioerror_t (*device_constructor_t)(IOController& controller, IODevice*& device);

struct device_class_info_t {
	const FlashString& name;
	device_constructor_t constructor;
};

typedef const device_class_info_t (*device_class_t)();

using IODeviceClassMap = HashMap<String, device_class_t>;
using IODeviceList = Vector<IODevice*>;

static inline ioerror_t ioGetDeserializeError(const DeserializationError& error)
{
	switch(error.code()) {
	case DeserializationError::Ok:
		return ioe_success;
	case DeserializationError::NoMemory:
	case DeserializationError::TooDeep:
		return ioe_nomem;
	default:
		return ioe_bad_config;
	}
}

/**
 * @brief JSON document handling with IO error codes
 */
class IOJsonDocument : public DynamicJsonDocument
{
public:
	IOJsonDocument(size_t capacity) : DynamicJsonDocument(capacity)
	{
		debug_i("IOJsonDocument@%p capacity = %u", this, capacity);
	}

	~IOJsonDocument()
	{
#ifndef SMING_RELEASE
		auto used = memoryUsage();
		auto avail = capacity() - used;
		debug_i("~IOJsonDocument@%p used = %u, avail = %u", this, used, avail);
		if(avail < (capacity() / 16)) {
			debug_w("WARNING! Document nearly full");
		}
#endif
	}

	ioerror_t loadFromFile(const String& filename)
	{
		FileStream stream(filename);
		if(!stream.isValid()) {
			return ioe_file;
		}
		ReadBufferingStream buffer(stream, 64);
		auto dserr = deserializeJson(*this, buffer);
		auto err = ioGetDeserializeError(dserr);
		if(err) {
			debug_e("IOJsonDocument load ERROR '%s': %s", filename.c_str(), ioerrorString(err).c_str());
		} else {
			debug_i("IOJsonDocument@%p::load '%s', size = %u, json memory = %u", this, filename.c_str(),
					stream.getSize(), memoryUsage());
		}
		return err;
	}
};

/*
 * Serialises hardware requests on a physical bus.
 */
class IOController
{
	friend IODevice;

public:
	IOController(uint8_t instance) : m_devices(4, 4), m_instance(instance)
	{
	}

	virtual ~IOController()
	{
		freeDevices();
		stopTimer();
	}

	static void registerDeviceClass(const device_class_t devclass)
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

	bool verifyClass(String classname);

	void freeDevices();
	ioerror_t createDevice(JsonObjectConst config);

	IODevice* findDevice(const String& id);

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

protected:
	/*
	 * Returns false if request could not be queued. Caller may then either
	 * delete the request or try again later.
	 */
	ioerror_t submit(IORequest& request);

	virtual void execute(IORequest& request) = 0;

	void requestComplete(IORequest& request);

	void startTimer();
	void stopTimer();

protected:
	IODeviceList m_devices;
	FIFO<IORequest*, MAX_REQUESTS> m_queue;

private:
	void executeNext();

	void deviceError(IODevice& device);

	void startDevices();
	void stopDevices();

private:
	static IODeviceClassMap m_deviceClasses;
	SimpleTimer* m_deviceCheckTimer = nullptr;
	uint8_t m_instance = 0;
};

// Identifies a device node
typedef uint16_t devnode_id_t;
// Special value to indicate all nodes
const devnode_id_t NODES_ALL = 0xFFFF;

// For a normal on/off output node
enum __attribute__((packed)) devnode_state_t {
	state_none = 0, // No state information available (i.e. no nodes)
	state_off = _BV(0),
	state_on = _BV(1),
	state_someon = _BV(2),
	state_unknown = _BV(3)
};

static inline devnode_state_t operator|=(devnode_state_t& lhs, const devnode_state_t rhs)
{
	lhs = static_cast<devnode_state_t>(lhs | rhs);
	return lhs;
}

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
class IODevice
{
	friend IORequest;
	friend IOController;

public:
	/*
	 * Inherited classes should implement this method.
	 * User code then registers required device classes
	 */
	// static void registerClass()
	IODevice(IOController& controller) : m_controller(controller)
	{
	}

	virtual ~IODevice()
	{
	}

	virtual IORequest* createRequest() = 0;

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

	IOController& controller() const
	{
		return m_controller;
	}

	device_state_t state()
	{
		return m_state;
	}

	// Typically devices have a contiguous valid range of node IDs
	virtual devnode_id_t nodeIdMin() const
	{
		return 0;
	}

	virtual devnode_id_t nodeIdMax() const
	{
		return 0;
	}

	// 0 if device doesn't support nodes
	virtual uint16_t maxNodes() const
	{
		return 0;
	}

	virtual devnode_state_t getNodeState(devnode_id_t nodeId) const
	{
		return state_none;
	}

protected:
	virtual ioerror_t init(JsonObjectConst config);
	virtual ioerror_t start();
	virtual ioerror_t stop();

	ioerror_t submit(IORequest& request)
	{
		return m_controller.submit(request);
	}

	virtual void requestComplete(IORequest& request);

protected:
	String m_id;
	String m_name;
	IOController& m_controller;
	device_state_t m_state = devstate_stopped;
};

class IOControl;

/** @brief CIORequest represents a single user request/response over a bus.
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
class IORequest
{
	friend IOController;

public:
	IORequest(IODevice& device) : m_device(device)
	{
		debug_d("CIORequest 0x%08X created", (uint32_t)this);
	}

	virtual ~IORequest()
	{
		debug_d("CIORequest 0x%08X (%s) destroyed", (uint32_t)this, m_id.c_str());
	}

	void setConnection(WSCommandConnection* connection)
	{
		m_connection = connection;
	}

	void setControl(IOControl* control)
	{
		m_control = control;
	}

	IODevice& device() const
	{
		return m_device;
	}

	request_status_t status() const
	{
		return m_status;
	}

	WSCommandConnection* connection() const
	{
		return m_connection;
	}

	String caption();

	virtual ioerror_t parseJson(JsonObjectConst json);

	ioerror_t submit();

	/*
	 * Usually called by device or controller, but can also be used to
	 * pass a request to its callback first, for example on a configuration
	 * error.
	 */
	void complete(request_status_t status);

	/*
	 * Get response as JSON message, in textual format
	 */
	virtual void getJson(JsonObject json) const;

	void setID(const String value)
	{
		m_id = value;
	}

	void setCommand(io_command_t cmd)
	{
		debug_d("setCommand(0x%08X: %s)", cmd, IoCommandToStr(cmd).c_str());
		m_command = cmd;
	}

	bool nodeQuery(devnode_id_t nodeId)
	{
		m_command = ioc_query;
		return setNode(nodeId);
	}

	bool nodeOff(devnode_id_t nodeId)
	{
		setCommand(ioc_off);
		return setNode(nodeId);
	}

	bool nodeOn(devnode_id_t nodeId)
	{
		setCommand(ioc_on);
		return setNode(nodeId);
	}

	bool nodeToggle(devnode_id_t nodeId)
	{
		setCommand(ioc_toggle);
		return setNode(nodeId);
	}

	virtual bool setNode(devnode_id_t nodeId)
	{
		return false;
	}

	virtual devnode_state_t getNodeState(devnode_id_t nodeId)
	{
		return state_unknown;
	}

	/*
	 * Generic set state command. 0 is off, otherwise on.
	 * Dimmable nodes use percentage level 0 - 100.
	 */
	virtual bool setNodeState(devnode_id_t nodeId, devnode_state_t state)
	{
		if(state == state_on) {
			setCommand(ioc_on);
		} else if(state == state_off) {
			setCommand(ioc_off);
		} else {
			return false;
		}
		return setNode(nodeId);
	}

	const String& id() const
	{
		return m_id;
	}

	io_command_t command() const
	{
		return m_command;
	}

protected:
	void queued();

protected:
	IODevice& m_device;
	//
	IOControl* m_control = nullptr;
	//
	WSCommandConnection* m_connection = nullptr;
	// Active command
	io_command_t m_command = ioc_undefined;
	//
	request_status_t m_status = status_pending;
	// User assigned request ID
	String m_id;
};

using IOControllerMap = HashMap<String, IOController*>;

class CIODeviceManager : public WSCommandHandler
{
public:
	/** @brief Controllers register themselves so they can be located
	 *  @note we don't own the controller; these are typically static objects
	 */
	void registerController(IOController& controller);

	/** @brief Device classes call this to register themselves
	 *
	 */
	void registerDeviceClass(device_class_t devclass);

	IOControllerMap& controllers()
	{
		return m_controllers;
	}

	ioerror_t begin(const String& configFile);

	ioerror_t end();

	void start();

	ioerror_t stop();

	bool canStop();

	IODevice* findDevice(const String& id);
	ioerror_t createRequest(const String& devid, IORequest*& request);

	/* CCommandHandler */

	String getMethod() const override;

	UserRole getMinAccess() const override
	{
		return UserRole::User;
	}

	void handleMessage(WSCommandConnection* connection, JsonObject json, IOControl* control);

	void handleMessage(WSCommandConnection* connection, JsonObject json) override
	{
		handleMessage(connection, json, nullptr);
	}

	/** @brief set the callback handler function for all I/O requests
	 *  @note Callback invoked twice; once when executed, then again when completed.
	 */
	void setCallback(IoCallback callback)
	{
		m_callback = callback;
	}

	/** @brief invoke the callback, if one is registered
	 *  @note called by IOController
	 */
	void callback(IORequest& request)
	{
		if(m_callback)
			m_callback(request);
	}

private:
	IOControllerMap m_controllers; ///< We don't own the controllers
	IoCallback m_callback = nullptr;
};

extern CIODeviceManager devmgr;
