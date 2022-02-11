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

#include <IO/Device.h>
#include "Controller.h"

namespace IO
{
namespace RS485
{
constexpr unsigned DEFAULT_BAUDRATE = 9600;

class Device : public IO::Device
{
public:
	struct Config {
		IO::Device::Config base;
		struct Slave {
			uint16_t address;
			uint8_t segment;
			unsigned baudrate;
			unsigned timeout; ///< Max time between command/response
		};
		Slave slave;
	};

	Device(Controller& controller) : IO::Device(controller)
	{
	}

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	Controller& controller()
	{
		return reinterpret_cast<Controller&>(IO::Device::controller());
	}

	uint16_t address() const override
	{
		return m_config.address;
	}

	uint8_t segment() const
	{
		return m_config.segment;
	}

	unsigned baudrate() const
	{
		return m_config.baudrate ?: DEFAULT_BAUDRATE;
	}

	void handleEvent(IO::Request* request, Event event) override;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

private:
	Config::Slave m_config;
};

} // namespace RS485
} // namespace IO
