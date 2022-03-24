/**
 * Modbus/RID35/Device.h
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
namespace RID35
{
class Device : public Modbus::Device
{
public:
	class Factory : public FactoryTemplate<Device>
	{
	public:
		const FlashString& deviceClass() const override
		{
			return FS("rid35");
		}
	};

	static const Factory factory;

	using Modbus::Device::Device;

	IO::Request* createRequest() override;

	DevNode::ID nodeIdMin() const override
	{
		return 0x01;
	}

	uint16_t maxNodes() const override
	{
		return 255;
	}
};

} // namespace RID35
} // namespace Modbus
} // namespace IO
