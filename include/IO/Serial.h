#pragma once

#include "Error.h"
#include <driver/uart.h>

namespace IO
{
class Serial
{
public:
	struct Config {
		uint32_t baudrate;
		smg_uart_format_t format;
	};

	virtual ~Serial()
	{
		close();
	}

	/**
	 * @brief Initialise the serial port with a default configuration
	 */
	ErrorCode open(uint8_t uart_nr);

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

	void setCallback(smg_uart_callback_t callback, void* param)
	{
		smg_uart_set_callback(uart, callback, param);
	}

	void setBreak(bool state)
	{
		smg_uart_set_break(uart, true);
	}

	size_t read(void* buffer, size_t size)
	{
		return smg_uart_read(uart, buffer, size);
	}

	size_t write(const void* data, size_t len)
	{
		return smg_uart_write(uart, data, len);
	}

	void swap(uint8_t txPin = 1)
	{
		smg_uart_swap(uart, txPin);
	}

	void clear(smg_uart_mode_t mode = UART_FULL)
	{
		smg_uart_flush(uart, mode);
	}

	const Config& getConfig() const
	{
		return activeConfig;
	}

	void setConfig(const Config& cfg);

private:
	Config activeConfig{9600, UART_8N1};
	smg_uart_t* uart{nullptr};
};

} // namespace IO
