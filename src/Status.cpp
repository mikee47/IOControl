/*
 * status.cpp
 *
 *  Created on: 28 May 2018
 *      Author: mikee47
 */

#include <IO/Status.h>
#include <FlashString/Vector.hpp>

namespace IO
{
DEFINE_FSTR(ATTR_STATUS, "status");
DEFINE_FSTR_LOCAL(STATUS_SUCCESS, "success");
DEFINE_FSTR_LOCAL(STATUS_PENDING, "pending");
DEFINE_FSTR_LOCAL(STATUS_ERROR, "error");
DEFINE_FSTR_LOCAL(STATUS_UNKNOWN, "unknown");

// Json error node
DEFINE_FSTR_LOCAL(ATTR_ERROR, "error");
DEFINE_FSTR(ATTR_CODE, "code");
DEFINE_FSTR(ATTR_TEXT, "text");
DEFINE_FSTR_LOCAL(ATTR_ARG, "arg");

String toString(Status status)
{
	switch(status) {
	case Status::success:
		return STATUS_SUCCESS;
	case Status::pending:
		return STATUS_PENDING;
	case Status::error:
		return STATUS_ERROR;
	default:
		return STATUS_UNKNOWN;
	}
}

void setStatus(JsonObject json, Status status)
{
	json[ATTR_STATUS] = toString(status);
}

void setError(JsonObject json, int code, const String& text, const String& arg)
{
	setStatus(json, Status::error);
	JsonObject err = json.createNestedObject(ATTR_ERROR);
	err[ATTR_CODE] = code;
	if(text) {
		err[ATTR_TEXT] = text;
	}
	if(arg) {
		err[ATTR_ARG] = arg;
	}
}

#define XX(tag, value) DEFINE_FSTR_LOCAL(statusstr_##tag, #tag);
IOERROR_MAP(XX)
#undef XX

#define XX(tag, value) &statusstr_##tag,
DEFINE_FSTR_VECTOR(ioerrorStrings, FSTR::String, IOERROR_MAP(XX))
#undef XX

String toString(Error err)
{
	auto n = unsigned(err);
	String s = ioerrorStrings[n];
	return s ?: String(n);
}

Error setError(JsonObject json, Error err, const String& arg)
{
	setError(json, int(err), toString(err), arg);
	return err;
}

} // namespace IO
