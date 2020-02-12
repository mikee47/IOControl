#include <SmingCore.h>
#include <FlashString/Stream.hpp>

#include <IO/Modbus/Debug.h>
#include <IO/Modbus/R421A/R421A.h>
#include <IO/DMX512/DMX512.h>

namespace
{
static IO::Serial serial0;
static IO::Modbus::Controller modbus0(serial0, 0);
static IO::DMX512::Controller dmx0(serial0, 0);

// Pin to switch  MAX485 between receive (low) and transmit (high)
static constexpr uint8_t MBPIN_TX_EN = 12;

IMPORT_FSTR_LOCAL(DEVMGR_CONFIG, PROJECT_DIR "/config/devices.json")

static constexpr uint8_t MODBUS_SLAVE_ID{10};

//
HardwareSerial dbgser(UART_ID_1);

// Called for execute or complete
void devmgrCallback(IO::Request& request)
{
	//	if(request.device().isInstanceOf(R421A::Device)) {
	//aaa
	//	}

	//	request.device().
	if(request.device().id() == "mb1") {
		auto& req = reinterpret_cast<IO::Modbus::R421A::Request&>(request);
		auto& response = req.response();
		debug_i("%s: %08x / %08x", __FUNCTION__, response.channelMask, response.channelStates);
	}

	auto status = request.status();

	auto& controller = request.device().controller();
	if(&controller == &modbus0) {
		if(status == IO::Status::pending) {
			//			g_userio.setDot(dot_ModbusStatus, led_on);
		} else {
			// Request has completed
			//			g_userio.setDot(dot_ModbusStatus, (status == Status::success) ? led_off : led_flash_slow);
		}
	}
	//	else if(&controller == &rfswitch0) {
	//#ifdef USERIO_ENABLE
	//		g_userio.setDot(dot_RFStatus, (request.status() == Status::pending) ? led_on : led_off);
	//		updatePanelStatus(request);
	//#endif
	//	}
}

static bool handleModbusRequest(IO::Modbus::ADU& adu)
{
	IO::Modbus::printRequest(dbgser, adu);
	if(adu.slaveAddress != MODBUS_SLAVE_ID) {
		return false; // ignore
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

	IO::Modbus::printResponse(dbgser, adu);

	return true;
}

static void IRAM_ATTR setSerialTransmit(bool enable)
{
	digitalWrite(MBPIN_TX_EN, enable);
}

IO::Error devmgrInit()
{
	auto err = serial0.open(UART0);
	if(!!err) {
		debug_e("Error opening serial port");
		return err;
	}

	// Set up the transmit enable GPIO which toggles MAX485 direction
	pinMode(MBPIN_TX_EN, OUTPUT);
	setSerialTransmit(false);
	serial0.onTransmit(setSerialTransmit);
	// Use alternate serial pins and put default ones in safe state
	serial0.swap();
//	pinMode(1, INPUT_PULLUP);
//	pinMode(3, INPUT_PULLUP);

	// Setup modbus stack
	modbus0.registerDeviceClass(IO::Modbus::R421A::deviceClass);
	IO::devmgr.registerController(modbus0);
	modbus0.onRequest(handleModbusRequest);

	// Setup RF switch stack
	//	RFSwitchController::registerDeviceClass(RFSwitchDevice::deviceClass);
	//	devmgr.registerController(rfswitch0);

	// At present conflicts with modbus0 - needs work to allow modbus and DMX to co-exist on same segment
	//	dmx0.registerDeviceClass(IO::DMX512::deviceClass);
	//	IO::devmgr.registerController(dmx0);

#ifndef ARCH_HOST
//	pixel0.registerDeviceClass(PixelDevice::deviceClass);
//	devmgr.registerController(pixel0);
#endif

	IO::devmgr.setCallback(devmgrCallback);
	//	IO::devmgr.start();

	//	R421A::Device* dev{nullptr};
	//	R421A::Device::Config cfg{};
	//	auto err = modbus0.createDevice(dev, cfg);

	StaticJsonDocument<1024> doc;
	FSTR::Stream str(DEVMGR_CONFIG);
	if(!Json::deserialize(doc, str)) {
		debug_e("Device config load failed");
		return IO::Error::bad_config;
	}

	return IO::devmgr.begin(doc.as<JsonObjectConst>());
}

Timer testTimer;

void systemReady()
{
	auto err = devmgrInit();

	/*
	{
		// Example of manual device creation
		auto dev = new IO::DMX512::Device(dmx0);
		IO::DMX512::Device::Config cfg;
		cfg.id = "dmx1";
		cfg.address = 1;
		cfg.nodeCount = 10;
		IO::Error err = dev->init(cfg);
		if(!!err) {
			// Failure
			delete dev;
		} else {
			// Success
			auto req = new IO::DMX512::Request(*dev);
		}
	}
	*/

	testTimer.initializeMs<5000>(InterruptCallback([]() {
		{
			IO::Request* req;
			auto err = IO::devmgr.createRequest("mb1", req);
			if(!err) {
				req->setID("Toggle all outputs");
				req->nodeToggle(IO::DevNode_ALL);
				//										req->setID("Toggle output #1");
				//										req->nodeToggle(IO::DevNode{1});
				req->submit();
			}
		}

		//			auto dev = reinterpret_cast<IO::DMX512::Device*>(IO::devmgr.findDevice("dmx1"));
		//			assert(dev->getClass() == IO::DMX512::deviceClass());
		//			auto req = new IO::DMX512::Request(*dev);

		/*
			IO::Request* req;
			auto err = IO::devmgr.createRequest("dmx1", req);
			assert(!err);
			req->setID("Adjust output #7");
			req->nodeAdjust(IO::DevNode{7}, 2);
			req->submit();
*/

		testTimer.startOnce();
	}));

	testTimer.startOnce();
}

} // namespace

void init()
{
#if DEBUG_BUILD
#ifdef ARCH_HOST
	dbgser.setPort(0);
#endif

	dbgser.setTxBufferSize(8192);
	dbgser.setRxBufferSize(0);
	dbgser.setTxWait(false);
	dbgser.begin(COM_SPEED_SERIAL);

	dbgser.systemDebugOutput(true);
	debug_i("\n\n********************************************************\n"
			"Hello\n");
#endif

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
