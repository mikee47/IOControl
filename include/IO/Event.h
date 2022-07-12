/**
 * Event.h
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

} // namespace IO

String toString(IO::Event event);
