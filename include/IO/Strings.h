#pragma once

#include <FlashString/String.hpp>

namespace IO
{
// Global flash strings
#define IO_FLASHSTRING_MAP(XX)                                                                                         \
	XX(command)                                                                                                        \
	XX(name)                                                                                                           \
	XX(device)                                                                                                         \
	XX(devices)                                                                                                        \
	XX(id)                                                                                                             \
	XX(address)                                                                                                        \
	XX(segment)                                                                                                        \
	XX(baudrate)                                                                                                       \
	XX(value)                                                                                                          \
	XX(channels)                                                                                                       \
	XX(states)                                                                                                         \
	XX(class)                                                                                                          \
	XX(controller)                                                                                                     \
	XX(status)                                                                                                         \
	XX(success)                                                                                                        \
	XX(pending)                                                                                                        \
	XX(error)                                                                                                          \
	XX(unknown)                                                                                                        \
	XX(code)                                                                                                           \
	XX(text)                                                                                                           \
	XX(arg)                                                                                                            \
	XX(node)                                                                                                           \
	XX(nodes)                                                                                                          \
	XX(devnodes)                                                                                                       \
	XX(count)                                                                                                          \
	XX(delay)

#define XX(tag) DECLARE_FSTR(FS_##tag)
IO_FLASHSTRING_MAP(XX)
#undef XX

} // namespace IO
