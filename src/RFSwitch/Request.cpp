/**
 * RFSwitch/Request.cpp
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

#include <IO/RFSwitch/Request.h>
#include <IO/RFSwitch/Device.h>
#include <IO/Strings.h>

namespace IO
{
namespace RFSwitch
{
void Request::send(uint32_t code, uint8_t repeats)
{
	m_code = code;
	m_repeats = repeats ?: device().repeats();
}

ErrorCode Request::parseJson(JsonObjectConst json)
{
	auto err = IO::Request::parseJson(json);
	if(err) {
		return err;
	}
	const char* s = json[FS_code];
	if(s == nullptr) {
		return Error::no_code;
	}
	m_code = strtoul(s, nullptr, 16);

	if(!Json::getValue(json[ATTR_REPEATS], m_repeats)) {
		m_repeats = device().repeats();
	}

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);

	//  writeProtocol(m_protocol, json);

	json[FS_code] = String(m_code, HEX);
}

} // namespace RFSwitch
} // namespace IO
