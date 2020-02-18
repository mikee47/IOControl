#include <IO/Serial/Port.h>

namespace IO
{
namespace Serial
{
Error Port::open(uint8_t uart_nr)
{
	if(uart != nullptr) {
		return Error::access_denied;
	}

	uart_config cfg{
		.uart_nr = uart_nr,
		.tx_pin = 1,
		.mode = UART_FULL,
		.baudrate = 9600,
		.config = UART_8N1,
	};
	uart = uart_init_ex(cfg);
	if(uart == nullptr) {
		return Error::bad_config;
	}

	uart_intr_config_t intr_cfg{
		// Allow a suitable timeout for receive packets
		.rx_timeout_thresh = 16,
		// Don't callback unless FIFO is actually empty
		.txfifo_empty_intr_thresh = 0,
		// Use max value
		.rxfifo_full_thresh = 0xff,
	};
	uart_intr_config(uart, &intr_cfg);
	// Don't report 'buffer full' early, but only when buffer is actually full
	uart->rx_headroom = 0;

	// We handle interrupts and notify client as appropriate
	uart_set_callback(uart, uartCallback, this);

	return Error::success;
}

void Port::close()
{
	uart_uninit(uart);
	uart = nullptr;
	currentState = nullptr;
}

bool Port::resizeBuffers(size_t rxSize, size_t txSize)
{
	if(uart == nullptr) {
		return false;
	}

	// Expand buffers if required
	if(rxSize > uart_rx_buffer_size(uart) && !uart_resize_rx_buffer(uart, rxSize)) {
		debug_e("Serial: Failed to set RX buffer size to %u", rxSize);
		return false;
	}
	if(txSize > uart_tx_buffer_size(uart) && !uart_resize_tx_buffer(uart, txSize)) {
		debug_e("Serial: Failed to set TX buffer size to %u", txSize);
		return false;
	}

	return true;
}

bool Port::acquire(State& state, const Config& cfg)
{
	if(uart == nullptr) {
		debug_e("Serial: Port not open");
		return false;
	}

	if(mode == Mode::Exclusive) {
		debug_w("Serial: Port in use");
		return false;
	}

	mode = cfg.mode;
	state.previousState = currentState;
	currentState = &state;
	configure(cfg);
	state.config = cfg;

	return true;
}

void Port::release(State& state)
{
	currentState = state.previousState;
	if(currentState != nullptr && &state != currentState) {
		state.previousState = nullptr;
		configure(currentState->config);
	}
	mode = Mode::Shared;
}

void Port::configure(const Config& cfg)
{
	uart_set_config(uart, cfg.config);
	uart_set_baudrate(uart, cfg.baudrate);
}

void Port::uartCallback(uart_t* uart, uint32_t status)
{
	auto serial = static_cast<Serial::Port*>(uart_get_callback_param(uart));
	// Guard against spurious interrupts
	if(serial == nullptr) {
		return;
	}

	auto state = serial->currentState;
	if(state == nullptr) {
		return;
	}

	// Tx FIFO empty
	if(status & UART_TXFIFO_EMPTY_INT_ST) {
		if(state->onTransmitComplete) {
			state->onTransmitComplete(*state);
		}
	}

	// Rx FIFO full or timeout
	if(status & (UART_RXFIFO_FULL_INT_ST | UART_RXFIFO_TOUT_INT_ST)) {
		if(state->onReceive) {
			state->onReceive(*state);
		}
	}
}

} // namespace Serial
} // namespace IO
