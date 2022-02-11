/**
 * RFSwitch/Device.cpp
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

#include <IO/RFSwitch/Device.h>
#include <IO/RFSwitch/Request.h>

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR_LOCAL(ATTR_TIMING, "timing")
DEFINE_FSTR_LOCAL(ATTR_STARTH, "starth")
DEFINE_FSTR_LOCAL(ATTR_STARTL, "startl")
DEFINE_FSTR_LOCAL(ATTR_PERIOD, "period")
DEFINE_FSTR_LOCAL(ATTR_BIT0, "bit0")
DEFINE_FSTR_LOCAL(ATTR_BIT1, "bit1")
DEFINE_FSTR_LOCAL(ATTR_GAP, "gap")
DEFINE_FSTR(ATTR_REPEATS, "repeats")

const unsigned RF_DEFAULT_REPEATS = 20;

namespace
{
DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "rfswitch")

ErrorCode createDevice(IO::Controller& controller, const char* id, IO::Device*& device)
{
	if(!controller.verifyClass(CONTROLLER_CLASSNAME)) {
		return Error::bad_controller_class;
	}

	device = new Device(reinterpret_cast<Controller&>(controller), id);
	return device ? Error::success : Error::no_mem;
}

} // namespace

const DeviceClassInfo deviceClass()
{
	return {DEVICE_CLASSNAME, createDevice};
}

ErrorCode Device::init(const Config& config)
{
	auto err = IO::Device::init(config.base);
	if(err) {
		return err;
	}

	m_timing = config.timing;
	m_repeats = config.repeats ?: RF_DEFAULT_REPEATS;

	return Error::success;
}

ErrorCode Device::init(JsonObjectConst config)
{
	Config cfg{};
	parseJson(config, cfg);
	return init(cfg);
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	IO::Device::parseJson(json, cfg.base);

	JsonObjectConst timing = json[ATTR_TIMING];
	cfg.timing.starth = timing[ATTR_STARTH];
	cfg.timing.startl = timing[ATTR_STARTL];
	cfg.timing.period = timing[ATTR_PERIOD];
	cfg.timing.bit0 = timing[ATTR_BIT0];
	cfg.timing.bit1 = timing[ATTR_BIT1];
	cfg.timing.gap = timing[ATTR_GAP];
	cfg.repeats = json[ATTR_REPEATS];
}

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

} // namespace RFSwitch
} // namespace IO
