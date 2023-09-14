/**
 * DMX512/Request.cpp
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

#include <IO/DMX512/Request.h>
#include <IO/Strings.h>
#include <SimpleTimer.h>

namespace IO::DMX512
{
/*
 * We don't need to use the queue as requests do not perform any I/O.
 * Instead, device state is updated and echoed on next slave update.
 */
void Request::submit()
{
	// Only update command gets queued
	if(getCommand() == Command::update) {
		return IO::Request::submit();
	}

	// All others are executed immediately
	auto err = getDevice().execute(*this);
	if(err < 0) {
		debug_e("Request failed, %s", Error::toString(err).c_str());
	}
	complete(err);
}

ErrorCode Request::parseJson(JsonObjectConst json)
{
	ErrorCode err = IO::Request::parseJson(json);
	if(err) {
		return err;
	}
	value = json[FS_value];
	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);
	const auto& dev = const_cast<Request*>(this)->getDevice();

	if(getCommand() == Command::query && devNode == DevNode_ALL) {
		auto nodes = json.createNestedArray(FS_nodes);
		auto values = json.createNestedArray(FS_value);
		auto n = dev.maxNodes();
		for(unsigned i = 0; i < n; ++i) {
			nodes.add(i);
			values.add(dev.getNodeData(i).getValue());
		}

		return;
	}

	json[FS_node] = devNode.id;
	json[FS_value] = dev.getNodeData(devNode.id).getValue();
}

bool Request::setNode(DevNode node)
{
	if(!getDevice().isValid(node)) {
		return false;
	}

	devNode = node;
	return true;
}

} // namespace IO::DMX512
