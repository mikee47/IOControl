#include "DevNode.h"

namespace IO
{
DevNode::State getState(DevNode::States states)
{
	using State = DevNode::State;

	if(states.none()) {
		return State::unknown;
	}

	if(states == State::on) {
		return State::on;
	}

	if(states == State::off) {
		return State::off;
	}

	return State::someon;
}

}; // namespace IO
