/**
 * Modbus/NT18B07/Request.h
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

#include "../Request.h"
#include "Device.h"

namespace IO::Modbus::NT18B07
{
class Request : public Modbus::Request
{
public:
	Request(Device& device) : Modbus::Request(device)
	{
	}

	void getJson(JsonObject json) const override;

	Device& getDevice() const
	{
		return static_cast<Device&>(device);
	}

	Function fillRequestData(PDU::Data& data) override;
	ErrorCode callback(PDU& pdu) override;
};

} // namespace IO::Modbus::NT18B07
