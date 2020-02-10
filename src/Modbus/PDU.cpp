#include <IO/Modbus/PDU.h>

namespace IO
{
namespace Modbus
{
namespace
{
// Perform unaligned byteswap on array of uint16_t
void bswap(void* values, unsigned count)
{
	auto p = static_cast<uint8_t*>(values);
	while(count--) {
		std::swap(p[0], p[1]);
		p += 2;
	}
}

} // namespace

String toString(Exception exception)
{
	switch(exception) {
	case Exception::Success:
		return F("Success");
	case Exception::IllegalFunction:
		return F("IllegalFunction");
	case Exception::IllegalDataAddress:
		return F("IllegalDataAddress");
	case Exception::IllegalDataValue:
		return F("IllegalDataValue");
	case Exception::SlaveDeviceFailure:
		return F("SlaveDeviceFailure");
	default:
		return F("Unknown") + String(unsigned(exception));
	}
}

/**
 * @brief Get size (in bytes) of PDU Data for request packet
 */
size_t PDU::getRequestDataSize(Direction dir) const
{
	switch(function()) {
	case Function::ReadCoils: {
	case Function::ReadDiscreteInputs: {
		auto bitCount = data.readCoils.request.quantityOfCoils;
		if(dir == Direction::Incoming) {
			bitCount = __builtin_bswap16(bitCount);
		}
		return 2 + (bitCount + 7) / 8;
	}

	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters:
	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
		return 4;

	case Function::WriteMultipleCoils:
	case Function::WriteMultipleRegisters:
		return 5 + data.writeMultipleCoils.request.byteCount;

	case Function::MaskWriteRegister:
		return 6;

	case Function::ReadWriteMultipleRegisters:
		return 9 + data.readWriteMultipleRegisters.request.writeByteCount;

	case Function::ReadExceptionStatus:
	case Function::GetComEventCounter:
	case Function::GetComEventLog:
	case Function::ReportSlaveID:
	default:
		return 0;
	}
	}
}

/**
 * @brief Get size (in bytes) of PDU Data in received response packet
 */
size_t PDU::getResponseDataSize() const
{
	if(exceptionFlag()) {
		return 1;
	}

	switch(function()) {
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs:
	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters:
	case Function::ReportSlaveID:
	case Function::ReadWriteMultipleRegisters:
	case Function::GetComEventLog:
		return 1 + data.readCoils.response.byteCount;

	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
	case Function::GetComEventCounter:
	case Function::WriteMultipleCoils:
	case Function::WriteMultipleRegisters:
		return 4;

	case Function::ReadExceptionStatus:
		return 1;

	case Function::MaskWriteRegister:
		return 6;

	default:
		return 0;
	}
}

void PDU::swapRequestByteOrder(Direction dir)
{
	if(exceptionFlag()) {
		return;
	}

	switch(function()) {
	case Function::None:
	case Function::GetComEventLog:
	case Function::ReadExceptionStatus:
	case Function::ReportSlaveID:
		break;

	case Function::ReadCoils:
	case Function::ReadDiscreteInputs:
	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters:
	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
	case Function::GetComEventCounter:
	case Function::WriteMultipleCoils:
		bswap(&data.readCoils.request.startAddress, 2);
		break;

	case Function::WriteMultipleRegisters: {
		auto& req{data.writeMultipleRegisters.request};
		auto count = req.quantityOfRegisters;
		bswap(&req.startAddress, 2);
		bswap(req.values, (dir == Direction::Incoming) ? req.quantityOfRegisters : count);
		break;
	}

	case Function::MaskWriteRegister: {
		bswap(&data.maskWriteRegister.request.address, 3);
		break;
	}

	case Function::ReadWriteMultipleRegisters: {
		auto& req = data.readWriteMultipleRegisters.request;
		auto count = req.writeByteCount;
		bswap(&req.readAddress, 3);
		bswap(&req.writeValues, ((dir == Direction::Incoming) ? req.writeByteCount : count) / 2);
		break;
	}
	}
}

void PDU::swapResponseByteOrder()
{
	if(exceptionFlag()) {
		return;
	}

	switch(function()) {
	case Function::None:
	case Function::ReadCoils:
	case Function::ReadDiscreteInputs:
	case Function::ReadExceptionStatus:
	case Function::ReportSlaveID:
		break;

	case Function::ReadHoldingRegisters: {
	case Function::ReadInputRegisters:
	case Function::ReadWriteMultipleRegisters:
		auto& req = data.readHoldingRegisters.response;
		bswap(req.values, req.byteCount / 2);
		break;
	}

	case Function::WriteSingleCoil:
	case Function::WriteSingleRegister:
	case Function::GetComEventCounter:
	case Function::WriteMultipleCoils:
	case Function::WriteMultipleRegisters:
		bswap(&data.writeSingleCoil.response.outputAddress, 2);
		break;

	case Function::GetComEventLog:
		bswap(&data.getComEventLog.response.status, 3);
		break;

	case Function::MaskWriteRegister:
		bswap(&data.maskWriteRegister.response.address, 3);
		break;
	}
}

} // namespace Modbus
} // namespace IO
