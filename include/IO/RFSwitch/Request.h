/*
 * IORFSwitch.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#pragma once

#include <IO/Request.h>
#include "Device.h"

namespace IO
{
namespace RFSwitch
{
class Request : public IO::Request
{
	friend Device;
	friend Controller;

public:
	Request(IO::Device& device) : IO::Request(device)
	{
		// Only one command applicable, may as well be the default
		setCommand(Command::set);
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(IO::Request::device());
	}

	ErrorCode parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	/*
	 * We'll get called with NODES_ALL because no nodes are explictly specified.
	 */
	bool setNode(DevNode node) override
	{
		return node == DevNode_ALL;
	}

	void send(uint32_t code, uint8_t repeats = 0);

	uint32_t code() const
	{
		return m_code;
	}

	uint8_t repeats() const
	{
		return m_repeats;
	}

protected:
	void callback()
	{
		// No response to interpret here, so we're done
		complete(Error::success);
	}

private:
	uint32_t m_code = 0;
	uint8_t m_repeats;
};

} // namespace RFSwitch
} // namespace IO
