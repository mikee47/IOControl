#include "Status.h"
#include "Strings.h"
#include <FlashString/Vector.hpp>

namespace IO
{
String toString(Status status)
{
	switch(status) {
	case Status::success:
		return FS_success;
	case Status::pending:
		return FS_pending;
	case Status::error:
		return FS_error;
	default:
		return FS_unknown;
	}
}

void setStatus(JsonObject json, Status status)
{
	json[FS_status] = toString(status);
}

void setError(JsonObject json, int code, const String& text, const String& arg)
{
	setStatus(json, Status::error);
	JsonObject err = json.createNestedObject(FS_error);
	err[FS_code] = code;
	if(text) {
		err[FS_text] = text;
	}
	if(arg) {
		err[FS_arg] = arg;
	}
}

} // namespace IO
