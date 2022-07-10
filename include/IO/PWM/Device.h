/**
 * PWM/Device.h
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
namespace PWM
{
class Request;

class Device : public IO::Device
{
	friend Request;

public:
	class Factory : public IO::Device::Factory
	{
	public:
		IO::Device* createDevice(IO::Controller& controller, const char* id) const override
		{
			return new Device(static_cast<Controller&>(controller), id);
		}

		const FlashString& controllerClass() const override
		{
			return CONTROLLER_CLASSNAME;
		}

		const FlashString& deviceClass() const override
		{
			DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "pwm")
			return DEVICE_CLASSNAME;
		}
	};

	static const Factory factory;

	struct Config {
		IO::Device::Config base;
		uint32_t freq; ///< PWM frequency
		int channel;   ///< LEDC PWM channel number
		uint8_t pin;   ///< GPIO pin number
		bool invert;   ///< Whether to invert GPIO
	};

	const DeviceType type() const override
	{
		return DeviceType::PWM;
	}

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	Device(Controller& controller, const char* id) : IO::Device(controller, id)
	{
	}

	IO::Request* createRequest() override;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	ErrorCode execute(Request& request);

private:
	uint8_t channel{0};
#ifndef ARCH_ESP32
	uint16_t value;
#endif
};

} // namespace PWM
} // namespace IO
