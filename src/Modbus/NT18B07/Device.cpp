/**
 * Modbus/NT18B07/Device.cpp
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

#include <IO/Modbus/NT18B07/Device.h>
#include <IO/Modbus/NT18B07/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
namespace NT18B07
{
const Device::Factory Device::factory;

ErrorCode Device::init(const Config& config)
{
	auto err = Modbus::Device::init(config.modbus);
	if(err) {
		return err;
	}

	memcpy(comp, config.comp, sizeof(comp));

	debug_hex(INFO, "COMP", comp, sizeof(comp));

	return Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	Modbus::Device::parseJson(json, cfg.modbus);
	JsonArrayConst jc = json[F("comp")];
	for(unsigned i = 0; i < channelCount; ++i) {
		cfg.comp[i].a = jc[i * 2] | 10;
		cfg.comp[i].b = jc[i * 2 + 1] | 0;
	}
}

ErrorCode Device::init(JsonObjectConst json)
{
	Config cfg{};
	parseJson(json, cfg);
	return init(cfg);
}

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

int16_t Device::getIntValue(uint8_t channel) const
{
	if(channel >= channelCount) {
		return 0;
	}

	auto& c = comp[channel];
	int value = int(c.a) * values[channel];
	value += (value < 0) ? -5 : 5;
	value /= 10;
	value += c.b;

	return value;
}

void Device::getRawValues(JsonArray json) const
{
	for(unsigned i = 0; i < channelCount; ++i) {
		json.add(values[i]);
	}
}

void Device::getValues(TempData& data) const
{
	for(unsigned i = 0; i < channelCount; ++i) {
		data[i] = getIntValue(i);
	}
}

void Device::getValues(JsonArray json) const
{
	for(unsigned i = 0; i < channelCount; ++i) {
		json.add(getValue(i));
	}
}

} // namespace NT18B07
} // namespace Modbus
} // namespace IO
