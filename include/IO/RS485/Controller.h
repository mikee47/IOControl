#pragma once

#include <IO/Controller.h>
#include <IO/Serial.h>
#include <SimpleTimer.h>

namespace IO
{
namespace RS485
{
DECLARE_FSTR(CONTROLLER_CLASSNAME)

class Controller : public IO::Controller
{
public:
	Controller(Serial& serial, uint8_t instance) : IO::Controller(instance), serial{serial}
	{
	}

	String classname() override
	{
		return CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

	bool busy() const override
	{
		return request != nullptr;
	}

	/**
	 * @brief Callback to handle hardware transmit/receive selection
	 * Typically called from interrupt context so implementation MUST be marked IRAM_ATTR
	 * @param direction
	 */
	using SetDirectionCallback = void (*)(Direction direction);

	/**
	 * @brief Set the transmit callback handler
	 *
	 * Typically with RS485 a GPIO is used to toggle between transmit/receive modes.
	 * Using a callback allows flexibility for your particular hardware implementation.
	 * You don't need to set this up if your hardware handles the switch automatically.
	 *
	 * @param callback
	 */
	void onSetDirection(SetDirectionCallback callback)
	{
		setDirectionCallback = callback;
	}

	/**
	 * @brief Whilst a port is acquired, call this method to being or end transmission
	 * @param enable true to transmit, false to receive
	 * @note Port should be left in receive mode on request completion
	 */
	void IRAM_ATTR setDirection(IO::Direction direction)
	{
		if(setDirectionCallback != nullptr) {
			setDirectionCallback(direction);
		}
	}

	Serial& getSerial()
	{
		return serial;
	}

	void handleEvent(Request* request, Event event) override;

	using OnRequestDelegate = Delegate<void(Controller& controller)>;

	void onRequest(OnRequestDelegate callback)
	{
		requestCallback = callback;
	}

	void send(const void* data, size_t size);

protected:
	virtual void handleIncomingRequest()
	{
		if(requestCallback) {
			requestCallback(*this);
		}
	}

private:
	static void IRAM_ATTR uartCallback(uart_t* uart, uint32_t status);
	void receiveComplete();

private:
	Serial& serial;
	SetDirectionCallback setDirectionCallback{nullptr};
	Request* request{nullptr};
	OnRequestDelegate requestCallback;
	SimpleTimer timer; ///< Use to schedule callback and timeout
	Serial::Config m_savedConfig{};
};

} // namespace RS485
} // namespace IO
