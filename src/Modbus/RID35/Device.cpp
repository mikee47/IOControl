/**
 * Modbus/RID35/Device.cpp
 *
 * Created on: 24 March 2022
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

#include <IO/Modbus/RID35/Device.h>
#include <IO/Modbus/RID35/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
namespace RID35
{
const Device::Factory Device::factory;

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

} // namespace RID35
} // namespace Modbus
} // namespace IO
