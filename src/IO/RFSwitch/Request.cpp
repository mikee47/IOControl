#include "Request.h"
#include "Device.h"

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR(ATTR_CODE, "code")

void Request::send(uint32_t code, uint8_t repeats)
{
	m_code = code;
	m_repeats = repeats ?: device().repeats();
}

Error Request::parseJson(JsonObjectConst json)
{
	Error err = IO::Request::parseJson(json);
	if(!!err) {
		return err;
	}
	const char* s = json[ATTR_CODE];
	if(s == nullptr) {
		return Error::no_code;
	}
	m_code = strtoul(s, nullptr, 16);

	Json::getValue(json[ATTR_REPEATS], m_repeats);

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);

	//  writeProtocol(m_protocol, json);

	json[ATTR_CODE] = String(m_code, HEX);
}

} // namespace RFSwitch
} // namespace IO
