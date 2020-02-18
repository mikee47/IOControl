/*
 * IODMX512.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include <IO/DMX512/DMX512.h>
#include "Clock.h"
#include "Digital.h"
//#include "ledtable.h"

namespace IO
{
namespace DMX512
{
// Device configuration
DEFINE_FSTR(CONTROLLER_CLASSNAME, "dmx")
DEFINE_FSTR(DEVICE_CLASSNAME, "dmx")

//
DEFINE_FSTR_LOCAL(ATTR_ADDRESS, "address")
DEFINE_FSTR_LOCAL(ATTR_VALUE, "value")

/* DMX minimum timings per E1.11 */
constexpr unsigned DMX_BREAK{92};
constexpr unsigned DMX_MAB{12}; // Mark After Break
constexpr uint32_t DMX_BAUDRATE{250000};
constexpr uint8_t DMX_SERIAL_CONFIG{SERIAL_8N2};

//
#define DMX_UPDATE_CHANGED_MS 10	///< Slave data has changed
#define DMX_UPDATE_PERIODIC_MS 1000 // 5000	///< Periodic update interval

/* Request */

/*
 * We don't need to use the queue as requests do not perform any I/O.
 * Instead, device state is updated and echoed on next slave update.
 */
Error Request::submit()
{
	auto err = device().execute(*this);
	if(!!err) {
		debug_e("Request failed, %s", toString(err).c_str());
		complete(Status::error);
	} else {
		complete(Status::success);
	}

	return err;
}

/* Controller */

void Controller::start()
{
	if(!serial.resizeBuffers(0, MaxPacketSize)) {
		return;
	}

	m_timer.setCallback(
		[](void* arg) {
			auto ctrl = static_cast<Controller*>(arg);
			ctrl->updateSlaves();
		},
		this);

	IO::Controller::start();
}

/*
 * Inherited classes call this before their own stop() code.
 */
void Controller::stop()
{
	m_timer.stop();
	serial.release(state);
	//	Serial.onTransmitComplete(nullptr);
	//	Serial.end();
	IO::Controller::stop();
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
 *  	- Move ModbusController into a new module and rename IORS485 (or use base IO::Serial::Controller)
 *  	- Incorporate multi-protocol support (presently Modbus and DMX512), using code from modbus driver
 *  	- Merge remaining code from modbus driver into ModbusDevice / Request classes
 *  	- Delete modbus driver
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
void Controller::execute(IO::Request& request)
{
	debug_e("DMX512 requests are not queued!");
	assert(false);
}

void Controller::deviceChanged()
{
	if(!m_changed) {
		m_timer.setIntervalMs<DMX_UPDATE_CHANGED_MS>();
		m_timer.startOnce();
		m_changed = true;
	}
}

void Controller::updateSlaves()
{
	//	debug_i("Controller::updateSlaves()");

	/*
	// Determine how many slots we need
	uint16_t maxAddr = 0;
	for (unsigned i = 0; i < m_devices.count(); ++i) {
		auto dev = reinterpret_cast<Device*>(m_devices[i]);
		uint16_t addr = dev->address() + dev->nodeIdMax();
		maxAddr = max(maxAddr, addr);
	}

	debug_i("maxaddr = %u", maxAddr);

*/
	Serial::Config cfg{
		.mode{Serial::Mode::Exclusive},
		.config{DMX_SERIAL_CONFIG},
		.baudrate{DMX_BAUDRATE},
	};
	if(!serial.acquire(state, cfg)) {
		debug_e("DMX: Serial busy");
		return;
	}

	m_changed = false;

	const uint16_t maxAddr = 512;

	// data + padding - note address #0 isn't used and is always set to 0 for lighting applications
	unsigned dataSize = maxAddr + 1 + 2;
	uint8_t data[dataSize];
	memset(data, 0, dataSize);
	data[0] = 0x00; // Lighting start code
	for(unsigned i = 0; i < m_devices.count(); ++i) {
		auto dev = reinterpret_cast<Device*>(m_devices[i]);
		if(dev->update()) {
			m_changed = true;
		}
		for(unsigned nodeId = dev->nodeIdMin(); nodeId <= dev->nodeIdMax(); ++nodeId) {
			auto& nodeData = dev->getNodeData(nodeId);
			unsigned addr = dev->address() + nodeId;
			assert(addr > 0 && addr <= maxAddr);
			// @todo: Device should perform any necessary translation
			//			data[addr] = led(nodeData.value);
			data[addr] = nodeData.value;
		}
	}

	//	debug_hex(INFO, "SLOTS", data, 8);

	serial.setTransmit(true);
	serial.setBreak(true);
	delayMicroseconds(DMX_BREAK);
	serial.setBreak(false);
	delayMicroseconds(DMX_MAB);
	serial.write(data, dataSize);

	m_updating = true;
}

void Controller::transmitComplete(Serial::State& state)
{
	System.queueCallback(
		[](void* param) {
			auto controller = static_cast<Controller*>(param);
			controller->transmitComplete();
		},
		state.controller);
}

void Controller::transmitComplete()
{
	serial.setTransmit(false);
	serial.release(state);
	m_updating = false;

	// Schedule next update
	if(m_changed) {
		m_timer.setIntervalMs<DMX_UPDATE_CHANGED_MS>();
		m_timer.startOnce();
	} else if(DMX_UPDATE_PERIODIC_MS != 0) {
		m_timer.setIntervalMs<DMX_UPDATE_PERIODIC_MS>();
		m_timer.startOnce();
	}
}

/* Request */

Error Request::parseJson(JsonObjectConst json)
{
	Error err = IO::Request::parseJson(json);
	if(!!err) {
		return err;
	}
	m_value = json[ATTR_VALUE].as<unsigned>();
	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);
	json[IO::ATTR_NODE] = m_node.id;
	json[ATTR_VALUE] = m_value;
}

bool Request::setNode(DevNode node)
{
	if(!device().isValid(node)) {
		return false;
	}

	m_node = node;
	return true;
}

/* Device */

static Error createDevice(IO::Controller& controller, IO::Device*& device)
{
	if(!controller.verifyClass(CONTROLLER_CLASSNAME)) {
		return Error::bad_controller_class;
	}

	device = new Device(reinterpret_cast<Controller&>(controller));
	return device ? Error::success : Error::nomem;
}

const DeviceClassInfo deviceClass()
{
	return {DEVICE_CLASSNAME, createDevice};
}

Error Device::init(const Config& config)
{
	Error err = IO::Device::init(config);
	if(!!err) {
		return err;
	}
	m_address = config.address;
	m_nodeCount = config.nodeCount ?: 1;
	assert(m_nodeData == nullptr);
	m_nodeData = new NodeData[m_nodeCount];
	memset(m_nodeData, 0, sizeof(NodeData) * m_nodeCount);

	return Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	IO::Device::parseJson(json, cfg);
	cfg.address = json[ATTR_ADDRESS] | 0x01;
	cfg.nodeCount = json[ATTR_COUNT] | 1;
}

Error Device::init(JsonObjectConst config)
{
	Config cfg{};
	parseJson(config, cfg);
	return init(cfg);
}

bool Device::update()
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

Error Device::execute(Request& request)
{
	auto node = request.node();
	if(!isValid(node)) {
		return Error::bad_node;
	}

	Error err{};

	// Apply request to device data
	auto apply = [&](unsigned nodeId) {
		auto& data = m_nodeData[nodeId];
		switch(request.command()) {
		case Command::off:
			data.disable();
			break;
		case Command::on:
			if(data.target == 0) {
				data.target = 100; // Default brightness
			}
			data.enable();
			break;
		case Command::adjust: {
			data.setTarget(data.target + request.value());
			data.enable();
			break;
		}
		case Command::send:
			data.setValue(request.value());
			break;
		default:
			err = Error::bad_command;
		}
	};

	if(node == DevNode_ALL) {
		for(unsigned id = 0; id < m_nodeCount; ++id) {
			apply(id);
		}
	} else {
		apply(node.id);
	}

	controller().deviceChanged();

	return err;
}

} // namespace DMX512
} // namespace IO
