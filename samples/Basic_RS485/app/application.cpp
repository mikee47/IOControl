#include <SmingCore.h>
#include <esp_spi_flash.h>
#include <IOControls.h>

#include <Modbus/R421A.h>
static ModbusController modbus0(0);

#include <DMX512/IODMX512.h>
static DMX512Controller DMX0(0);

#include <ioControls.h>

//
static HardwareSerial dbgser(UART_ID_1);

// Called for execute or complete
static void devmgrCallback(IORequest& request)
{
	request_status_t status = request.status();

	const IOController& controller = request.device().controller();
	if(&controller == &modbus0) {
		if(status == status_pending) {
#ifdef USERIO_ENABLE
			g_userio.setDot(dot_ModbusStatus, led_on);
#endif
		} else {
#ifdef USERIO_ENABLE
			// Request has completed
			g_userio.setDot(dot_ModbusStatus, (status == status_success) ? led_off : led_flash_slow);
			updatePanelStatus(request);
#endif

			// Broadcast results, but only on success; otherwise just return error status to caller
			if(status == status_success) {
				sendStatus(request, nullptr);
				return;
			}
		}
	} else if(&controller == &rfswitch0) {
#ifdef USERIO_ENABLE
		g_userio.setDot(dot_RFStatus, (request.status() == status_pending) ? led_on : led_off);
		updatePanelStatus(request);
#endif
	}

	// Return status to caller
	auto conn = request.connection();
	if(conn != nullptr) {
		sendStatus(request, conn);
	}
}

static void networkStatusChanged(network_change_t nwc)
{
#ifdef USERIO_ENABLE
	led_state_t state;
	if(WifiAccessPoint.isEnabled()) {
		state = led_flash_fast;
	} else if(WifiStation.isConnected()) {
		state = led_on;
	} else {
		state = led_flash_slow;
	}
	g_userio.setDot(dot_WiFiStatus, state);
#endif
}

#ifdef USERIO_ENABLE

/*
 * Obtain displayed text for a button.
 *
 * @param button
 * @param flags
 *
 * @returns text to display
 */
static String getButtonText(button_t button, button_flags_t flags)
{
	// Long press is for system buttons
	if(flags & bf_long) {
		switch(button) {
		case btn_addr:
			return DISP_ADDR;
		case btn_ap:
			return WifiAccessPoint.isEnabled() ? DISP_AP_OFF : DISP_AP_ON;
		case btn_reset:
			return DISP_RESET;
		default:
			return DISP_BUTTON_UNASSIGNED;
		}
	}

	if(button == btn_none) {
		return (flags & bf_select) ? String(DISP_SELECT) : String::nullstr;
	}

	String id = CUserIO::getButtonId(button, flags);
	const IOControl* c = ioControls.find(id);
	return c ? c->name() : id;
}

static void button_callback(button_t button, button_flags_t flags)
{
	debug_i("button_callback(%d, %02X)", button, flags);

	// If this is just a notification, update displayed button text
	if((flags & bf_execute) == 0) {
		g_userio.setMessage(getButtonText(button, flags));
		return;
	}

	g_userio.setMessage(nullptr);

	// Long press is for system commands
	if(flags & bf_long) {
		switch(button) {
		case btn_addr: {
			EStationConnectionStatus status = WifiStation.getConnectionStatus();
			if(status == eSCS_GotIP) {
				g_userio.setMessage(WifiStation.getIP().toString(), IPADDR_TIMEOUT);
			} else {
				g_userio.setMessage((status == eSCS_Connecting) ? DISP_WIFI_NOIP : DISP_WIFI_ERROR, MESSAGE_TIMEOUT);
			}
			break;
		}
		case btn_ap:
			networkManager.accessPointMode(!WifiAccessPoint.isEnabled());
			break;
		btn_reset:
			g_system.restart();
			break;
		default:;
		}
		return;
	}

	String id = CUserIO::getButtonId(button, flags);
	if(id.length() != 0) {
		if(!ioControls.trigger(id)) {
			g_userio.setMessage(DISP_BUTTON_UNASSIGNED, MESSAGE_TIMEOUT);
		}
	}
}

#endif

static ioerror_t devmgrInit()
{
	//return ioe_cancelled; // @todo testing

	// Setup modbus stack
	ModbusController::registerDeviceClass(ModbusR421ADevice::deviceClass);
	devmgr.registerController(modbus0);

	// Setup RF switch stack
	RFSwitchController::registerDeviceClass(RFSwitchDevice::deviceClass);
	devmgr.registerController(rfswitch0);

	// At present conflicts with modbus0 - needs work to allow modbus and DMX to co-exist on same segment
	DMX512Controller::registerDeviceClass(DMX512Device::deviceClass);
	devmgr.registerController(DMX0);

#ifndef ARCH_HOST
	pixel0.registerDeviceClass(PixelDevice::deviceClass);
	devmgr.registerController(pixel0);
#endif

	devmgr.setCallback(devmgrCallback);

	return initDevices(true);
}

static ioerror_t initDevices(bool full)
{
	DynamicJsonDocument doc(2048);
	ioerror_t err = ioe_success;
	if(full) {
		if(!Json::loadFromFile(doc, FILE_DEVMGR)) {
			debug_e("%s: Config load failed", __PRETTY_FUNCTION__);
			return ioe_bad_config;
		}
		err = devmgr.begin(doc.as<JsonObjectConst>());
		if(err) {
			return err;
		}
	}

	if(!Json::loadFromFile(doc, FILE_PANEL)) {
		debug_e("%s: Config load failed", __PRETTY_FUNCTION__);
		return ioe_bad_config;
	}

	return ioControls.load(doc.as<JsonObject>());
}

/*
 * Deal with change on AUX input.
 *
 * Action is defined in panel configuration. Primary use case
 * is for intruder alarm, which activates or toggles the input.
 * Config typically uses this to set the state of exterior
 * lights.
 *
 */
static void auxInputCallback(bool state)
{
	PSTR_ARRAY(id, "aux_off");
	if(state) {
		id[5] = 'n';
		id[6] = '\0';
	}
	ioControls.trigger(id);
}

static void systemReady()
{
	{
		DynamicJsonDocument config(512);
		if(Json::loadFromFile(config, FILE_TIMEMGMT)) {
			timeManager.configure(config.as<JsonObjectConst>());
		}
		timeManager.onChange(TimeChangeDelegate(&IOTimerList::onTimeChange, &ioTimers));
	}

	ioerror_t err = devmgrInit();

	auxInput.begin(auxInputCallback);

	socketManager.registerHandler(devmgr);
	socketManager.registerHandler(g_fileManager);
	g_fileManager.onUpload([](const FileUpload& upload) {
		if(upload.filename() == FILE_DEVMGR) {
			initDevices(true);
		} else if(upload.filename() == FILE_PANEL) {
			initDevices(false);
		}
	});

	socketManager.registerHandler(networkManager);

	socketManager.registerHandler(g_authManager);
	g_authManager.onLoginComplete(LoginDelegate(&WebsocketManager::loginComplete, &socketManager));

	socketManager.registerHandler(g_webServer);

	socketManager.registerHandler(g_system);

	socketManager.registerHandler(g_firmwareUpdate);

	g_webServer.start();

#ifdef USERIO_ENABLE
	if(err)
		g_userio.setMessage(DISP_CONFIG_ERROR);
	else
		g_userio.setMessage(DISP_CONFIG_OK, MESSAGE_TIMEOUT);
#endif

	// @todo user can reset to default config via front panel
}

#include "milightremote.h"

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

	LOAD_FSTR(dt, COMPILE_DATETIME)
	debug_i("Firmware compiled %s", dt);

	//  testAll(dbgser);

	// Ensure pins don't float
#ifndef ENABLE_GDB
	pinMode(1, INPUT_PULLUP);
	pinMode(3, INPUT_PULLUP);
#endif
	pinMode(14, INPUT_PULLUP);

#ifdef ARCH_HOST
	setDigitalHooks(nullptr);
#endif

	//  milightInit();

	//
#ifdef USERIO_ENABLE
	g_userio.begin(button_callback);
	g_userio.setDot(dot_WiFiStatus, led_flash_slow);
	// Gets cleared on successful init
	g_userio.setMessage(DISP_STARTUP);
#endif

	if(!g_fileManager.init(fwfsImage.flashData)) {
#ifdef USERIO_ENABLE
		g_userio.setMessage(DISP_SYSTEM_ERROR);
#endif
	}

	System.onReady(systemReady);

	networkManager.onStatusChange(networkStatusChanged);
	networkManager.begin();
}
