#include <IO/Controller.h>
#include <IO/Request.h>
#include <IO/DeviceManager.h>
#include <IO/Strings.h>

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

// Controller attempts device restart on error at this interval
#define DEVICECHECK_INTERVAL 10000

namespace IO
{
DeviceClassMap Controller::m_deviceClasses;

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
	String cls = config[FS_class];
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

Error Controller::submit(Request* request)
{
	if(request->command() == Command::undefined) {
		delete request;
		return Error::no_command;
	}

	/* Can re-submit a request instead of completing it to retry or progress
	 * a multi-IO call without having to create a new request object.
	 * So we only need to be in the queue once.
	 * Callback is invoked only at initial execution.
	 */
	// Same request might be submitted multiple times before completing
	if(m_queue.count() && m_queue.peek() == request) {
		debug_d("Re-submitting request %s", request->caption().c_str());
		// Execute directly, don't invoke callback
		request->handleEvent(Event::Execute);
		return Error::success;
	}

	debug_d("Queueing request %s", request->caption().c_str());
	if(!m_queue.enqueue(request)) {
		delete request;
		return Error::queue_full;
	}

	executeNext();
	return Error::success;
}

void Controller::handleEvent(Request* request, Event event)
{
	switch(event) {
	case Event::Execute:
		devmgr.callback(*request);
		break;

	case Event::RequestComplete:
		devmgr.callback(*request);

		// If this was a queued request, remove it
		if(m_queue.peek() == request) {
			m_queue.dequeue();
		}

		delete request;

		executeNext();
		break;

	case Event::ReceiveComplete:
	case Event::TransmitComplete:
		break;

	case Event::Timeout:
		request->complete(Status::error);
		break;
	}
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
		req->handleEvent(Event::Execute);
	}
}

} // namespace IO
