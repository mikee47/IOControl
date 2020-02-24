#include "Event.h"
#include <FlashString/Vector.hpp>

namespace IO
{
#define XX(tag) DEFINE_FSTR_LOCAL(evtstr_##tag, #tag)
IO_EVENT_MAP(XX)
#undef XX

#define XX(tag) &evtstr_##tag,
DEFINE_FSTR_VECTOR(eventStrings, FSTR::String, IO_EVENT_MAP(XX))
#undef XX

String toString(Event event)
{
	return eventStrings[unsigned(event)];
}

} // namespace IO
