/*
 * RFSwitch.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 */

#include "RFSwitch.h"
#include <Digital.h>
#include <Platform/System.h>
#include "ObjectArray.h"
#include <esp_attr.h>

RFSwitch::TransmitState RFSwitch::m_transmitState;
unsigned RFSwitch::m_lowDuration;       ///< Calculated when high started to balance bit period
uint32_t RFSwitch::m_transmitData;      //< The data to transmit
uint32_t RFSwitch::m_transmitMask;      ///< Position of bit to transmit next
RFSwitch::Protocol RFSwitch::m_protocol; //< Protocol for active transmission
RFSwitchCallback RFSwitch::m_callback; ///< Callback for transmit complete
uint32_t RFSwitch::m_callbackParam;
HardwareTimer RFSwitch::m_hardwareTimer;

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
#define RC_PULSE_EXTENSION  24

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
#define LATENCY   12

void RFSwitch::begin(RFSwitchCallback callback)
{
	m_callback = callback;
}

// Transistor buffer inverts output sense, so low to turn on
#define setOutput(state) \
  digitalWrite(RC_OUTPUT_PIN, !state)

void IRAM_ATTR RFSwitch::setTransmit(TransmitState state, bool output, unsigned duration)
{
	setOutput(output);

	if (output) {
		m_lowDuration = m_protocol.timing.period - duration;
		duration += RC_PULSE_EXTENSION;
	}
	else
		duration -= RC_PULSE_EXTENSION;

	m_transmitState = state;
	m_hardwareTimer.setIntervalUs(duration - LATENCY);
	m_hardwareTimer.startOnce();
}

void IRAM_ATTR RFSwitch::transmitInterruptHandler()
{
	if (m_transmitState == startHigh) {
		setTransmit(startLow, false, m_protocol.timing.startl);
		return;
	}

	if (m_transmitState == dataHigh) {
		setTransmit(dataLow, false, m_lowDuration);
		return;
	}

	// @todo This is for a 24-bit protocol, consider adding this to config as a 'bitcount' parameter
	if (m_transmitState == startLow)
		m_transmitMask = 0x00800000;

	if (m_transmitMask != 0) {
		setTransmit(dataHigh, true,
			(m_transmitData & m_transmitMask) ? m_protocol.timing.bit1 : m_protocol.timing.bit0);
		m_transmitMask >>= 1;
		// Final low period is extended to create a gap before repeating
		if (m_transmitMask == 0)
			m_lowDuration += m_protocol.timing.gap;
		return;
	}

	// Packet sent, again ?
	--m_protocol.repeats;
	if (m_protocol.repeats == 0) {
		// All done
		setOutput(false);

		// 1/7/18 This seems to help, perhaps by allowing output to settle a little bit higher than when driven
		pinMode(RC_OUTPUT_PIN, INPUT);

		m_hardwareTimer.stop();

		System.queueCallback([](uint32_t) {
			m_transmitState = idle;
			m_callback(m_transmitData, m_callbackParam);
		});

		return;
	}

	// Send again
	setTransmit(startHigh, true, m_protocol.timing.starth);
}

bool RFSwitch::busy() const
{
	return (m_transmitState != idle);
}

/*
 Begin transmission of a data packet, repeated.
 This is done using timer interrupts so returns immediately.
 Call will fail if transmission is already in progress.
 */
bool RFSwitch::transmit(const Protocol& protocol, uint32_t code, uint32_t param)
{
	if (busy())
		return false;

	m_callbackParam = param;
	m_transmitData = code;
	m_protocol = protocol;
	pinMode(RC_OUTPUT_PIN, OUTPUT);

	m_hardwareTimer.setCallback(transmitInterruptHandler);
	setTransmit(startHigh, true, m_protocol.timing.starth);

	return true;
}

