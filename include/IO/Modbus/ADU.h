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
	 * @brief Initialise an outgoing request packet
	 *
	 * The `slaveId` and `pdu` fields must be correctly initialised.
	 *
	 * @retval size_t Size of ADU, 0 on error
	 */
	size_t prepareRequest();

	size_t prepareResponse();

	/**
	 * @brief Parse a received response packet
	 * @param receivedSize How much data was received
	 * @retval Error Result of parsing
	 */
	Error parseResponse(size_t receivedSize);

	/**
	 * @brief Parse a received request packet
	 * @param receivedSize How much data was received
	 * @retval Error Result of parsing
	 */
	Error parseRequest(size_t receivedSize);

	size_t getResponseSize() const
	{
		return 1 + pdu.getResponseSize() + 2; // slaveId + data + CRC
	}

	size_t getRequestSize() const
	{
		return 1 + pdu.getRequestSize() + 2; // slaveId + data + CRC
	}
};

static_assert(offsetof(ADU, pdu) == 1, "ADU alignment error");

} // namespace Modbus
} // namespace IO
