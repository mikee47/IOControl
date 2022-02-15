/**
 * Request.h
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

#pragma once

#include "Error.h"
#include "DevNode.h"
#include "Event.h"
#include <debug_progmem.h>

namespace IO
{
/*
 * IO Commands
 */
#define IOCOMMAND_MAP(XX)                                                                                              \
	XX(undefined, "Undefined or invalid")                                                                              \
	XX(query, "Query node states")                                                                                     \
	XX(off, "Turn node off or set to minimum")                                                                         \
	XX(on, "Turn node on or set to maximum")                                                                           \
	XX(toggle, "Toggle node(s) between on and off")                                                                    \
	XX(latch, "Relay nodes")                                                                                           \
	XX(momentary, "Relay nodes")                                                                                       \
	XX(delay, "Relay nodes")                                                                                           \
	XX(set, "Set value")                                                                                               \
	XX(adjust, "Adjust value")                                                                                         \
	XX(update, "Perform update cycle (e.g. DMX512)")

enum class Command {
#define XX(tag, comment) tag,
	IOCOMMAND_MAP(XX)
#undef XX
};

String toString(Command cmd);
bool fromString(Command& cmd, const char* str);

class Device;
class Request;

/**
 * @brief Request represents a single user request/response over a bus.
 *
 * Inherited classes provide additional methods to encapsulate
 * specific commands or functions.
 * Create the appropriate object using new(), or using the device manager,
 * then configure the request. Finally, call submit(). If submit() is not
 * called then the request object must be delete()ed.
 * When the request has completed the callback is invoked, after
 * which the request object is destroyed.
 *
 * Each call to submit() _always_ has a corresponding call to complete(),
 * even if the request is never attempted. This ensures error
 * is reported to the callback and the object released.
 *
 * In brief, ownership of a request belongs with the user up until submit()
 * is called, which passes ownership to the IO mechanism.
 */
class Request : public LinkedObjectTemplate<Request>
{
public:
	using OwnedList = OwnedLinkedObjectListTemplate<Request>;

	/**
	 * @brief Per-request callback
	 * A request goes through the following states:
	 *
	 *   - submitted  request.submit()
	 *   - queued     Controller places request on internal queue
	 *   - executed   Controller retrieves request from queue
	 *   - completed  Request fully handled, status indicates success/failure
	 *
	 * A Controller invokes this callback twice, when a request is about to
	 * be executed and again when it has completed.
	 * Call `request.isPending()` to determine which.
	 */
	using Callback = Delegate<void(const Request& request)>;

	Request(Device& device) : m_device(device)
	{
		debug_d("Request %p created", this);
	}

	// Prevent copying; if required, add `virtual clone()`
	Request(const Request&) = delete;

	virtual ~Request()
	{
		debug_d("Request %p (%s) destroyed", this, m_id.c_str());
	}

	Device& device() const
	{
		return m_device;
	}

	ErrorCode error() const
	{
		return m_error;
	}

	bool isPending() const
	{
		return m_error == Error::pending;
	}

	String caption();

	virtual ErrorCode parseJson(JsonObjectConst json);

	/**
	 * @brief Submit a request
	 *
	 * The request is added to the controller's queue.
	 * If the queue is empty, it starts execution immediately.
	 * The result of the request is posted to the callback routine.
	 */
	virtual void submit();

	/*
	 * Usually called by device or controller, but can also be used to
	 * pass a request to its callback first, for example on a configuration
	 * error.
	 */
	void complete(ErrorCode err);

	/*
	 * Get response as JSON message, in textual format
	 */
	virtual void getJson(JsonObject json) const;

	void setID(const String& value)
	{
		m_id = value;
	}

	void setCommand(Command cmd)
	{
		debug_d("setCommand(0x%08x: %s)", cmd, toString(cmd).c_str());
		m_command = cmd;
	}

	void onComplete(Callback callback)
	{
		m_callback = callback;
	}

	bool nodeQuery(DevNode node)
	{
		m_command = Command::query;
		return setNode(node);
	}

	bool nodeOff(DevNode node)
	{
		setCommand(Command::off);
		return setNode(node);
	}

	bool nodeOn(DevNode node)
	{
		setCommand(Command::on);
		return setNode(node);
	}

	bool nodeToggle(DevNode node)
	{
		setCommand(Command::toggle);
		return setNode(node);
	}

	virtual bool nodeAdjust(DevNode node, int value)
	{
		return false;
	}

	virtual bool setNode(DevNode node)
	{
		return false;
	}

	virtual DevNode::States getNodeStates(DevNode node)
	{
		return DevNode::State::unknown;
	}

	/*
	 * Generic set state command. 0 is off, otherwise on.
	 * Dimmable nodes use percentage level 0 - 100.
	 */
	virtual bool setNodeState(DevNode node, DevNode::State state)
	{
		if(state == DevNode::State::on) {
			setCommand(Command::on);
		} else if(state == DevNode::State::off) {
			setCommand(Command::off);
		} else {
			return false;
		}
		return setNode(node);
	}

	const CString& id() const
	{
		return m_id;
	}

	Command command() const
	{
		return m_command;
	}

	virtual void handleEvent(Event event);

private:
	Device& m_device;
	Callback m_callback;
	Command m_command{Command::undefined}; ///< Active command
	ErrorCode m_error{Error::pending};
	CString m_id; ///< User assigned request ID
};

} // namespace IO
