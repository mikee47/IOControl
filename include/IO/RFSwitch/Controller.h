/**
 * RFSwitch/Controller.h
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
	Controller(uint8_t instance, uint8_t outputPin, bool outputInvert) : IO::Controller(instance)
	{
		this->outputPin = outputPin;
		this->outputInvert = outputInvert;
	}

	const FlashString& classname() const override
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

	static void __forceinline setOutput(bool state)
	{
		digitalWrite(outputPin, state ^ outputInvert);
	}

	static void setTransmit(TransmitState state, bool output, unsigned duration);
	static void transmitInterruptHandler();
	bool execute(IO::Request& request);

	static uint32_t m_transmitData; //< The data to transmit
	static uint32_t m_transmitMask; //< Position of bit to transmit next
	static uint16_t m_lowDuration;  //< Calculated when high started to balance bit period
	static uint8_t m_repeats;		//< How many remaining code repeats
	static Request* m_request;		//< Active request
	static HardwareTimer m_hardwareTimer;
	static uint8_t outputPin;
	static bool outputInvert;
	static volatile TransmitState m_transmitState;
};

} // namespace RFSwitch
} // namespace IO
