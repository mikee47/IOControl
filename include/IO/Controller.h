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

/**
 * @brief Some controllers specify a transfer direction
 */
enum class Direction {
	Incoming,
	Outgoing,
	Idle,
};

/**
 * @brief A Controller is responsible for serialising requests for a physical bus
 */
class Controller
{
	friend Device;

public:
	/**
	 * @brief Construct a controller instance
	 * @param instance Instance number, must be unique for each class of controller
	 */
	Controller(uint8_t instance) : m_instance(instance)
	{
	}

	virtual ~Controller()
	{
	}

	/**
	 * @brief Register a device factory
	 *
	 * Each factory has a class name which allows the device manager to dynamically
	 * construct Device instances as directed by configuration.
	 */
	static void registerDeviceClass(const Device::Factory& factory)
	{
		m_deviceClasses.addElement(&factory);
		debug_i("Device class '%s' registered", String(factory.deviceClass()).c_str());
	}

	/**
	 * @brief Get the controller instance number
	 */
	uint8_t instance() const
	{
		return m_instance;
	}

	/**
	 * @brief Get list of devices for this controller
	 */
	Device::OwnedList& devices()
	{
		return m_devices;
	}

	/**
	 * @brief Destroy all devices for this controller
	 */
	void freeDevices();

	/**
	 * @brief Create a new devicce
	 * @param id Unique identifier for the device
	 * @param config Device configuration
	 * @param device (OUT) The constructed device
	 * @retval ErrorCode
	 * 
	 * @note Created devices are owned by this controller. DO NOT delete them manually!
	 */
	ErrorCode createDevice(const char* id, JsonObjectConst config, Device*& device);

	/**
	 * @brief Create a new device as a concrete type
	 * @param id Unique identifier for the device
	 * @param config Device configuration
	 * @param device (OUT) The constructed device, owned by this controller
	 * @retval ErrorCode
	 *
	 * This method template is used to manually constructing device instances.
	 */
	template <class DeviceClass>
	ErrorCode createDevice(const char* id, const typename DeviceClass::Config& config, DeviceClass*& device);

	/**
	 * @brief Locate a device from its identifier
	 */
	Device* findDevice(const String& id);

	/**
	 * @brief Get the class name for this Controller
	 */
	virtual const FlashString& classname() const = 0;

	/**
	 * @brief Start the controller
	 *
	 * Controllers may now initiate communications with registered devices.
	 * Typically they'll query device status, etc.
	 */
	virtual void start()
	{
		startDevices();
	}

	/**
	 * @brief Stop all controllers
	 *
	 * This method is called by the Device Manager, applications shouldn't need it.
	 *
	 * @note MUST call canStop() first to ensure it's safe to stop!
	 */
	virtual void stop()
	{
		stopDevices();
	}

	/**
	 * @brief Check if it's OK to stop this controller
	 */
	virtual bool canStop() const
	{
		return m_queue.count() == 0;
	}

	/**
	 * @brief Get the fully-qualified unique controller identifier
	 */
	String id() const
	{
		String s;
		s += classname();
		s += '#';
		s += m_instance;
		return s;
	}

protected:
	/**
	 * @brief Implementations override this method to process events as they pass through the stack
	 */
	virtual void handleEvent(Request* request, Event event);

	/**
	 * @brief Queue a request
	 *
	 * The request is started immediately if the queue is empty.
	 */
	void submit(Request* request);

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
	Request::OwnedList m_queue;
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
