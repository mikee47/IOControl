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

#include <IO/Modbus/Request.h>

namespace IO
{
namespace Modbus
{
void Request::callback(PDU& pdu)
{
	complete(!m_exception ? Status::success : Status::error);
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);

	if(status() == Status::error) {
		setError(json, unsigned(m_exception), toString(m_exception));
	}
}

} // namespace Modbus
} // namespace IO
