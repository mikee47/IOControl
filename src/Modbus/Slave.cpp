/*
 * IOModbus.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 * Modbus uses UART0 via MAX485 chip with extra GPIO to control direction.
 * We use custom serial port code because it's actually simpler and much more efficient
 * than using the framework.
 *
 *  - Transactions will easily fit into hardware FIFO so no need for additional memory overhead;
 *  - ISR set to trigger on completion of transmit/receive packets rather than individual bytes;
 *
 */

#include <IO/Modbus/Slave.h>
#include <IO/RS485/Controller.h>

namespace IO
{
namespace Modbus
{
Error readRequest(RS485::Controller& controller, ADU& adu)
{
	// Read packet
	auto& serial = controller.getSerial();
	auto receivedSize = serial.read(adu.buffer, ADU::MaxSize);

	// Parse the received packet
	Error err = adu.parseRequest(receivedSize);

	if(!!err) {
		debug_e("MB: %s", toString(err).c_str());
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

} // namespace Modbus
} // namespace IO
