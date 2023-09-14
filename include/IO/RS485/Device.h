/**
 * RS485/Device.h
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

#include "../Device.h"
#include "Controller.h"

namespace IO
{
namespace RS485
{
constexpr unsigned DEFAULT_BAUDRATE = 9600;
constexpr unsigned DEFAULT_TIMEOUT = 800;

/**
 * @brief Base device class for communicating with an RS485 slave
 */
class Device : public IO::Device
{
public:
	template <class DeviceClass> class FactoryTemplate : public Factory
	{
	public:
		IO::Device* createDevice(IO::Controller& controller, const char* id) const override
		{
			return new DeviceClass(static_cast<RS485::Controller&>(controller), id);
		}

		const FlashString& controllerClass() const override
		{
			return CONTROLLER_CLASSNAME;
		}
	};

	/**
	 * @brief RS485 configuration
	 */
	struct Config {
		IO::Device::Config base;
		struct Slave {
			/**
			 * Network device address or 'slave ID'
			 */
			uint16_t address;
			/**
			 * Application-defined value for multiplexed serial ports.
			 * For example, several MAX485 transceivers can be connected to one
			 * serial port with appropriate multiplexing logic.
			 *
			 * See `IO::RS485::Controller::SetDirectionCallback`
			 */
			uint8_t segment;
			/**
			 * Serial link speed
			 */
			unsigned baudrate;
			/**
			 * Max time between command/response in milliseconds.
			 */
			unsigned timeout;
		};
		Slave slave;
	};

	Device(Controller& controller, const char* id) : IO::Device(controller, id)
	{
	}

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	const DeviceType type() const override
	{
		return DeviceType::RS485;
	}

	Controller& getController()
	{
		return static_cast<Controller&>(controller);
	}

	uint16_t address() const override
	{
		return slaveConfig.address;
	}

	uint8_t segment() const
	{
		return slaveConfig.segment;
	}

	unsigned baudrate() const
	{
		return slaveConfig.baudrate ?: DEFAULT_BAUDRATE;
	}

	unsigned timeout() const
	{
		return slaveConfig.timeout ?: DEFAULT_TIMEOUT;
	}

	void handleEvent(IO::Request* request, Event event) override;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

private:
	Config::Slave slaveConfig;
};

} // namespace RS485
} // namespace IO
