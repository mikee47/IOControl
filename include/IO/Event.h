#pragma once

#include <WString.h>

namespace IO
{
/**
 * @brief Event code used during request processing for internal notification between request/device/controller objects
 */
#define IO_EVENT_MAP(XX)                                                                                               \
	XX(Execute)                                                                                                        \
	XX(TransmitComplete)                                                                                               \
	XX(ReceiveComplete)                                                                                                \
	XX(RequestComplete)                                                                                                \
	XX(Timeout)

enum class Event {
#define XX(tag) tag,
	IO_EVENT_MAP(XX)
#undef XX
};

String toString(Event event);

} // namespace IO
