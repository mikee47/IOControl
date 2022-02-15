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

#include "Request.h"
#include "DeviceType.h"
#include <ArduinoJson.h>
#include <Data/LinkedObjectList.h>

namespace IO
{
class Device;
class Controller;

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
class Device : public LinkedObjectTemplate<Device>
{
	friend Request;
	friend Controller;

public:
	class Factory
	{
	public:
		virtual Device* createDevice(IO::Controller& controller, const char* id) const = 0;
		virtual const FlashString& controllerClass() const = 0;
		virtual const FlashString& deviceClass() const = 0;

		bool operator==(const String& className) const
		{
			return this->deviceClass() == className;
		}
	};

	using OwnedList = OwnedLinkedObjectListTemplate<Device>;

	struct Config {
		String name;
	};

	/*
	 * Inherited classes should implement this method.
	 * User code then registers required device classes
	 */
	Device(Controller& controller, const char* id) : m_controller(controller), m_id(id)
	{
	}

	virtual ~Device()
	{
	}

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

	void submit(Request* request);

private:
	Controller& m_controller;
	CString m_id;
	CString m_name;
	device_state_t m_state{devstate_stopped};
};

} // namespace IO
