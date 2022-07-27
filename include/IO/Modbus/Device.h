/**
 * Modbus/Device.h
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

#include "../RS485/Device.h"
#include "ADU.h"

namespace IO
{
namespace Modbus
{
class Request;

/**
 * @brief A virtual device, represents a modbus slave device.
 *
 * Actual devices must implement:
 *
 *   - createRequest()
 *   - callback()
 *   - fillRequestData()
 */
class Device : public RS485::Device
{
public:
	class Factory : public FactoryTemplate<Device>
	{
	public:
		const FlashString& deviceClass() const override
		{
			return FS("modbus");
		}
	};

	static const Factory factory;

	using RS485::Device::Device;

	ErrorCode init(const RS485::Device::Config& config);

	const DeviceType type() const override
	{
		return DeviceType::Modbus;
	}

	/**
	 * @brief Handle a broadcast message
	 */
	virtual void onBroadcast(const ADU& adu)
	{
	}

	/**
	 * @brief Handle a message specifically for this device
	 */
	virtual void onRequest(ADU& adu)
	{
	}

	IO::Request* createRequest() override;

	void handleEvent(IO::Request* request, Event event) override;

	using TransferCallback = void (*)(const void* data, size_t length, bool send);

	static void onTransfer(TransferCallback callback)
	{
		transferCallback = callback;
	}

private:
	ErrorCode execute(Request* request);
	ErrorCode readResponse(Request* request);

	static TransferCallback transferCallback;
	Function requestFunction{};
};

} // namespace Modbus
} // namespace IO
