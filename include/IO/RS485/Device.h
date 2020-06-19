#pragma once

#include <IO/Device.h>
#include "Controller.h"

namespace IO
{
namespace RS485
{
class Device : public IO::Device
{
public:
	Device(Controller& controller) : IO::Device(controller)
	{
	}

	Controller& controller()
	{
		return reinterpret_cast<Controller&>(IO::Device::controller());
	}
};

} // namespace RS485
} // namespace IO
