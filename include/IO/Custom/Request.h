/**
 * Custom/Request.h
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

namespace IO::Custom
{
class Request : public IO::Request
{
public:
	Request(IO::Device& device) : IO::Request(device)
	{
	}

	Device& getDevice()
	{
		return static_cast<Device&>(device);
	}

	ErrorCode parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	bool setValue(int value) override
	{
		this->value = value;
		return true;
	}

	int getValue() const
	{
		return value;
	}

	DevNode getNode() const
	{
		return node;
	}

	bool setNode(DevNode node) override
	{
		this->node = node;
		return true;
	}

	void submit() override;

private:
	DevNode node;
	int value{0};
};

} // namespace IO::Custom
