#include <IO/RS485/Controller.h>
#include <IO/Request.h>
#include "Platform/System.h"
#include <driver/uart.h>

namespace IO
{
namespace RS485
{
// Device configuration
DEFINE_FSTR(CONTROLLER_CLASSNAME, "RS485")

constexpr unsigned TRANSACTION_TIMEOUT_MS = 800;

void Controller::start()
{
	serial.setCallback(uartCallback, this);
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

void Controller::uartCallback(uart_t* uart, uint32_t status)
{
	auto controller = static_cast<Controller*>(uart_get_callback_param(uart));
	// Guard against spurious interrupts
	if(controller == nullptr) {
		return;
	}

	// Tx FIFO empty
	if(status & UART_TXFIFO_EMPTY_INT_ST) {
		controller->setDirection(Direction::Incoming);
		if(controller->request != nullptr) {
			System.queueCallback(
				[](void* param) {
					auto req = static_cast<Request*>(param);
					req->handleEvent(Event::TransmitComplete);
				},
				controller->request);
		}
	}

	// Rx FIFO full or timeout
	if(status & (UART_RXFIFO_FULL_INT_ST | UART_RXFIFO_TOUT_INT_ST)) {
		// Wait a bit before processing the response. This enforces a minimum inter-message delay
		controller->timer
			.initializeMs<50>(
				[](void* param) {
					auto ctrl = static_cast<Controller*>(param);
					ctrl->receiveComplete();
				},
				controller)
			.startOnce();
	}
}

void Controller::handleEvent(Request* request, Event event)
{
	switch(event) {
	case Event::Execute:
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
		this->request = nullptr;
		serial.setConfig(m_savedConfig);
		break;

	case Event::Timeout: {
		//
		uint8_t buffer[256];
		auto receivedSize = serial.read(buffer, sizeof(buffer));
		debug_hex(INFO, "TIMEOUT", buffer, receivedSize);
		//

		debug_w("RS485: Timeout");
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
	// NUL pad so we don'final byte doesn't get cut off
	uint8_t nul{0};
	serial.write(&nul, 1);

	debug_i("MB: Sent %u bytes...", size);
}

} // namespace RS485
} // namespace IO