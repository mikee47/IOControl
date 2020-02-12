#pragma once

#include "Control.h"
#include <driver/uart.h>

namespace IO
{
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
class Serial
{
public:
	// Config which is swapped about when a controller acquires the port
	struct Config {
		PortInfo info;
		uint16_t baudrate;
		uint8_t config; // uart config values such as start/stop bits, parity, etc.
	};

	// Contains fixed initialisation data plus state for a client
	struct State {
		Controller* controller;
		uart_callback_t callback;
		void* callbackParam;
		uint16_t rxBufferSize;
		uint16_t txBufferSize;
		// Used internally by Serial
		PortInfo::Mode interrupt;
		Config config;
	};

	virtual ~Serial()
	{
		close();
	}

	/**
	 * @brief Initialise the serial port with a default configuration
	 */
	bool open(uint8_t uart_nr);

	/**
	 * @brief Close the port
	 */
	void close();

	/**
	 * @brief Initialise a state object
	 * @param state Caller should initialise this
	 * @retval bool true on success
	 */
	bool initState(State& state);

	/**
	 * @brief Request use of the port for a transaction
	 */
	virtual uart_t* acquire(State& state, const Config& cfg);

	/**
	 * @brief Release port for others to use
	 */
	virtual void release(State* state);

	/**
	 * @brief Whilst a port is acquired, call this method to being a transmission
	 * @param device The device making port access
	 * @param enable true to transmit, false to switch back to receive
	 */
	virtual void setTransmit(Request* request, bool enable)
	{
	}

private:
	void configure(const Config& cfg);

	uart_t* uart{nullptr};
	State* currentState{nullptr}; ///< Not owned
};

} // namespace IO
