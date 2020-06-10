/*
 * RFSniffer.h
 *
 *  Created on: 21 June 2018
 *      Author: mikee47
 *
 */

#pragma once

#include <esp_attr.h>
#include <Delegate.h>
#include "ObjectArray.h"

/**
 * Description of a single pule, which consists of a high signal
 * whose duration is "high" times the base pulse length, followed
 * by a low signal lasting "low" times the base pulse length.
 * Thus, the pulse overall lasts (high+low)*pulseLength
 */
struct highlow_t {
	uint16_t high;
	uint16_t low;
};

// Stores timing information during receive for subsequent analysis
class CRFTimingBuffer : public ObjectArray<highlow_t>
{
private:
	unsigned m_count = 0;

public:
	bool init(unsigned _size)
	{
		reset();
		return ObjectArray::init(_size);
	}

	void reset()
	{
		m_count = 0;
	}

	bool write(const highlow_t& value)
	{
		if(m_count >= size())
			return false;
		(*this)[m_count++] = value;
		return true;
	}

	unsigned count() const
	{
		return m_count;
	}
};

/*
 * Decoded packets are passed to callback. This is called on first
 * packet (with repeats = 0), then on next code change or timeout
 * with repeats = repeat count.
 *
 * Two types of remote have been tested which are outwardly identical.
 * The first has two 12V batteries with repeats = 80, the second
 * has one battery with repeats=45.
 *
 */
struct rfsniff_packet_t {
	uint32_t code;
	unsigned repeats;
};

/** @brief  Delegate callback type for RF Switch transaction
 */
// Delegate constructor usage: (&YourClass::method, this)
typedef Delegate<void(const rfsniff_packet_t& pkt)> RFSniffCallback;

class RFSniffer
{
public:
	RFSniffer()
	{
	}

	void begin(RFSniffCallback delegateFunction, bool verbose);

	void end();

	const CRFTimingBuffer& timings()
	{
		return m_timingBuffer;
	}

private:
	// Receive pin change interrupt
	static void IRAM_ATTR handleInterrupt();
	static void receiveTimeout(void*);

	static void packetCallback(uint32_t param);
	static void IRAM_ATTR queueCallback();

private:
	static CRFTimingBuffer m_timingBuffer;
};
