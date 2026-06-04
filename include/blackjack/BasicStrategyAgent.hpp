#pragma once

#include "blackjack/Agent.hpp"

namespace blackjack {

// Textbook basic strategy encoded as a decision table (hard totals, soft
// totals, and doubling). It learns nothing -- it is the near-optimal benchmark
// the reinforcement-learning agents are measured against.
class BasicStrategyAgent : public Agent {
public:
    Action act(const State& s, const std::vector<Action>& legal) const override;
    std::string name() const override { return "Basic-Strategy"; }
};

} // namespace blackjack
