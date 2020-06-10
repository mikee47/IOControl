/*
 * IORFSwitch.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include "IORFSwitch.h"

DEFINE_FSTR(RFSWITCH_CONTROLLER_CLASSNAME, "rfswitch")

// Our device name
#define DEVICE_NAME RFSWITCH_CONTROLLER_CLASSNAME

DEFINE_FSTR_LOCAL(ATTR_TIMING, "timing")
DEFINE_FSTR_LOCAL(ATTR_STARTH, "starth")
DEFINE_FSTR_LOCAL(ATTR_STARTL, "startl")
DEFINE_FSTR_LOCAL(ATTR_PERIOD, "period")
DEFINE_FSTR_LOCAL(ATTR_BIT0, "bit0")
DEFINE_FSTR_LOCAL(ATTR_BIT1, "bit1")
DEFINE_FSTR_LOCAL(ATTR_GAP, "gap")
DEFINE_FSTR_LOCAL(ATTR_REPEATS, "repeats")

#define RF_DEFAULT_REPEATS 20

static ioerror_t readProtocol(RFSwitch::Protocol& protocol, JsonObjectConst config)
{
	JsonObjectConst timing = config[ATTR_TIMING];
	protocol.timing.starth = timing[ATTR_STARTH];
	protocol.timing.startl = timing[ATTR_STARTL];
	protocol.timing.period = timing[ATTR_PERIOD];
	protocol.timing.bit0 = timing[ATTR_BIT0];
	protocol.timing.bit1 = timing[ATTR_BIT1];
	protocol.timing.gap = timing[ATTR_GAP];
	protocol.repeats = config[ATTR_REPEATS];

	if(protocol.repeats == 0) {
		protocol.repeats = RF_DEFAULT_REPEATS;
	}

	return ioe_success;
}

/*
static void writeProtocol(const Protocol::Protocol& protocol, JsonObject config)
{
	JsonObject timing = config.createNestedObject(ATTR_TIMING);
	timing[ATTR_STARTH] = protocol.timing.starth;
	timing[ATTR_STARTL] = protocol.timing.startl;
	timing[ATTR_PERIOD] = protocol.timing.period;
	timing[ATTR_BIT0] = protocol.timing.bit0;
	timing[ATTR_BIT1] = protocol.timing.bit1;
	timing[ATTR_GAP] = protocol.timing.gap;
	config[ATTR_REPEATS] = protocol.repeats;
}
*/

/* CRFSwitchRequest */

void RFSwitchRequest::send(uint32_t code, uint8_t repeats)
{
	m_code = code;
	m_protocol = device().protocol();
	if(repeats != 0) {
		m_protocol.repeats = repeats;
	}
}

ioerror_t RFSwitchRequest::parseJson(JsonObjectConst json)
{
	ioerror_t err = IORequest::parseJson(json);
	if(err) {
		return err;
	}
	const char* s = json[ATTR_CODE];
	if(s == nullptr) {
		return ioe_no_code;
	}
	m_code = strtoul(s, nullptr, 16);

	m_protocol = device().protocol();
	Json::getValue(json[ATTR_REPEATS], m_protocol.repeats);

	return ioe_success;
}

void RFSwitchRequest::getJson(JsonObject json) const
{
	IORequest::getJson(json);

	//  writeProtocol(m_protocol, json);

	json[ATTR_CODE] = String(m_code, HEX);
}

/* CRFSwitchDevice */

ioerror_t RFSwitchDevice::init(JsonObjectConst config)
{
	ioerror_t err = IODevice::init(config);
	if(err) {
		return err;
	}

	return readProtocol(m_protocol, config);
}

/* CRFSwitchController */

void RFSwitchController::start()
{
	m_rfswitch.begin(RFSwitchCallback([](uint32_t code, uint32_t param) {
		auto req = reinterpret_cast<RFSwitchRequest*>(param);
		assert(req != nullptr);
		req->callback();
	}));
	IOController::start();
}

void RFSwitchController::stop()
{
	IOController::stop();
}

/*
 * Might want to update this in future to take a byte array. It's likely
 * though that a single code is sufficient.
 */
void RFSwitchController::execute(IORequest& request)
{
	if(request.command() != ioc_send) {
		debug_err(ioe_bad_command, request.caption());
		request.complete(status_error);
		return;
	}

	auto& req = reinterpret_cast<RFSwitchRequest&>(request);
	if(!m_rfswitch.transmit(req.protocol(), req.code(), (uint32_t)&req)) {
		debug_e("! execute(): transmit busy");
	}
}

static ioerror_t createDevice(IOController& controller, IODevice*& device)
{
	if(!controller.verifyClass(RFSWITCH_CONTROLLER_CLASSNAME)) {
		return ioe_bad_controller_class;
	}

	device = new RFSwitchDevice(reinterpret_cast<RFSwitchController&>(controller));
	return device ? ioe_success : ioe_nomem;
}

const device_class_info_t RFSwitchDevice::deviceClass()
{
	return {DEVICE_NAME, createDevice};
}
