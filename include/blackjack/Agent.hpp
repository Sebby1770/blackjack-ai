#pragma once

#include <string>
#include <vector>

#include "blackjack/Action.hpp"

namespace blackjack {

// Abstract policy: given a state and the legal actions, return one of them.
// Every decision maker in the project -- learned or hand-coded -- implements
// this interface, which lets the evaluator score them all identically.
class Agent {
public:
    virtual ~Agent() = default;

    // Choose one of `legal` for the given `state`. Must be deterministic and
    // side-effect free so evaluation is reproducible (training mutates state
    // through separate, agent-specific methods).
    virtual Action act(const State& state,
                       const std::vector<Action>& legal) const = 0;

    virtual std::string name() const = 0;
};

} // namespace blackjack
