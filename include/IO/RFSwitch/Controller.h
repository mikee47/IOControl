#pragma once

#include <IO/Controller.h>
#include <HardwareTimer.h>

namespace IO
{
namespace RFSwitch
{
DECLARE_FSTR(CONTROLLER_CLASSNAME)

class Request;

class Controller : public IO::Controller
{
public:
	Controller(uint8_t instance) : IO::Controller(instance)
	{
	}

	String classname() override
	{
		return CONTROLLER_CLASSNAME;
	}

	void handleEvent(IO::Request* request, Event event) override;

	bool busy() const override
	{
		return m_transmitState != TransmitState::idle;
	}

private:
	enum TransmitState {
		idle,	  ///< Not transmitting
		startHigh, ///< Sending start transition
		startLow,  ///< Sending start transition
		dataHigh,  ///< Sending data bit
		dataLow,   ///< Sending data bit
	};

	static void setTransmit(TransmitState state, bool output, unsigned duration);
	static void transmitInterruptHandler();
	bool execute(IO::Request& request);

	static uint32_t m_transmitData; //< The data to transmit
	static uint32_t m_transmitMask; //< Position of bit to transmit next
	static uint16_t m_lowDuration;  //< Calculated when high started to balance bit period
	static uint8_t m_repeats;		//< How many remaining code repeats
	static TransmitState m_transmitState;
	static Request* m_request; //< Active request
	static HardwareTimer m_hardwareTimer;
};

} // namespace RFSwitch
} // namespace IO
