#include "Device.h"
#include "Request.h"

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR(CONTROLLER_CLASSNAME, "rfswitch")
constexpr FlashString& DEVICE_CLASSNAME = RFSWITCH_CONTROLLER_CLASSNAME;

Error Device::init(JsonObjectConst config)
{
	Error err = IODevice::init(config);
	if(err) {
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
	return device ? Error::success : Error::nomem;
}

} // namespace

const Device::DeviceClassInfo classInfo() const
{
	return {DEVICE_CLASSNAME, createDevice};
}

} // namespace RFSwitch
} // namespace IO
