#include <IO/Modbus/Debug.h>
#include <stringconversion.h>

namespace IO
{
namespace Modbus
{
static constexpr int numberBase = HEX;

template <typename T> static size_t print(Print& p, T value)
{
	switch(numberBase) {
	case HEX: {
		char buf[8];
		buf[0] = '0';
		buf[1] = 'x';
		ultoa_wp(value, &buf[2], numberBase, sizeof(value) * 2, '0');
		return p.print(buf);
	}

	case DEC:
	default:
		return p.print(value);
	}
}

static size_t print(Print& p, const char* name, uint16_t var)
{
	size_t n{};
	n += p.println();
	n += p.print(name);
	n += p.print(" = ");
	n += print(p, var);
	return n;
}

enum class ValueFormat {
	bit,
	byte,
	word,
};

static size_t printValues(Print& p, const char* name, const void* values, size_t count, ValueFormat format)
{
	constexpr size_t lineBreak{10};

	size_t n{};
	n += p.println();
	n += p.print(name);
	n += p.print(" =");
	auto valptr = static_cast<const uint8_t*>(values);
	bool sep = false;
	for(size_t i = 0; i < count; ++i) {
		if(i > 0) {
			n += p.print(", ");
		}
		if(i % lineBreak == 0) {
			n += p.println();
			String off(i);
			while(off.length() < 4) {
				off = ' ' + off;
			}
			n += p.print(off);
			n += p.print(": ");
		}

		if(format == ValueFormat::bit) {
			uint8_t byte = *valptr;
			if(i % 8 == 7) {
				++valptr;
			}
			auto bit = (byte >> (i % 8)) & 1;
			n += p.print(bit ? '1' : '0');
		} else {
			uint16_t val = *valptr++;
			if(format == ValueFormat::word) {
				val |= uint16_t(*valptr) << 8;
				++valptr;
			}
			n += print(p, val);
		}
	}
	return n;
}

#define printField(name) n += print(p, _F(#name), req.name)

size_t printRequest(Print& p, const PDU& pdu)
{
	size_t n{};
	n += p.print(_F("function = "));
	n += p.print(toString(pdu.function()));

	switch(pdu.function()) {
	case Function::None:
	case Function::ReadExceptionStatus:
	case Function::ReportServerId:
	case Function::GetComEventCounter:
	case Function::GetComEventLog:
		break;

	case Function::ReadCoils: {
		auto& req{pdu.data.readCoils.request};
		printField(startAddress);
		printField(quantityOfCoils);
		break;
	}

	case Function::ReadDiscreteInputs: {
		auto& req{pdu.data.readDiscreteInputs.request};
		printField(startAddress);
		printField(quantityOfInputs);
		break;
	}

	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters: {
		auto& req{pdu.data.readInputRegisters.request};
		printField(startAddress);
		printField(quantityOfRegisters);
		break;
	}

	case Function::ReadWriteMultipleRegisters: {
		auto& req{pdu.data.readWriteMultipleRegisters.request};
		printField(readAddress);
		printField(quantityToRead);
		printField(writeAddress);
		printField(quantityToWrite);
		printField(writeByteCount);
		n += printValues(p, _F("values"), req.writeValues, req.quantityToWrite, ValueFormat::word);
		break;
	}

	case Function::WriteSingleCoil: {
		auto& req{pdu.data.writeSingleCoil.request};
		printField(outputAddress);
		printField(outputValue);
		break;
	}

	case Function::WriteSingleRegister: {
		auto& req{pdu.data.writeSingleRegister.request};
		printField(address);
		printField(value);
		break;
	}

	case Function::WriteMultipleCoils: {
		auto& req{pdu.data.writeMultipleCoils.request};
		printField(startAddress);
		printField(quantityOfOutputs);
		printField(byteCount);
		n += printValues(p, _F("values"), req.values, req.quantityOfOutputs, ValueFormat::bit);
		break;
	}

	case Function::WriteMultipleRegisters: {
		auto& req{pdu.data.writeMultipleRegisters.request};
		printField(startAddress);
		printField(quantityOfRegisters);
		printField(byteCount);
		n += printValues(p, _F("values"), req.values, req.quantityOfRegisters, ValueFormat::word);
		break;
	}

	case Function::MaskWriteRegister: {
		auto& req{pdu.data.maskWriteRegister.request};
		printField(address);
		printField(andMask);
		printField(orMask);
		break;
	}
	}

	n += p.println();
	return n;
}

#undef printField
#define printField(name) n += print(p, _F(#name), rsp.name)

size_t printResponse(Print& p, const PDU& pdu)
{
	size_t n{};
	n += p.print(_F("function = "));
	n += p.print(toString(pdu.function()));

	switch(pdu.function()) {
	case Function::None:
		break;

	case Function::ReadExceptionStatus: {
		auto& rsp{pdu.data.readExceptionStatus.response};
		printField(outputData);
		break;
	}

	case Function::ReportServerId: {
		auto& rsp{pdu.data.reportServerId.response};
		printField(byteCount);
		printField(serverId);
		printField(runStatus);
		n += printValues(p, _F("data"), rsp.data, rsp.getCount(), ValueFormat::byte);
		break;
	}

	case Function::GetComEventCounter: {
		auto& rsp{pdu.data.getComEventCounter.response};
		printField(status);
		printField(eventCount);
		break;
	}

	case Function::GetComEventLog: {
		auto& rsp{pdu.data.getComEventLog.response};
		printField(byteCount);
		printField(status);
		printField(eventCount);
		printField(messageCount);
		n += printValues(p, _F("events"), rsp.events, rsp.eventCount, ValueFormat::word);
		break;
	}

	case Function::ReadCoils: {
		auto& rsp{pdu.data.readCoils.response};
		printField(byteCount);
		n += printValues(p, _F("coilStatus"), rsp.coilStatus, rsp.byteCount * 8, ValueFormat::bit);
		break;
	}

	case Function::ReadDiscreteInputs: {
		auto& rsp{pdu.data.readDiscreteInputs.response};
		printField(byteCount);
		n += printValues(p, _F("inputStatus"), rsp.inputStatus, rsp.byteCount * 8, ValueFormat::bit);
		break;
	}

	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters: {
		auto& rsp{pdu.data.readHoldingRegisters.response};
		printField(byteCount);
		n += printValues(p, _F("values"), rsp.values, rsp.byteCount / 2, ValueFormat::word);
		break;
	}

	case Function::ReadWriteMultipleRegisters: {
		auto& rsp{pdu.data.readWriteMultipleRegisters.response};
		break;
	}

	case Function::WriteSingleCoil: {
		auto& rsp{pdu.data.writeSingleCoil.response};
		printField(outputAddress);
		printField(outputValue);
		break;
	}

	case Function::WriteSingleRegister: {
		auto& rsp{pdu.data.writeSingleRegister.response};
		printField(address);
		printField(value);
		break;
	}

	case Function::WriteMultipleCoils: {
		auto& rsp{pdu.data.writeMultipleCoils.response};
		break;
	}

	case Function::WriteMultipleRegisters: {
		auto& rsp{pdu.data.writeMultipleRegisters.response};
		break;
	}

	case Function::MaskWriteRegister: {
		auto& rsp{pdu.data.maskWriteRegister.response};
		printField(address);
		printField(andMask);
		printField(orMask);
		break;
	}
	}

	n += p.println();
	return n;
}

size_t printRequest(Print& p, const ADU& adu)
{
	size_t n{};
	n += p.print(_F("slaveAddress = "));
	n += p.println(adu.slaveAddress);
	n += printRequest(p, adu.pdu);
	return n;
}

size_t printResponse(Print& p, const ADU& adu)
{
	size_t n{};
	n += p.print(_F("slaveAddress = "));
	n += p.println(adu.slaveAddress);
	n += printResponse(p, adu.pdu);
	return n;
}

} // namespace Modbus
} // namespace IO
