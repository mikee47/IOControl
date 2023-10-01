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

#include <IO/RS485/Device.h>
#include <IO/RS485/Request.h>
#include "Platform/System.h"
#include <driver/uart.h>

// ESP32 has a TX_DONE interrupt, others do not
#ifdef ARCH_ESP32
#define USE_TXDONE_INTR
#endif

namespace IO::RS485
{
// Device configuration
DEFINE_FSTR(CONTROLLER_CLASSNAME, "rs485")

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

void IRAM_ATTR Controller::uartCallbackStatic(smg_uart_t* uart, uint32_t status)
{
	auto controller = static_cast<Controller*>(smg_uart_get_callback_param(uart));
	// Guard against spurious interrupts
	if(controller != nullptr) {
		controller->uartCallback(status);
	}
}

void Controller::staticTransmitComplete(void* param)
{
	auto ctrl = static_cast<Controller*>(param);
	if(ctrl->request && ctrl->flags[Flag::transmit_complete]) {
		ctrl->request->handleEvent(Event::TransmitComplete);
	}
}

void IRAM_ATTR Controller::uartCallback(uint32_t status)
{
#ifdef USE_TXDONE_INTR
	if(status & UART_STATUS_TX_DONE) {
#else
	if(status & UART_STATUS_TXFIFO_EMPTY) {
#endif
		setDirection(request == nullptr ? Direction::Idle : Direction::Incoming);
		status = 0;
		// Guard against timeout firing before this callback
		if(request != nullptr && !flags[Flag::transmit_complete]) {
			flags += Flag::transmit_complete;
			System.queueCallback(staticTransmitComplete, this);
		}
		resetTransactionTime();
	}

	// Rx FIFO full or timeout
	if(status & (UART_STATUS_RXFIFO_FULL | UART_STATUS_RXFIFO_TOUT)) {
		flags += Flag::data_received;
		// Allow more time, there could be more data inbound
		timer.startOnce();
		resetTransactionTime();
	}
}

void Controller::staticTimerHandler(void* param)
{
	auto ctrl = static_cast<Controller*>(param);
	ctrl->timerHandler();
}

void Controller::handleEvent(IO::Request* request, Event event)
{
	switch(event) {
	case Event::Execute: {
		this->request = request;
		flags = 0;
		auto& device = static_cast<const Device&>(request->device);
		// Put a timeout on the overall transaction
		timer.initializeMs(device.timeout(), staticTimerHandler, this);
		timer.startOnce();
		savedConfig = serial.getConfig();
		break;
	}

	case Event::RequestComplete:
		timer.stop();
		setDirection(Direction::Idle);
		this->request = nullptr;
		serial.setConfig(savedConfig);
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

void Controller::timerHandler()
{
	if(flags[Flag::data_received]) {
		flags = 0;
		if(request == nullptr) {
			handleIncomingRequest();
		} else {
			request->handleEvent(Event::ReceiveComplete);
		}
		return;
	}

	flags = 0;
	request->handleEvent(Event::Timeout);
}

void Controller::send(const void* data, size_t size)
{
	setDirection(Direction::Outgoing);
	serial.write(data, size);
#ifndef USE_TXDONE_INTR
#ifdef ARCH_RP2040
#define NULPADLEN 5
#else
#define NULPADLEN 1
#endif
	// NUL pad so final byte doesn't get cut off
	uint8_t nul[NULPADLEN]{};
	serial.write(nul, NULPADLEN);
#endif

	debug_d("MB: Sent %u bytes...", size);
}

} // namespace IO::RS485
