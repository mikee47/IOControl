#include <IO/Error.h>
#include <IO/Strings.h>
#include <FlashString/Vector.hpp>

namespace IO
{
namespace
{
#define XX(tag, value) DEFINE_FSTR(errstr_##tag, #tag);
IOERROR_STD_MAP(XX)
#undef XX

#define XX(tag, value) &errstr_##tag,
DEFINE_FSTR_VECTOR(errorStrings, FSTR::String, IOERROR_STD_MAP(XX))
#undef XX
} // namespace

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
		if(text) {
			obj[FS_text] = text;
		}
		if(arg) {
			obj[FS_arg] = arg;
		}
	}

	return err;
}

} // namespace IO
