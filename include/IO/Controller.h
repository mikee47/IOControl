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
#include <WHashMap.h>
#include <WVector.h>

namespace IO
{
using GetDeviceClass = const DeviceClassInfo (*)();
using DeviceClassMap = HashMap<String, GetDeviceClass>;
using IODeviceList = Vector<Device*>;

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
	ErrorCode createDevice(const char* id, JsonObjectConst config);

	template <class DeviceClass>
	ErrorCode createDevice(DeviceClass* device, const char* id, const typename DeviceClass::Config& config);

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
		String s;
		s += classname();
		s += '#';
		s += m_instance;
		return s;
	}

	virtual bool busy() const = 0;

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
	void executeNext();

	void deviceError(Device& device);

	void startDevices();
	void stopDevices();

	IODeviceList m_devices;
	RequestQueue m_queue;
	static DeviceClassMap m_deviceClasses;
	SimpleTimer* m_deviceCheckTimer{nullptr};
	uint8_t m_instance;
};

template <class DeviceClass>
ErrorCode Controller::createDevice(DeviceClass* device, const char* id, const typename DeviceClass::Config& config)
{
	DeviceClassInfo cls = DeviceClass::deviceClass();
	assert(cls.constructor);

	Device* dev = nullptr;
	dev = nullptr;
	ErrorCode err = cls.constructor(*this, dev);
	if(err) {
		debug_err(err, String(cls.name));
		return err;
	}
	assert(dev != nullptr);
	err = reinterpret_cast<DeviceClass*>(dev)->init(config);
	if(err) {
		delete dev;
		debug_err(err, String(cls.name));
		return err;
	}

	m_devices.addElement(dev);
	debug_d("Device %s created, class %s", dev->caption().c_str(), String(cls.name).c_str());

	device = reinterpret_cast<DeviceClass*>(dev);

	return err;
}

} // namespace IO
