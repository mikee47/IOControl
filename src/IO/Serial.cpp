#include <IO/Serial.h>

namespace IO
{
ErrorCode Serial::open(uint8_t uart_nr)
{
	if(uart != nullptr) {
		return Error::access_denied;
	}

	smg_uart_config cfg{
		.uart_nr = uart_nr,
		.tx_pin = 1,
		.mode = UART_FULL,
		.baudrate = activeConfig.baudrate,
		.config = activeConfig.config,
	};
	uart = smg_uart_init_ex(cfg);
	if(uart == nullptr) {
		return Error::bad_config;
	}

	smg_uart_intr_config_t intr_cfg{
		// Allow a suitable timeout for receive packets
		.rx_timeout_thresh = 16,
		// Don't callback unless FIFO is actually empty
		.txfifo_empty_intr_thresh = 0,
		// Use max value
		.rxfifo_full_thresh = 0xff,
	};
	smg_uart_intr_config(uart, &intr_cfg);
	// Don't report 'buffer full' early, but only when buffer is actually full
	uart->rx_headroom = 0;

	return Error::success;
}

void Serial::close()
{
	smg_uart_uninit(uart);
	uart = nullptr;
}

bool Serial::resizeBuffers(size_t rxSize, size_t txSize)
{
	if(uart == nullptr) {
		return false;
	}

	// Expand buffers if required
	if(rxSize > smg_uart_rx_buffer_size(uart) && !smg_uart_resize_rx_buffer(uart, rxSize)) {
		debug_e("Serial: Failed to set RX buffer size to %u", rxSize);
		return false;
	}
	if(txSize > smg_uart_tx_buffer_size(uart) && !smg_uart_resize_tx_buffer(uart, txSize)) {
		debug_e("Serial: Failed to set TX buffer size to %u", txSize);
		return false;
	}

	return true;
}

void Serial::setConfig(const Config& cfg)
{
	smg_uart_set_config(uart, cfg.config);
	smg_uart_set_baudrate(uart, cfg.baudrate);
}

} // namespace IO
