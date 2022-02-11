/**
 * Device.h
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

#include "Error.h"
#include "DevNode.h"
#include "DeviceType.h"
#include "Event.h"
#include <ArduinoJson.h>

namespace IO
{
class Request;
class Device;
class Controller;

using DeviceConstructor = ErrorCode (&)(Controller& controller, Device*& device);

struct DeviceClassInfo {
	const FlashString& name;
	DeviceConstructor constructor;

	bool operator==(const DeviceClassInfo& other) const
	{
		return &name == &other.name;
	}
};

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

	virtual const DeviceClassInfo classInfo() const = 0;

	virtual const DeviceType type() const = 0;

	ErrorCode init(const Config& config);
	virtual ErrorCode init(JsonObjectConst config) = 0;

	virtual Request* createRequest() = 0;

	const CString& id() const
	{
		return m_id;
	}

	const CString& name() const
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

	virtual void handleEvent(Request* request, Event event);

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	virtual ErrorCode start();
	virtual ErrorCode stop();

	ErrorCode submit(Request* request);

private:
	CString m_id;
	CString m_name;
	Controller& m_controller;
	device_state_t m_state = devstate_stopped;
};

} // namespace IO
