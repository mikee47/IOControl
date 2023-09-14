/**
 * Custom/Device.h
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

#include <IO/Device.h>
#include "Controller.h"

namespace IO::Custom
{
class Request;

class Device : public IO::Device
{
	friend Request;

public:
	template <class DeviceClass> class FactoryTemplate : public IO::Device::Factory
	{
	public:
		IO::Device* createDevice(IO::Controller& controller, const char* id) const override
		{
			return new DeviceClass(static_cast<Controller&>(controller), id);
		}

		const FlashString& controllerClass() const override
		{
			return CONTROLLER_CLASSNAME;
		}
	};

	using IO::Device::Device;

	const DeviceType type() const override
	{
		return DeviceType::Custom;
	}

	IO::Request* createRequest() override;

	virtual int getNodeValue(IO::DevNode node) const
	{
		return 0;
	}

protected:
	virtual ErrorCode execute(Request& request) = 0;
	virtual void getRequestJson(const Request& request, JsonObject json) const;

private:
	uint32_t param1{0};
	uint32_t param2{0};
};

} // namespace IO::Custom
