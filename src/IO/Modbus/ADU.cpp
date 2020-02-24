#include "ADU.h"
#include <debug_progmem.h>

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
size_t ADU::prepareRequest()
{
	pdu.swapRequestByteOrder();
	return preparePacket(pdu.getRequestSize());
}

size_t ADU::prepareResponse()
{
	pdu.swapResponseByteOrder();
	return preparePacket(pdu.getResponseSize());
}

size_t ADU::preparePacket(size_t pduSize)
{
	if(pduSize == 0) {
		debug_e("MB: PDU invalid");
		return 0;
	}

	auto size{1 + pduSize}; // slave ID
	if(size + 2 > MaxSize) {
		debug_e("MB: ADU too big");
		return 0;
	}

	auto crc = crc16_update(0xFFFF, buffer, size);
	buffer[size++] = uint8_t(crc);
	buffer[size++] = uint8_t(crc >> 8);

	debug_hex(DBG, ">", buffer, size);

	if(size < MinSize) {
		debug_e("MB: ADU too small");
		return 0;
	}

	return size;
}

Error ADU::parseRequest(size_t receivedSize)
{
	auto err = parsePacket(receivedSize, pdu.getRequestSize());
	pdu.swapRequestByteOrder();
	return err;
}

Error ADU::parseResponse(size_t receivedSize)
{
	auto err = parsePacket(receivedSize, pdu.getResponseSize());
	pdu.swapResponseByteOrder();
	return err;
}

Error ADU::parsePacket(size_t receivedSize, size_t pduSize)
{
	if(receivedSize < MinSize) {
		if(receivedSize != 0) {
			debug_w("MB: %u bytes received, require at least %u", receivedSize, MinSize);
		}
		return Error::bad_size;
	}

	auto aduSize = 1 + pduSize + 2; // slaveAddress + data + CRC
	if(receivedSize < aduSize) {
		debug_w("MB: Only %u bytes read, %u expected", receivedSize, aduSize);
		return Error::bad_size;
	}

	debug_hex(DBG, "<", buffer, aduSize);

	auto crc = crc16_update(0xFFFF, buffer, aduSize);
	if(crc != 0) {
		return Error::bad_checksum;
	}

	return Error::success;
}

} // namespace Modbus
} // namespace IO
