/**
 * Modbus/Function.cpp
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

#include <IO/Modbus/Function.h>
#include <FlashString/Map.hpp>

using namespace IO::Modbus;

namespace
{
#define XX(tag, value) DEFINE_FSTR_LOCAL(str_##tag, #tag)
MODBUS_FUNCTION_MAP(XX)
#undef XX

#define XX(tag, value) {Function::tag, &str_##tag},
DEFINE_FSTR_MAP_LOCAL(map, Function, FSTR::String, MODBUS_FUNCTION_MAP(XX))
#undef XX

} // namespace

String toString(IO::Modbus::Function function)
{
	auto v = map[function];
	return v ? String(v) : F("Unknown_") + String(unsigned(function));
}
