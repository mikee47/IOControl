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

#include "Device.h"
#include "Request.h"
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
Error Device::init(const Config& config)
{
	auto err = IO::Device::init(config);
	if(!!err) {
		return err;
	}

	if(config.slave.address == 0) {
		return Error::no_address;
	}

	if(config.slave.baudrate == 0) {
		return Error::no_baudrate;
	}

	m_config = config.slave;

	auto& ctrl = reinterpret_cast<IO::RS485::Controller&>(controller());
	if(!ctrl.getSerial().resizeBuffers(ADU::MaxSize, ADU::MaxSize)) {
		return Error::no_mem;
	}

	return Error::success;
}

Error Device::init(JsonObjectConst config)
{
	Config cfg{};
	parseJson(config, cfg);
	return init(cfg);
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	IO::Device::parseJson(json, cfg);
	cfg.slave.address = json[FS_address];
	cfg.slave.baudrate = json[FS_baudrate];
}

void Device::handleEvent(IO::Request* request, Event event)
{
	auto req = reinterpret_cast<Request*>(request);

	switch(event) {
	case Event::Execute:
		execute(req);
		break;

	case Event::ReceiveComplete:
		readResponse(req);
		break;

	case Event::TransmitComplete:
	case Event::Timeout:
		break;

	case Event::RequestComplete:
		requestFunction = Function::None;
		break;
	}

	IO::RS485::Device::handleEvent(request, event);
}

/*
 * Submit a modbus command.
 *
 * The ADU must be valid and contain two reserved bytes at the end for the checksum,
 * which this method calculates.
 *
 * We write data into the hardware FIFO: no need for any software buffering.
 *
 */
void Device::execute(Request* request)
{
	// Fill out the ADU packet
	ADU adu;
	requestFunction = request->fillRequestData(adu.pdu.data);
	adu.pdu.setFunction(requestFunction);
	adu.slaveAddress = request->device().address();
	auto aduSize = adu.prepareRequest();
	if(aduSize == 0) {
		request->complete(Status::error);
		return;
	}

	// Prepare UART for comms
	IO::Serial::Config cfg{
		.baudrate = baudrate(),
		.config = UART_8N1,
	};
	auto& serial = controller().getSerial();
	serial.setConfig(cfg);
	serial.clear();

	// OK, issue the request
	controller().send(adu.buffer, aduSize);

	IO::RS485::Device::handleEvent(request, Event::Execute);
}

void Device::readResponse(Request* request)
{
	// Read packet
	ADU adu;
	auto& serial = controller().getSerial();
	auto receivedSize = serial.read(adu.buffer, ADU::MaxSize);

	// Parse the received packet
	Error err;
	err = adu.parseResponse(receivedSize);
	if(!!err) {
		return;
	}

	// In master mode, check for consistency with current request
	if(adu.slaveAddress != request->device().address()) {
		// Mismatch with command slave ID
		err = Error::bad_param;
	} else if(adu.pdu.function() != requestFunction) {
		// Mismatch with command function
		err = Error::bad_command;
	} else {
		err = Error::success;
	}

	if(!!err) {
		debug_e("MB: %s", toString(err).c_str());
		request->complete(Status::error);
		return;
	}

	debug_d("MB: received '%s': %s", toString(adu.pdu.function()).c_str(), toString(adu.pdu.exception()).c_str());
	request->callback(adu.pdu);
}

} // namespace Modbus
} // namespace IO
