/**
 * Custom/Device.cpp
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

#include <IO/Custom/Device.h>
#include <IO/Custom/Request.h>
#include <IO/Strings.h>

namespace IO::Custom
{
IO::Request* Device::createRequest()
{
	return new Request(*this);
}

void Device::getRequestJson(const Request& request, JsonObject json) const
{
	auto n = maxNodes();
	if(n <= 1 || request.getCommand() != Command::query) {
		json[FS_value] = request.getValue();
		json[FS_node] = request.getNode().id;
		return;
	}

	auto nodes = json.createNestedArray(FS_nodes);
	auto values = json.createNestedArray(FS_value);
	for(auto i = nodeIdMin(); i <= nodeIdMax(); ++i) {
		nodes.add(i);
		values.add(getNodeValue({i}));
	}
}

} // namespace IO::Custom
