#include <IO/Serial.h>

namespace IO
{
Error Serial::open(uint8_t uart_nr)
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
	};
	uart_intr_config(uart, &intr_cfg);
	// Don't report 'buffer full' early, but only when buffer is actually full
	uart->rx_headroom = 0;

	// We handle interrupts and notify client as appropriate
	uart_set_callback(uart, uartCallback, this);

	return Error::success;
}

void Serial::close()
{
	uart_uninit(uart);
	uart = nullptr;
	currentState = nullptr;
}

bool Serial::initState(State& state)
{
	if(uart == nullptr || state.controller == nullptr) {
		return false;
	}

	// Expand buffers if required
	if(state.rxBufferSize > uart_rx_buffer_size(uart)) {
		uart_resize_rx_buffer(uart, state.rxBufferSize);
	}
	if(state.txBufferSize > uart_tx_buffer_size(uart)) {
		uart_resize_tx_buffer(uart, state.txBufferSize);
	}

	return true;
}

bool Serial::acquire(State& state, const Config& cfg)
{
	if(uart == nullptr) {
		return false;
	}

	/*
	 * If port has been acquired already, for example by MODBUS, then ask if it's OK to borrow it.
	 */
	if(currentState != nullptr) {
		if(currentState->config.mode != Mode::Shared) {
			debug_w("Port in use");
			return false;
		}
	}

	configure(cfg);

	if(&state != currentState) {
		state.config = cfg;
		state.previous = currentState;
		currentState = &state;
	}

	return true;
}

void Serial::release(State& state)
{
	if(&state == currentState) {
		return;
	}

	currentState = state.previous;
	state.previous = nullptr;
	if(currentState != nullptr) {
		configure(currentState->config);
	}
}

void Serial::configure(const Config& cfg)
{
	uart_set_config(uart, cfg.config);
	uart_set_baudrate(uart, cfg.baudrate);
}

void Serial::uartCallback(uart_t* uart, uint32_t status)
{
	auto serial = static_cast<Serial*>(uart_get_callback_param(uart));
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

} // namespace IO
