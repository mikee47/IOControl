/*
 * RFSwitch.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 * Based on RC Switch library but completely rewritten for ESP8266 and Sming.
 *
 */

#pragma once

#include <HardwareTimer.h>

#define RC_OUTPUT_PIN 0

/** @brief  Delegate callback type for RF Switch transaction
 */
// Delegate constructor usage: (&YourClass::method, this)
typedef Delegate<void(uint32_t code, uint32_t param)> RFSwitchCallback;

class RFSwitch
{
public:
	/// Protocol configuration
	struct Protocol {
		/// Timings in microseconds
		struct {
			uint16_t starth; ///< Width of start High pulse
			uint16_t startl; ///< Width of start Low pulse
			uint16_t period; ///< Bit period
			uint16_t bit0;   ///< Width of a '0' high pulse
			uint16_t bit1;   ///< Width of a '1' high pulse
			uint16_t gap;	///< Gap after final bit before repeating
		} timing;
		uint8_t repeats; ///< Number of times to repeat code
	};

	//
	enum TransmitState {
		idle,	  ///< Not transmitting
		startHigh, ///< Sending start transition
		startLow,  ///< Sending start transition
		dataHigh,  ///< Sending data bit
		dataLow,   ///< Sending data bit
	};

	RFSwitch()
	{
	}

	void begin(RFSwitchCallback delegateFunction);

	bool busy() const;

	bool transmit(const Protocol& protocol, uint32_t code, uint32_t param);

private:
	static void setTransmit(TransmitState state, bool output, unsigned duration);
	static void transmitInterruptHandler();

private:
	static TransmitState m_transmitState;
	static unsigned m_lowDuration;		///< Calculated when high started to balance bit period
	static uint32_t m_transmitData;		//< The data to transmit
	static uint32_t m_transmitMask;		///< Position of bit to transmit next
	static Protocol m_protocol;			//< Protocol for active transmission
	static RFSwitchCallback m_callback; ///< Callback for transmit complete
	static uint32_t m_callbackParam;
	static HardwareTimer m_hardwareTimer;
};
