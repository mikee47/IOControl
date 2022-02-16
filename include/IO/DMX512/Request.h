/**
 * DMX512/Request.h
 *
 *  Created on: 5 November 2018
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

namespace IO
{
namespace DMX512
{
class Request : public IO::Request
{
public:
	Request(Device& device) : IO::Request(static_cast<IO::Device&>(device))
	{
	}

	Device& getDevice()
	{
		return static_cast<Device&>(device);
	}

	ErrorCode parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	bool setNode(DevNode node) override;

	DevNode node() const
	{
		return devNode;
	}

	bool nodeAdjust(DevNode node, int value) override
	{
		setCommand(Command::adjust);
		setValue(value);
		return setNode(node);
	}

	void setValue(int newValue)
	{
		value = newValue;
	}

	int getValue() const
	{
		return value;
	}

	void submit() override;

private:
	int value{};
	DevNode devNode{};
};

} // namespace DMX512
} // namespace IO
