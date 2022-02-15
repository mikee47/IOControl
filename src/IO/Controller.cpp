/**
 * Controller.cpp
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

#include <IO/Controller.h>
#include <IO/Request.h>
#include <IO/DeviceManager.h>
#include <IO/Strings.h>
#include <IO/Debug.h>

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

ErrorCode Controller::createDevice(const char* id, JsonObjectConst config)
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
	auto err = create(*this, id, device);
	if(err) {
		debug_err(err, cls);
		return err;
	}
	assert(device != nullptr);
	err = device->init(config);
	if(err) {
		delete device;
		debug_err(err, cls);
		return err;
	}

	m_devices.add(device);
	debug_d("Device %s created, class %s", device->caption().c_str(), cls.c_str());

	return err;
}

void Controller::freeDevices()
{
	m_devices.clear();
}

Device* Controller::findDevice(const String& id)
{
	for(auto& dev : m_devices) {
		if(dev.id() == id) {
			return &dev;
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

	if(!m_deviceCheckTimer) {
		m_deviceCheckTimer.reset(new SimpleTimer);
		if(!m_deviceCheckTimer) {
			return;
		}

		m_deviceCheckTimer->initializeMs<DEVICECHECK_INTERVAL>(
			[](void* arg) { static_cast<Controller*>(arg)->startDevices(); }, this);
	}

	m_deviceCheckTimer->startOnce();
}

void Controller::stopTimer()
{
	m_deviceCheckTimer.reset();
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
	for(Device& device : m_devices) {
		auto err = device.start();

		debug_i("%s->start(): %s", device->caption().c_str(), Error::toString(err).c_str());

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

void Controller::stopDevices()
{
	stopTimer();
	for(auto& dev : m_devices) {
		dev.stop();
	}
}

/*
 * An error occurred on a device. Schedule a restart operation.
 */
void Controller::deviceError(Device& device)
{
	startTimer();
}

ErrorCode Controller::submit(Request* request)
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
		request->complete(Error::timeout);
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
