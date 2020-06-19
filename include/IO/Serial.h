#pragma once

#include "Error.h"
#include <driver/uart.h>
#include <espinc/uart_register.h>

namespace IO
{
class Serial
{
public:
	struct Config {
		uint32_t baudrate;
		uint8_t config; // uart config values such as start/stop bits, parity, etc.
	};

	virtual ~Serial()
	{
		close();
	}

	/**
	 * @brief Initialise the serial port with a default configuration
	 */
	Error open(uint8_t uart_nr);

	/**
	 * @brief Close the port
	 */
	void close();

	/**
	 * @brief Set required buffer sizes
	 *
	 * Serial buffers are expanded if required, but not reduced.
	 *
	 * @param rxSize
	 * @param txSize
	 * @retval bool true on success
	 */
	bool resizeBuffers(size_t rxSize, size_t txSize);

	void setCallback(uart_callback_t callback, void* param)
	{
		uart_set_callback(uart, callback, param);
	}

	void setBreak(bool state)
	{
		uart_set_break(uart, true);
	}

	size_t read(void* buffer, size_t size)
	{
		return uart_read(uart, buffer, size);
	}

	size_t write(const void* data, size_t len)
	{
		return uart_write(uart, data, len);
	}

	void swap(uint8_t txPin = 1)
	{
		uart_swap(uart, txPin);
	}

	void clear(uart_mode_t mode = UART_FULL)
	{
		uart_flush(uart, mode);
	}

	const Config& getConfig() const
	{
		return activeConfig;
	}

	void setConfig(const Config& cfg);

private:
	Config activeConfig{9600, UART_8N1};
	uart_t* uart{nullptr};
};

} // namespace IO
