#include "Device.h"
#include "Request.h"
#include "Controller.h"
#include "Strings.h"

namespace IO
{
Error Device::init(const Config& config)
{
	if(!config.id) {
		return Error::no_device_id;
	}

	m_id = config.id;
	m_name = config.name;
	return Error::success;
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	cfg.id = json[FS_id].as<const char*>();
	cfg.name = json[FS_name].as<const char*>();
}

/*
 * Perform device-specific startup operations.
 * By default we fetch device state.
 *
 * This method will be called periodically by controller if
 * uninitialised or faultly. Note that stop() is NOT called
 * first. That is only called if, for example, controller is being restarted
 * or configuration reloaded.
 */
Error Device::start()
{
	if(m_state == devstate_normal || m_state == devstate_starting) {
		return Error::success;
	}

	Request* req = createRequest();
	if(req == nullptr) {
		return Error::no_mem;
	}

	// This fails if device doesn't have any nodes
	if(!req->nodeQuery(DevNode_ALL)) {
		delete req;
		m_state = devstate_normal;
		return Error::success;
	}

	auto err = req->submit();
	if(!err) {
		m_state = devstate_starting;
	}

	return err;
}

/*
 * Inherited classes might override this method to place device in a low-power state.
 */
Error Device::stop()
{
	return Error::success;
}

Error Device::submit(Request* request)
{
	return m_controller.submit(request);
}

void Device::handleEvent(Request* request, Event event)
{
	if(event == Event::RequestComplete) {
		if(request->status() == Status::error) {
			m_state = devstate_fault;
			m_controller.deviceError(*this);
		} else if(request->status() == Status::success && m_state == devstate_starting) {
			m_state = devstate_normal;
		}
	}

	m_controller.handleEvent(request, event);
}

String Device::caption()
{
	return m_controller.id() + '/' + m_id;
}

} // namespace IO