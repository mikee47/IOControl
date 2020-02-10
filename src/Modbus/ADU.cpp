#include <IO/Modbus/ADU.h>

namespace
{
uint16_t crc16_update(uint16_t crc, uint8_t a)
{
	crc ^= a;

	for(unsigned i = 0; i < 8; ++i) {
		if(crc & 1)
			crc = (crc >> 1) ^ 0xA001;
		else
			crc = (crc >> 1);
	}

	return crc;
}

uint16_t crc16_update(uint16_t crc, const void* data, size_t count)
{
	auto p = static_cast<const uint8_t*>(data);
	while(count--) {
		crc = crc16_update(crc, *p++);
	}

	return crc;
}

} // namespace

namespace IO
{
namespace Modbus
{
size_t ADU::initRequest()
{
	auto size = 1 + pdu.getRequestSize(Direction::Outgoing); // SlaveID + PDU
	if(size < MinSize) {
		debug_e("MB: ADU too small");
		return 0;
	}

	if(size > MaxSize) {
		debug_e("MB: ADU too big");
		return 0;
	}

	pdu.swapRequestByteOrder(Direction::Outgoing);
	auto crc = crc16_update(0xFFFF, buffer, size);
	buffer[size++] = uint8_t(crc);
	buffer[size++] = uint8_t(crc >> 8);

	return size;
}

Error ADU::parseResponse(size_t receivedSize)
{
	if(receivedSize < MinSize) {
		if(receivedSize != 0) {
			debug_w("MB: %u bytes received, require at least %u", receivedSize, MinSize);
		}
		return Error::bad_size;
	}

	auto aduSize = getResponseSize();

	if(receivedSize < aduSize) {
		debug_w("MB: Only %u bytes read, %u expected", receivedSize, aduSize);
		return Error::bad_size;
	}

	debug_hex(DBG, "<", buffer, aduSize);

	auto crc = crc16_update(0xFFFF, buffer, aduSize);
	if(crc != 0) {
		return Error::bad_checksum;
	}

	pdu.swapResponseByteOrder();
	return Error::success;
}

Error ADU::parseRequest(size_t receivedSize)
{
	if(receivedSize < MinSize) {
		if(receivedSize != 0) {
			debug_w("MB: %u bytes received, require at least %u", receivedSize, MinSize);
		}
		return Error::bad_size;
	}

	auto aduSize = getRequestSize(Direction::Incoming);

	if(receivedSize < aduSize) {
		debug_w("MB: Only %u bytes read, %u expected", receivedSize, aduSize);
		return Error::bad_size;
	}

	debug_hex(DBG, "<", buffer, aduSize);

	auto crc = crc16_update(0xFFFF, buffer, aduSize);
	if(crc != 0) {
		return Error::bad_checksum;
	}

	pdu.swapResponseByteOrder();
	return Error::success;
}

} // namespace Modbus
} // namespace IO
