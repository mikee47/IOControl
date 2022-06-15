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
DeviceFactoryList Controller::deviceClasses;

const Device::Factory* Controller::findDeviceClass(const String& className)
{
	for(auto& factory : deviceClasses) {
		if(*factory == className) {
			return factory;
		}
	}

	return nullptr;
}

ErrorCode Controller::constructDevice(const Device::Factory& factory, const char* id, Device*& device)
{
	if(factory.controllerClass() != this->classname()) {
		debug_e("[IO] Wrong controller class '%s' for device '%s', require '%s'", String(factory.deviceClass()).c_str(),
				String(classname()).c_str(), String(factory.controllerClass()).c_str());
		return Error::bad_controller_class;
	}

	device = factory.createDevice(*this, id);
	if(device == nullptr) {
		auto err = IO::Error::no_mem;
		debug_err(err, String(factory.deviceClass()));
		return err;
	}

	return Error::success;
}

ErrorCode Controller::createDevice(const char* id, JsonObjectConst config, Device*& device)
{
	String cls = config[FS_class];
	auto factory = findDeviceClass(cls);
	if(factory == nullptr) {
		debug_e("Device class '%s' not registered", cls.c_str());
		return Error::bad_device_class;
	}

	auto err = constructDevice(*factory, id, device);
	if(err) {
		return err;
	}

	err = device->init(config);
	if(err) {
		delete device;
		device = nullptr;
		debug_err(err, cls);
		return err;
	}

	devices.add(device);
	debug_d("Device %s created, class %s", device->caption().c_str(), cls.c_str());

	return err;
}

void Controller::freeDevices()
{
	devices.clear();
}

Device* Controller::findDevice(const String& id)
{
	return std::find(devices.begin(), devices.end(), id);
}

/*
 * Use timer to attempt device re-starts
 */
void Controller::checkDevices()
{
	PRINT_HEAP();

	timer.initializeMs<DEVICECHECK_INTERVAL>([](void* arg) { static_cast<Controller*>(arg)->startDevices(); }, this);
	timer.startOnce();
}

/*
 * Inherited controllers call this after their own begin() code.
 * We schedule device initialisation here.
 */
void Controller::startDevices()
{
	debug_i("%s.startDevices()", getId().c_str());

	PRINT_HEAP();

	timer.stop();
	unsigned failCount = 0;
	for(auto& device : devices) {
		auto err = device.start();

		debug_i("%s->start(): %s", device.caption().c_str(), Error::toString(err).c_str());

		PRINT_HEAP();

		if(err) {
			++failCount;
		}
	}

	// Use the timer to retry later
	if(failCount != 0) {
		checkDevices();
	}
}

void Controller::stopDevices()
{
	timer.stop();
	for(auto& dev : devices) {
		dev.stop();
	}
}

/*
 * An error occurred on a device. Schedule a restart operation.
 */
void Controller::deviceError(Device& device)
{
	checkDevices();
}

void Controller::submit(Request* request)
{
	/*
	 * Can re-submit a request instead of completing it to retry or progress
	 * a multi-IO call without having to create a new request object.
	 * So we only need to be in the queue once.
	 * Callback is invoked only at initial execution.
	 */
	if(queue.head() == request) {
		debug_d("Re-submitting request %s", request->caption().c_str());
	} else {
		debug_d("Queueing request %s", request->caption().c_str());
		queue.add(request);
	}

	if(queue.head() == request) {
		executeNext();
	}
}

void Controller::handleEvent(Request* request, Event event)
{
	switch(event) {
	case Event::Execute:
		devmgr.invokeCallback(*request);
		break;

	case Event::RequestComplete:
		devmgr.invokeCallback(*request);

		// Requests don't need to be queued (e.g. DMX512 handles them immediately as it only updates internal state)
		if(queue.head() == request) {
			queue.remove(request);
		} else {
			delete request;
		}

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

void Controller::executeNext()
{
	auto req = queue.head();
	if(req == nullptr) {
		return;
	}

	int interval = req->device.minTransactionInterval() - lastTransactionEnd.elapsedTime();
	if(interval > 0) {
		timer.initializeMs(
			interval, [](void* arg) { static_cast<Controller*>(arg)->executeNext(); }, this);
		timer.startOnce();
		return;
	}

	debug_i("Executing request %p, %s: %s", req, req->id().c_str(), toString(req->getCommand()).c_str());
	req->handleEvent(Event::Execute);
}

} // namespace IO
