/**
 * Event.cpp
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

#include <IO/Event.h>
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
