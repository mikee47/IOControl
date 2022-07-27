#include <SmingCore.h>
#include "Fan.h"
#include <IO/Modbus/Debug.h>
#include <IO/Modbus/Slave.h>
#include <LittleFS.h>

namespace
{
struct Config {
	const char* filename{"config.json"};
	uint8_t slave_id{2};

	void load();
	void save();
};

IO::Serial serial0;
IO::RS485::Controller rs485_0(serial0, 0);
SimpleTimer timer;
Config config;

#define LOCALSTR(str) DEFINE_FSTR(FS_##str, #str)

LOCALSTR(slaveid)

enum PinAssignment {
	// Slice 1, 2
	PIN_FAN1_TACHO = 3,
	PIN_FAN1_PWM = 4,
	// Slice 3, 4
	PIN_FAN2_TACHO = 7,
	PIN_FAN2_PWM = 8,
	// Slice 5, 6
	PIN_FAN3_TACHO = 11,
	PIN_FAN3_PWM = 12,
	// Pins to switch MAX485 direction lines
	PIN_MBTXEN = 16, // Active high
	PIN_MBRXEN = 17, // Active low
	// Modbus UART connections
	PIN_MBTX = 20,
	PIN_MBRX = 21,
};

constexpr size_t fanCount{3};
Fan fans[fanCount]{
	{"Fan 1", PIN_FAN1_TACHO, PIN_FAN1_PWM},
	{"Fan 2", PIN_FAN2_TACHO, PIN_FAN2_PWM},
	{"Fan 3", PIN_FAN3_TACHO, PIN_FAN3_PWM},
};

enum ModbusAddress {
	MBADDR_FAN1 = 0,
	MBADDR_FAN2 = 1,
	MBADDR_FAN3 = 2,
	MBADDR_FAN_MIN = MBADDR_FAN1,
	MBADDR_FAN_MAX = MBADDR_FAN3,
	MBADDR_SLAVE_ID = 254,
};

void handleRS485Request(IO::RS485::Controller& controller)
{
	using namespace IO::Modbus;

	ADU adu;
	auto err = readRequest(controller, adu);
	if(err) {
		return;
	}

#ifndef SMING_RELEASE
	printRequest(Serial, adu);
#endif
	if(adu.slaveAddress != config.slave_id) {
		return;
	}

	auto func = adu.pdu.function();
	switch(func) {
	case Function::WriteSingleRegister: {
		auto& data = adu.pdu.data.writeSingleRegister;
		auto& req{data.request};
		auto& rsp{data.response};

		if(req.address == MBADDR_SLAVE_ID) {
			config.slave_id = req.value;
			config.save();
			break;
		}

		if(req.address < MBADDR_FAN_MIN || req.address > MBADDR_FAN_MAX) {
			adu.pdu.setException(Exception::IllegalDataAddress);
			break;
		}

		auto& fan = fans[req.address - MBADDR_FAN_MIN];
		fan.setLevel(req.value);
		rsp.value = fan.getLevel();
		break;
	}

	case Function::WriteMultipleRegisters: {
		auto& data = adu.pdu.data.writeMultipleRegisters;
		auto& req{data.request};
		auto& rsp{data.response};
		auto addr = req.startAddress;
		if(addr < MBADDR_FAN_MIN || addr > MBADDR_FAN_MAX) {
			adu.pdu.setException(Exception::IllegalDataAddress);
			break;
		}
		auto n = std::min(size_t(req.quantityOfRegisters), 1U + MBADDR_FAN_MAX - addr);
		for(unsigned i = 0; i < n; ++i) {
			auto& fan = fans[addr - MBADDR_FAN_MIN + i];
			fan.setLevel(req.values[i]);
		}
		rsp.quantityOfRegisters = n;
		break;
	}

	case Function::ReadHoldingRegisters:
	case Function::ReadInputRegisters: {
		auto& data = adu.pdu.data.readHoldingRegisters;
		auto& req{data.request};
		auto& rsp{data.response};
		auto addr = req.startAddress;
		if(addr < MBADDR_FAN_MIN || addr > MBADDR_FAN_MAX) {
			adu.pdu.setException(Exception::IllegalDataAddress);
			break;
		}
		auto n = std::min(size_t(req.quantityOfRegisters), 1U + MBADDR_FAN_MAX - addr);
		rsp.setCount(n);
		for(unsigned i = 0; i < n; ++i) {
			auto& fan = fans[addr - MBADDR_FAN_MIN + i];
			rsp.values[i] = (func == Function::ReadInputRegisters) ? fan.getRpm() : fan.getLevel();
		}
		break;
	}

	default:
		adu.pdu.setException(Exception::IllegalFunction);
	}

#ifndef SMING_RELEASE
	printResponse(Serial, adu);
#endif
	sendResponse(controller, adu);
}

void IRAM_ATTR setSerialDirection(uint8_t segment, IO::Direction direction)
{
	digitalWrite(PIN_MBTXEN, direction == IO::Direction::Outgoing);
	digitalWrite(PIN_MBRXEN, direction == IO::Direction::Outgoing);
}

bool initModbus()
{
	auto err = serial0.open(UART1, PIN_MBTX, PIN_MBRX);
	if(err) {
		debug_e("Error opening serial port: %s", toString(err).c_str());
		return false;
	}

	// Set up the transmit enable GPIO which toggles MAX485 direction
	pinMode(PIN_MBTXEN, OUTPUT);
	pinMode(PIN_MBRXEN, OUTPUT);
	setSerialDirection(0, IO::Direction::Incoming);
	rs485_0.onSetDirection(setSerialDirection);
	rs485_0.onRequest(handleRS485Request);
	rs485_0.start();

	return true;
}

void Config::load()
{
	StaticJsonDocument<1024> doc;
	if(!Json::loadFromFile(doc, filename)) {
		return;
	}
	auto json = doc.as<JsonObject>();
	slave_id = json[FS_slaveid];
}

void Config::save()
{
	StaticJsonDocument<1024> doc;
	auto json = doc.to<JsonObject>();
	json[FS_slaveid] = slave_id;

	Json::saveToFile(doc, filename);
}

} // namespace

void init()
{
	Serial.begin(COM_SPEED_SERIAL);
	Serial.systemDebugOutput(true);

	lfs_mount();
	config.load();

	for(auto& fan : fans) {
		fan.begin();
	}

	initModbus();

	timer.initializeMs<1000>([]() {
		for(auto& fan : fans) {
			fan.readRpm();
#ifndef SMING_RELEASE
			debug_i("%s: %d RPM", fan.name(), fan.getRpm());
#endif
		}
	});
	timer.start();
}
