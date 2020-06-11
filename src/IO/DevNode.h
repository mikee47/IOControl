#pragma once

#include "BitSet.h"

namespace IO
{
/**
 * @brief Identifies a device node
 */
struct DevNode {
	using ID = uint16_t;
	ID id;

	// For a normal on/off output node
	enum class State {
		off,
		on,
		someon,
		unknown,
	};

	using States = BitSet<uint8_t, State>;

	bool operator==(const DevNode& other) const
	{
		return id == other.id;
	}
};

// Special value to indicate all nodes
static constexpr DevNode DevNode_ALL{0xFFFF};

DevNode::State getState(DevNode::States states);

} // namespace IO
