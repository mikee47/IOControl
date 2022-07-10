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

/**
 * @brief Handles requests for a specific device; the requests are executed by the relevant controller.
 */
class Device : public LinkedObjectTemplate<Device>
{
	friend Request;
	friend Controller;

public:
	/**
	 * @brief Abstract class which allows device instances to be created
	 */
	class Factory
	{
	public:
		/**
		 * @brief Create a new device instance
		 * @param controller The owning controller
		 * @param id Unique identifier for the device
		 * @retval Device* The constructed instance
		 *
		 * Called by `DeviceManager::createDevice()`
		 */
		virtual Device* createDevice(IO::Controller& controller, const char* id) const = 0;

		/**
		 * @brief Return the expected controller type for this device class, e.g. 'rs485'
		 *
		 * The Device Manager uses this value to verify that devices are constructed using the correct controller.
		 */
		virtual const FlashString& controllerClass() const = 0;

		/**
		 * @brief Return the Device class name, e.g. 'r421a'
		 */
		virtual const FlashString& deviceClass() const = 0;

		bool operator==(const String& className) const
		{
			return this->deviceClass() == className;
		}
	};

	using OwnedList = OwnedLinkedObjectListTemplate<Device>;

	/**
	 * @brief Inherited classes expand this definition as required.
	 */
	struct Config {
		String name;
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
	enum class State {
		stopped,  ///< Awaiting initialisation by controller
		starting, ///< Initialisation in progress
		fault,	///< Initialisation or other request failed
		normal,   ///< Normal operation
	};

	/**
	 * @brief Device constructor
	 * @param controller The owning controller
	 * @param id Unique device identifier
	 */
	Device(Controller& controller, const char* id) : controller(controller), id(id)
	{
	}

	virtual ~Device()
	{
	}

	virtual const DeviceType type() const = 0;

	ErrorCode init(const Config& config);
	virtual ErrorCode init(JsonObjectConst config) = 0;

	/**
	 * @brief Create a request object for this device
	 * @retval Request* Caller must destroy or submit the request
	 */
	virtual Request* createRequest() = 0;

	/**
	 * @brief The unique device identifier
	 */
	const CString& getId() const
	{
		return id;
	}

	bool operator==(const String& id) const
	{
		return this->id == id;
	}

	/**
	 * @brief Optional descriptive name for the device
	 */
	const CString& getName() const
	{
		return name ?: id;
	}

	/**
	 * @brief Devices with a numeric address should implement this method
	 */
	virtual uint16_t address() const
	{
		return 0;
	}

	/**
	 * @brief Obtain a descriptive caption for this device
	 */
	String caption() const;

	/**
	 * @brief Obtain the owning controller
	 */
	Controller& getController() const
	{
		return controller;
	}

	/**
	 * @brief Get current device state
	 */
	State getState()
	{
		return state;
	}

	/**
	 * @brief Get minimum valid Node ID for this device
	 *
	 * Typically devices have a contiguous valid range of node IDs
	 */
	virtual DevNode::ID nodeIdMin() const
	{
		return 0;
	}

	/**
	 * @brief Get maximum valid Node ID for this device
	 */
	DevNode::ID nodeIdMax() const
	{
		return nodeIdMin() + maxNodes() - 1;
	}

	/**
	 * @brief Determine maximum number of nodes supported by the device
	 * @retval uint16_t 0 if device doesn't support nodes
	 */
	virtual uint16_t maxNodes() const
	{
		return 0;
	}

	/**
	 * @brief Return the current set of states for all nodes controlled by this device
	 *
	 * Used to determine if, say, all nodes are ON, OFF or a combination.
	 */
	virtual DevNode::States getNodeStates(DevNode node) const
	{
		return DevNode::States{};
	}

	/**
	 * @brief Implementations may override this method to customise event handling
	 */
	virtual void handleEvent(Request* request, Event event);

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	virtual ErrorCode start();
	virtual ErrorCode stop();

	void submit(Request* request);

	Controller& controller;

private:
	CString id;
	CString name;
	State state{};
};

} // namespace IO
