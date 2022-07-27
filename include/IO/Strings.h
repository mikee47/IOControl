/**
 * Strings.h
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

#pragma once

#include <FlashString/String.hpp>

namespace IO
{
// Global flash strings
#define IO_FLASHSTRING_MAP(XX)                                                                                         \
	XX(command)                                                                                                        \
	XX(function)                                                                                                       \
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
	XX(delay)                                                                                                          \
	XX(timeout)                                                                                                        \
	XX(interval)

#define XX(tag) DECLARE_FSTR(FS_##tag)
IO_FLASHSTRING_MAP(XX)
#undef XX

} // namespace IO
