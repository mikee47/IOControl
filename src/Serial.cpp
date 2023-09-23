/**
 * Serial.cpp
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

#include <IO/Serial.h>

namespace IO
{
ErrorCode Serial::open(uint8_t uart_nr, uint8_t txPin, uint8_t rxPin)
{
	if(uart != nullptr) {
		return Error::access_denied;
	}

	smg_uart_config_t cfg{
		.uart_nr = uart_nr,
		.tx_pin = txPin,
		.rx_pin = rxPin,
		.mode = UART_FULL,
		.options = 0,
		.baudrate = activeConfig.baudrate,
		.format = activeConfig.format,
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
#ifdef ARCH_ESP32
		.intr_mask = UART_STATUS_TX_DONE,
		.intr_enable = UART_STATUS_TX_DONE,
#endif
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
	if(activeConfig == cfg) {
		return;
	}
	smg_uart_set_format(uart, cfg.format);
	smg_uart_set_baudrate(uart, cfg.baudrate);
	activeConfig  = cfg;
}

} // namespace IO
