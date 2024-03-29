/**
 * Error.cpp
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

#include <IO/Error.h>
#include <IO/Strings.h>
#include <FlashString/Vector.hpp>

namespace IO
{
#define XX(tag, value) DEFINE_FSTR_LOCAL(errstr_##tag, #tag);
IOERROR_STD_MAP(XX)
#undef XX

#define XX(tag, value) &errstr_##tag,
DEFINE_FSTR_VECTOR_LOCAL(errorStrings, FSTR::String, IOERROR_STD_MAP(XX))
#undef XX

String Error::toString(ErrorCode err)
{
	switch(err) {
	case success:
		return FS_success;
	case pending:
		return FS_pending;
	default:
		err = err - 1 - max_common;
	}
	String s = errorStrings[abs(err)];
	return s ?: String(err);
}

ErrorCode setSuccess(JsonObject json)
{
	json[FS_status] = FS_success;
	return Error::success;
}

ErrorCode setPending(JsonObject json)
{
	json[FS_status] = FS_pending;
	return Error::pending;
}

ErrorCode setError(JsonObject json, ErrorCode err, const String& text, const String& arg)
{
	if(err == Error::pending) {
		json[FS_status] = FS_pending;
	} else if(err == Error::success) {
		json[FS_status] = FS_success;
	} else {
		json[FS_status] = FS_error;
		JsonObject obj = json.createNestedObject(FS_error);
		obj[FS_code] = err;
		obj[FS_text] = text ?: Error::toString(err);
		if(arg) {
			obj[FS_arg] = arg;
		}
	}

	return err;
}

} // namespace IO
