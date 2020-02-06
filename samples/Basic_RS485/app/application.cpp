#include <SmingCore.h>
#include <FlashString/Stream.hpp>

#include <IO/Modbus/R421A/R421A.h>
static Modbus::Controller modbus0(0);

//#include <DMX512/IODMX512.h>
//static DMX512Controller DMX0(0);

IMPORT_FSTR_LOCAL(DEVMGR_CONFIG, PROJECT_DIR "/config/devices.json")

//
static HardwareSerial dbgser(UART_ID_1);

// Called for execute or complete
static void devmgrCallback(IO::Request& request)
{
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

static IO::Error devmgrInit()
{
	// Setup modbus stack
	modbus0.registerDeviceClass(R421A::Device::deviceClass);
	IO::devmgr.registerController(modbus0);

	// Setup RF switch stack
	//	RFSwitchController::registerDeviceClass(RFSwitchDevice::deviceClass);
	//	devmgr.registerController(rfswitch0);

	// At present conflicts with modbus0 - needs work to allow modbus and DMX to co-exist on same segment
	//	DMX512Controller::registerDeviceClass(DMX512Device::deviceClass);
	//	devmgr.registerController(DMX0);

#ifndef ARCH_HOST
//	pixel0.registerDeviceClass(PixelDevice::deviceClass);
//	devmgr.registerController(pixel0);
#endif

	IO::devmgr.setCallback(devmgrCallback);
	IO::devmgr.start();

//	R421A::Device* dev{nullptr};
//	R421A::Device::Config cfg;
//	auto err = modbus0.createDevice(dev, cfg);

	StaticJsonDocument<1024> doc;
	FSTR::Stream str(DEVMGR_CONFIG);
	if(!Json::deserialize(doc, str)) {
		debug_e("Device config load failed");
		return IO::Error::bad_config;
	}

	return IO::devmgr.begin(doc.as<JsonObjectConst>());
}

static void systemReady()
{
	devmgrInit();
}

void init()
{
	// Configure serial ports
	//  Serial.systemDebugOutput(false);
	Serial.end();

#if DEBUG_BUILD
#ifdef ARCH_HOST
	dbgser.setPort(0);
#endif

	dbgser.setTxBufferSize(4096);
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
