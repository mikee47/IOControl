/*
 * IODMX512.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include "IODMX512.h"
#include "HardwareSerial.h"
#include "Clock.h"
#include "Digital.h"
#include "ledtable.h"

// Device configuration
DEFINE_FSTR(DMX512_CONTROLLER_CLASSNAME, "DMX")

// Our device name
DEFINE_FSTR(DMX512_DEVICE_NAME, "dmx")

//
DEFINE_FSTR_LOCAL(ATTR_ADDRESS, "address")

// Pin to switch  MAX485 between receive (low) and transmit (high)
#define MBPIN_TX_EN 12

/* DMX minimum timings per E1.11 */
#define DMX_BREAK 92
#define DMX_MAB 12 // Mark After Break

//
#define DMX_BAUDRATE 250000

//
#define DMX_UPDATE_CHANGED_MS 10	///< Slave data has changed
#define DMX_UPDATE_PERIODIC_MS 1000 // 5000	///< Periodic update interval

static void dmxStart()
{
	auto uart = Serial.getUart();
	if(uart != nullptr) {
		uart_set_break(uart, true);
		delayMicroseconds(DMX_BREAK);
		uart_set_break(uart, false);
		delayMicroseconds(DMX_MAB);
	}
}

/* CDMX512Controller */

/*
 * Inherited classes call this after their own start() code.
 */
void DMX512Controller::start()
{
	Serial.end();
	Serial.setUartCallback(nullptr);
	Serial.setTxBufferSize(520);
	Serial.setRxBufferSize(0);
	Serial.begin(DMX_BAUDRATE, SERIAL_8N2);

	// Using alternate serial pins
	Serial.swap();

	// Put default serial pins in safe state
	pinMode(1, INPUT_PULLUP);
	pinMode(3, INPUT_PULLUP);

	digitalWrite(MBPIN_TX_EN, 0);
	pinMode(MBPIN_TX_EN, OUTPUT);

	Serial.onTransmitComplete(TransmitCompleteDelegate(&DMX512Controller::transmitComplete, this));

	m_timer.setCallback(
		[](void* arg) {
			auto ctrl = static_cast<DMX512Controller*>(arg);
			ctrl->updateSlaves();
		},
		this);

	IOController::start();
}

/*
 * Inherited classes call this before their own stop() code.
 */
void DMX512Controller::stop()
{
	m_timer.stop();
	Serial.onTransmitComplete(nullptr);
	Serial.end();
	IOController::stop();
}

/*
 *  TODO
 *
 *  Devices should maintain state. Controller iterates all devices during an update, marks outstanding
 *  requests as pending and completes them when done.
 *  DMX spec. says we should be pumping out updates continuously, so need to decide on update interval.
 *  That would be set as a parameter of the controller.
 *
 *  There is the question of how Modbus and DMX can co-exist:
 *
 *  	- Move ModbusController into a new module and rename IORS485
 *  	- Incorporate multi-protocol support (presently Modbus and DMX512), using code from modbus.cpp
 *  	- Merge remaining code from modbus.cpp into ModbusDevice / Request classes
 *  	- Delete modbus.cpp, modbus.h
 *  	- DMXDevice will maintain state information for itself (address plus data for each node)
 *  	- DMX requests are executed immediately by the device, which notifies the controller that a DMX update is required
 *  	- RS485Controller builds its output packet from the data in each device, so serial driver handles all buffering
 *  	- RS485Controller handles DMX transfers using an internal timer; this will be set to some short value (e.g. 10ms) when a change
 *  		is notified by a device. On transfer completion it will be set to a longer value, e.g. 500ms.
 *  	- A minimum interval between DMX packets must be observed. When a DMX transfer completes, if the timer is less than this minimum
 *  		interval it will simply be reset to the minimum value. This also handles the situation where the timer expires while a transfer
 *  		is already in progress.
 *
 * We do have two serial ports though, so simplest solution would be to switch to debug port for release build as we don't need RX. That would
 * require a second RS485 interface, of course.
 */
void DMX512Controller::execute(IORequest& request)
{
	// Apply request to owning device and pend
	auto req = reinterpret_cast<DMX512Request&>(request);
	auto err = req.device().execute(req);
	if(err < 0) {
		debug_e("Request failed, %s", ioerrorString(err).c_str());
		request.complete(status_error);
	} else {
		// Request will be completed when next update cycle completes
		m_timer.setIntervalMs<DMX_UPDATE_CHANGED_MS>();
		m_timer.startOnce();
		m_changed = true;
		request.complete(status_success);
	}
}

void DMX512Controller::updateSlaves()
{
	//	debug_i("DMX512Controller::updateSlaves()");

	/*
	// Determine how many slots we need
	uint16_t maxAddr = 0;
	for (unsigned i = 0; i < m_devices.count(); ++i) {
		auto dev = reinterpret_cast<DMX512Device*>(m_devices[i]);
		uint16_t addr = dev->address() + dev->nodeIdMax();
		maxAddr = max(maxAddr, addr);
	}

	debug_i("maxaddr = %u", maxAddr);

*/

	m_changed = false;

	const uint16_t maxAddr = 512;

	// data + padding - note address #0 isn't used and is always set to 0 for lighting applications
	unsigned dataSize = maxAddr + 1 + 2;
	uint8_t data[dataSize];
	memset(data, 0, dataSize);
	data[0] = 0x00; // Lighting start code
	for(unsigned i = 0; i < m_devices.count(); ++i) {
		auto dev = reinterpret_cast<DMX512Device*>(m_devices[i]);
		if(dev->update()) {
			m_changed = true;
		}
		for(unsigned node = dev->nodeIdMin(); node <= dev->nodeIdMax(); ++node) {
			auto& nodeData = dev->getNodeData(node);
			unsigned addr = dev->address() + node;
			assert(addr > 0 && addr <= maxAddr);
			data[addr] = led(nodeData.value);
		}
	}

	//	debug_hex(INFO, "SLOTS", data, 8);

	digitalWrite(MBPIN_TX_EN, 1);
	dmxStart();
	Serial.write(data, dataSize);

	m_updating = true;
}

void DMX512Controller::transmitComplete(HardwareSerial& serial)
{
	digitalWrite(MBPIN_TX_EN, 0);

	//	debug_i("DMX512Controller::transmitComplete()");

	/* We schedule next periodic update now, rather than in the callback,
	 * because additional requests may come in before then which set the
	 * timer to a shorter value
	 */
	if(m_changed) {
		m_timer.setIntervalMs<DMX_UPDATE_CHANGED_MS>();
		m_timer.startOnce();
	} else if(DMX_UPDATE_PERIODIC_MS != 0) {
		m_timer.setIntervalMs<DMX_UPDATE_PERIODIC_MS>();
		m_timer.startOnce();
	}
	m_updating = false;
}

/* DMX512Request */

ioerror_t DMX512Request::parseJson(JsonObjectConst json)
{
	ioerror_t err = IORequest::parseJson(json);
	if(err) {
		return err;
	}
	const char* s = json[ATTR_CODE];
	m_code = s ? strtoul(s, nullptr, 10) : 0;
	return ioe_success;
}

void DMX512Request::getJson(JsonObject json) const
{
	IORequest::getJson(json);
	json[ATTR_NODE] = m_node;
	json[ATTR_CODE] = String(m_code);
}

/* DMX512Device */

static ioerror_t createDevice(IOController& controller, IODevice*& device)
{
	if(!controller.verifyClass(DMX512_CONTROLLER_CLASSNAME)) {
		return ioe_bad_controller_class;
	}

	device = new DMX512Device(reinterpret_cast<DMX512Controller&>(controller));
	return device ? ioe_success : ioe_nomem;
}

const device_class_info_t DMX512Device::deviceClass()
{
	return {DMX512_DEVICE_NAME, createDevice};
}

ioerror_t DMX512Device::init(JsonObjectConst config)
{
	ioerror_t err = IODevice::init(config);
	if(err) {
		return err;
	}

	m_address = config[ATTR_ADDRESS];
	m_nodeCount = config[ATTR_COUNT];
	if(m_nodeCount == 0) {
		m_nodeCount = 1;
	}
	m_nodeData = new DmxNodeData[m_nodeCount];
	memset(m_nodeData, 0, sizeof(DmxNodeData) * m_nodeCount);

	return ioe_success;
}

bool DMX512Device::update()
{
	bool res = false;
	for(unsigned i = 0; i < m_nodeCount; ++i) {
		auto& nodeData = m_nodeData[i];
		if(nodeData.adjust()) {
			res = true;
		}
	}
	return res;
}

ioerror_t DMX512Device::execute(DMX512Request& request)
{
	unsigned node = request.node();
	if(node >= m_nodeCount) {
		return ioe_bad_node;
	}

	// Apply request to device data
	auto& nodeData = m_nodeData[node];
	switch(request.command()) {
	case ioc_off:
		nodeData.disable();
		break;
	case ioc_on:
		if(nodeData.target == 0) {
			nodeData.target = 100; // Default brightness
		}
		nodeData.enable();
		break;
	case ioc_adjust: {
		nodeData.setTarget(nodeData.target + request.code());
		nodeData.enable();
		break;
	}
	case ioc_send:
		nodeData.setValue(request.code());
		break;
	default:
		return ioe_bad_command;
	}
	return ioe_success;
}
