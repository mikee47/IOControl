/**
 * RFSwitch/Request.h
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

namespace IO
{
namespace RFSwitch
{
class Request : public IO::Request
{
	friend Device;
	friend Controller;

public:
	Request(IO::Device& device) : IO::Request(device)
	{
		// Only one command applicable, may as well be the default
		setCommand(Command::set);
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(IO::Request::device());
	}

	ErrorCode parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	/*
	 * We'll get called with NODES_ALL because no nodes are explicitly specified.
	 */
	bool setNode(DevNode node) override
	{
		return node == DevNode_ALL;
	}

	void send(uint32_t code, uint8_t repeats = 0);

	uint32_t code() const
	{
		return m_code;
	}

	uint8_t repeats() const
	{
		return m_repeats;
	}

protected:
	void callback()
	{
		// No response to interpret here, so we're done
		complete(Error::success);
	}

private:
	uint32_t m_code{0};
	uint8_t m_repeats{0};
};

} // namespace RFSwitch
} // namespace IO
