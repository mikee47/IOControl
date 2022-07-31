/**
 * Modbus/NT18B07/Device.h
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
 * Rayleigh Instruments RI-D35 energy meter
 * ========================================
 *
 * Device registers are read-only.
 * The Node ID corresponds to the register address.
 * 
 ****/

#pragma once

#include "../Device.h"

namespace IO
{
namespace Modbus
{
namespace NT18B07
{
const size_t channelCount{7};
using TempData = int16_t[channelCount];

class Device : public Modbus::Device
{
	friend class Request;

public:
	class Factory : public FactoryTemplate<Device>
	{
	public:
		const FlashString& deviceClass() const override
		{
			return FS("nt18b07");
		}
	};

	static const Factory factory;

	/**
	 * @brief NT18B07 device configuration
	 */
	struct Config {
		Modbus::Device::Config modbus; ///< Basic modbus configuration
		/* Compensate channel: Tout = a * Tin + b */
		struct Comp {
			int8_t a;
			int8_t b;
		};
		using CompArray = Comp[channelCount];
		CompArray comp;
	};

	using Modbus::Device::Device;

	ErrorCode init(const Config& config);
	ErrorCode init(JsonObjectConst config) override;

	IO::Request* createRequest() override;

	int16_t getRawValue(uint8_t channel) const
	{
		return (channel < channelCount) ? values[channel] : 0;
	}

	/**
	 * @brief Get temperature value in 0.1C increments (avoids floating point)
	 */
	int16_t getIntValue(uint8_t channel) const;

	float getValue(uint8_t channel) const
	{
		return getIntValue(channel) / 10.0;
	}

	void getRawValues(JsonArray json) const;
	void getValues(TempData& data) const;
	void getValues(JsonArray json) const;

	uint16_t maxNodes() const override
	{
		return channelCount;
	}

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

	void updateValues(const void* values, size_t count)
	{
		memcpy(this->values, values, std::min(count, channelCount) * sizeof(int16_t));
	}

	TempData values{-111, -222, -333, -444, -555, -666, -777};
	Config::CompArray comp;
};

} // namespace NT18B07
} // namespace Modbus
} // namespace IO
