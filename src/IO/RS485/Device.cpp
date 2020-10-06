/*
 * Device.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 */

#include <IO/RS485/Device.h>
#include <IO/Request.h>
#include <IO/Strings.h>

namespace IO
{
namespace RS485
{
ErrorCode Device::init(const Config& config)
{
	auto err = IO::Device::init(config.base);
	if(err) {
		return err;
	}

	if(config.slave.address == 0) {
		return Error::no_address;
	}

	if(config.slave.baudrate == 0) {
		return Error::no_baudrate;
	}

	m_config = config.slave;

	return Error::success;
}

ErrorCode Device::init(JsonObjectConst config)
{
	Config cfg{};
	parseJson(config, cfg);
	return init(cfg);
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	IO::Device::parseJson(json, cfg.base);
	cfg.slave.segment = json[FS_segment];
	cfg.slave.address = json[FS_address];
	cfg.slave.baudrate = json[FS_baudrate];
}

void Device::handleEvent(IO::Request* request, Event event)
{
	auto& dev = reinterpret_cast<Device&>(request->device());

	switch(event) {
	case Event::Execute: {
		controller().setSegment(dev.segment());
		break;
	}

	case Event::TransmitComplete:
	case Event::Timeout:
	case Event::ReceiveComplete:
	case Event::RequestComplete:
		break;
	}

	IO::Device::handleEvent(request, event);
}

} // namespace RS485
} // namespace IO
