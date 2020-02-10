#pragma once

#include "PDU.h"
#include <IO/Status.h>

namespace IO
{
namespace Modbus
{
// Buffer to construct requests and process responses
union ADU {
	static constexpr size_t MinSize{5};
	static constexpr size_t MaxSize{256};

	struct {
		uint8_t slaveId;
		PDU pdu;
	};
	uint8_t buffer[MaxSize];

	/**
	 * @name Prepare outgoing packet
	 * The `slaveId` and `pdu` fields must be correctly initialised.
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
