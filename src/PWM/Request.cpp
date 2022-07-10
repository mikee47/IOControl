/**
 * PWM/Request.cpp
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

#include <IO/PWM/Request.h>
#include <IO/PWM/Device.h>
#include <IO/Strings.h>

namespace IO
{
namespace PWM
{
void Request::submit()
{
	auto err = getDevice().execute(*this);
	if(err < 0) {
		debug_e("Request failed, %s", Error::toString(err).c_str());
	}
	complete(err);
}

ErrorCode Request::parseJson(JsonObjectConst json)
{
	auto err = IO::Request::parseJson(json);
	if(err) {
		return err;
	}
	value = json[FS_value];

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);

	json[FS_value] = value;
}

} // namespace PWM
} // namespace IO
