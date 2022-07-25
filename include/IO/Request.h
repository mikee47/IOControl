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

} // namespace IO

String toString(IO::Command cmd);
bool fromString(IO::Command& cmd, const char* str);

namespace IO
{
class Device;
class Request;

/**
 * @brief Request represents a single user request/response over a bus.
 *
 * Inherited classes provide additional methods to encapsulate
 * specific commands or functions.
 * Create the appropriate object using new() or the device manager,
 * then configure the request. Finally, call submit(). If submit() is not
 * called then the request object must be deleted.
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

	Request(Device& device) : device(device)
	{
		debug_d("Request %p created", this);
	}

	// Prevent copying; if required, add `virtual clone()`
	Request(const Request&) = delete;

	virtual ~Request()
	{
		debug_d("Request %p (%s) destroyed", this, requestId.c_str());
	}

	/**
	 * @brief Request error code defaults to 'pending' and is set on completion
	 */
	ErrorCode error() const
	{
		return errorCode;
	}

	bool isPending() const
	{
		return errorCode == Error::pending;
	}

	/**
	 * @brief Get a descriptive caption for this request
	 */
	String caption() const;

	/**
	 * @brief Fill this request from a JSON description
	 */
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

	/**
	 * @brief Get result of a completed request in JSON format
	 */
	virtual void getJson(JsonObject json) const;

	/**
	 * @brief Request identifiers are optional, useful for tracking remote requests
	 */
	void setID(const String& value)
	{
		requestId = value;
	}

	/**
	 * @brief Set the command code
	 */
	void setCommand(Command cmd)
	{
		debug_d("setCommand(0x%08x: %s)", cmd, toString(cmd).c_str());
		command = cmd;
	}

	/**
	 * @brief Set the request completion callback
	 */
	void onComplete(Callback callback)
	{
		this->callback = callback;
	}

	bool nodeQuery(DevNode node)
	{
		command = Command::query;
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

	/**
	 * @brief For nodes supporting analogue state (e.g. brightness)
	 * @{
	 */
	bool nodeSet(DevNode node, int value)
	{
		setCommand(Command::set);
		return setNode(node) && setValue(value);
	}

	bool nodeAdjust(DevNode node, int value)
	{
		setCommand(Command::adjust);
		return setNode(node) && setValue(value);
	}
	/** @} /

	/**
	 * @brief If nodes are supported, implement this method
	 */
	virtual bool setNode(DevNode node)
	{
		return false;
	}

	/**
	 * @brief If nodes support values, implement this method
	 */
	virtual bool setValue(int value)
	{
		return false;
	}

	/**
	 * @brief Query node status from response
	 */
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

	/**
	 * @brief Get the request ID, if there is one
	 */
	const CString& id() const
	{
		return requestId;
	}

	Command getCommand() const
	{
		return command;
	}

	/**
	 * @brief Implementations may override this method as required
	 */
	virtual void handleEvent(Event event);

	Device& device;

private:
	Callback callback;
	Command command{Command::undefined}; ///< Active command
	ErrorCode errorCode{Error::pending};
	CString requestId; ///< User assigned request ID
};

} // namespace IO
