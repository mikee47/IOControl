#include "Controller.h"

namespace IO
{
namespace RFSwitch
{
DEFINE_FSTR(CONTROLLER_CLASSNAME, "rfswitch")

void Controller::start()
{
	m_rfswitch.begin(RFSwitchCallback([](uint32_t code, uint32_t param) {
		auto req = static_cast<Request*>(param);
		assert(req != nullptr);
		req->callback();
	}));
	IO::Controller::start();
}

void Controller::stop()
{
	IO::Controller::stop();
}

void Controller::handleEvent(Request* request, Event event)
{
	if(event == Event::Execute) {
		execute(*request);
	}

	IO::Controller::handleEvent(request, event);
}

/*
 * Might want to update this in future to take a byte array. It's likely
 * though that a single code is sufficient.
 */
void Controller::execute(IO::Request& request)
{
	if(request.command() != ioc_send) {
		debug_err(ioe_bad_command, request.caption());
		request.complete(status_error);
		return;
	}

	auto& req = reinterpret_cast<RFSwitchRequest&>(request);
	if(!m_rfswitch.transmit(req.protocol(), req.code(), (uint32_t)&req)) {
		debug_e("! execute(): transmit busy");
	}
}

} // namespace RFSwitch
} // namespace IO
