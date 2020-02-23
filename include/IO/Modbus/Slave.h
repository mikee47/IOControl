#pragma once

#include <IO/RS485/Controller.h>
#include "ADU.h"

namespace IO
{
namespace Modbus
{
Error readRequest(RS485::Controller& controller, ADU& adu);
void sendResponse(RS485::Controller& controller, ADU& adu);

} // namespace Modbus
} // namespace IO
