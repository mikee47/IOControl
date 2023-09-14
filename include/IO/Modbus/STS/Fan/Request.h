/**
 * Modbus/STS/Fan/Request.h
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

#include "../../Request.h"
#include "Device.h"
#include <Data/Range.h>

namespace IO::Modbus::STS::Fan
{
class Request : public Modbus::Request
{
public:
	Request(Device& device) : Modbus::Request(device)
	{
	}

	Device& getDevice() const
	{
		return static_cast<Device&>(device);
	}

	bool setNode(DevNode node) override
	{
		this->node = node;
		return true;
	}

	bool setValue(int value) override
	{
		this->value = TRange(0, 100).clip(value);
		return true;
	}

	Function fillRequestData(PDU::Data& data) override;
	ErrorCode callback(PDU& pdu) override;

	void getJson(JsonObject json) const override;

private:
	uint8_t index{0};
	uint8_t value;
	DevNode node;
};

} // namespace IO::Modbus::STS::Fan
