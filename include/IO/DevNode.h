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
	ID id;

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
