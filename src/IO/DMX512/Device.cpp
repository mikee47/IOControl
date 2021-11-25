/*
 * IODMX512.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include <IO/DMX512/Device.h>
#include <IO/DMX512/Request.h>
#include <IO/RS485/Controller.h>
#include <IO/Strings.h>

namespace IO
{
namespace DMX512
{
SimpleTimer Device::m_timer;
bool Device::m_changed{false};
bool Device::m_updating{false};

namespace
{
DEFINE_FSTR_LOCAL(DEVICE_CLASSNAME, "dmx")

/* DMX minimum timings per E1.11 */
constexpr unsigned DMX_BREAK{92};
constexpr unsigned DMX_MAB{12}; // Mark After Break
constexpr uint32_t DMX_BAUDRATE{250000};
constexpr uint8_t DMX_SERIAL_CONFIG{UART_8N2};

//
#define DMX_UPDATE_CHANGED_MS 10	///< Slave data has changed
#define DMX_UPDATE_PERIODIC_MS 1000 // 5000	///< Periodic update interval

ErrorCode createDevice(IO::Controller& controller, IO::Device*& device)
{
	if(!controller.verifyClass(RS485::CONTROLLER_CLASSNAME)) {
		return Error::bad_controller_class;
	}

	device = new Device(reinterpret_cast<IO::RS485::Controller&>(controller));
	return device ? Error::success : Error::no_mem;
}

} // namespace

const DeviceClassInfo deviceClass()
{
	return {DEVICE_CLASSNAME, createDevice};
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

void Device::updateSlaves()
{
	debug_i("DMX512: updateSlaves()");

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
	auto& serial = controller().getSerial();

	Serial::Config cfg{
		.baudrate = DMX_BAUDRATE,
		.config = DMX_SERIAL_CONFIG,
	};
	serial.setConfig(cfg);

	const uint16_t maxAddr = 512;

	// data + padding - note address #0 isn't used and is always set to 0 for lighting applications
	unsigned dataSize = maxAddr + 1 + 2;
	uint8_t data[dataSize];
	memset(data, 0, dataSize);
	data[0] = 0x00; // Lighting start code
	for(unsigned i = 0; i < controller().devices().count(); ++i) {
		auto dev = reinterpret_cast<Device*>(controller().devices()[i]);
		if(dev->type() != DeviceType::DMX512) {
			continue;
		}

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

	debug_hex(INFO, ">", data, dataSize, 0, 32);

	controller().setDirection(Direction::Outgoing);
	serial.setBreak(true);
	delayMicroseconds(DMX_BREAK);
	serial.setBreak(false);
	delayMicroseconds(DMX_MAB);
	serial.write(data, dataSize);
	uint8_t c{0};
	serial.write(&c, 1);

	m_updating = true;
	m_changed = false;
}

ErrorCode Device::init(const Config& config)
{
	ErrorCode err = IO::RS485::Device::init(config.base);
	if(err) {
		return err;
	}
	m_nodeCount = config.nodeCount ?: 1;
	assert(m_nodeData == nullptr);
	m_nodeData = new NodeData[m_nodeCount];
	memset(m_nodeData, 0, sizeof(NodeData) * m_nodeCount);

	auto& serial = controller().getSerial();
	if(!serial.resizeBuffers(0, MaxPacketSize)) {
		return Error::no_mem;
	}

	m_timer.setCallback(
		[](void* arg) {
			auto dev = static_cast<Device*>(arg);
			auto req = dev->createRequest();
			req->setCommand(Command::update);
			req->submit();
		},
		this);
	m_timer.setIntervalMs<DMX_UPDATE_CHANGED_MS>();
	m_timer.startOnce();

	return Error::success;
}

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	IO::RS485::Device::parseJson(json, cfg.base);
	if(cfg.base.slave.address == 0) {
		cfg.base.slave.address = 0x01;
	}
	cfg.nodeCount = json[FS_count] | 1;
}

ErrorCode Device::init(JsonObjectConst config)
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

void Device::handleEvent(IO::Request* request, Event event)
{
	switch(event) {
	case Event::Execute:
		assert(request->command() == Command::update);
		updateSlaves();
		break;

	case Event::TransmitComplete:
		assert(m_updating);
		m_updating = false;

		// Schedule next update
		if(m_changed) {
			m_timer.setIntervalMs<DMX_UPDATE_CHANGED_MS>();
			m_timer.startOnce();
		} else if(DMX_UPDATE_PERIODIC_MS != 0) {
			m_timer.setIntervalMs<DMX_UPDATE_PERIODIC_MS>();
			m_timer.startOnce();
		}
		request->complete(Error::success);
		return;

	case Event::ReceiveComplete:
	case Event::RequestComplete:
	case Event::Timeout:
		break;
	}

	IO::RS485::Device::handleEvent(request, event);
}

ErrorCode Device::execute(Request& request)
{
	auto node = request.node();
	if(!isValid(node)) {
		return Error::bad_node;
	}

	ErrorCode err{};

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
		case Command::set:
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

	if(!m_changed) {
		m_timer.setIntervalMs<DMX_UPDATE_CHANGED_MS>();
		m_timer.startOnce();
		m_changed = true;
	}

	return err;
}

} // namespace DMX512
} // namespace IO
