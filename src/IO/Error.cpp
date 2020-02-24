#include "Status.h"
#include "Error.h"
#include <FlashString/Vector.hpp>

namespace IO
{
#define XX(tag, value) DEFINE_FSTR_LOCAL(errstr_##tag, #tag);
IOERROR_MAP(XX)
#undef XX

#define XX(tag, value) &errstr_##tag,
DEFINE_FSTR_VECTOR(errorStrings, FSTR::String, IOERROR_MAP(XX))
#undef XX

String toString(Error err)
{
	auto n = unsigned(err);
	String s = errorStrings[n];
	return s ?: String(n);
}

Error setError(JsonObject json, Error err, const String& arg)
{
	setError(json, int(err), toString(err), arg);
	return err;
}

} // namespace IO
