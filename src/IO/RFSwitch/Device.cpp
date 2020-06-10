#include "Device.h"
#include "Request.h"

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR(CONTROLLER_CLASSNAME, "rfswitch")
const FlashString& DEVICE_CLASSNAME = CONTROLLER_CLASSNAME;

namespace
{
DEFINE_FSTR_LOCAL(ATTR_TIMING, "timing")
DEFINE_FSTR_LOCAL(ATTR_STARTH, "starth")
DEFINE_FSTR_LOCAL(ATTR_STARTL, "startl")
DEFINE_FSTR_LOCAL(ATTR_PERIOD, "period")
DEFINE_FSTR_LOCAL(ATTR_BIT0, "bit0")
DEFINE_FSTR_LOCAL(ATTR_BIT1, "bit1")
DEFINE_FSTR_LOCAL(ATTR_GAP, "gap")
DEFINE_FSTR(ATTR_REPEATS, "repeats")

const unsigned RF_DEFAULT_REPEATS = 20;

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

	return Error::success;
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

Error Device::init(JsonObjectConst config)
{
	Error err = IO::Device::init(config);
	if(!!err) {
		return err;
	}

	return readProtocol(m_protocol, config);
}

namespace
{
Error createDevice(IO::Controller& controller, IO::Device*& device)
{
	if(!controller.verifyClass(CONTROLLER_CLASSNAME)) {
		return Error::bad_controller_class;
	}

	device = new Device(reinterpret_cast<Controller&>(controller));
	return device ? Error::success : Error::no_mem;
}

} // namespace

const DeviceClassInfo Device::classInfo() const
{
	return {DEVICE_CLASSNAME, createDevice};
}

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

} // namespace RFSwitch
} // namespace IO
