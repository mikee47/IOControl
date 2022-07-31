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
#include <FlashString/Array.hpp>

namespace IO
{
namespace Modbus
{
namespace RID35
{
namespace
{
#define XX(reg, tag, ...) DEFINE_FSTR(regmap_tag_##tag, #tag)
RID35_STDREG_MAP(XX)
RID35_OVFREG_MAP(XX)
#undef XX

struct RegInfo {
	const FlashString* tag;
	Unit unit;
};

#define XX(reg, tag, unit, ...) {&regmap_tag_##tag, Unit::unit},
DEFINE_FSTR_ARRAY(regInfo, RegInfo, RID35_STDREG_MAP(XX) RID35_OVFREG_MAP(XX))
#undef XX

String valueToString(float value, Unit unit)
{
	switch(unit) {
	case Unit::KW:
		return String(round(value * 1000)) + 'W';
	case Unit::KVAR:
		return String(round(value * 1000)) + "VAR";
	case Unit::KVA:
		return String(round(value * 1000)) + "VA";
	case Unit::KWH:
		return String(value) + "KWh";
	case Unit::KVARH:
		return String(value) + "KVARh";
	case Unit::KVAH:
		return String(value) + "KVAh";
	case Unit::VOLT:
		return String(value) + 'V';
	case Unit::AMP:
		return String(value, 3) + 'A';
	case Unit::HERTZ:
		return String(value) + "Hz";
	case Unit::NONE:
	default:
		return String(value);
	}
}

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

String Device::getValueString(Register reg) const
{
	return valueToString(getValue(reg), regInfo[unsigned(reg)].unit);
}

void Device::getValues(JsonObject json) const
{
	if(!regValid) {
		return;
	}

	for(unsigned i = 0; i < unsigned(Register::MAX); ++i) {
		json[*regInfo[i].tag] = getValueString(Register(i));
	}
}

} // namespace RID35
} // namespace Modbus
} // namespace IO

String toString(IO::Modbus::RID35::Register reg)
{
	return *IO::Modbus::RID35::regInfo[unsigned(reg)].tag;
}
