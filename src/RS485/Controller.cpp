/**
 * RS485/Controller.cpp
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

#include <IO/RS485/Controller.h>
#include <IO/Request.h>
#include "Platform/System.h"
#include <driver/uart.h>

namespace IO
{
namespace RS485
{
// Device configuration
DEFINE_FSTR(CONTROLLER_CLASSNAME, "rs485")

constexpr unsigned TRANSACTION_TIMEOUT_MS = 800;

void Controller::start()
{
	serial.setCallback(uartCallbackStatic, this);
	request = nullptr;
	IO::Controller::start();
}

/*
 * Inherited classes call this before their own stop() code.
 */
void Controller::stop()
{
	IO::Controller::stop();
	serial.setCallback(nullptr, nullptr);
}

void Controller::uartCallbackStatic(smg_uart_t* uart, uint32_t status)
{
	auto controller = static_cast<Controller*>(smg_uart_get_callback_param(uart));
	// Guard against spurious interrupts
	if(controller != nullptr) {
		controller->uartCallback(status);
	}
}

void Controller::uartCallback(uint32_t status)
{
	// Tx FIFO empty
	if(status & UART_STATUS_TXFIFO_EMPTY) {
		setDirection(Direction::Incoming);
		if(request != nullptr) {
			System.queueCallback(
				[](void* param) {
					auto req = static_cast<Request*>(param);
					req->handleEvent(Event::TransmitComplete);
				},
				request);
		}
	}

	// Rx FIFO full or timeout
	if(status & (UART_STATUS_RXFIFO_FULL | UART_STATUS_RXFIFO_TOUT)) {
		// Wait a bit before processing the response. This enforces a minimum inter-message delay
		timer.initializeMs<50>(
			[](void* param) {
				auto ctrl = static_cast<Controller*>(param);
				ctrl->receiveComplete();
			},
			this);
		timer.startOnce();
	}
}

void Controller::handleEvent(Request* request, Event event)
{
	switch(event) {
	case Event::Execute:
		this->request = request;
		// Put a timeout on the overall transaction
		timer.initializeMs<TRANSACTION_TIMEOUT_MS>(
			[](void* param) {
				auto req = static_cast<Request*>(param);
				req->handleEvent(Event::Timeout);
			},
			request);
		timer.startOnce();
		m_savedConfig = serial.getConfig();
		break;

	case Event::RequestComplete:
		timer.stop();
		setDirection(IO::Direction::Idle);
		this->request = nullptr;
		serial.setConfig(m_savedConfig);
		break;

	case Event::Timeout: {
		uint8_t buffer[256];
		auto receivedSize = serial.read(buffer, sizeof(buffer));
		if(receivedSize != 0) {
			debug_hex(INFO, "TIMEOUT", buffer, receivedSize);
		}
		debug_w("[RS485] Request '%s' timeout", request->caption().c_str());
		break;
	}

	case Event::TransmitComplete:
	case Event::ReceiveComplete:
		break;
	}

	IO::Controller::handleEvent(request, event);
}

void Controller::receiveComplete()
{
	if(request == nullptr) {
		handleIncomingRequest();
	} else {
		request->handleEvent(Event::ReceiveComplete);
	}
}

void Controller::send(const void* data, size_t size)
{
	setDirection(Direction::Outgoing);
	serial.write(data, size);
	// NUL pad so final byte doesn't get cut off
	uint8_t nul{0};
	serial.write(&nul, 1);

	debug_i("MB: Sent %u bytes...", size);
}

} // namespace RS485
} // namespace IO