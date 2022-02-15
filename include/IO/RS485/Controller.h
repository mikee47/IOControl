/**
 * RS485/Controller.h
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
#include <IO/Serial.h>
#include <SimpleTimer.h>

namespace IO
{
namespace RS485
{
DECLARE_FSTR(CONTROLLER_CLASSNAME)

class Controller : public IO::Controller
{
public:
	Controller(Serial& serial, uint8_t instance) : IO::Controller(instance), serial(serial)
	{
	}

	const FlashString& classname() const override
	{
		return CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

	/**
	 * @brief Callback to handle hardware transmit/receive selection
	 * Typically called from interrupt context so implementation MUST be marked IRAM_ATTR
	 * @param segment Identifies physical connection for shared/multiplexed port
	 * @param direction
	 */
	using SetDirectionCallback = void (*)(uint8_t segment, Direction direction);

	/**
	 * @brief Set the transmit callback handler
	 *
	 * Typically with RS485 a GPIO is used to toggle between transmit/receive modes.
	 * Using a callback allows flexibility for your particular hardware implementation.
	 * You don't need to set this up if your hardware handles the switch automatically.
	 *
	 * @param callback
	 */
	void onSetDirection(SetDirectionCallback callback)
	{
		setDirectionCallback = callback;
	}

	/**
	 * @brief Whilst a port is acquired, call this method to being or end transmission
	 * @param enable true to transmit, false to receive
	 * @note Port should be left in receive mode on request completion
	 */
	void IRAM_ATTR setDirection(IO::Direction direction)
	{
		if(setDirectionCallback != nullptr) {
			setDirectionCallback(segment, direction);
		}
	}

	Serial& getSerial()
	{
		return serial;
	}

	void handleEvent(Request* request, Event event) override;

	using OnRequestDelegate = Delegate<void(Controller& controller)>;

	void onRequest(OnRequestDelegate callback)
	{
		requestCallback = callback;
	}

	void setSegment(uint8_t segment)
	{
		this->segment = segment;
	}

	void send(const void* data, size_t size);

protected:
	virtual void handleIncomingRequest()
	{
		if(requestCallback) {
			requestCallback(*this);
		}
	}

private:
	static void IRAM_ATTR uartCallbackStatic(smg_uart_t* uart, uint32_t status);
	void IRAM_ATTR uartCallback(uint32_t status);
	void receiveComplete();

private:
	Serial& serial;
	SetDirectionCallback setDirectionCallback{nullptr};
	Request* request{nullptr}; ///< Current outgoing request (if any)
	uint8_t segment{0};		   ///< Active bus segment
	OnRequestDelegate requestCallback;
	SimpleTimer timer; ///< Use to schedule callback and timeout
	Serial::Config m_savedConfig{};
};

} // namespace RS485
} // namespace IO
