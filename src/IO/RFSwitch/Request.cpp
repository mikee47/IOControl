#include "Request.h"

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR_LOCAL(ATTR_TIMING, "timing")
DEFINE_FSTR_LOCAL(ATTR_STARTH, "starth")
DEFINE_FSTR_LOCAL(ATTR_STARTL, "startl")
DEFINE_FSTR_LOCAL(ATTR_PERIOD, "period")
DEFINE_FSTR_LOCAL(ATTR_BIT0, "bit0")
DEFINE_FSTR_LOCAL(ATTR_BIT1, "bit1")
DEFINE_FSTR_LOCAL(ATTR_GAP, "gap")
DEFINE_FSTR_LOCAL(ATTR_REPEATS, "repeats")

#define RF_DEFAULT_REPEATS 20

namespace
{
Error readProtocol(Protocol& protocol, JsonObjectConst config)
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

} // namespace

void Request::send(uint32_t code, uint8_t repeats)
{
	m_code = code;
	m_protocol = device().protocol();
	if(repeats != 0) {
		m_protocol.repeats = repeats;
	}
}

Error Request::parseJson(JsonObjectConst json)
{
	Error err = IO::Request::parseJson(json);
	if(err) {
		return err;
	}
	const char* s = json[ATTR_CODE];
	if(s == nullptr) {
		return Error::no_code;
	}
	m_code = strtoul(s, nullptr, 16);

	m_protocol = device().protocol();
	Json::getValue(json[ATTR_REPEATS], m_protocol.repeats);

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	IORequest::getJson(json);

	//  writeProtocol(m_protocol, json);

	json[ATTR_CODE] = String(m_code, HEX);
}

} // namespace RFSwitch
} // namespace IO
