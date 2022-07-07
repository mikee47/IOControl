/**
 * Modbus/RID35/Device.cpp
 *
 * Created on: 24 March 2022
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

#include <IO/Modbus/RID35/Device.h>
#include <IO/Modbus/RID35/Request.h>
#include <IO/Strings.h>
#include <FlashString/Vector.hpp>

namespace IO
{
namespace Modbus
{
namespace RID35
{
namespace
{
#define XX(name) DEFINE_FSTR(regmap_str_##name, #name)
RID35_STDREG_MAP(XX)
RID35_OVFREG_MAP(XX)
#undef XX
#define XX(name) &regmap_str_##name,
DEFINE_FSTR_VECTOR(regNames, FlashString, RID35_STDREG_MAP(XX) RID35_OVFREG_MAP(XX))
#undef XX

} // namespace

const Device::Factory Device::factory;

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

void Device::updateRegisters(const void* values, size_t count)
{
	if(count != registerCount) {
		regValid = false;
		return;
	}
	memcpy(regValues, values, count * sizeof(uint16_t));
	regValid = true;
}

uint32_t Device::getRawValue(Register reg) const
{
	if(!regValid) {
		return 0;
	}

	auto i = unsigned(reg) * 2;
	if(i < stdRegCount) {
		return (regValues[i] << 16) | regValues[i + 1];
	}

	return regValues[stdRegCount + unsigned(reg) - unsigned(Register::TotalKwh)];
}

float Device::getValue(Register reg) const
{
	union {
		uint32_t val;
		float f;
	} u;
	u.val = getRawValue(reg);
	return (reg < Register::TotalKwh) ? u.f : u.val;
}

void Device::getValues(JsonObject json) const
{
	if(!regValid) {
		return;
	}

	for(unsigned i = 0; i < unsigned(Register::MAX); ++i) {
		json[regNames[i]] = getValue(Register(i));
	}
}

} // namespace RID35
} // namespace Modbus
} // namespace IO
