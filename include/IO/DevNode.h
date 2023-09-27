/**
 * DevNode.h
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

#include <Data/BitSet.h>
#include <vector>

namespace IO
{
/**
 * @brief Identifies a device node
 */
struct DevNode {
	using List = std::vector<DevNode>;
	using ID = uint16_t;
	ID id{};

	// For a normal on/off output node
	enum class State {
		off,
		on,
		someon,
		unknown,
		MAX,
	};

	using States = BitSet<uint8_t, State, size_t(State::MAX)>;

	bool operator==(const DevNode& other) const
	{
		return id == other.id;
	}
};

// Special value to indicate all nodes
static constexpr DevNode DevNode_ALL{0xFFFF};

DevNode::State getState(DevNode::States states);

} // namespace IO
