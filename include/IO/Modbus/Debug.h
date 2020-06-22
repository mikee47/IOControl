#pragma once

#include <IO/Modbus/ADU.h>
#include <Print.h>

namespace IO
{
namespace Modbus
{
/**
 * @name Debug print support, outputs contents of packet
 * @{
 */
size_t printRequest(Print& p, const PDU& pdu);
size_t printRequest(Print& p, const ADU& adu);

size_t printResponse(Print& p, const PDU& pdu);
size_t printResponse(Print& p, const ADU& adu);
/** @} */

} // namespace Modbus
} // namespace IO
