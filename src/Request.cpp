/**
 * Request.cpp
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

#include <IO/Request.h>
#include <IO/Device.h>
#include <IO/Strings.h>
#include <FlashString/Vector.hpp>

namespace IO
{
#define XX(tag, comment) DEFINE_FSTR(cmdstr_##tag, #tag)
IOCOMMAND_MAP(XX)
#undef XX

#define XX(tag, comment) &cmdstr_##tag,
DEFINE_FSTR_VECTOR(commandStrings, FSTR::String, IOCOMMAND_MAP(XX))
#undef XX

String toString(Command cmd)
{
	return commandStrings[unsigned(cmd)];
}

bool fromString(Command& cmd, const char* str)
{
	auto i = commandStrings.indexOf(str);
	if(i < 0) {
		debug_w("Unknown IO command '%s'", str);
		return false;
	}

	cmd = Command(i);
	return true;
}

ErrorCode Request::parseJson(JsonObjectConst json)
{
	const char* id;
	if(Json::getValue(json[FS_id], id)) {
		m_id = id;
	}

	// Command is optional - may have already been set
	const char* cmd;
	if(Json::getValue(json[FS_command], cmd) && !fromString(m_command, cmd)) {
		return Error::bad_command;
	}

	DevNode node;
	JsonArrayConst arr;
	if(Json::getValue(json[FS_node], node.id)) {
		unsigned count = json[FS_count] | 1;
		while(count--) {
			if(!setNode(node)) {
				return Error::bad_node;
			}
			node.id++;
		}
	} else if(Json::getValue(json[FS_nodes], arr)) {
		for(DevNode::ID id : arr) {
			if(!setNode(DevNode{id})) {
				return Error::bad_node;
			}
		}
	}
	// all nodes
	else if(!setNode(DevNode_ALL)) {
		return Error::bad_node;
	}

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	if(m_id.length() != 0) {
		json[FS_id] = m_id.c_str();
	}
	json[FS_command] = toString(m_command);
	json[FS_device] = m_device.id().c_str();
	setError(json, m_error);
}

void Request::submit()
{
	m_device.submit(this);
}

void Request::handleEvent(Event event)
{
	m_device.handleEvent(this, event);
}

/*
 * Request has completed. Device will destroy Notify originator then destroy this request.
 */
void Request::complete(ErrorCode err)
{
	debug_i("Request %p (%s) complete - %s", this, m_id.c_str(), Error::toString(err).c_str());
	assert(err != Error::pending);
	m_error = err;
	if(m_callback) {
		m_callback(*this);
	}
	handleEvent(Event::RequestComplete);
}

String Request::caption() const
{
	String s(uint32_t(this), HEX);
	s += " (";
	s += m_device.caption();
	s += '/';
	s += m_id.c_str();
	s += ')';
	return s;
}

} // namespace IO
