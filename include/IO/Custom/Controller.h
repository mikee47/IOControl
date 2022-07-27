/**
 * Custom/Controller.h
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

#include <IO/Controller.h>

namespace IO
{
namespace Custom
{
DECLARE_FSTR(CONTROLLER_CLASSNAME)

class Device;
class Request;

/**
 * @brief Controller for PWM output
 */
class Controller : public IO::Controller
{
	friend class Device;

public:
	Controller(uint8_t instance) : IO::Controller(instance)
	{
	}

	const FlashString& classname() const override
	{
		return CONTROLLER_CLASSNAME;
	}

	void handleEvent(IO::Request* request, Event event) override;
};

} // namespace Custom
} // namespace IO
