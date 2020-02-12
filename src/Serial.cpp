#include <IO/Serial.h>
#include <driver/uart.h>
#include <espinc/uart_register.h>

namespace IO
{
static void IRAM_ATTR uart_callback(uart_t* uart, uint32_t status)
{
	auto serial = static_cast<Serial*>(uart_get_callback_param(uart));
	if(serial == nullptr) {
		return;
	}

	//	serial->currentState
}

bool Serial::open(uint8_t uart_nr)
{
	close();

	uart_config cfg{
		.uart_nr = uart_nr,
		.tx_pin = 1,
		.mode = UART_FULL,
		.baudrate = 9600,
		.config = UART_8N1,
	};
	uart = uart_init_ex(cfg);
	if(uart == nullptr) {
		debug_e("UART init failed");
		return false;
	}

	return true;
}

void Serial::close()
{
	uart_uninit(uart);
	uart = nullptr;
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

uart_t* Serial::acquire(State& state, const Config& cfg)
{
	if(uart == nullptr) {
		return nullptr;
	}

	/*
	 * If port has been acquired already, for example by MODBUS, then ask if it's OK to borrow it.
	 *
	 */
	if(currentState != nullptr) {
		if(!currentState->controller->allowPortAccess(state->controller, cfg.info)) {
			debug_w("Port in use");
			return nullptr;
		}
	}

	configure(cfg);

	state.interrupt = cfg.info.interrupt;
	state.oldState = currentState;
	currentState = &state;

	return uart;
}

void Serial::release(State& state)
{
	currentState = state.oldState;
	if(currentState != nullptr) {
		configure(currentState->config);
	}
}

void Serial::configure(Config& cfg)
{
	uart_set_config(uart, cfg.config);
	uart_set_baudrate(uart, cfg.baudrate);
}

} // namespace IO
