/**
 * RFSwitch/ControlleDevice.h
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
namespace RFSwitch
{
DECLARE_FSTR(ATTR_REPEATS)

/**
 * @brief Protocol timings in microseconds
 */
struct Timing {
	uint16_t starth; ///< Width of start High pulse
	uint16_t startl; ///< Width of start Low pulse
	uint16_t period; ///< Bit period
	uint16_t bit0;   ///< Width of a '0' high pulse
	uint16_t bit1;   ///< Width of a '1' high pulse
	uint16_t gap;	///< Gap after final bit before repeating
};

/*
 * A specific type of RF device protocol.
 * Actual RF I/O is performed by Controller.
 */
class Device : public IO::Device
{
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
			DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "rfswitch")
			return DEVICE_CLASSNAME;
		}
	};

	static const Factory factory;

	struct Config {
		IO::Device::Config base;
		Timing timing;
		uint8_t repeats;
	};

	const DeviceType type() const override
	{
		return DeviceType::RFSwitch;
	}

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	Device(Controller& controller, const char* id) : IO::Device(controller, id)
	{
	}

	IO::Request* createRequest() override;

	const Timing& getTiming() const
	{
		return timing;
	}

	uint8_t getRepeats() const
	{
		return repeats;
	}

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

protected:
	Timing timing;
	uint8_t repeats; ///< Number of times to repeat code
};

} // namespace RFSwitch
} // namespace IO
