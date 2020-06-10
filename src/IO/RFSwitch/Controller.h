#pragma once

#include <IO/Controller.h>
#include "RFSwitch.h"

namespace IO
{
namespace RFSwitch
{
DECLARE_FSTR(CONTROLLER_CLASSNAME)

class Controller : public Controller
{
public:
	Controller(uint8_t instance) : IO::Controller(instance)
	{
	}

	String classname() override
	{
		return CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;
	void handleEvent(Request* request, Event event) override;

	bool busy() const override
	{
		return m_rfswitch.busy();
	}

private:
	void rfswitchCallback(bool transmit, uint32_t code, uint32_t param);
	void execute(IORequest& request) override;

private:
	RFSwitch m_rfswitch;
};

} // namespace RFSwitch
} // namespace IO
