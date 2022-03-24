/**
 * Modbus/RID35/Request.h
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
 ****/

#pragma once

#include "../Request.h"
#include "Device.h"

namespace IO
{
namespace Modbus
{
namespace RID35
{
enum class Register {
	// Each value is a 32-bit float, occupying 2 16-bit modbus registers
	TotalActiveEnergy = 0x01,
	ImportActiveEnergy = 0x03,
	ExportActiveEnergy = 0x05,
	TotalReactiveEnergy = 0x07,
	ImportReactiveEnergy = 0x09,
	ExportReactiveEnergy = 0x0b,
	ApparentEnergy = 0x0d,
	ActivePower = 0x0f,
	ReactivePower = 0x11,
	ApparentPower = 0x13,
	Voltage = 0x15,
	Current = 0x17,
	PowerFactor = 0x19,
	Frequency = 0x1b,
	MaxDemandActivePower = 0x1d,
	MaxDemandReactivePower = 0x1f,
	MaxDemandApparentPower = 0x21,
	// Rollover registers
	TotalKwh = 0x96,
	ImportKwh = 0x97,
	ExportKwh = 0x98,
	TotalKvarh = 0x99,
	ImportKvarh = 0x9a,
	ExportKvarh = 0x9b,
	Kvah = 0x9c,
};

constexpr size_t RegisterCount = 34;
constexpr size_t RolloverRegisterCount = 7;

class Request : public Modbus::Request
{
public:
	Request(Device& device) : Modbus::Request(device)
	{
	}

	void getJson(JsonObject json) const override;

	const Device& getDevice() const
	{
		return static_cast<const Device&>(device);
	}

	Function fillRequestData(PDU::Data& data) override;
	ErrorCode callback(PDU& pdu) override;

private:
	uint16_t regValues[RegisterCount]{};
	uint16_t regCount{0};
};

} // namespace RID35
} // namespace Modbus
} // namespace IO
