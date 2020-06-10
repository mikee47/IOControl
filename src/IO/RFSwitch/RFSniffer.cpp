/*
 * RFSniffer.cpp
 *
 *  Created on: 21 June 2018
 *      Author: mikee47
 *
 */

#include "RFSniffer.h"
#include "SimpleTimer.h"
#include "Clock.h"
#include "Platform/System.h"
#include "Digital.h"
#include "Interrupts.h"

static RFSniffCallback m_callback;
// True for full timing information
static bool m_verbose;

#ifndef RC_INPUT_PIN
#define RC_INPUT_PIN 14
#endif

/* COMMON TO I-LUMOS */
//TODO: move to header file
#define RC_BIT_PERIOD 475
#define RC_START_HIGH 413
#define RC_START_LOW 420
// High pulse duration; low duration set to achieve bit period
#define RC_DATA_0 284
#define RC_DATA_1 168

/* Sniffer parameters - high pulse widths in us */

// Discard packets which are too small
//# define MIN_PACKET_SIZE     100
//
#define START_LOW_LENGTH_MIN 800 // 1200

// Identifies a start bit
#define RC_MIN_START_LENGTH 320

// Limits for pulse widths within packet - packet abandoned if limits violated
#define RC_MIN_PULSE_WIDTH 100 // 50
#define RC_MAX_PULSE_WIDTH 600

// Like FIFO, except it is heap-allocated at runtime
template <typename T> class CFifo : public ObjectArray<T>
{
public:
	bool init(unsigned _size)
	{
		_read = _write = 0;
		return ObjectArray<T>::init(_size);
	}

	unsigned used()
	{
		return _write - _read;
	}

	T read()
	{
		if(_write == _read)
			return T();

		T value = (*this)[_read];
		_read = (_read + 1) % ObjectArray<T>::size();
		return value;
	}

	bool write(const T& value)
	{
		unsigned i = (_write + 1) % ObjectArray<T>::size();
		if(i == _read)
			return false;

		(*this)[_write] = value;
		_write = i;
		return true;
	}

private:
	unsigned _read = 0;
	unsigned _write = 0;
};

// Packet high/low timings
CRFTimingBuffer RFSniffer::m_timingBuffer;

/*
 * Number of timings to buffer, i.e. how much of transmit burst to capture.
 * We store a complete timing capture so this uses a lot of memory.
 */
#define RC_MAX_TIMINGS 2048

/*
 *  One packet is approx. 12.24ms, including start bit, so max. 82 packets/s
 *  but these are repeated. We don't store repeats but increment a count so
 *  buffer can be small.
 */
#define RC_MAX_PACKETS 16

class CPacketBuffer : public CFifo<rfsniff_packet_t>
{
private:
	rfsniff_packet_t m_packet;

public:
	bool init(unsigned _size)
	{
		m_packet = {};
		return CFifo::init(_size);
	}

	// Return true if data written to FIFO
	bool write(uint32_t value)
	{
		if(value == m_packet.code) {
			++m_packet.repeats;
			return false;
		}

		bool ret = flush();
		m_packet.code = value;
		m_packet.repeats = 0;
		// Start of sniff
		if(CFifo::write(m_packet))
			ret = true;

		++m_packet.repeats;
		return ret;
	}

	// Return true if data written to FIFO
	bool flush()
	{
		bool ret = false;
		if(m_packet.repeats) {
			ret = CFifo::write(m_packet);
			m_packet = {0, 0};
		}
		return ret;
	}
};

static CPacketBuffer m_packetBuffer;
static bool m_callbackPending;

// Timeout after burst to reset state
static SimpleTimer m_receiveTimer;

#define RECEIVE_TIMEOUT 100

/**
 * Enable receiving data
 */
void RFSniffer::begin(RFSniffCallback callback, bool verbose)
{
	m_callback = callback;
	m_verbose = verbose;

	m_timingBuffer.init(RC_MAX_TIMINGS);
	m_packetBuffer.init(RC_MAX_PACKETS);

	attachInterrupt(RC_INPUT_PIN, handleInterrupt, CHANGE);

	m_receiveTimer.initializeMs<RECEIVE_TIMEOUT>(receiveTimeout);
	m_receiveTimer.startOnce();
}

/**
 * Disable receiving data
 */
void RFSniffer::end()
{
	detachInterrupt(digitalPinToInterrupt(RC_INPUT_PIN));

	m_packetBuffer.free();
	m_timingBuffer.free();
}

void RFSniffer::packetCallback(uint32_t param)
{
	m_callbackPending = false;
	while(m_packetBuffer.used()) {
		m_callback(m_packetBuffer.read());
	}

	m_timingBuffer.reset();
}

void RFSniffer::queueCallback()
{
	if(!m_callbackPending) {
		System.queueCallback(packetCallback);
		m_callbackPending = true;
	}
}

void IRAM_ATTR RFSniffer::handleInterrupt()
{
	static enum __attribute__((packed)) { waiting, receiving } mode;

	// Whether we're receiving a high level or low level
	static bool line_is_high;

	// Indicates bit being decoded within packet
	static int bitpos;
	static uint32_t bitbuffer;

	static unsigned long lastTime;
	// For glitch removal, remember time for transition before last
	//	static unsigned long lastTime_1;
	const long time = micros();
	const unsigned int duration = time - lastTime;
	line_is_high = !line_is_high;

	// // Glitch filter
	// if (duration < RC_MIN_PULSE_WIDTH) {
	//   // Glitch - ignore
	//   lastTime = lastTime_1;
	//   return;
	// }

	lastTime = time;

	// Look for start pulse
	if(duration > START_LOW_LENGTH_MIN) {
		if(mode == receiving) {
			// @todo timing buffer accessed by callback so can't reset
			m_packetBuffer.flush();
			//      // Short packets are discarded
			//      if (buffer->count >= MIN_PACKET_SIZE) {
			//        // Receiving: marks end of packet
			//        m_timingCount++;
			//      }
		} else {
			// Waiting: Just check line has gone high then we can stay in sync
			if(!digitalRead(RC_INPUT_PIN)) {
				return; // Nope - line is low, ignore
			}
			// Proceed to reception
			line_is_high = true;
		}

		mode = receiving;
		// Start a new packet
		bitpos = 0;

		return;
	}

	if(mode == waiting)
		return;

	if((duration < RC_MIN_PULSE_WIDTH) || (duration > RC_MAX_PULSE_WIDTH)) {
		// Abandon packet
		mode = waiting;
	}

	unsigned value = duration; //(duration + (RC_PULSE_WIDTH / 2)) / RC_PULSE_WIDTH;

	static highlow_t highlow;

	if(line_is_high) {
		highlow.low = value;
		m_timingBuffer.write(highlow);
		return;
	}

	highlow.high = value;

	/*
	 OK, so now we know the packet structure we can decode contents properly.

	 We can do a rough decode using just the high pulse width:

	 0: 120 - 150us
	 1: 240 - 270us
	 S: 370 - 390us

	 Anything outside these ranges and we discard the packet and wait for the
	 next start bit.

	 Codes are 24-bits with a start bit.

	 */

	if(duration >= RC_MIN_START_LENGTH) {
		// Start bit
		bitpos = 1; // Abandon any previous packet; should probably log this.
		bitbuffer = 0;
	} else if(bitpos > 0) {
		bitbuffer <<= 1;
		if(duration < 200) {
			bitbuffer |= 0x00000001;
		}
		if(bitpos == 24) {
			// If buffer is full, discard value
			if(m_packetBuffer.write(bitbuffer)) {
				queueCallback();
			}
			bitpos = 0;
			m_receiveTimer.startOnce();
		} else {
			++bitpos;
		}
	}
}

void RFSniffer::receiveTimeout(void*)
{
	if(m_packetBuffer.flush()) {
		queueCallback();
	}
}
