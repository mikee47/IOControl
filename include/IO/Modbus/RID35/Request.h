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

// Register name and type
#define RID35_STDREG_MAP(XX)                                                                                           \
	XX(TotalActiveEnergy)                                                                                              \
	XX(ImportActiveEnergy)                                                                                             \
	XX(ExportActiveEnergy)                                                                                             \
	XX(TotalReactiveEnergy)                                                                                            \
	XX(ImportReactiveEnergy)                                                                                           \
	XX(ExportReactiveEnergy)                                                                                           \
	XX(ApparentEnergy)                                                                                                 \
	XX(ActivePower)                                                                                                    \
	XX(ReactivePower)                                                                                                  \
	XX(ApparentPower)                                                                                                  \
	XX(Voltage)                                                                                                        \
	XX(Current)                                                                                                        \
	XX(PowerFactor)                                                                                                    \
	XX(Frequency)                                                                                                      \
	XX(MaxDemandActivePower)                                                                                           \
	XX(MaxDemandReactivePower)                                                                                         \
	XX(MaxDemandApparentPower)

#define RID35_OVFREG_MAP(XX)                                                                                           \
	XX(TotalKwh)                                                                                                       \
	XX(ImportKwh)                                                                                                      \
	XX(ExportKwh)                                                                                                      \
	XX(TotalKvarh)                                                                                                     \
	XX(ImportKvarh)                                                                                                    \
	XX(ExportKvarh)                                                                                                    \
	XX(Kvah)

namespace IO
{
namespace Modbus
{
enum class RegisterType {
	Int16,
	Float32,
};

namespace RID35
{
enum class Register {
#define XX(name) name,
	RID35_STDREG_MAP(XX) RID35_OVFREG_MAP(XX)
#undef XX
};

constexpr uint16_t StdRegBase = 0x01;
constexpr uint16_t OvfRegBase = 0x96;
constexpr size_t StdRegCount = (1 + unsigned(Register::MaxDemandApparentPower)) * 2;
constexpr size_t OvfRegCount = 1 + unsigned(Register::Kvah) - unsigned(Register::TotalKwh);
constexpr size_t RegisterCount = StdRegCount + OvfRegCount;

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
	uint8_t regCount{0};
};

} // namespace RID35
} // namespace Modbus
} // namespace IO
