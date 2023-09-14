/**
 * Modbus/Slave.cpp
 *
 *  Created on: 1 May 2018
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
 * Modbus slaves
 * =============
 *
 * The application for which this library was developed uses Modbus via UART0 with MAX485 chip,
 * and extra GPIO to control direction.
 *
 * We use the uart driver (via IO::Serial class) for efficient and precise timing of transfers.
 *
 *  - Transactions will easily fit into hardware FIFO so no need for additional memory overhead;
 *  - ISR set to trigger on completion of transmit/receive packets rather than individual bytes;
 *
 ****/

#include <IO/Modbus/Slave.h>
#include <IO/RS485/Controller.h>

namespace IO::Modbus
{
ErrorCode readRequest(RS485::Controller& controller, ADU& adu)
{
	// Read packet
	auto& serial = controller.getSerial();
	auto receivedSize = serial.read(adu.buffer, ADU::MaxSize);

	// Parse the received packet
	ErrorCode err = adu.parseRequest(receivedSize);

	if(err) {
		debug_e("MB: %s", Error::toString(err).c_str());
	} else {
		debug_d("MB: received '%s': %s", toString(adu.pdu.function()).c_str(), toString(adu.pdu.exception()).c_str());
	}

	return err;
}

void sendResponse(RS485::Controller& controller, ADU& adu)
{
	auto aduSize = adu.prepareResponse();
	controller.send(adu.buffer, aduSize);
}

} // namespace IO::Modbus
