/**
 * Modbus/Debug.h
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

#include "ADU.h"
#include <Print.h>

namespace IO::Modbus
{
/**
 * @name Debug print support, outputs contents of packet
 * @{
 */
size_t printRequest(Print& p, const PDU& pdu);
size_t printRequest(Print& p, const ADU& adu);

size_t printResponse(Print& p, const PDU& pdu);
size_t printResponse(Print& p, const ADU& adu);
/** @} */

} // namespace IO::Modbus
