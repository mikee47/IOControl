#pragma once

#include "../Control.h"
#include <driver/uart.h>
#include <espinc/uart_register.h>

namespace IO
{
namespace Serial
{
enum class Mode {
	Shared,
	Exclusive,
};

// Config which is swapped about when a controller acquires the port
struct Config {
	Mode mode;
	uint8_t config; // uart config values such as start/stop bits, parity, etc.
	uint32_t baudrate;
};

struct State;

// Called from interrupt context so implementations must be marked IRAM_ATTR
using Callback = void (*)(State& state);

// Contains fixed initialisation data plus state for a client
struct State {
	Controller* controller;
	Callback onTransmitComplete;
	Callback onReceive;
	// Used internally by Serial
	Config config;
	State* previousState;
};

/**
 * @brief Class to arbitrate serial port access
 *
 * This allows multiple protocol controllers to share the same serial port.
 * One controller may also be nominated to handle un-solicited messages.
 *
 * An example scenerios is where DMX512 and MODBUS devices share the same RS485 network.
 *
 * This class may be overridden to implement custom behaviours such as enabling MAX485 transmit.
 * Multiple segments can be implemented by multiplexing a single serial port amongst several
 * MAX485 devices, so that selection may also be handled via subclassing.
 *
 * The port instances is then passed to the relevant controllers during setup.
 */
class Port
{
public:
	virtual ~Port()
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

	/**
	 * @brief Request use of the port for a transaction
	 */
	virtual bool acquire(State& state, const Config& cfg);

	/**
	 * @brief Release port for others to use
	 */
	virtual void release(State& state);

	/**
	 * @brief Callback to handle transmit/receive hardware selection
	 * Typically called from interrupt context so implementation MUST be marked IRAM_ATTR
	 * @param enable true to transmit, false to switch back to receive
	 */
	using TransmitCallback = void (*)(bool enable);

	/**
	 * @brief Set the transmit callback handler
	 *
	 * Typically with RS485 a GPIO is used to toggle between transmit/receive modes.
	 * Using a callback allows flexibility for your particular hardware implementation.
	 * You don't need to set this up if your hardware handles the switch automatically.
	 *
	 * @param callback
	 */
	void onTransmit(TransmitCallback callback)
	{
		transmitCallback = callback;
	}

	/**
	 * @brief Whilst a port is acquired, call this method to being or end transmission
	 * @param enable true to transmit, false to receive
	 * @note Port should be left in receive mode on request completion
	 */
	void IRAM_ATTR setTransmit(bool enable)
	{
		if(transmitCallback != nullptr) {
			transmitCallback(enable);
		}
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

private:
	void configure(const Config& cfg);
	static void IRAM_ATTR uartCallback(uart_t* uart, uint32_t status);

	uart_t* uart{};
	State* currentState{}; ///< Not owned
	Mode mode{};
	TransmitCallback transmitCallback{};
};

} // namespace Serial
} // namespace IO
