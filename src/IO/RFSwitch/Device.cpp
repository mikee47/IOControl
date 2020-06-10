#include "Device.h"
#include "Request.h"

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR(CONTROLLER_CLASSNAME, "rfswitch")
const FlashString& DEVICE_CLASSNAME = CONTROLLER_CLASSNAME;

DEFINE_FSTR_LOCAL(ATTR_TIMING, "timing")
DEFINE_FSTR_LOCAL(ATTR_STARTH, "starth")
DEFINE_FSTR_LOCAL(ATTR_STARTL, "startl")
DEFINE_FSTR_LOCAL(ATTR_PERIOD, "period")
DEFINE_FSTR_LOCAL(ATTR_BIT0, "bit0")
DEFINE_FSTR_LOCAL(ATTR_BIT1, "bit1")
DEFINE_FSTR_LOCAL(ATTR_GAP, "gap")
DEFINE_FSTR(ATTR_REPEATS, "repeats")

const unsigned RF_DEFAULT_REPEATS = 20;

Error Device::init(JsonObjectConst config)
{
	Error err = IO::Device::init(config);
	if(!!err) {
		return err;
	}

	JsonObjectConst timing = config[ATTR_TIMING];
	m_timing.starth = timing[ATTR_STARTH];
	m_timing.startl = timing[ATTR_STARTL];
	m_timing.period = timing[ATTR_PERIOD];
	m_timing.bit0 = timing[ATTR_BIT0];
	m_timing.bit1 = timing[ATTR_BIT1];
	m_timing.gap = timing[ATTR_GAP];
	m_repeats = config[ATTR_REPEATS];

	if(m_repeats == 0) {
		m_repeats = RF_DEFAULT_REPEATS;
	}

	return Error::success;
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
