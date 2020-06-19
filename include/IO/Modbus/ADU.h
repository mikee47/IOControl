#pragma once

#include "PDU.h"
#include <IO/Error.h>

namespace IO
{
namespace Modbus
{
// Buffer to construct RTU requests and process responses
union ADU {
	static constexpr uint8_t BROADCAST_ADDRESS{0x00};
	static constexpr size_t MinSize{4};
	static constexpr size_t MaxSize{256};

	struct {
		uint8_t slaveAddress;
		PDU pdu;
	};
	uint8_t buffer[MaxSize];

	/**
	 * @name Prepare outgoing packet
	 * The `slaveAddress` and `pdu` fields must be correctly initialised.
	 * @retval size_t Size of ADU, 0 on error
	 * @{
	 */
	size_t prepareRequest();
	size_t prepareResponse();
	/** @} */

	/**
	 * @name Parse a received packet
	 * @param receivedSize How much data was received
	 * @retval Error Result of parsing
	 * @{
	 */
	Error parseRequest(size_t receivedSize);
	Error parseResponse(size_t receivedSize);
	/** @} */

private:
	size_t preparePacket(size_t pduSize);
	Error parsePacket(size_t receivedSize, size_t pduSize);
};

static_assert(offsetof(ADU, pdu) == 1, "ADU alignment error");

} // namespace Modbus
} // namespace IO
