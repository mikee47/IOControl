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
namespace RID35
{
enum class Register {
#define XX(name) name,
	RID35_STDREG_MAP(XX) RID35_OVFREG_MAP(XX)
#undef XX
		MAX
};

constexpr uint16_t stdRegBase = 0x01;
constexpr uint16_t ovfRegBase = 0x96;
constexpr size_t stdRegCount = (1 + unsigned(Register::MaxDemandApparentPower)) * 2;
constexpr size_t ovfRegCount = 1 + unsigned(Register::Kvah) - unsigned(Register::TotalKwh);
constexpr size_t registerCount = stdRegCount + ovfRegCount;

class Device : public Modbus::Device
{
	friend class Request;

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

	bool isValid() const
	{
		return regValid;
	}

	uint32_t getRawValue(Register reg) const;
	float getValue(Register reg) const;
	void getValues(JsonObject json) const;

private:
	// values may be unaligned (from packed structure) so this avoids compiler errors
	void updateRegisters(const void* values, size_t count);

	uint16_t regValues[registerCount]{};
	bool regValid{false};
};

} // namespace RID35
} // namespace Modbus
} // namespace IO
