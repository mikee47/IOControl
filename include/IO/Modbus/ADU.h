/**
 * Modbus/ADU.h
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
 *
 * This file is part of the IOControl Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#pragma once

#include "PDU.h"
#include "../Error.h"

namespace IO::Modbus
{
// Buffer to construct RTU requests and process responses
struct ADU {
	static constexpr uint8_t BROADCAST_ADDRESS{0x00};
	static constexpr size_t MinSize{4};
	static constexpr size_t MaxSize{256};

	union {
		struct {
			uint8_t slaveAddress;
			PDU pdu;
		};
		uint8_t buffer[MaxSize];
	};

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
	 * @retval ErrorCode Result of parsing
	 * @{
	 */
	ErrorCode parseRequest(size_t receivedSize);
	ErrorCode parseResponse(size_t receivedSize);
	/** @} */

private:
	size_t preparePacket(size_t pduSize);
	ErrorCode parsePacket(size_t receivedSize, size_t pduSize);
};

static_assert(offsetof(ADU, pdu) == 1, "ADU alignment error");

} // namespace IO::Modbus
