/*
 * IODMX512.h
 *
 *  Created on: 5 November 2018
 *      Author: mikee47
 *
 */

#pragma once

#include <IO/Request.h>
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

	Device& device()
	{
		return reinterpret_cast<Device&>(IO::Request::device());
	}

	Error parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	bool setNode(DevNode node) override;

	DevNode node() const
	{
		return m_node;
	}

	bool nodeAdjust(DevNode node, int value) override
	{
		setCommand(Command::adjust);
		setValue(value);
		return setNode(node);
	}

	void setValue(int code)
	{
		m_value = code;
	}

	int value() const
	{
		return m_value;
	}

	Error submit() override;

private:
	int m_value{};
	DevNode m_node{};
};

} // namespace DMX512
} // namespace IO
