/**
 * Controller.h
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

#include "Device.h"
#include <WVector.h>

namespace IO
{
using DeviceFactoryList = Vector<const Device::Factory*>;

// Maximum queued requests per controller
#ifndef IOCONTROL_MAX_REQUESTS
#define IOCONTROL_MAX_REQUESTS 16
#endif

/**
 * @brief Some controllers specify a transfer direction
 */
enum class Direction {
	Incoming,
	Outgoing,
	Idle,
};

/*
 * Serialises hardware requests on a physical bus.
 */
class Controller
{
	friend Device;

public:
	using RequestQueue = FIFO<Request*, IOCONTROL_MAX_REQUESTS>;

	Controller(uint8_t instance) : m_instance(instance)
	{
	}

	virtual ~Controller()
	{
	}

	static void registerDeviceClass(const Device::Factory& factory)
	{
		m_deviceClasses.addElement(&factory);
		debug_i("Device class '%s' registered", String(factory.deviceClass()).c_str());
	}

	uint8_t instance() const
	{
		return m_instance;
	}

	Device::OwnedList& devices()
	{
		return m_devices;
	}

	void freeDevices();

	ErrorCode createDevice(const char* id, JsonObjectConst config, Device*& device);
	template <class DeviceClass>
	ErrorCode createDevice(const char* id, const typename DeviceClass::Config& config, DeviceClass*& device);

	Device* findDevice(const String& id);

	virtual const FlashString& classname() const = 0;

	virtual void start()
	{
		startDevices();
	}

	// MUST call canStop() first to ensure it's safe to stop
	virtual void stop()
	{
		stopDevices();
	}

	virtual bool canStop() const
	{
		return m_queue.count() == 0;
	}

	String id() const
	{
		String s;
		s += classname();
		s += '#';
		s += m_instance;
		return s;
	}

	virtual void handleEvent(Request* request, Event event);

protected:
	/*
	 * Returns false if request could not be queued. Caller may then either
	 * delete the request or try again later.
	 */
	ErrorCode submit(Request* request);

	void startTimer();
	void stopTimer();

private:
	ErrorCode constructDevice(const Device::Factory& factory, const char* id, Device*& device);

	void executeNext();

	void deviceError(Device& device);

	void startDevices();
	void stopDevices();

	const Device::Factory* findDeviceClass(const String& className);

	Device::OwnedList m_devices;
	RequestQueue m_queue;
	static DeviceFactoryList m_deviceClasses;
	std::unique_ptr<SimpleTimer> m_deviceCheckTimer;
	uint8_t m_instance;
};

template <class DeviceClass>
ErrorCode Controller::createDevice(const char* id, const typename DeviceClass::Config& config, DeviceClass*& device)
{
	auto& factory = DeviceClass::factory;
	Device* dev;
	auto err = constructDevice(factory, id, dev);
	if(err) {
		return err;
	}

	device = static_cast<DeviceClass*>(device);
	err = device->init(config);
	if(err) {
		delete device;
		device = nullptr;
		debug_err(err, String(factory.deviceClass()));
		return err;
	}

	m_devices.add(device);
	debug_d("Device %s created, class %s", device->caption().c_str(), String(factory.deviceClass()).c_str());

	return err;
}

} // namespace IO
