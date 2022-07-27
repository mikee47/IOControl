/**
 * Modbus/Device.cpp
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
 ****/

#include <IO/Modbus/Device.h>
#include <IO/Modbus/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace Modbus
{
ErrorCode Device::init(const RS485::Device::Config& config)
{
	auto& ctrl = static_cast<IO::RS485::Controller&>(controller);
	if(!ctrl.getSerial().resizeBuffers(ADU::MaxSize, ADU::MaxSize)) {
		debug_e("Failed to resize serial buffers");
		//		return Error::no_mem;
	}

	return IO::RS485::Device::init(config);
}

void Device::handleEvent(IO::Request* request, Event event)
{
	auto req = static_cast<Request*>(request);

	switch(event) {
	case Event::Execute: {
		IO::RS485::Device::handleEvent(request, event);
		ErrorCode err = execute(req);
		if(err != Error::pending) {
			request->complete(err);
			return;
		}
		break;
	}

	case Event::ReceiveComplete: {
		auto err = readResponse(req);
		if(err != Error::pending) {
			request->complete(err);
		}
		break;
	}

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
ErrorCode Device::execute(Request* request)
{
	// Fill out the ADU packet
	ADU adu;
	requestFunction = request->fillRequestData(adu.pdu.data);
	adu.pdu.setFunction(requestFunction);
	adu.slaveAddress = request->getAddress();
	auto aduSize = adu.prepareRequest();
	if(aduSize == 0) {
		return Error::bad_size;
	}

	// Prepare UART for comms
	IO::Serial::Config cfg{
		.baudrate = baudrate(),
		.format = UART_8N1,
	};
	auto& serial = getController().getSerial();
	serial.setConfig(cfg);
	serial.clear();

	// OK, issue the request
	getController().send(adu.buffer, aduSize);
	return Error::pending;
}

ErrorCode Device::readResponse(Request* request)
{
	// Read packet
	ADU adu;
	auto& serial = getController().getSerial();
	auto receivedSize = serial.read(adu.buffer, ADU::MaxSize);

	// Parse the received packet
	ErrorCode err = adu.parseResponse(receivedSize);

	if(!err) {
		// In master mode, check for consistency with current request
		if(adu.slaveAddress != request->device.address()) {
			// Mismatch with command slave ID
			err = Error::bad_param;
		} else if(adu.pdu.function() != requestFunction) {
			// Mismatch with command function
			err = Error::bad_command;
		}
	}

	if(err) {
		debug_i("MB: Received %u bytes, err = %d (%s)", receivedSize, err, Error::toString(err).c_str());
	} else {
		debug_d("MB: received '%s': %s", toString(adu.pdu.function()).c_str(), toString(adu.pdu.exception()).c_str());
		switch(adu.pdu.exception()) {
		case Exception::Success:
			err = request->callback(adu.pdu);
			break;
		case Exception::IllegalDataValue:
			err = Error::bad_param;
			break;
		case Exception::IllegalFunction:
			err = Error::bad_command;
			break;
		case Exception::IllegalDataAddress:
		case Exception::SlaveDeviceFailure:
		default:
			err = Error::bad_node;
		}
	}

	return err;
}

} // namespace Modbus
} // namespace IO
