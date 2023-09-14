/**
 * Modbus/STS/Fan/Device.cpp
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

#include <IO/Modbus/STS/Fan/Device.h>
#include <IO/Modbus/STS/Fan/Request.h>
#include <IO/Strings.h>

namespace IO::Modbus::STS::Fan
{
const Device::Factory Device::factory;

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

void Device::getValues(JsonObject json) const
{
	auto jspeed = json.createNestedArray(F("speed"));
	auto jrpm = json.createNestedArray(F("rpm"));
	for(unsigned i = 0; i < channelCount; ++i) {
		jspeed.add(data.speed[i]);
		jrpm.add(data.rpm[i]);
	}
}

} // namespace IO::Modbus::STS::Fan
