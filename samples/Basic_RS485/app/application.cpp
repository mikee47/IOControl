#include <SmingCore.h>
#include <FlashString/Stream.hpp>

#include <IO/Modbus/Debug.h>
#include <IO/Modbus/Slave.h>
#include <IO/Modbus/R421A/Request.h>
#include <IO/DMX512/Request.h>
#include <IO/DeviceManager.h>

namespace
{
IO::Serial serial0;
IO::RS485::Controller rs485_0(serial0, 0);
Timer testTimer;

// Pin to switch  MAX485 between receive (low) and transmit (high)
constexpr uint8_t MBPIN_TX_EN = 12;

IMPORT_FSTR_LOCAL(DEVMGR_CONFIG, PROJECT_DIR "/config/devices.json")

/*
 * See handleRS485Request function.
 */
constexpr uint8_t MODBUS_SLAVE_ID{10};

// Called for execute or complete
void devmgrCallback(const IO::Request& request)
{
	if(request.device == "mb1") {
		auto& req = static_cast<const IO::Modbus::R421A::Request&>(request);
		auto& response = req.getResponse();
		debug_i("%s: %08x / %08x", __FUNCTION__, response.channelMask, response.channelStates);
	}

	switch(request.device.type()) {
	case IO::DeviceType::Modbus:
		if(request.isPending()) {
			// Request has been submitted to hardware
		} else {
			// Request has completed
		}
		break;

	case IO::DeviceType::RFSwitch:
		break;

	default:
		break;
	}
}

/*
 * Although the IO Control stack is generally a master, unsolicited requests on a modbus
 * segment get routed here. That means asynchronous slave operation is supported!
 */
void handleRS485Request(IO::RS485::Controller& controller)
{
	IO::Modbus::ADU adu;
	auto err = IO::Modbus::readRequest(controller, adu);
	if(err) {
		return;
	}

	IO::Modbus::printRequest(Serial, adu);
	if(adu.slaveAddress != MODBUS_SLAVE_ID) {
		// This example looks for requests only for a specific slave device
		// Can also handle broadcasts though
		return;
	}

	switch(adu.pdu.function()) {
	case IO::Modbus::Function::ReadDiscreteInputs: {
		auto& data = adu.pdu.data.readDiscreteInputs;
		auto req{data.request};
		auto& rsp{data.response};
		rsp.setCount(req.quantityOfInputs);
		for(unsigned i = 0; i < req.quantityOfInputs; ++i) {
			rsp.setInput(i, i & 1);
		}
		break;
	}

	case IO::Modbus::Function::ReadInputRegisters: {
		auto& data = adu.pdu.data.readInputRegisters;
		auto req{data.request};
		auto& rsp{data.response};
		rsp.setCount(req.quantityOfRegisters);
		for(unsigned i = 0; i < req.quantityOfRegisters; ++i) {
			rsp.values[i] = req.startAddress + i;
		}
		break;
	}

	case IO::Modbus::Function::ReportServerId: {
		auto& rsp = adu.pdu.data.reportServerId.response;
		rsp.serverId = 0x20;
		rsp.runStatus = rsp.runstatus_on;
		memcpy(rsp.data, "12345", 5);
		rsp.setCount(5);
		break;
	}

	default:
		adu.pdu.setException(IO::Modbus::Exception::IllegalFunction);
	}

	IO::Modbus::printResponse(Serial, adu);
	IO::Modbus::sendResponse(controller, adu);
}

/*
 * This example uses a single RS485 segment.
 * i.e. assumes there's a single line driver connected to the serial port.
 *
 * RS485 doesn't like star topologies as reflections mess up signal integrity.
 * This can be fixed by adding a second MAX485 driver, even using the same serial port.
 * The direction lines can be multiplexed using two GPIO signals, managed via this callback.
 */
void IRAM_ATTR setSerialDirection(uint8_t segment, IO::Direction direction)
{
	digitalWrite(MBPIN_TX_EN, direction == IO::Direction::Outgoing);
}

IO::ErrorCode devmgrInit()
{
	auto err = serial0.open(0);
	if(err) {
		debug_e("Error opening serial port");
		return err;
	}

	// Set up the transmit enable GPIO which toggles MAX485 direction
	pinMode(MBPIN_TX_EN, OUTPUT);
	setSerialDirection(0, IO::Direction::Incoming);
	rs485_0.onSetDirection(setSerialDirection);
	// Use alternate serial pins and put default ones in safe state
	serial0.swap();

	// Setup modbus stack
	rs485_0.registerDeviceClass(IO::Modbus::R421A::Device::factory);
	rs485_0.registerDeviceClass(IO::DMX512::Device::factory);
	IO::devmgr.registerController(rs485_0);
	rs485_0.onRequest(handleRS485Request);

	IO::devmgr.setCallback(devmgrCallback);

	StaticJsonDocument<1024> doc;
	FSTR::Stream str(DEVMGR_CONFIG);
	if(!Json::deserialize(doc, str)) {
		debug_e("Device config load failed");
		return IO::Error::bad_config;
	}

	return IO::devmgr.begin(doc.as<JsonObjectConst>());
}

void issueRequests()
{
	{
		IO::Request* req;
		auto err = IO::devmgr.createRequest("mb1", req);
		if(!err) {
			req->setID("Toggle all outputs");
			req->nodeToggle(IO::DevNode_ALL);
			// Setting a request ID is optional, could use for webpage state management in request callback
			// req->setID("toggle#1");
			// req->nodeToggle(IO::DevNode{1});
			req->submit();
		}
	}

	// Submit a request using code
	IO::Request* req;
	auto err = IO::devmgr.createRequest("dmx1", req);
	if(err) {
		debug_e("Error creating request: %s", IO::Error::toString(err).c_str());
	} else {
		req->setID("adjust#7");
		req->nodeAdjust(IO::DevNode{7}, 2);
		req->submit();
	}

	auto webRequestCallback = [](const IO::Request& request) {
		if(request.isPending()) {
			debug_i("Web request '%s' pending...", request.caption().c_str());
		} else {
			debug_i("Web request '%s' complete, %s", request.caption().c_str(),
					IO::Error::toString(request.error()).c_str());
		}
	};

	// This might be a request received from web client
	StaticJsonDocument<512> doc;
	auto json = doc.to<JsonObject>();
	json["id"] = F("adjust#1");
	json["device"] = "dmx2";
	json["node"] = 1;
	json["command"] = "adjust";
	json["value"] = 3;
	IO::devmgr.handleMessage(json, webRequestCallback);

	json["id"] = F("toggle#5");
	json["device"] = "mb1";
	json["node"] = 5;
	json["command"] = "toggle";
	IO::devmgr.handleMessage(json, webRequestCallback);

	testTimer.startOnce();
}

void systemReady()
{
	auto err = devmgrInit();
	Serial << "devmgrInit(): " << IO::Error::toString(err) << endl;

	/*
	{
		// Example of manual device creation
		IO::DMX512::Device::Config cfg{
			.rs485 =
				{
					.base = {.name = F("Example DMX device")},
					.slave = {.address = 1, .baudrate = 9600},
				},
			.nodeCount = 10,
		};
		IO::DMX512::Device* dev;
		auto err = rs485_0.createDevice("dmx1", cfg, dev);
		if(err) {
			// Failure
		} else {
			// Success
			auto req = new IO::DMX512::Request(*dev);
			req->nodeToggle(IO::DevNode_ALL);
			req->submit();
		}
	}
	*/

	testTimer.initializeMs<2000>(issueRequests);
	testTimer.startOnce();
}

} // namespace

void init()
{
	Serial.setPort(1);
	Serial.begin(COM_SPEED_SERIAL);
	Serial.systemDebugOutput(true);
	debug_i("\n\n********************************************************\n"
			"Hello\n");

	// Ensure pins don't float
#ifndef ENABLE_GDB
	pinMode(1, INPUT_PULLUP);
	pinMode(3, INPUT_PULLUP);
#endif
	pinMode(14, INPUT_PULLUP);

#ifdef ARCH_HOST
	setDigitalHooks(nullptr);
#endif

	System.onReady(systemReady);
}
