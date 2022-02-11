/**
 * RFSwitch/Controller.cpp
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

#include <IO/RFSwitch/Controller.h>
#include <IO/RFSwitch/Request.h>

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR(CONTROLLER_CLASSNAME, "rfswitch")

uint8_t Controller::outputPin;
bool Controller::outputInvert;
uint32_t Controller::m_transmitData;
uint32_t Controller::m_transmitMask;
uint16_t Controller::m_lowDuration;
uint8_t Controller::m_repeats;
volatile Controller::TransmitState Controller::m_transmitState;
Request* Controller::m_request;
HardwareTimer Controller::m_hardwareTimer;

/*
 * Add a bit to pulse width to compensate for transmitter response.
 This doesn't appear to be linear so tweaking required.
 This could be due to poor transmitter antenna matching. It may also be
 dependent on transmitter supply voltage. The transistor base resistor will
 have an effect; perhaps a lower value?
 The received signal is not square; this suggests low pass filtering on the
 modulation input.

 SOLUTION: Driving the input at 5v (VCC) using transistor buffer works better than 3.3v.

 NOTE: Interestingly, the receiver has those 4-bit gaps so I guess that's
 nothing to do with transmission or 'sync' blank periods.

 14/7/18

 Got picoscope with software to analyse timings using SRX880 receiver module.
 Timings taken from R1/OFF and used to calibrate transmitter. Signal on DATA
 input to transmitter is spot-on (with latency = 12) and period is also
 accurate, but HIGH pulses are out. Added 100uF tantalum to help stablise.

 Still requires correction for high time. Still don't know if this error is
 induced in the transmitter or receiver. I guess try another receiver.
 Anyway, with these figures we consistently get the correct timings at the
 receiver so that will do.
 */
#define RC_PULSE_EXTENSION 24

/*
 NOTE: During programming GPIO0 oscillates at 25MHz. Don't know why; we should
 filter this out or find a different pin to use for transmit.
 */

/*
 *  Adjustment for interrupt latency for each timed transition (H -> L and L -> H)
 *  This value was obtained by measuring input to transmitter with scope.
 *  The hardware timer class was modified to use the NMI, thus we can pre-empt other
 *  ISRs and keep jitter to absolute minimum. It would be nice if there were hardware
 *  output compare circuitry on this thing... maybe it's just hidden...
 */
#define LATENCY 12

void Controller::handleEvent(IO::Request* request, Event event)
{
	switch(event) {
	case Event::Execute:
		if(!execute(*request)) {
			return;
		}
		break;

	case Event::RequestComplete:
		m_request = nullptr;
		m_transmitState = idle;
		break;

	case Event::Timeout:
	case Event::TransmitComplete:
	case Event::ReceiveComplete:
		break;
	}

	IO::Controller::handleEvent(request, event);
}

void IRAM_ATTR Controller::setTransmit(TransmitState state, bool output, unsigned duration)
{
	setOutput(output);

	if(output) {
		m_lowDuration = m_request->device().timing().period - duration;
		duration += RC_PULSE_EXTENSION;
	} else {
		duration -= RC_PULSE_EXTENSION;
	}

	m_transmitState = state;
	m_hardwareTimer.setIntervalUs(duration - LATENCY);
	m_hardwareTimer.startOnce();
}

void IRAM_ATTR Controller::transmitInterruptHandler()
{
	if(m_request == nullptr) {
		return;
	}

	auto& timing = m_request->device().timing();
	if(m_transmitState == startHigh) {
		setTransmit(startLow, false, timing.startl);
		return;
	}

	if(m_transmitState == dataHigh) {
		setTransmit(dataLow, false, m_lowDuration);
		return;
	}

	// @todo This is for a 24-bit protocol, consider adding this to config as a 'bitcount' parameter
	if(m_transmitState == startLow) {
		m_transmitMask = 0x00800000;
	}

	if(m_transmitMask != 0) {
		setTransmit(dataHigh, true, (m_transmitData & m_transmitMask) ? timing.bit1 : timing.bit0);
		m_transmitMask >>= 1;
		// Final low period is extended to create a gap before repeating
		if(m_transmitMask == 0) {
			m_lowDuration += timing.gap;
		}
		return;
	}

	// Packet sent, again ?
	--m_repeats;
	if(m_repeats != 0) {
		// Send again
		return setTransmit(startHigh, true, timing.starth);
	}

	// All done
	setOutput(false);

	// 1/7/18 This seems to help, perhaps by allowing output to settle a little bit higher than when driven
	pinMode(outputPin, INPUT);

	m_hardwareTimer.stop();

	System.queueCallback([](void*) { m_request->complete(Error::success); });
}

/*
 * Begin transmission of a data packet, repeated.
 * This is done using timer interrupts so returns immediately.
 * Might want to update this in future to take a byte array. It's likely
 * though that a single code is sufficient.
 */
bool Controller::execute(IO::Request& request)
{
	assert(m_transmitState == TransmitState::idle);
	assert(m_request == nullptr);

	if(request.command() != Command::set) {
		debug_err(Error::bad_command, request.caption());
		request.complete(Error::bad_command);
		return false;
	}

	m_request = reinterpret_cast<Request*>(&request);
	m_transmitData = m_request->code();
	m_repeats = m_request->repeats();
	pinMode(outputPin, OUTPUT);

	m_hardwareTimer.setCallback(transmitInterruptHandler);
	setTransmit(startHigh, true, m_request->device().timing().starth);
	return true;
}

} // namespace RFSwitch
} // namespace IO
