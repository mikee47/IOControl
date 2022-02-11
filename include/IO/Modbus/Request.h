/**
 * Modbus/Request.h
 *
 *  Created on: 1 May 2018
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

#include <IO/Request.h>
#include "Device.h"
#include "PDU.h"

namespace IO
{
namespace Modbus
{
class Request : public IO::Request
{
public:
	Request(Device& device) : IO::Request(device)
	{
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(IO::Request::device());
	}

	virtual Function fillRequestData(PDU::Data& data) = 0;

	/**
	 * @brief Process a received PDU
	 * @param pdu
	 * @retval ErrorCode If request is re-submitted, return Error::pending,
	 * otherwise request will be completed with given error.
	 */
	virtual ErrorCode callback(PDU& pdu) = 0;
};

} // namespace Modbus
} // namespace IO
