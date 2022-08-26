/**
 * Modbus/STS/Fan/Device.h
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
 *
 * STS Fan Controller - see samples/Fan
 *
 ****/

#pragma once

#include "../../Device.h"

namespace IO
{
namespace Modbus
{
namespace STS
{
namespace Fan
{
const size_t channelCount{3};

struct FanData {
	uint16_t rpm[channelCount];
	uint8_t speed[channelCount];
};

class Device : public Modbus::Device
{
	friend class Request;

public:
	class Factory : public FactoryTemplate<Device>
	{
	public:
		const FlashString& deviceClass() const override
		{
			return FS("sts/fan");
		}
	};

	static const Factory factory;

	using Modbus::Device::Device;

	IO::Request* createRequest() override;

	const FanData& getData() const
	{
		return data;
	}

	uint16_t maxNodes() const override
	{
		return channelCount;
	}

	void getValues(JsonObject json) const;

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	FanData data{};
};

} // namespace Fan
} // namespace STS
} // namespace Modbus
} // namespace IO
