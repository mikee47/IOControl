/*
 * IORFSwitch.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#pragma once

#include <IOControl.h>
#include "RFSwitch.h"

DECLARE_FSTR(RFSWITCH_CONTROLLER_CLASSNAME)

class RFSwitchDevice;
class RFSwitchController;

class RFSwitchRequest : public IORequest
{
	friend RFSwitchDevice;
	friend RFSwitchController;

public:
	RFSwitchRequest(RFSwitchDevice& device) : IORequest(reinterpret_cast<IODevice&>(device))
	{
		// Only one command applicable, may as well be the default
		setCommand(ioc_send);
	}

	const RFSwitchDevice& device() const
	{
		return reinterpret_cast<const RFSwitchDevice&>(m_device);
	}

	ioerror_t parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	/*
	 * We'll get called with NODES_ALL because no nodes are explictly specified.
	 */
	bool setNode(devnode_id_t nodeId) override
	{
		return (nodeId == NODES_ALL);
	}

	void send(uint32_t code, uint8_t repeats = 0);

	uint32_t code() const
	{
		return m_code;
	}

	const RFSwitch::Protocol& protocol() const
	{
		return m_protocol;
	}

protected:
	void callback()
	{
		// No response to interpret here, so we're done
		complete(status_success);
	}

private:
	uint32_t m_code = 0;
	RFSwitch::Protocol m_protocol;
};

/*
 * CRFSwitchDevice represents a specific type of RF device protocol.
 * Actual RF I/O is performed via the CRFSwitchController.
 */
class RFSwitchDevice : public IODevice
{
public:
	RFSwitchDevice(RFSwitchController& controller) : IODevice(reinterpret_cast<IOController&>(controller))
	{
	}

	static const device_class_info_t deviceClass();

	IORequest* createRequest() override
	{
		return new RFSwitchRequest(*this);
	}

	const RFSwitch::Protocol& protocol() const
	{
		return m_protocol;
	}

	ioerror_t start()
	{
		m_state = devstate_normal;
		return ioe_success;
	}

protected:
	ioerror_t init(JsonObjectConst config);

protected:
	RFSwitch::Protocol m_protocol;
};

class RFSwitchController : public IOController
{
public:
	RFSwitchController(uint8_t instance) : IOController(instance)
	{
	}

	String classname() override
	{
		return RFSWITCH_CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

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
